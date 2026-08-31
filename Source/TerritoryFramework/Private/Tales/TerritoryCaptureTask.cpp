#include "Tales/TerritoryCaptureTask.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Navigation/MapMarker.h"
#include "Engine/World.h"

void UTerritoryCaptureTask::BeginTask()
{
	Super::BeginTask();

	if (!OwningComp)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureTask] BeginTask called with no OwningComp"));
		return;
	}

	UWorld* World = OwningComp->GetWorld();
	if (!World)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureTask] BeginTask: no World"));
		return;
	}

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureTask] BeginTask: no Registry subsystem"));
		return;
	}
	Registry->OnTerritoryRegistered.AddUniqueDynamic(
		this, &UTerritoryCaptureTask::OnTerritoryRegistered);
	Registry->OnTerritoryUnregistered.AddUniqueDynamic(
		this, &UTerritoryCaptureTask::OnTerritoryUnregistered);

	CachedTerritory = Registry->GetTerritoryByTag(TargetTerritoryTag);

	if (!CachedTerritory.IsValid())
	{
		if (const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
			Settings && Settings->ShouldDebugTales())
		{
			UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureTask] Territory '%s' not registered yet — waiting for registration"),
				*TargetTerritoryTag.ToString());
		}
		bWaitingForRegistration = true;
		return;
	}

	CachedTerritory->OnTerritoryOwnershipChanged.AddUniqueDynamic(
		this, &UTerritoryCaptureTask::OnTerritoryControlChanged);

	// Store the initial owner for loss detection
	InitialOwner = CachedTerritory->GetOwningFaction();

	// Check if already in the desired state
	if (bCompleteOnLoss)
	{
		// Complete if already unclaimed
		if (!CachedTerritory->GetOwningFaction().IsValid())
		{
			CompleteTask();
			return;
		}
	}
	else if (RequiredCapturingFaction.IsValid())
	{
		if (CachedTerritory->IsOwnedByFaction(RequiredCapturingFaction))
		{
			CompleteTask();
			return;
		}
	}
}

void UTerritoryCaptureTask::EndTask()
{
	if (CachedTerritory.IsValid())
	{
		CachedTerritory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryCaptureTask::OnTerritoryControlChanged);
	}

	if (UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr)
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryCaptureTask::OnTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryCaptureTask::OnTerritoryUnregistered);
		}
	}
	bWaitingForRegistration = false;

	Super::EndTask();
}

void UTerritoryCaptureTask::OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (Territory != CachedTerritory.Get()) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugTales())
	{
		UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureTask] %s ownership changed: %s → %s"),
			*TargetTerritoryTag.ToString(), *OldOwner.ToString(), *NewOwner.ToString());
	}

	if (bCompleteOnLoss)
	{
		if (InitialOwner.IsValid() && (!NewOwner.IsValid() || NewOwner != InitialOwner))
		{
			CompleteTask();
		}
	}
	else if (RequiredCapturingFaction.IsValid())
	{
		if (NewOwner == RequiredCapturingFaction)
		{
			CompleteTask();
		}
	}
	else if (NewOwner.IsValid())
	{
		CompleteTask();
	}
}

void UTerritoryCaptureTask::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (!Territory || Territory->GetTerritoryTag() != TargetTerritoryTag) return;

	// Territory just registered — bind now
	CachedTerritory = Territory;
	bWaitingForRegistration = false;

	Territory->OnTerritoryOwnershipChanged.AddUniqueDynamic(
		this, &UTerritoryCaptureTask::OnTerritoryControlChanged);

	InitialOwner = Territory->GetOwningFaction();

	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugTales())
	{
		UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureTask] Territory '%s' late-bound after registration"),
			*TargetTerritoryTag.ToString());
	}

	// Check if already in desired state
	if (bCompleteOnLoss)
	{
		if (!Territory->GetOwningFaction().IsValid())
		{
			CompleteTask();
		}
	}
	else if (RequiredCapturingFaction.IsValid())
	{
		if (Territory->IsOwnedByFaction(RequiredCapturingFaction))
		{
			CompleteTask();
		}
	}

	if (SpawnedMarker)
	{
		SpawnedMarker->RemoveMarker();
		SpawnedMarker = nullptr;
		SpawnDefaultNavigationMarker();
	}
}

void UTerritoryCaptureTask::OnTerritoryUnregistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)bWasUnregistered;
	if (!Territory || Territory != CachedTerritory.Get()) return;
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(
		this, &UTerritoryCaptureTask::OnTerritoryControlChanged);
	CachedTerritory.Reset();
	bWaitingForRegistration = true;
}

FText UTerritoryCaptureTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmpty()) return DescriptionOverride;

	if (bCompleteOnLoss)
	{
		return FText::FromString(FString::Printf(TEXT("Lose control of %s"), *TargetTerritoryTag.ToString()));
	}

	if (RequiredCapturingFaction.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("Capture %s for %s"),
			*TargetTerritoryTag.ToString(), *RequiredCapturingFaction.ToString()));
	}

	return FText::FromString(FString::Printf(TEXT("Capture %s"), *TargetTerritoryTag.ToString()));
}

FText UTerritoryCaptureTask::GetTaskProgressText_Implementation() const
{
	if (CachedTerritory.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("%.0f%%"), CachedTerritory->GetControlProgress() * 100.f));
	}
	return Super::GetTaskProgressText_Implementation();
}

FVector UTerritoryCaptureTask::GetNavigationMarkerLocation_Implementation() const
{
	return CachedTerritory.IsValid()
		? FVector::ZeroVector : MarkerSettings.ActorFallbackLocation;
}

AActor* UTerritoryCaptureTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return CachedTerritory.Get();
}

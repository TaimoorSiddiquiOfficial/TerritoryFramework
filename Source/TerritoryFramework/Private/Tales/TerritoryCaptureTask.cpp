#include "Tales/TerritoryCaptureTask.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TalesComponent.h"
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

	CachedTerritory = Registry->GetTerritoryByTag(TargetTerritoryTag);

	if (!CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureTask] Territory '%s' not found in registry"),
			*TargetTerritoryTag.ToString());
		return;
	}

	CachedTerritory->OnTerritoryControlChanged.AddDynamic(this, &UTerritoryCaptureTask::OnTerritoryControlChanged);

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
		CachedTerritory->OnTerritoryControlChanged.RemoveDynamic(this, &UTerritoryCaptureTask::OnTerritoryControlChanged);
	}

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
		// Complete when territory is lost by its initial owner.
		// Only fires if there WAS an initial owner — capturing an unclaimed territory is not a "loss".
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

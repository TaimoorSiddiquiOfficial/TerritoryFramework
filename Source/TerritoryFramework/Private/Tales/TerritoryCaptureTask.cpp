#include "Tales/TerritoryCaptureTask.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"

void UTerritoryCaptureTask::BeginTask()
{
	Super::BeginTask();

	if (!OwningComp) return;

	UWorld* World = OwningComp->GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		CachedTerritory = Registry->GetTerritoryByTag(TargetTerritoryTag);
	}

	if (CachedTerritory.IsValid())
	{
		CachedTerritory->OnTerritoryControlChanged.AddDynamic(this, &UTerritoryCaptureTask::OnTerritoryControlChanged);

		// Check if already captured
		if (bCompleteOnLoss)
		{
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

	if (bCompleteOnLoss)
	{
		if (!NewOwner.IsValid())
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

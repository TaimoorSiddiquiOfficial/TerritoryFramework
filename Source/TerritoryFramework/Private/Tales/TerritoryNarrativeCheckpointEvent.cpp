#include "Tales/TerritoryNarrativeCheckpointEvent.h"

#include "ArsenalSettings.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "TerritoryNarrativeCheckpointEvent"

namespace TerritoryNarrativeCheckpoint
{
	bool ReadActiveSave(const UNarrativeSaveSubsystem* SaveSubsystem,
		FString& OutName, int32& OutSlot)
	{
		if (!SaveSubsystem) return false;

		const FStrProperty* NameProperty = FindFProperty<FStrProperty>(
			SaveSubsystem->GetClass(), TEXT("CurrentSaveName"));
		const FIntProperty* SlotProperty = FindFProperty<FIntProperty>(
			SaveSubsystem->GetClass(), TEXT("CurrentSaveSlot"));
		if (!NameProperty || !SlotProperty) return false;

		OutName = NameProperty->GetPropertyValue_InContainer(SaveSubsystem);
		OutSlot = SlotProperty->GetPropertyValue_InContainer(SaveSubsystem);
		return !OutName.IsEmpty() && OutSlot >= 0;
	}

	FString BuildFallbackName(const int32 CampaignIndex)
	{
		const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
		const FString Prefix = Settings && !Settings->DefaultSaveName.IsEmpty()
			? Settings->DefaultSaveName : TEXT("NarrativeSave");
		return Prefix + FString::FromInt(FMath::Max(0, CampaignIndex));
	}
}

UTerritoryNarrativeCheckpointEvent::UTerritoryNarrativeCheckpointEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EventRuntime = EEventRuntime::Start;
	EventFilter = EEventFilter::EF_Anyone;
	bRefireOnLoad = false;
}

void UTerritoryNarrativeCheckpointEvent::ExecuteEvent_Implementation(
	APawn* Target, APlayerController* Controller,
	UTalesComponent* NarrativeComponent)
{
	Super::ExecuteEvent_Implementation(Target, Controller, NarrativeComponent);
	if (!TerritoryTales::DoEventConditionsPass(
		this, Target, Controller, NarrativeComponent)) return;
	if (!NarrativeComponent || !NarrativeComponent->HasAuthority()) return;

	UWorld* World = NarrativeComponent->GetWorld();
	UNarrativeSaveSubsystem* SaveSubsystem = World
		? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
	if (!World || !SaveSubsystem)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[QuestCheckpoint] Narrative Save subsystem is unavailable; checkpoint was not written."));
		return;
	}

	FString SaveName = SaveNameOverride;
	int32 SaveSlot = FMath::Max(0, PlatformUserSlot);
	if (SaveName.IsEmpty())
	{
		if (!TerritoryNarrativeCheckpoint::ReadActiveSave(
			SaveSubsystem, SaveName, SaveSlot))
		{
			SaveName = TerritoryNarrativeCheckpoint::BuildFallbackName(
				FallbackCampaignIndex);
			SaveSlot = FMath::Max(0, PlatformUserSlot);
		}
	}

	// Let the complete quest state transition and its other Narrative events
	// settle before the world snapshot is serialized.
	TWeakObjectPtr<UNarrativeSaveSubsystem> WeakSaveSubsystem = SaveSubsystem;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakSaveSubsystem, SaveName, SaveSlot]()
		{
			UNarrativeSaveSubsystem* Subsystem = WeakSaveSubsystem.Get();
			if (!Subsystem) return;
			if (!Subsystem->Save(SaveName, SaveSlot))
			{
				UE_LOG(LogTerritory, Warning,
					TEXT("[QuestCheckpoint] Could not save '%s' (platform slot %d). Saving may be disabled or the write failed."),
					*SaveName, SaveSlot);
			}
			else
			{
				UE_LOG(LogTerritory, Verbose,
					TEXT("[QuestCheckpoint] Saved current Narrative quest state to '%s' (platform slot %d)."),
					*SaveName, SaveSlot);
			}
		}));
}

FString UTerritoryNarrativeCheckpointEvent::GetGraphDisplayText_Implementation()
{
	return SaveNameOverride.IsEmpty()
		? TEXT("Save current Narrative campaign checkpoint")
		: FString::Printf(TEXT("Save checkpoint: %s"), *SaveNameOverride);
}

FText UTerritoryNarrativeCheckpointEvent::GetHintText_Implementation()
{
	return LOCTEXT("CheckpointHint", "Save checkpoint");
}

#undef LOCTEXT_NAMESPACE

#include "AI/TerritoryNPCActivityComponent.h"

#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/StrongObjectPtr.h"

void UTerritoryNPCActivityComponent::PrepareForSave_Implementation()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;
	// Narrative 2.4.2 resets activities and goals, but appends generators. A fresh
	// snapshot must describe only generators that still exist at this save point.
	SavedGoalGenerators.Reset();
	Super::PrepareForSave_Implementation();
}

void UTerritoryNPCActivityComponent::Load_Implementation()
{
	ANarrativeNPCController* Controller = Cast<ANarrativeNPCController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority()
		|| Controller->IsActorBeingDestroyed() || !GetWorld()
		|| GetWorld()->bIsTearingDown) return;
	// Narrative can restore actors before BeginPlay. Its normal cache is not set yet;
	// use the same owning controller so saved generators initialize safely.
	OwnerController = Controller;

	// Older saves can contain every previous snapshot. Last occurrence wins;
	// preserve its original order so loading does not depend on hash iteration.
	TSet<UClass*> Seen;
	for (int32 Index = SavedGoalGenerators.Num() - 1; Index >= 0; --Index)
	{
		UClass* Class = SavedGoalGenerators[Index].Class.Get();
		if (!IsValid(Class) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| IgnoredSavedGoalGeneratorClasses.Contains(Class) || Seen.Contains(Class))
		{
			SavedGoalGenerators.RemoveAt(Index);
			continue;
		}
		Seen.Add(Class);
	}

	// Narrative skips AddGoalGenerator when the definition already created that
	// class. Restore its saved fields before Native performs activity selection,
	// keeping the live object and its existing event bindings intact.
	for (const FSavedNPCGoalGenerator& Record : SavedGoalGenerators)
	{
		if (UNPCGoalGenerator* Generator = GetGoalGenerator(Record.Class))
		{
			// Native records are deltas from class defaults. Loading those bytes
			// directly onto a changed live object leaves omitted default values stale.
			// Expand the record on an uninitialized temporary object, then apply its
			// full saved state. Never initialize it or attach a second set of callbacks.
			TStrongObjectPtr<UNPCGoalGenerator> Snapshot(
				NewObject<UNPCGoalGenerator>(this, Record.Class, NAME_None, RF_Transient));
			FMemoryReader Reader(Record.Data);
			FObjectAndNameAsStringProxyArchive Archive(Reader, true);
			Archive.ArIsSaveGame = true;
			Snapshot->Serialize(Archive);
			TArray<uint8> FullState;
			FMemoryWriter Writer(FullState);
			FObjectAndNameAsStringProxyArchive FullArchive(Writer, true);
			FullArchive.ArIsSaveGame = true;
			FullArchive.ArNoDelta = true;
			Snapshot->Serialize(FullArchive);
			FMemoryReader FullReader(FullState);
			FObjectAndNameAsStringProxyArchive LoadArchive(FullReader, true);
			LoadArchive.ArIsSaveGame = true;
			Generator->Serialize(LoadArchive);
		}
	}
	Super::Load_Implementation();
}

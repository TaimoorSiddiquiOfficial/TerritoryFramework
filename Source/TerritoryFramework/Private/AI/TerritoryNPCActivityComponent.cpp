#include "AI/TerritoryNPCActivityComponent.h"

#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Perception/AIPerceptionComponent.h"
#include "TimerManager.h"
#include "Misc/EngineVersionComparison.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/StrongObjectPtr.h"

void UTerritoryNPCActivityComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		// Keep Native's existing selection timer. Its sight sense reports visibility
		// edges, not later changes to the stored strength or Territory attitude.
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_RescoreGoals, this,
			&UTerritoryNPCActivityComponent::RefreshPerceptionAndRescore,
			FMath::Max(0.05f, RescoreInterval), true);
	}
}

void UTerritoryNPCActivityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RescoreGoals);
	DeliveredPerception.Reset();
	DeliveryGenerators.Reset();
	DeliveryPawn.Reset();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryNPCActivityComponent::RefreshPerceptionAndRescore()
{
	RefreshStoredPerception();
	if (IsValid(GetOwner()) && GetOwner()->HasAuthority()
		&& !GetOwner()->IsActorBeingDestroyed() && IsActive()
		&& GetWorld() && !GetWorld()->bIsTearingDown)
	{
		RescoreGoals();
	}
}

void UTerritoryNPCActivityComponent::RefreshStoredPerception()
{
	ANarrativeNPCController* Controller = Cast<ANarrativeNPCController>(GetOwner());
	ANarrativeNPCCharacter* NPC = Controller ? Controller->GetControlledNPC() : nullptr;
	if (bDeliveringPerception) return;
	if (!IsValid(Controller) || !Controller->HasAuthority()
		|| Controller->IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown
		|| !IsActive() || !IsValid(NPC) || NPC->IsActorBeingDestroyed()
		|| !NPC->IsAlive() || !FTerritoryNarrativeProAdapter::IsCharacterReady(NPC))
	{
		DeliveredPerception.Reset();
		DeliveryPawn.Reset();
		return;
	}
	UAIPerceptionComponent* Perception = Controller->GetAIPerceptionComponent();
	if (!IsValid(Perception) || Perception->GetOwner() != Controller) return;

	// Native exposes this dispatcher in BP_NarrativeNPCController. Forward its
	// real signature to its currently installed generators only. Do not broadcast
	// a fabricated engine sense event or rerun the controller's greeting logic.
	const FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(
		Controller->GetClass(), TEXT("OnPerceptionUpdated"));
	if (!Property || !Property->SignatureFunction || Property->SignatureFunction->NumParms != 2) return;
	const FObjectPropertyBase* ActorParameter = FindFProperty<FObjectPropertyBase>(
		Property->SignatureFunction, TEXT("Actor"));
	const FStructProperty* StimulusParameter = FindFProperty<FStructProperty>(
		Property->SignatureFunction, TEXT("Stimulus"));
	if (!ActorParameter || ActorParameter->PropertyClass != AActor::StaticClass()
		|| !StimulusParameter || StimulusParameter->Struct != FAIStimulus::StaticStruct()) return;
	const FMulticastScriptDelegate* NativeDelegate = Property->GetMulticastDelegate(
		Property->ContainerPtrToValuePtr<void>(Controller));
	if (!NativeDelegate) return;
	FMulticastScriptDelegate CurrentGenerators = *NativeDelegate;
	TArray<TWeakObjectPtr<UNPCGoalGenerator>> Generators;
	for (UObject* Listener : NativeDelegate->GetAllObjects())
	{
		UNPCGoalGenerator* Generator = Cast<UNPCGoalGenerator>(Listener);
		if (!IsValid(Generator) || !GoalGenerators.Contains(Generator))
			CurrentGenerators.RemoveAll(Listener);
		else Generators.AddUnique(Generator);
	}
	if (Generators.IsEmpty()) return;
	if (DeliveryPawn.Get() != NPC || DeliveryGenerators != Generators)
	{
		DeliveredPerception.Reset();
		DeliveryPawn = NPC;
		DeliveryGenerators = Generators;
	}

	TGuardValue<bool> Delivering(bDeliveringPerception, true);
	TArray<AActor*> KnownActors;
	Perception->GetKnownPerceivedActors(nullptr, KnownActors);
	TSet<TWeakObjectPtr<AActor>> Retained;
	for (AActor* Target : KnownActors)
	{
		if (!IsValid(Target) || Target == NPC || Target->IsActorBeingDestroyed()
			|| Target->GetWorld() != GetWorld()) continue;
		if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Target);
			Character && !Character->IsAlive()) continue;
		FActorPerceptionBlueprintInfo Info;
		if (!Perception->GetActorsPerception(Target, Info)) continue;
		const ETeamAttitude::Type Attitude = CastChecked<INarrativeTeamAgentInterface>(NPC)->GetTeamAttitudeTowards(*Target);
		const FDeliveredPerception* Previous = DeliveredPerception.Find(Target);
		TArray<FAIStimulus> ToDeliver;
		for (int32 Index = 0; Index < Info.LastSensedStimuli.Num(); ++Index)
		{
			const FAIStimulus& Stimulus = Info.LastSensedStimuli[Index];
			if (!Stimulus.Type.IsValid() || !Stimulus.WasSuccessfullySensed()
				|| Stimulus.IsExpired() || !FMath::IsFinite(Stimulus.Strength)) continue;
			const FAIStimulus* Old = Previous && Previous->Stimuli.IsValidIndex(Index)
				? &Previous->Stimuli[Index] : nullptr;
			if (!Old || Previous->Attitude != Attitude || Old->Type != Stimulus.Type
				|| !Old->WasSuccessfullySensed()
				|| !FMath::IsNearlyEqual(Old->Strength, Stimulus.Strength))
			{
				ToDeliver.Add(Stimulus);
			}
		}
		// Copy/cache before calling Blueprint; callbacks can remove actors or goals.
		DeliveredPerception.Add(Target, {Info.LastSensedStimuli, Attitude});
		Retained.Add(Target);
		for (const FAIStimulus& Stimulus : ToDeliver)
		{
			if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !IsActive()
				|| !IsValid(Controller) || Controller->IsActorBeingDestroyed()
				|| Controller->GetControlledNPC() != NPC || !NPC->IsAlive()) return;
			FStructOnScope Parameters(Property->SignatureFunction);
			ActorParameter->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Target);
			StimulusParameter->CopyCompleteValue(
				StimulusParameter->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &Stimulus);
			// Each callback can remove another generator or stream out its pawn.
			// Filter a copy per listener so no stale listener runs later in the batch.
			for (const TWeakObjectPtr<UNPCGoalGenerator>& Generator : Generators)
			{
				if (!Generator.IsValid() || !GoalGenerators.Contains(Generator.Get())) continue;
				if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !IsActive()
					|| Controller->GetControlledNPC() != NPC || !IsValid(NPC)
					|| NPC->IsActorBeingDestroyed() || !NPC->IsAlive()) return;
				FMulticastScriptDelegate SingleGenerator = CurrentGenerators;
				for (UObject* Listener : SingleGenerator.GetAllObjects())
					if (Listener != Generator.Get()) SingleGenerator.RemoveAll(Listener);
#if UE_VERSION_OLDER_THAN(5, 8, 0)
				SingleGenerator.ProcessMulticastDelegate<UObject>(Parameters.GetStructMemory());
#else
				SingleGenerator.ProcessDelegate<UObject>(Parameters.GetStructMemory());
#endif
			}
		}
	}
	for (auto It = DeliveredPerception.CreateIterator(); It; ++It)
		if (!Retained.Contains(It.Key())) It.RemoveCurrent();
}

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
	// Observations remain Native-owned. Re-deliver only after its restored
	// generators and pawn are ready; no delivery cache is part of the save record.
	DeliveredPerception.Reset();
	DeliveryGenerators.Reset();
	DeliveryPawn.Reset();

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

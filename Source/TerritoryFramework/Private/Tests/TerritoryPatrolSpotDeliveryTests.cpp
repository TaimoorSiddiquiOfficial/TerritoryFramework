#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCGoalItem.h"
#include "NarrativeActorProvider.h"
#include "AI/NarrativeNPCController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/NPCInteractionComponent.h"
#include "Tests/TerritoryPatrolSpotTestFixtures.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

namespace
{
	UNarrativeInteractableComponent* MakeDeliveryProp(UWorld* World, const FVector& Location)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) return nullptr;
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		UNarrativeInteractableComponent* Interactable = NewObject<UNarrativeInteractableComponent>(Actor);
		Interactable->RegisterComponent();
		Interactable->InteractionSlots.SetNum(2);
		Interactable->SlotStatuses.SetNum(2);
		for (FInteractionSlotConfig& Slot : Interactable->InteractionSlots)
		{
			Slot.SlotInteractBehavior = NewObject<UTerritoryTestSlotBehavior>(Interactable);
		}
		return Interactable;
	}
}

/**
 * A patrol node's Spot is delivered to the guard through Narrative Pro's interaction component,
 * and Narrative Pro keeps that component on the NPC's *controller* rather than on the pawn:
 * ANarrativeNPCController creates it and the vendor's own interact graph reads it back as
 * OwnerController.InteractionComponent.
 *
 * This test drives the production delivery path, so it fails if that path goes back to asking only
 * the character - which no guard can satisfy, and which leaves every authored spot undelivered
 * while looking exactly like a spot that found no prop. The premise is asserted rather than
 * assumed, because a future Narrative release could move the component back onto the pawn and this
 * would then be the test that says so.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolSpotDelivery,
	"TerritoryFramework.Guards.Regression.PatrolSpotClaimerComesFromController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolSpotDelivery::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Patrol spot delivery test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);

	ATerritoryGuardCharacter* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	if (!TestNotNull(TEXT("Territory guard"), Guard))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Read the controller back off the guard rather than keeping the one spawned here. A spawned
	// character can already have auto-spawned its AIControllerClass, and GetNPCController() prefers
	// that cached controller over the pawn's current one - so the controller this test must inspect
	// is whichever one the production path will actually resolve, not the one it happens to create.
	ANarrativeNPCController* Controller = Guard->GetNPCController();
	if (!Controller)
	{
		Controller = World->SpawnActor<ANarrativeNPCController>();
		if (TestNotNull(TEXT("Narrative NPC controller"), Controller))
		{
			Controller->Possess(Guard);
			Controller = Guard->GetNPCController();
		}
	}
	if (!TestNotNull(TEXT("The guard reaches its controller through GetNPCController"), Controller))
	{
		World->DestroyWorld(false);
		return false;
	}

	// ─── The premise the delivery path depends on ───
	TestNull(TEXT("A guard character carries no interaction component of its own"),
		Guard->FindComponentByClass<UNarrativeInteractionComponent>());
	UNPCInteractionComponent* ControllerInteraction = Controller->GetInteractionComponent();
	if (!TestNotNull(TEXT("Its Narrative controller carries the interaction component"),
		ControllerInteraction))
	{
		World->DestroyWorld(false);
		return false;
	}

	UNPCActivityComponent* Activities = Controller->FindComponentByClass<UNPCActivityComponent>();
	if (!TestNotNull(TEXT("The controller owns the activity component"), Activities))
	{
		World->DestroyWorld(false);
		return false;
	}
	Activities->Activate();

	// The goal class has to be exactly the interact activity's SupportedGoalType, because AddGoal
	// files every goal under its own runtime class and GetGoals is a plain map lookup on that key.
	// The vendor's own Goal_Interact is that class.
	UClass* InteractGoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Interact/Goals/Goal_Interact.Goal_Interact_C"));
	if (!TestNotNull(TEXT("The vendor interact goal class loads"), InteractGoalClass))
	{
		World->DestroyWorld(false);
		return false;
	}
	Guard->SpotInteractGoalClass = InteractGoalClass;

	// ─── An enabled spot is delivered, aimed through the controller's component ───
	FTerritoryPatrolNode SpotNode;
	SpotNode.Location = FVector(500.f, 0.f, 0.f);
	SpotNode.Spot.bEnabled = true;
	SpotNode.Spot.SearchRadius = 600.f;
	SpotNode.Spot.SpotActorClass = AActor::StaticClass();
	UNarrativeInteractableComponent* FirstProp = MakeDeliveryProp(World, SpotNode.Location);
	UNarrativeInteractableComponent* SecondProp = MakeDeliveryProp(World, FVector(3000.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("First real interactable"), FirstProp)
		|| !TestNotNull(TEXT("Second real interactable"), SecondProp))
	{
		World->DestroyWorld(false);
		return false;
	}
	// Only slot 1 matches. A prop-only tag check followed by the vendor's unfiltered fallback
	// would choose slot 0 (equal distance, first wins), violating this authoring constraint.
	SpotNode.Spot.SpotTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill"), false);
	if (!TestTrue(TEXT("Fixture slot tag is registered"), SpotNode.Spot.SpotTag.IsValid()))
	{
		World->DestroyWorld(false);
		return false;
	}
	FirstProp->InteractionSlots[1].SlotTag = SpotNode.Spot.SpotTag;
	SecondProp->InteractionSlots[1].SlotTag = SpotNode.Spot.SpotTag;
	TestTrue(TEXT("An enabled spot is delivered to the guard"),
		Guard->DeliverPatrolSpot(SpotNode));

	UNPCGoalItem* Delivered = Guard->TerritorySpotGoal.Get();
	if (TestNotNull(TEXT("The delivered spot goal is cached on the guard"), Delivered))
	{
		TestTrue(TEXT("The delivered goal is the vendor's own interact goal class"),
			Delivered->GetClass() == InteractGoalClass);

		// Verify the real vendor initialization, not only a populated provider: its cached
		// interactable is also the key used for duplicate admission and cleanup.
		FStructProperty* ProviderProperty =
			FindFProperty<FStructProperty>(Delivered->GetClass(), TEXT("ActorToInteractWith"));
		if (TestNotNull(TEXT("The vendor goal still exposes ActorToInteractWith"), ProviderProperty))
		{
			FInstancedActorProvider* Aim =
				ProviderProperty->ContainerPtrToValuePtr<FInstancedActorProvider>(Delivered);
			UNarrativeActorProvider_LevelReference* Provider =
				Aim ? Cast<UNarrativeActorProvider_LevelReference>(Aim->Provider) : nullptr;
			if (TestNotNull(TEXT("The goal binds the resolved actor with Narrative's level provider"), Provider))
			{
				TestTrue(TEXT("The provider keeps the exact resolved actor"),
					Provider->ProvideActor(Guard) == FirstProp->GetOwner());
			}
		}
		TestTrue(TEXT("Vendor Initialize and GetGoalKey use the real interactable"),
			Delivered->GetGoalKey() == FirstProp);
		TestFalse(TEXT("Transient patrol visits are not saved"), Delivered->bSaveGoal);
		TestFalse(TEXT("Vendor startup success must not immediately cancel the visit"), Delivered->bRemoveOnSucceeded);
		TestEqual(TEXT("Narrative owns the authored total visit deadline"), Delivered->GoalLifetime, SpotNode.Spot.MaximumVisitSeconds);
		TestEqual(TEXT("The authored visit score reaches Narrative arbitration"), Delivered->DefaultScore, SpotNode.Spot.GoalScore);
		if (FIntProperty* SlotProperty = FindFProperty<FIntProperty>(Delivered->GetClass(), TEXT("SlotIdx")))
		{
			TestEqual(TEXT("Only the tag-matching slot is delivered"),
				SlotProperty->GetPropertyValue_InContainer(Delivered), 1);
		}
		else AddError(TEXT("Vendor SlotIdx contract is missing"));
	}

	TestTrue(TEXT("Revisiting the same prop replaces the patrol-owned keyed goal"),
		Guard->DeliverPatrolSpot(SpotNode));
	TestEqual(TEXT("Repeat visits keep exactly one registered goal"),
		Activities->GetGoals(InteractGoalClass).Goals.Num(), 1);
	TestTrue(TEXT("A repeat visit creates a fresh initialized goal"), Guard->TerritorySpotGoal.Get() != Delivered);
	UNPCGoalItem* ServerGoal = Guard->TerritorySpotGoal;
	Guard->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A simulated proxy cannot clear an authority-owned visit"),
		Guard->DeliverPatrolSpot(FTerritoryPatrolNode()));
	TestTrue(TEXT("Client refusal preserves the authority goal"), Guard->TerritorySpotGoal == ServerGoal);
	Guard->SetRole(ROLE_Authority);
	SpotNode.Location = SecondProp->GetOwner()->GetActorLocation();
	TestTrue(TEXT("A different spot is delivered after the first"), Guard->DeliverPatrolSpot(SpotNode));
	TestEqual(TEXT("Moving to another spot replaces the previous goal"),
		Activities->GetGoals(InteractGoalClass).Goals.Num(), 1);
	if (TestNotNull(TEXT("The retargeted goal is cached"), Guard->TerritorySpotGoal.Get()))
	{
		TestTrue(TEXT("Vendor cached target and unique-map key move to the second prop"),
			Guard->TerritorySpotGoal->GetGoalKey() == SecondProp);
	}
	bool bFoundOld = false;
	TestNull(TEXT("The old prop key has been removed"),
		Activities->GetGoalByKey(InteractGoalClass, FirstProp, bFoundOld));

	// ─── A node with no spot releases whatever an earlier node delivered ───
	FTerritoryPatrolNode NoSpotNode;
	TestFalse(TEXT("A node that authors no spot delivers nothing"),
		Guard->DeliverPatrolSpot(NoSpotNode));
	TestNull(TEXT("A node that authors no spot releases the previous delivery"),
		Guard->TerritorySpotGoal.Get());
	TestEqual(TEXT("No orphaned interaction goals survive enabled-enabled-disabled"),
		Activities->GetGoals(InteractGoalClass).Goals.Num(), 0);

	// An authored/story goal to this same prop owns its own entry. Patrol must not adopt it,
	// erase its key on failed admission, or remove it while cleaning up a different patrol goal.
	UNPCGoalItem* StoryGoal = NewObject<UNPCGoalItem>(Activities, InteractGoalClass);
	UNarrativeActorProvider_LevelReference* StoryProvider = NewObject<UNarrativeActorProvider_LevelReference>(StoryGoal);
	StoryProvider->SoftActorReference = SecondProp->GetOwner();
	FStructProperty* StoryAimProperty = FindFProperty<FStructProperty>(InteractGoalClass, TEXT("ActorToInteractWith"));
	if (TestNotNull(TEXT("Story goal actor-provider contract"), StoryAimProperty))
	{
		StoryAimProperty->ContainerPtrToValuePtr<FInstancedActorProvider>(StoryGoal)->Provider = StoryProvider;
		TestTrue(TEXT("Independent story goal registers"), Activities->AddGoal(StoryGoal) == StoryGoal);
		TestFalse(TEXT("Patrol refuses a prop already owned by another goal"), Guard->DeliverPatrolSpot(SpotNode));
		Guard->DeliverPatrolSpot(NoSpotNode);
		bool bFoundStory = false;
		TestTrue(TEXT("Cleanup preserves the independent goal and its key"),
			Activities->GetGoalByKey(InteractGoalClass, SecondProp, bFoundStory) == StoryGoal);
		Activities->RemoveGoal(StoryGoal);
	}

	TestTrue(TEXT("A valid visit can be delivered again"), Guard->DeliverPatrolSpot(SpotNode));
	Activities->PrepareForSave_Implementation();
	FArrayProperty* SavedGoalsProperty = FindFProperty<FArrayProperty>(Activities->GetClass(), TEXT("SavedGoals"));
	if (TestNotNull(TEXT("Narrative exposes its reflected SavedGoals save record"), SavedGoalsProperty))
	{
		FStructProperty* RecordProperty = CastField<FStructProperty>(SavedGoalsProperty->Inner);
		if (TestTrue(TEXT("The saved array retains the FSavedGoalItem contract"),
			RecordProperty && RecordProperty->Struct == FSavedGoalItem::StaticStruct()))
		{
			const TArray<FSavedGoalItem>* SavedGoals =
				SavedGoalsProperty->ContainerPtrToValuePtr<TArray<FSavedGoalItem>>(Activities);
			TestFalse(TEXT("Narrative's actual save record excludes transient spot visits"),
				SavedGoals->ContainsByPredicate([InteractGoalClass](const FSavedGoalItem& Record)
				{
					return Record.Class == InteractGoalClass;
				}));
		}
	}
	Guard->DeliverPatrolSpot(NoSpotNode);
	Activities->Load_Implementation();
	TestEqual(TEXT("Loading the saved activity record does not resurrect a spot visit"),
		Activities->GetGoals(InteractGoalClass).Goals.Num(), 0);
	TestTrue(TEXT("A visit is rebuilt from the authored node after load"), Guard->DeliverPatrolSpot(SpotNode));

	// Exercise the actual vendor claim-selection graph after delivery. An automatic visit
	// must stay at SlotIdx=-1 so this graph reads CURRENT eligibility, not a stale early pin.
	SpotNode.Spot.SpotTag = FGameplayTag();
	TestTrue(TEXT("An untagged automatic visit is delivered"), Guard->DeliverPatrolSpot(SpotNode));
	UClass* InteractActivityClass = LoadClass<UNPCActivity>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Interact/Goals/BPA_Interact.BPA_Interact_C"));
	if (TestNotNull(TEXT("The vendor interact activity loads"), InteractActivityClass))
	{
		UNPCActivity* InteractActivity = NewObject<UNPCActivity>(Activities, InteractActivityClass);
		InteractActivity->SetOwner(Controller, Activities);
		FObjectPropertyBase* ActivityGoalProperty = FindFProperty<FObjectPropertyBase>(InteractActivityClass, TEXT("ActivityGoal"));
		UFunction* GetSlotsFunction = InteractActivity->FindFunction(TEXT("GetSlotsToClaim"));
		FArrayProperty* ReturnProperty = GetSlotsFunction
			? FindFProperty<FArrayProperty>(GetSlotsFunction, TEXT("ReturnValue")) : nullptr;
		if (TestNotNull(TEXT("Vendor activity goal binding"), ActivityGoalProperty)
			&& TestNotNull(TEXT("Vendor claim-selection function"), GetSlotsFunction)
			&& TestTrue(TEXT("Vendor claim-selection result is an integer array"),
				ReturnProperty && CastField<FIntProperty>(ReturnProperty->Inner)))
		{
			ActivityGoalProperty->SetObjectPropertyValue_InContainer(InteractActivity, Guard->TerritorySpotGoal);
			auto QueryClaimSlots = [&]()
			{
				FStructOnScope Params(GetSlotsFunction);
				InteractActivity->ProcessEvent(GetSlotsFunction, Params.GetStructMemory());
				return *ReturnProperty->ContainerPtrToValuePtr<TArray<int32>>(Params.GetStructMemory());
			};
			TestEqual(TEXT("Automatic selection initially considers both available slots"), QueryClaimSlots().Num(), 2);
			UNarrativeInteractionComponent* OtherClaimer = NewObject<UNarrativeInteractionComponent>(FirstProp->GetOwner());
			SecondProp->SlotStatuses[0].SlotUser = OtherClaimer;
			SecondProp->SlotStatuses[0].SlotStatus = EInteractionSlotStatus::ISS_Targeted;
			TArray<int32> CurrentSlots = QueryClaimSlots();
			TestTrue(TEXT("A slot targeted after delivery is excluded by the real claim graph"),
				!CurrentSlots.Contains(0) && CurrentSlots.Contains(1));
			SecondProp->SlotStatuses[0].SlotStatus = EInteractionSlotStatus::ISS_Occupied;
			CurrentSlots = QueryClaimSlots();
			TestTrue(TEXT("An unstealable slot occupied after delivery is excluded"),
				!CurrentSlots.Contains(0) && CurrentSlots.Contains(1));
			const int32 AlternateSlot = SecondProp->GetBestAvailableSlot(ControllerInteraction, CurrentSlots);
			TestTrue(TEXT("Narrative can target the still-available alternate slot"),
				ControllerInteraction->TargetInteractionSlot(SecondProp, AlternateSlot, false));
			TestTrue(TEXT("The unrelated occupant retains its claim"),
				SecondProp->SlotStatuses[0].SlotUser == OtherClaimer
				&& SecondProp->SlotStatuses[0].SlotStatus == EInteractionSlotStatus::ISS_Occupied);
			// No interaction ability was started in this fixture. Restore the targeted slot
			// through the interactable's public authority API before the next query.
			SecondProp->UpdateSlotStatus(ControllerInteraction, AlternateSlot, EInteractionSlotStatus::ISS_Free);
			SecondProp->SlotStatuses[0].SlotStatus = EInteractionSlotStatus::ISS_Free;
			SecondProp->SlotStatuses[0].SlotUser = nullptr;
			UTerritoryTestSlotBehavior* FirstSlotBehavior = CastChecked<UTerritoryTestSlotBehavior>(
				SecondProp->InteractionSlots[0].SlotInteractBehavior);
			FirstSlotBehavior->bUsableInTest = false;
			CurrentSlots = QueryClaimSlots();
			TestTrue(TEXT("Usability changes after delivery are respected at claim selection"),
				!CurrentSlots.Contains(0) && CurrentSlots.Contains(1));
			FirstSlotBehavior->bUsableInTest = true;

			UNPCGoalItem* VisitGoal = Guard->TerritorySpotGoal;
			UFunction* NotifyFunction = InteractActivity->FindFunction(TEXT("NotifySucceeded"));
			if (TestNotNull(TEXT("Vendor startup success notification"), NotifyFunction))
			{
				InteractActivity->ProcessEvent(NotifyFunction, nullptr);
				TestTrue(TEXT("The actual startup notification leaves the visit registered"),
					Activities->GetGoals(InteractGoalClass).Goals.Contains(VisitGoal));
			}
			UFunction* ScoreFunction = InteractActivity->FindFunction(TEXT("ScoreGoalItem"));
			FObjectPropertyBase* ScoreGoalProperty = ScoreFunction
				? FindFProperty<FObjectPropertyBase>(ScoreFunction, TEXT("Goal")) : nullptr;
			FFloatProperty* ScoreReturnProperty = ScoreFunction
				? FindFProperty<FFloatProperty>(ScoreFunction, TEXT("ReturnValue")) : nullptr;
			if (TestNotNull(TEXT("Vendor goal scoring function"), ScoreFunction)
				&& TestNotNull(TEXT("Vendor score goal parameter"), ScoreGoalProperty)
				&& TestNotNull(TEXT("Vendor score float result"), ScoreReturnProperty))
			{
				auto ScoreVisit = [&]()
				{
					FStructOnScope Params(ScoreFunction);
					ScoreGoalProperty->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), VisitGoal);
					InteractActivity->ProcessEvent(ScoreFunction, Params.GetStructMemory());
					return ScoreReturnProperty->GetPropertyValue_InContainer(Params.GetStructMemory());
				};
				TestTrue(TEXT("The visit scores before its total deadline"), ScoreVisit() > 0.f);
				VisitGoal->CreationTime = World->GetTimeSeconds() - VisitGoal->GoalLifetime - 1.f;
				TestTrue(TEXT("The actual vendor activity honors parent goal expiry"), ScoreVisit() < 0.f);

				// Let the real component consume that negative score. Its owner cache normally
				// comes from BeginPlay; this isolated world binds the same controller explicitly.
				FObjectPropertyBase* OwnerProperty = FindFProperty<FObjectPropertyBase>(Activities->GetClass(), TEXT("OwnerController"));
				if (TestNotNull(TEXT("Activity component owner binding"), OwnerProperty))
				{
					OwnerProperty->SetObjectPropertyValue_InContainer(Activities, Controller);
					UNPCActivity* RegisteredActivity = Activities->AddActivity(InteractActivityClass, false);
					if (TestNotNull(TEXT("The real interact activity registers for the expiry rescore"), RegisteredActivity))
					{
						Activities->RescoreGoals();
						TestFalse(TEXT("Narrative removes the expired visit during its normal rescore"),
							Activities->GetGoals(InteractGoalClass).Goals.Contains(VisitGoal));
						Activities->RemoveActivity(InteractActivityClass);
					}
				}
			}
		}
	}
	SpotNode.Location = FVector(10000.f, 0.f, 0.f);
	TestFalse(TEXT("An unresolved new node does not report delivery success"), Guard->DeliverPatrolSpot(SpotNode));
	TestEqual(TEXT("Failed retarget clears only the former patrol delivery"),
		Activities->GetGoals(InteractGoalClass).Goals.Num(), 0);

	// ─── A spot with no goal class is reported, never guessed ───
	Guard->SpotInteractGoalClass = nullptr;
	TestFalse(TEXT("An enabled spot without a goal class delivers nothing"),
		Guard->DeliverPatrolSpot(SpotNode));
	TestNull(TEXT("An enabled spot without a goal class caches nothing"),
		Guard->TerritorySpotGoal.Get());
	Guard->SpotInteractGoalClass = UNPCGoalItem::StaticClass();
	TestFalse(TEXT("An abstract goal class is rejected before construction"), Guard->DeliverPatrolSpot(SpotNode));

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

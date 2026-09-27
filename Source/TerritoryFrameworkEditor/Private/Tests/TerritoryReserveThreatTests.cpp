#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/TerritoryInvestigationGoal.h"
#include "AI/TerritoryInvestigationActivity.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"
#include "Tests/TerritoryReserveTestActivity.h"
#include "Tests/TerritoryConversationTestGameState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFReserveThreatHandoff,
	"TerritoryFramework.Guards.Reserves.RealDamageReportHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFReserveThreatHandoff::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Reserve report world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	World->SetGameState(World->SpawnActor<ATerritoryConversationTestGameState>());
	World->InitializeActorsForPlay(FURL());
	World->CreateAISystem();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(710, 711, 712, 713);
	Definition->GuardBehavior.bEngageAtWarInClaimedTerritory = true;
	auto* Profile = NewObject<UTerritoryStealthProfile>(Definition);
	Profile->bAllowStealthInfiltration = false;
	Profile->InvestigationDuration = 12.f;
	Definition->DefaultStealthProfile = Profile;
	Definition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Owned;
	Owned.OwningFaction = Bandits;
	Owned.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Owned);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::None);
	TestEqual(TEXT("The attacked faction begins neutral"), Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::None);
	auto* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Post->SetActorGUID_Implementation(FGuid(714, 715, 716, 717));
	Post->SetResolvedTerritory(Place);
	Post->bAutoSpawnReserves = false;
	const auto MakeGuard = [&](FGameplayTag Faction)
	{
		auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
		Cast<INarrativeTeamAgentInterface>(Guard)->AddFaction(Faction);
		Guard->OwningTerritory = Place;
		Guard->OwningTerritorySpawnPoint = Post;
		Guard->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(Guard, Guard);
		return Guard;
	};
	auto* Witness = MakeGuard(Bandits);
	auto* Source = MakeGuard(Heroes);
	Post->RegisterSpawnedGuard(Witness);
	FScriptDelegate Listener;
	Listener.BindUFunction(Witness, TEXT("HandleNarrativeDamagedBy"));
	Witness->GetNarrativeAbilitySystemComponent()->OnDamagedBy.Add(Listener);
	FGameplayEffectSpec Spec;
	Witness->SetActorLocation(FVector(100.f, 200.f, 80.f));
	Source->SetActorLocation(FVector(1000.f, 0.f, 80.f));
	const auto Damage = [&](float Amount)
	{
		Witness->GetNarrativeAbilitySystemComponent()->DamagedBy(Source->GetNarrativeAbilitySystemComponent(), Amount, Spec);
	};
	Damage(0.f);
	TestFalse(TEXT("Zero damage creates no report"), Post->RecentDamageSource.IsValid());
	Damage(12.f);
	TestTrue(TEXT("Actual Native ASC callback records its source"), Post->RecentDamageSource.Get() == Source);
	TestEqual(TEXT("Only the actual hit location is shared"), Post->RecentDamageLocation, Witness->GetActorLocation());
	const FVector HitLocation = Post->RecentDamageLocation;
	Source->SetActorLocation(FVector(2000.f, 300.f, 80.f));
	TestEqual(TEXT("Moving the source cannot update the retained report"), Post->RecentDamageLocation, HitLocation);
	Post->UnregisterGuard(Witness, EGuardRemovalReason::ManualRemoval);
	auto* Reserve = MakeGuard(Bandits);
	Post->RegisterSpawnedGuard(Reserve);
	// Keep Narrative's cached auto-spawned controller and activity ownership intact.
	// A second Possess changes APawn::Controller but not GetNPCController's cache.
	auto* Controller = Reserve->GetNPCController();
	if (!TestNotNull(TEXT("Reserve's Native controller"), Controller)
		|| !TestTrue(TEXT("Cached controller is the possessing controller"), Reserve->GetController() == Controller)
		|| !TestTrue(TEXT("Native controller owns this reserve"), Controller->GetControlledNPC() == Reserve)) return false;
	auto* Activities = Reserve->GetActivityComponent();
	if (!TestNotNull(TEXT("Native activity component"), Activities)
		|| !TestTrue(TEXT("Reserve activities belong to its possessing controller"), Activities->GetOwner() == Controller)) return false;
	if (!Activities->HasBegunPlay()) Activities->BeginPlay();
	// Submit to Native's real registry without asking a fixture with no navmesh to move.
	Activities->Deactivate();
	TestTrue(TEXT("A neutral damage report admits location investigation without combat permission"), Reserve->CanInvestigateRecentGuardDamage(Source));
	TestTrue(TEXT("Replacement receives the report through the existing Native investigation path"), Post->ApplyRecentGuardDamage(Reserve));
	const FNPCGoalContainer Goals = Activities->GetGoals(UTerritoryInvestigationGoal::StaticClass());
	if (!TestEqual(TEXT("Exactly one location investigation goal"), Goals.Goals.Num(), 1)) return false;
	auto* Goal = CastChecked<UTerritoryInvestigationGoal>(Goals.Goals[0]);
	TestTrue(TEXT("Investigation belongs to the reserve's possessing controller"), Goal->OwnerController == Controller);
	TestEqual(TEXT("Goal targets the event-time hit location, not the relocated attacker"), Goal->InvestigationLocation, HitLocation);
	TestFalse(TEXT("Report does not claim visual identification"), Goal->bIdentityConfirmed);
	const FArrayProperty* HostilesProperty = FindFProperty<FArrayProperty>(ANarrativeNPCCharacter::StaticClass(), TEXT("Hostiles"));
	if (!TestNotNull(TEXT("Native exposes its personal hostility list"), HostilesProperty)) return false;
	FScriptArrayHelper Hostiles(HostilesProperty, HostilesProperty->ContainerPtrToValuePtr<void>(Reserve));
	TestEqual(TEXT("Report does not fabricate the new guard's personal hostility"), Hostiles.Num(), 0);
	TestFalse(TEXT("A short-lived report is never campaign saved"), Goal->bSaveGoal);
	TestTrue(TEXT("Report lifetime remains bounded by its original expiry"), Goal->GoalLifetime <= Profile->InvestigationDuration);
	TestNotNull(TEXT("Existing investigation activity consumes the exact goal type"), Activities->GetActivity(UTerritoryInvestigationActivity::StaticClass()));
	TestFalse(TEXT("A valid report is still live"), Goal->ShouldCleanup());
	TestFalse(TEXT("Location handoff does not grant the neutral reserve combat permission"), Reserve->CanEngageTerritoryTarget(Source));
	Diplomacy->FormAlliance(Bandits, Heroes);
	TestFalse(TEXT("Alliance rejects a pending handoff"), Post->ApplyRecentGuardDamage(Reserve));
	TestTrue(TEXT("Alliance cancels an already delivered investigation"), Goal->ShouldCleanup());
	TestTrue(TEXT("Protected source cannot keep scoring"), Goal->GetGoalScore() < 0.f);
	Diplomacy->DeclareWar(Bandits, Heroes);
	Post->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client post cannot deliver a report"), Post->ApplyRecentGuardDamage(Reserve));
	Post->SetRole(ROLE_Authority);
	Post->RecentDamageExpiresAt = World->GetTimeSeconds() - 1.0;
	TestFalse(TEXT("Expired report cannot be renewed on replacement"), Post->ApplyRecentGuardDamage(Reserve));
	Post->RegisterSpawnedGuard(Reserve);
	Post->RememberGuardDamage(Reserve, Source);
	FNarrativeActorRecord Record;
	auto* Saves = World->GetSubsystem<UNarrativeSaveSubsystem>();
	TestTrue(TEXT("Native record captures durable post state"), Saves->CreateActorRecord(Post, Record));
	Saves->LoadActorFromRecord(Post, Record);
	TestFalse(TEXT("In-place Native load discards live threat reports"), Post->RecentDamageSource.IsValid());
	TestTrue(TEXT("Loading the post also invalidates its delivered transient report"), Goal->ShouldCleanup());
	Post->RememberGuardDamage(Reserve, Source);
	Post->HandleOwnershipTransition(EOwnershipTransitionReason::OwnerChanged);
	TestFalse(TEXT("Ownership transition discards old-owner report"), Post->RecentDamageSource.IsValid());
	Post->RegisterSpawnedGuard(Reserve);
	Post->RememberGuardDamage(Reserve, Source);
	Post->SetResolvedTerritory(nullptr);
	TestFalse(TEXT("Stream-out/rebinding cannot retain old Place report"), Post->RecentDamageSource.IsValid());
	TestTrue(TEXT("Destroying source makes the already delivered goal invalid"), Source->Destroy());
	TestTrue(TEXT("A dead or removed source cannot leave a live reserve goal"), Goal->ShouldCleanup());
	// Native AddGoal(false) invokes K2_RunActivity synchronously. An authored
	// activity may immediately remove its goal or change who owns the guard.
	Post->SetResolvedTerritory(Place);
	Post->RegisterSpawnedGuard(Reserve);
	Source = MakeGuard(Heroes);
	Post->RememberGuardDamage(Reserve, Source);
	Activities->RemoveGoal(Goal);
	Activities->RemoveActivity(UTerritoryInvestigationActivity::StaticClass());
	Profile->InvestigationActivityClass = UTerritoryReserveTestActivity::StaticClass();
	auto* CallbackActivity = CastChecked<UTerritoryReserveTestActivity>(Activities->AddActivity(UTerritoryReserveTestActivity::StaticClass(), false));
	int32 CallbackCount = 0;
	CallbackActivity->RunCallback = [&]()
	{
		++CallbackCount;
		Activities->RemoveGoal(Activities->GetCurrentActivityGoal());
	};
	Activities->Activate();
	TestFalse(TEXT("A goal removed by Native's immediate activity callback is not reported admitted"), Post->ApplyRecentGuardDamage(Reserve));
	TestEqual(TEXT("Actual K2_RunActivity dispatch reached the callback"), CallbackCount, 1);
	CallbackActivity->RunCallback = [&]() { Post->UnregisterGuard(Reserve, EGuardRemovalReason::ManualRemoval); };
	TestFalse(TEXT("A guard unregistered during Native activity start is not reported admitted"), Post->ApplyRecentGuardDamage(Reserve));
	CallbackActivity->RunCallback = nullptr;
	return true;
}

#endif

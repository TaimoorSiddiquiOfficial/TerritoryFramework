#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritorySavableData.h"
#include "Engine/World.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSaveDefaultReload,
	"TerritoryFramework.SaveLoad.Regression.DefaultValuesReplaceExistingCampaignState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSaveDefaultReload::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Save default world exists"), World)) return false;
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto LegacyActorRecord = [](AActor* Actor, const FNarrativeActorRecord& Current)
	{
		FNarrativeActorRecord Legacy = Current;
		Legacy.ByteData.Reset();
		FMemoryWriter Writer(Legacy.ByteData);
		FObjectAndNameAsStringProxyArchive Archive(Writer, true);
		Archive.ArIsSaveGame = true;
		Actor->AActor::Serialize(Archive);
		return Legacy;
	};
	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>();
	State->SetActorGUID_Implementation(FGuid(151, 152, 153, 154));
	FNarrativeActorRecord EmptyCampaign;
	TestTrue(TEXT("Narrative saves the empty campaign"), Save->CreateActorRecord(State, EmptyCampaign));
	const FNarrativeActorRecord LegacyCampaign = LegacyActorRecord(State, EmptyCampaign);
	const FNarrativeActorRecord* CampaignRecords[] = {&EmptyCampaign, &LegacyCampaign};
	for (const FNarrativeActorRecord* Record : CampaignRecords)
	{
		FReplicatedTreaty Treaty;
		Treaty.FactionA = Heroes;
		Treaty.FactionB = Bandits;
		Treaty.State = EDiplomacyState::War;
		State->SavedTreaties = {Treaty};
		FTerritoryAssaultRecord Assault;
		Assault.AssaultID = FGuid(155, 156, 157, 158);
		Assault.TargetTerritoryGUID = FGuid(159, 160, 161, 162);
		Assault.TargetTerritory = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
		Assault.AttackingFaction = Bandits;
		Assault.DefendingFaction = Heroes;
		Assault.State = ETerritoryAssaultState::Cancelled;
		State->SavedAssaults = {Assault};
		Save->LoadActorFromRecord(State, *Record);
		TestTrue(TEXT("An empty save clears previously populated saved treaties"), State->SavedTreaties.IsEmpty());
		TestTrue(TEXT("An empty save clears previously populated assault history"), State->SavedAssaults.IsEmpty());
		TestTrue(TEXT("The late-join treaty read model is empty after load"), State->GetAllTreaties().IsEmpty());
		TestTrue(TEXT("The late-join assault read model is empty after load"), State->GetAllAssaultSummaries().IsEmpty());
		TestFalse(TEXT("A treaty absent from the loaded campaign cannot remain at war"),
			World->GetSubsystem<UTerritoryDiplomacySubsystem>()->IsAtWar(Heroes, Bandits));
	}

	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>();
	Property->SetActorGUID_Implementation(FGuid(163, 164, 165, 166));
	FTerritoryOwnershipData Ownership = Property->GetOwnershipData();
	Ownership.OwningFaction = Heroes;
	Ownership.State = ETerritoryState::Claimed;
	Ownership.ControlProgress = 1.f;
	Ownership.DesiredGuardCount = 0;
	Ownership.GuardCost = 0;
	Property->CommitOwnershipData(Ownership);
	FNarrativeActorRecord ZeroProperty;
	TestTrue(TEXT("Narrative saves a claimed Property with zero upgrades and guards"),
		Save->CreateActorRecord(Property, ZeroProperty));
	const FNarrativeActorRecord LegacyProperty = LegacyActorRecord(Property, ZeroProperty);
	const FNarrativeActorRecord* PropertyRecords[] = {&ZeroProperty, &LegacyProperty};
	for (const FNarrativeActorRecord* Record : PropertyRecords)
	{
		Property->SetUpgradeLevel(3);
		FTerritoryOwnershipData Live = Property->GetOwnershipData();
		Live.GuardCost = 777;
		Live.DesiredGuardCount = 3;
		Property->CommitOwnershipData(Live);
		Save->LoadActorFromRecord(Property, *Record);
		TestEqual(TEXT("Saved zero replaces the current upgrade level"), Property->GetUpgradeLevel(), 0);
		TestEqual(TEXT("Saved zero replaces the current garrison target"), Property->GetDesiredGuardCount(), 0);
		TestEqual(TEXT("Nested ownership fields restore their saved zero too"), Property->GetGuardCost(), 0);
		TestEqual(TEXT("Non-default saved ownership is still restored"), Property->GetOwningFaction(), Heroes);
	}

	AActor* PlayerOwner = World->SpawnActor<AActor>();
	UTerritoryPlayerManagementComponent* Management = NewObject<UTerritoryPlayerManagementComponent>(PlayerOwner);
	PlayerOwner->AddInstanceComponent(Management);
	FNarrativeActorRecord EmptyPlayer;
	TestTrue(TEXT("Narrative saves the player's Territory component"), Save->CreateActorRecord(PlayerOwner, EmptyPlayer));
	FNarrativeActorRecord LegacyPlayer = EmptyPlayer;
	for (FNarrativeSaveComponent& ComponentRecord : LegacyPlayer.SavedComponents)
	{
		if (ComponentRecord.ComponentName != Management->GetFName()) continue;
		ComponentRecord.ByteData.Reset();
		FMemoryWriter Writer(ComponentRecord.ByteData);
		FObjectAndNameAsStringProxyArchive LegacyArchive(Writer, true);
		LegacyArchive.ArIsSaveGame = true;
		Management->UActorComponent::Serialize(LegacyArchive);
	}
	for (const FNarrativeActorRecord* Record : {&EmptyPlayer, &LegacyPlayer})
	{
		Management->LiveEvents.AddDefaulted();
		Management->EspionageAttemptSequence = 17;
		Save->LoadActorFromRecord(PlayerOwner, *Record);
		TestTrue(TEXT("Empty saved player history clears the current archive"), Management->GetLiveEvents().IsEmpty());
		TestEqual(TEXT("Saved zero restores the actual espionage decision sequence"), Management->EspionageAttemptSequence, 0);
	}
	World->DestroyWorld(false);
	UWorld* LegacyWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ATerritorySavableData* LegacyData = LegacyWorld->SpawnActor<ATerritorySavableData>();
	LegacyData->SetActorGUID_Implementation(FGuid(167, 168, 169, 170));
	Save = LegacyWorld->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord EmptyLegacyData;
	TestTrue(TEXT("Narrative can still save the deprecated actor in a world without WorldState"),
		Save->CreateActorRecord(LegacyData, EmptyLegacyData));
	const FNarrativeActorRecord OldLegacyData = LegacyActorRecord(LegacyData, EmptyLegacyData);
	const FNarrativeActorRecord* LegacyRecords[] = {&EmptyLegacyData, &OldLegacyData};
	for (const FNarrativeActorRecord* Record : LegacyRecords)
	{
		LegacyData->SavedReputation.Add(Heroes, 500);
		Save->LoadActorFromRecord(LegacyData, *Record);
		TestTrue(TEXT("Empty legacy reputation replaces its current map"), LegacyData->SavedReputation.IsEmpty());
		TestEqual(TEXT("Deprecated persistence also restores an empty authoritative reputation"),
			LegacyWorld->GetSubsystem<UTerritoryDiplomacySubsystem>()->GetReputation(Heroes), 0);
	}
	LegacyData->SetRole(ROLE_SimulatedProxy);
	LegacyData->SavedReputation.Add(Heroes, 500);
	LegacyData->Load_Implementation();
	TestEqual(TEXT("A legacy client actor cannot overwrite authoritative diplomacy"),
		LegacyWorld->GetSubsystem<UTerritoryDiplomacySubsystem>()->GetReputation(Heroes), 0);
	LegacyData->PrepareForSave_Implementation();
	TestEqual(TEXT("Legacy client preparation cannot mutate saved campaign data"),
		LegacyData->SavedReputation.FindRef(Heroes), 500);
	ATerritorySavableData* Detached = NewObject<ATerritorySavableData>();
	TestNull(TEXT("Detached legacy fixture has no world"), Detached->GetWorld());
	Detached->PrepareForSave_Implementation();
	Detached->Load_Implementation();
	LegacyWorld->DestroyWorld(false);
	return true;
}

#endif

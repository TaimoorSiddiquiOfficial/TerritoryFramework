#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNarrativeSpawnIdentity,
	"TerritoryFramework.Narrative.Regression.SpawnGUIDPrecedesStableActorRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNarrativeSpawnIdentity::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Native spawn identity world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	ATerritoryVolume* Territory = World->SpawnActor<ATerritoryVolume>();
	ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGuid TerritoryGUID(241, 242, 243, 244);
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	TMap<AActor*, FGuid> ObservedGUIDs;
	Probe->StableSpawnCallback = [&](AActor* Actor, FGuid ActorGUID)
	{
		if (Cast<ATerritoryAssaultCharacter>(Actor) || Cast<ATerritoryGuardCharacter>(Actor))
		{
			ObservedGUIDs.Add(Actor, ActorGUID);
		}
	};
	Save->OnStableActorSpawned.AddDynamic(Probe, &UTerritoryAuditEventProbe::StableActorSpawned);
	TArray<ANarrativeNPCCharacter*> SpawnedNPCs;
	TArray<FGuid> SpawnedGUIDs;
	for (bool bGuard : {false, true})
	{
		UNPCDefinition* Definition = NewObject<UNPCDefinition>();
		Definition->CharacterID = bGuard ? TEXT("AuditIdentityGuardCharacter") : TEXT("AuditIdentityAssaultCharacter");
		Definition->NPCID = bGuard ? TEXT("AuditIdentityGuardNPC") : TEXT("AuditIdentityAssaultNPC");
		Definition->NPCClassPath = bGuard ? ATerritoryGuardCharacter::StaticClass() : ATerritoryAssaultCharacter::StaticClass();
		Definition->bAllowMultipleInstances = true;
		const FGuid SpawnGUID(245, 246, 247, bGuard ? 249 : 248);
		const FTransform Transform(FVector(1000.f, bGuard ? 1000.f : 0.f, 100.f));
		ANarrativeNPCCharacter* NPC = bGuard
			? static_cast<ANarrativeNPCCharacter*>(ATerritoryGuardCharacter::SpawnThroughNarrative(
				Characters, Definition, Faction, TerritoryGUID, SpawnGUID, Transform, TEXT("AuditPost"), nullptr, {}, Territory, Post))
			: static_cast<ANarrativeNPCCharacter*>(ATerritoryAssaultCharacter::SpawnThroughNarrative(
				Characters, Definition, Faction, TerritoryGUID, SpawnGUID, Transform, TEXT("AuditApproach"), nullptr, {},
				FGuid(250, 251, 252, 253), TerritoryTag));
		if (!TestNotNull(TEXT("Actual Narrative spawning returns a Territory NPC"), NPC)) continue;
		SpawnedNPCs.Add(NPC);
		SpawnedGUIDs.Add(SpawnGUID);
		TestTrue(TEXT("Narrative emits its actual stable-actor spawn event"), ObservedGUIDs.Contains(NPC));
		TestEqual(TEXT("The stable spawn event sees the final assigned GUID"), ObservedGUIDs.FindRef(NPC), SpawnGUID);
		TestEqual(TEXT("Native GUID lookup resolves the newly spawned NPC"), Save->LookupActorByGUID(SpawnGUID), static_cast<AActor*>(NPC));
		FNarrativeActorRecord Record;
		TestTrue(TEXT("The same NPC produces a Native save record"), Save->CreateActorRecord(NPC, Record));
		TestEqual(TEXT("The Native record keeps the registration GUID"), Record.ActorGUID, SpawnGUID);
		NPC->SetActorLocation(Transform.GetLocation() + FVector(100.f, 100.f, 100.f));
		Save->LoadActorFromRecord(NPC, Record);
		TestTrue(TEXT("Native load restores the NPC transform"), NPC->GetActorTransform().Equals(Record.Transform));
		TestEqual(TEXT("Native GUID lookup remains correct after record load"), Save->LookupActorByGUID(SpawnGUID), static_cast<AActor*>(NPC));
	}
	if (SpawnedNPCs.Num() == 2)
	{
		SpawnedNPCs[0]->Destroy();
		TestNull(TEXT("Destroy removes only the departed NPC from Native lookup"), Save->LookupActorByGUID(SpawnedGUIDs[0]));
		TestEqual(TEXT("The other NPC's stable lookup remains intact"), Save->LookupActorByGUID(SpawnedGUIDs[1]), static_cast<AActor*>(SpawnedNPCs[1]));
	}
	Save->OnStableActorSpawned.RemoveAll(Probe);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/Dialogue.h"
#include "Tales/TerritoryPartyDialogue.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyDialogueDefinitionBinding,
	"TerritoryFramework.Factions.Dialogue.DefinitionBindingAndLateOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyDialogueDefinitionBinding::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Dialogue binding world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Interactor = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Guard)->AddFaction(Bandits);
	Cast<INarrativeTeamAgentInterface>(Interactor)->AddFaction(Heroes);
	auto* Resolver = Guard->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>();
	if (!TestNotNull(TEXT("Native diplomacy component"), Resolver)) return false;
	auto* StaleCache = NewObject<UTerritoryDiplomacyDialogueProfile>();
	auto* Profile = NewObject<UTerritoryDiplomacyDialogueProfile>();
	Profile->WarDialogue = UDialogue::StaticClass();
	Profile->AllianceDialogue = UTerritoryPartyDialogue::StaticClass();
	Resolver->SetDialogueProfiles(StaleCache, {});
	TestTrue(TEXT("Non-territory owner retains explicit profile binding"), Resolver->GetDialogueProfileForOwner() == StaleCache);
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(721, 722, 723, 724);
	Definition->GuardBehavior.DialogueProfile = Profile;
	Definition->ApplyToTerritory(Place);
	// Simulate the pointer arriving after BeginPlay without running the server's
	// behavior initializer. Actual channel delivery still needs the live net gate.
	Guard->SetRole(ROLE_SimulatedProxy);
	Guard->OwningTerritory = Place;
	TestTrue(TEXT("Late owner resolves Definition without the server-only cache"), Resolver->GetDialogueProfileForOwner() == Profile);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	Diplomacy->DeclareWar(Bandits, Heroes);
	TestEqual(TEXT("Current War selects the authored relationship slot"), Resolver->ResolveDialogueForInteractor(Interactor, nullptr).Get(), UDialogue::StaticClass());
	Diplomacy->FormAlliance(Bandits, Heroes);
	TestEqual(TEXT("Relationship changes select Alliance without rebinding cache"), Resolver->ResolveDialogueForInteractor(Interactor, nullptr).Get(), UTerritoryPartyDialogue::StaticClass());
	auto* FactionProfile = NewObject<UTerritoryDiplomacyDialogueProfile>();
	FTerritoryFactionDialogueProfile Mapping;
	Mapping.Faction = Bandits;
	Mapping.DialogueProfile = FactionProfile;
	Definition->GuardBehavior.FactionDialogueProfiles.Add(Mapping);
	TestTrue(TEXT("Exact owner-faction mapping overrides the fallback"), Resolver->GetDialogueProfileForOwner() == FactionProfile);
	Definition->GuardBehavior.FactionDialogueProfiles.Reset();
	Definition->GuardBehavior.DialogueProfile = nullptr;
	TestNull(TEXT("Clearing current authoring cannot resurrect stale server cache"), Resolver->GetDialogueProfileForOwner());
	TestEqual(TEXT("Empty authored profile keeps Native NPC fallback"), Resolver->ResolveDialogueForInteractor(Interactor, UDialogue::StaticClass()).Get(), UDialogue::StaticClass());
	TestTrue(TEXT("Read-only resolution leaves the old cache untouched"), Resolver->DialogueProfile == StaleCache);
	Guard->SetRole(ROLE_Authority);
	return true;
}

#endif

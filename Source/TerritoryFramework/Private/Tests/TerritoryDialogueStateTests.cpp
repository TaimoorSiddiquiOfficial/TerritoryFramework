#include "Misc/AutomationTest.h"
#include "Core/TerritoryDialogueTags.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Contract for the Territory dialogue-state tags.
 *
 * These tags are the read-model that ambient Narrative tagged dialogue entries filter on
 * (FTaggedDialogue::RequiredTags / BlockedTags). The diplomacy subsystem stays the authority for
 * the relationship, so this test only proves the mapping and that every tag is registered.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDialogueStateTagMapping,
	"TerritoryFramework.Dialogue.StateTagMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDialogueStateTagMapping::RunTest(const FString& Parameters)
{
	using namespace TerritoryDialogueTags;

	// Every tag must resolve, otherwise a Required/Blocked filter in a dialogue entry can never match.
	TestTrue(TEXT("StateAlly tag is registered"), StateAlly.GetTag().IsValid());
	TestTrue(TEXT("StateNeutral tag is registered"), StateNeutral.GetTag().IsValid());
	TestTrue(TEXT("StateWary tag is registered"), StateWary.GetTag().IsValid());
	TestTrue(TEXT("StateHostile tag is registered"), StateHostile.GetTag().IsValid());
	TestTrue(TEXT("StateConversing tag is registered"), StateConversing.GetTag().IsValid());

	// War always wins, even for a shared exact faction: a betrayal must never read as friendly.
	TestEqual(TEXT("War maps to Hostile"),
		GetDialogueStateTag(EDiplomacyState::War, false), StateHostile.GetTag());
	TestEqual(TEXT("War wins over a shared faction"),
		GetDialogueStateTag(EDiplomacyState::War, true), StateHostile.GetTag());

	// Friendly relationships.
	TestEqual(TEXT("Alliance maps to Ally"),
		GetDialogueStateTag(EDiplomacyState::Alliance, false), StateAlly.GetTag());
	TestEqual(TEXT("A shared faction outside war maps to Ally"),
		GetDialogueStateTag(EDiplomacyState::None, true), StateAlly.GetTag());

	// Cautious relationship.
	TestEqual(TEXT("Ceasefire maps to Wary"),
		GetDialogueStateTag(EDiplomacyState::Ceasefire, false), StateWary.GetTag());

	// Neutral relationships.
	TestEqual(TEXT("No treaty maps to Neutral"),
		GetDialogueStateTag(EDiplomacyState::None, false), StateNeutral.GetTag());
	TestEqual(TEXT("Trade agreement maps to Neutral"),
		GetDialogueStateTag(EDiplomacyState::TradeAgreement, false), StateNeutral.GetTag());
	TestEqual(TEXT("Non-aggression pact maps to Neutral"),
		GetDialogueStateTag(EDiplomacyState::NonAggression, false), StateNeutral.GetTag());

	// The mapping must never hand out the in-dialogue tag: that tag is applied by the dialogue owner.
	TestNotEqual(TEXT("Mapping never returns the Conversing tag"),
		GetDialogueStateTag(EDiplomacyState::Alliance, false), StateConversing.GetTag());

	return true;
}

/**
 * Contract for the dialogue-state enum that the Territory dialogue state component writes to an
 * NPC's Ability System. Every enum value must map onto its own registered tag, because the tagged
 * dialogue entries filter on exactly one of them.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDialogueStateEnumMapping,
	"TerritoryFramework.Dialogue.StateEnumMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDialogueStateEnumMapping::RunTest(const FString& Parameters)
{
	using namespace TerritoryDialogueTags;

	TestEqual(TEXT("Ally enum maps to the Ally tag"),
		GetDialogueStateTagForState(ETerritoryDialogueState::Ally), StateAlly.GetTag());
	TestEqual(TEXT("Neutral enum maps to the Neutral tag"),
		GetDialogueStateTagForState(ETerritoryDialogueState::Neutral), StateNeutral.GetTag());
	TestEqual(TEXT("Wary enum maps to the Wary tag"),
		GetDialogueStateTagForState(ETerritoryDialogueState::Wary), StateWary.GetTag());
	TestEqual(TEXT("Hostile enum maps to the Hostile tag"),
		GetDialogueStateTagForState(ETerritoryDialogueState::Hostile), StateHostile.GetTag());
	TestEqual(TEXT("Conversing enum maps to the Conversing tag"),
		GetDialogueStateTagForState(ETerritoryDialogueState::Conversing), StateConversing.GetTag());

	// The relationship mapping must land on the same tags the enum mapping hands out, otherwise a
	// state machine and the component would disagree about which line a relationship selects.
	TestEqual(TEXT("Relationship and enum mappings agree for war"),
		GetDialogueStateTag(EDiplomacyState::War, false),
		GetDialogueStateTagForState(ETerritoryDialogueState::Hostile));
	TestEqual(TEXT("Relationship and enum mappings agree for alliance"),
		GetDialogueStateTag(EDiplomacyState::Alliance, false),
		GetDialogueStateTagForState(ETerritoryDialogueState::Ally));

	// The component resolves a relationship, maps it to a tag, and stores the enum back from that tag
	// (UTerritoryDiplomacyDialogueComponent::ApplyDialogueStateFromInteractor). A tag that maps back to
	// a different state would make the stored read-model disagree with what the NPC actually wears.
	const ETerritoryDialogueState States[] = {
		ETerritoryDialogueState::Ally, ETerritoryDialogueState::Neutral,
		ETerritoryDialogueState::Wary, ETerritoryDialogueState::Hostile,
		ETerritoryDialogueState::Conversing,
	};
	for (const ETerritoryDialogueState State : States)
	{
		TestEqual(FString::Printf(TEXT("Tag round-trip for state %d"), static_cast<int32>(State)),
			GetDialogueStateForTag(GetDialogueStateTagForState(State)), State);
	}

	// An unregistered tag must fall back to the neutral read-model, never to a hostile one.
	TestEqual(TEXT("Unknown tag resolves to Neutral"),
		GetDialogueStateForTag(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Territory.DialogueState.DoesNotExist")), false)),
		ETerritoryDialogueState::Neutral);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

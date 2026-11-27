#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayTagContainer.h"
#include "UI/TerritoryDossierTypes.h"
#include "UI/TerritoryDossierWidget.h"

namespace
{
	/** Tag from the running tag registry; an unregistered tag stays invalid instead of crashing. */
	FGameplayTag DossierTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), /*ErrorIfNotFound=*/false);
	}

	FTerritoryDossierRecordView MakeDossierView(
		const FGameplayTag EvidenceTag,
		const FGuid SubjectId,
		const FGameplayTag Faction,
		const FGameplayTag Territory,
		const int32 Weight,
		const double GameTimeSeconds)
	{
		FTerritoryDossierRecordView View;
		View.EvidenceTag = EvidenceTag;
		View.Category = UTerritoryDossierWidget::ResolveDossierCategory(EvidenceTag);
		View.SubjectId = SubjectId;
		View.InstigatorFaction = Faction;
		View.SourceTerritory = Territory;
		View.Weight = Weight;
		View.GameTimeSeconds = GameTimeSeconds;
		return View;
	}
}

/**
 * The Dossier is Abstract, so its instance surface cannot be constructed in a unit world here.
 * These tests pin the two halves that decide what a player reads: which drawer a fact lands in,
 * and the text a row shows. The ledger projection itself is covered by the evidence ledger tests.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDossierCategoryMapping,
	"TerritoryFramework.Dossier.Behavior.CategoriesFollowTheEvidenceTagFamily",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierCategoryMapping::RunTest(const FString& Parameters)
{
	const FGameplayTag CaptureCommitted = DossierTag(TEXT("Territory.Evidence.Capture.Committed"));
	const FGameplayTag GuardKilled = DossierTag(TEXT("Territory.Evidence.Combat.GuardKilled"));
	const FGameplayTag StealthExposed = DossierTag(TEXT("Territory.Evidence.Stealth.Exposed"));
	const FGameplayTag TreatySigned = DossierTag(TEXT("Territory.Evidence.Treaty.Signed"));
	const FGameplayTag QuestCompleted = DossierTag(TEXT("Territory.Evidence.Tales.QuestCompleted"));

	if (!CaptureCommitted.IsValid() || !GuardKilled.IsValid() || !StealthExposed.IsValid()
		|| !TreatySigned.IsValid() || !QuestCompleted.IsValid())
	{
		AddError(TEXT("The Territory.Evidence.* tags are not registered in this run, so the drawer mapping cannot be verified"));
		return false;
	}

	TestEqual(TEXT("Capture facts land in the Control drawer"),
		UTerritoryDossierWidget::ResolveDossierCategory(CaptureCommitted), ETerritoryDossierCategory::Control);
	TestEqual(TEXT("Combat facts land in the Conflict drawer"),
		UTerritoryDossierWidget::ResolveDossierCategory(GuardKilled), ETerritoryDossierCategory::Conflict);
	TestEqual(TEXT("Stealth facts land in the Stealth drawer"),
		UTerritoryDossierWidget::ResolveDossierCategory(StealthExposed), ETerritoryDossierCategory::Stealth);
	TestEqual(TEXT("Treaty facts land in the Diplomacy drawer"),
		UTerritoryDossierWidget::ResolveDossierCategory(TreatySigned), ETerritoryDossierCategory::Diplomacy);
	TestEqual(TEXT("Tales facts land in the Tales drawer"),
		UTerritoryDossierWidget::ResolveDossierCategory(QuestCompleted), ETerritoryDossierCategory::Tales);

	const FGameplayTag UnknownFamily = DossierTag(TEXT("Territory.Evidence"));
	TestEqual(TEXT("A fact outside every known family is not invented into one"),
		UTerritoryDossierWidget::ResolveDossierCategory(UnknownFamily), ETerritoryDossierCategory::All);
	TestEqual(TEXT("An invalid tag is not invented into a drawer either"),
		UTerritoryDossierWidget::ResolveDossierCategory(FGameplayTag()), ETerritoryDossierCategory::All);

	TestTrue(TEXT("The All filter admits every drawer"),
		UTerritoryDossierWidget::MatchesDossierFilter(ETerritoryDossierCategory::All, ETerritoryDossierCategory::Conflict));
	TestTrue(TEXT("A specific filter admits its own drawer"),
		UTerritoryDossierWidget::MatchesDossierFilter(ETerritoryDossierCategory::Conflict, ETerritoryDossierCategory::Conflict));
	TestFalse(TEXT("A specific filter rejects another drawer"),
		UTerritoryDossierWidget::MatchesDossierFilter(ETerritoryDossierCategory::Conflict, ETerritoryDossierCategory::Diplomacy));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDossierRecordText,
	"TerritoryFramework.Dossier.Behavior.RecordTextStaysTraceable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierRecordText::RunTest(const FString& Parameters)
{
	const FGameplayTag GuardKilled = DossierTag(TEXT("Territory.Evidence.Combat.GuardKilled"));
	const FGameplayTag AttackerKilled = DossierTag(TEXT("Territory.Evidence.Combat.AttackerKilled"));
	const FGameplayTag Faction = DossierTag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag Territory = DossierTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));

	if (!GuardKilled.IsValid() || !AttackerKilled.IsValid() || !Faction.IsValid() || !Territory.IsValid())
	{
		AddError(TEXT("Tags needed to format a Dossier row are missing from the registry in this run"));
		return false;
	}

	const FGuid SubjectId = FGuid::NewGuid();
	const FTerritoryDossierRecordView View = MakeDossierView(
		GuardKilled, SubjectId, Faction, Territory, /*Weight=*/ 7, /*GameTimeSeconds=*/ 1234.5);

	const FText Headline = UTerritoryDossierWidget::GetEvidenceHeadlineText(View);
	const FText OtherHeadline = UTerritoryDossierWidget::GetEvidenceHeadlineText(
		MakeDossierView(AttackerKilled, SubjectId, Faction, Territory, 7, 1234.5));

	TestFalse(TEXT("A known fact gets a headline"), Headline.IsEmpty());
	TestFalse(TEXT("Two different facts do not share a headline"), Headline.EqualTo(OtherHeadline));

	const FString Meta = UTerritoryDossierWidget::GetEvidenceMetaText(View).ToString();
	TestTrue(TEXT("The row names the faction the fact is attributed to"), Meta.Contains(TEXT("Bandits")));
	TestTrue(TEXT("The row names the Territory the fact happened in"), Meta.Contains(TEXT("Blacksmith")));
	TestTrue(TEXT("The row carries the subject identity"), Meta.Contains(SubjectId.ToString(EGuidFormats::Short)));
	TestTrue(TEXT("The row carries the campaign time"), Meta.Contains(TEXT("1234.5")));

	// A fact without a Territory or a subject still renders; it must not print empty separators as
	// if data were missing in a way the player cannot act on.
	const FTerritoryDossierRecordView Bare = MakeDossierView(GuardKilled, FGuid(), FGameplayTag(), FGameplayTag(), 1, 10.0);
	const FString BareMeta = UTerritoryDossierWidget::GetEvidenceMetaText(Bare).ToString();
	TestFalse(TEXT("A bare fact still shows its campaign time"), BareMeta.IsEmpty());
	TestFalse(TEXT("A bare fact does not print an empty faction slot"), BareMeta.Contains(TEXT("  |  |  ")));

	// An unfamiliar producer must stay visible instead of being flattened into a generic label.
	const FGameplayTag CustomFact = FGameplayTag::RequestGameplayTag(FName(TEXT("Territory.Evidence.Custom.Unmapped")), false);
	if (CustomFact.IsValid())
	{
		const FText CustomHeadline = UTerritoryDossierWidget::GetEvidenceHeadlineText(
			MakeDossierView(CustomFact, SubjectId, Faction, Territory, 1, 10.0));
		TestEqual(TEXT("An unmapped fact shows its own leaf name"),
			CustomHeadline.ToString(), FString(TEXT("Unmapped")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

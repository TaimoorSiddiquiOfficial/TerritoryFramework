#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetData.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Economy/TerritoryProductionTags.h"
#include "Items/NarrativeItem.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "Music/TaggedMusicSet.h"
#include "QuestBlueprint.h"
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProjectNPCDefinitionIdentityRegression,
	"TerritoryFramework.Editor.Identity.ProjectNPCDefinitionsUseDistinctGuids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProjectNPCDefinitionIdentityRegression::RunTest(const FString& Parameters)
{
	static const TCHAR* ProjectFixturePackages[] = {
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryBandit"),
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryHero"),
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryBanditAssault")
	};
	bool bAnyProjectFixtureExists = false;
	for (const TCHAR* PackageName : ProjectFixturePackages)
	{
		bAnyProjectFixtureExists |= FPackageName::DoesPackageExist(PackageName);
	}
	if (!bAnyProjectFixtureExists)
	{
		AddInfo(TEXT("Skipped optional TDA NPC-definition fixture; no project NPC definitions are installed."));
		return true;
	}

	const UNPCDefinition* BanditGuard = LoadObject<UNPCDefinition>(nullptr,
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryBandit.NPC_TerritoryBandit"));
	const UNPCDefinition* HeroGuard = LoadObject<UNPCDefinition>(nullptr,
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryHero.NPC_TerritoryHero"));
	const UNPCDefinition* BanditAssault = LoadObject<UNPCDefinition>(nullptr,
		TEXT("/Game/TerritoryFramework/AI/NPC_TerritoryBanditAssault.NPC_TerritoryBanditAssault"));
	TestNotNull(TEXT("Bandit guard definition is available"), BanditGuard);
	TestNotNull(TEXT("Hero guard definition is available"), HeroGuard);
	TestNotNull(TEXT("Bandit assault definition is available"), BanditAssault);
	if (!BanditGuard || !HeroGuard || !BanditAssault) return false;

	TestTrue(TEXT("Bandit guard owns a valid Narrative definition GUID"),
		BanditGuard->UniqueNPCGUID.IsValid());
	TestTrue(TEXT("Hero guard owns a valid Narrative definition GUID"),
		HeroGuard->UniqueNPCGUID.IsValid());
	TestTrue(TEXT("Bandit assault owns a valid Narrative definition GUID"),
		BanditAssault->UniqueNPCGUID.IsValid());
	TestNotEqual(TEXT("Bandit and Hero guard definitions have distinct GUIDs"),
		BanditGuard->UniqueNPCGUID, HeroGuard->UniqueNPCGUID);
	TestNotEqual(TEXT("Bandit guard and assault definitions have distinct GUIDs"),
		BanditGuard->UniqueNPCGUID, BanditAssault->UniqueNPCGUID);
	TestNotEqual(TEXT("Hero guard and assault definitions have distinct GUIDs"),
		HeroGuard->UniqueNPCGUID, BanditAssault->UniqueNPCGUID);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryDataValidatorModernApi,
	"TerritoryFramework.Editor.DataValidation.ModernApiExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryDataValidatorModernApi::RunTest(const FString& Parameters)
{
	UTerritoryDataValidator* Validator = NewObject<UTerritoryDataValidator>();
	UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	const FAssetData AssetData(Profile);
	const TConstArrayView<FAssetData> NoAssociatedAssets;

	FDataValidationContext ValidContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestTrue(TEXT("Modern validator accepts counterattack profiles"),
		Validator->CanValidateAsset_Implementation(AssetData, Profile, ValidContext));
	TestEqual(TEXT("Valid profile completes the modern validation path"),
		Validator->ValidateLoadedAsset_Implementation(AssetData, Profile, ValidContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("Empty faction forces are a warning, not an error"),
		ValidContext.GetNumErrors(), 0u);
	TestTrue(TEXT("Empty faction forces emit a validation warning"),
		ValidContext.GetNumWarnings() > 0u);

	Profile->MaxConsecutiveSpawnFailures = 0;
	FDataValidationContext InvalidContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("Invalid profile fails the modern validation path"),
		Validator->ValidateLoadedAsset_Implementation(AssetData, Profile, InvalidContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid spawn failure bound emits an error"),
		InvalidContext.GetNumErrors() > 0u);

	Profile->MaxConsecutiveSpawnFailures = 5;
	Profile->ParticipantSpacing = 0.f;
	Profile->SpawnPlacementAttemptsPerParticipant = 0;
	Profile->StalledMovementRetryInterval = 0.f;
	Profile->MaxStalledMovementRetries = 0;
	FDataValidationContext InvalidDeploymentContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("Invalid deployment and movement recovery settings are rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, InvalidDeploymentContext),
		EDataValidationResult::Invalid);
	TestEqual(TEXT("Every invalid deployment and movement recovery setting emits an error"),
		InvalidDeploymentContext.GetNumErrors(), 4u);
	Profile->ParticipantSpacing = 220.f;
	Profile->SpawnPlacementAttemptsPerParticipant = 4;
	Profile->StalledMovementRetryInterval = 1.5f;
	Profile->MaxStalledMovementRetries = 8;

	Profile->MinimumLaunchProbability = 0.5f;
	Profile->UnguardedLaunchProbability = 0.25f;
	FDataValidationContext NonMonotonicContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A profile that makes the first guard increase launch probability is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, NonMonotonicContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Non-monotonic launch configuration emits an error"),
		NonMonotonicContext.GetNumErrors() > 0u);
	Profile->MinimumLaunchProbability = 0.01f;
	Profile->UnguardedLaunchProbability = 1.f;

	UNPCDefinition* Definition = NewObject<UNPCDefinition>(Profile);
	Definition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	Definition->bAllowMultipleInstances = true;
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Force.AttackerDefinition = Definition;
	Profile->FactionForces = {Force};
	FDataValidationContext SpawnReadyContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("Native Territory assault class passes complete Narrative spawn validation"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, SpawnReadyContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("Spawn-ready class emits no validation error"),
		SpawnReadyContext.GetNumErrors(), 0u);

	Force.RecurringCounterCooldownGameTime = 0.f;
	Profile->FactionForces = {Force};
	FDataValidationContext InvalidRecurringContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A zero recurring cooldown is rejected instead of rerolling every update"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, InvalidRecurringContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid recurring cooldown emits a validation error"),
		InvalidRecurringContext.GetNumErrors() > 0u);
	Force.RecurringCounterCooldownGameTime = 900.f;
	Profile->FactionForces = {Force};
	Force.ScheduleMode = ETerritoryCounterScheduleMode::FiniteSeries;
	Force.MaximumScheduledAssaults = 0;
	Profile->FactionForces = {Force};
	FDataValidationContext InvalidFiniteScheduleContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A finite schedule with no allowed assault is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, InvalidFiniteScheduleContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid finite schedule emits a validation error"),
		InvalidFiniteScheduleContext.GetNumErrors() > 0u);
	Force.MaximumScheduledAssaults = 3;
	Force.TimePolicy = ETerritoryCounterTimePolicy::NarrativeTimeWindow;
	Force.TimeWindowStart = -1.f;
	Profile->FactionForces = {Force};
	FDataValidationContext InvalidTimeWindowContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("An out-of-range Narrative time window is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, InvalidTimeWindowContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid Narrative time window emits a validation error"),
		InvalidTimeWindowContext.GetNumErrors() > 0u);
	Force.TimeWindowStart = 1800.f;
	Force.TimeWindowEnd = 500.f;
	Profile->FactionForces = {Force};

	Definition->bAllowMultipleInstances = false;
	FDataValidationContext SingleInstanceContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A multi-pawn force rejects a single-instance Narrative definition"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, SingleInstanceContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Single-instance force mismatch emits a validation error"),
		SingleInstanceContext.GetNumErrors() > 0u);
	Definition->bAllowMultipleInstances = true;

	Definition->CharacterID = NAME_None;
	FDataValidationContext MissingCharacterIDContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A Narrative assault definition without CharacterID is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, MissingCharacterIDContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Missing CharacterID emits a validation error"),
		MissingCharacterIDContext.GetNumErrors() > 0u);
	Definition->CharacterID = TEXT("TestTerritoryAssault");

	Definition->NPCID = NAME_None;
	FDataValidationContext MissingNPCIDContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A Narrative assault definition without NPCID is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, MissingNPCIDContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Missing NPCID emits a validation error"),
		MissingNPCIDContext.GetNumErrors() > 0u);
	Definition->NPCID = TEXT("TestTerritoryAssault");

	Definition->NPCClassPath = ANarrativeNPCCharacter::StaticClass();
	FDataValidationContext WrongPawnContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A non-Territory Narrative pawn is rejected for physical assaults"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, WrongPawnContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid physical pawn emits a validation error"),
		WrongPawnContext.GetNumErrors() > 0u);

	UTerritoryGuardPostDefinition* GuardPost =
		NewObject<UTerritoryGuardPostDefinition>();
	const FAssetData GuardPostAssetData(GuardPost);
	FDataValidationContext EmptyGuardPostContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestTrue(TEXT("Modern validator accepts reusable guard post definitions"),
		Validator->CanValidateAsset_Implementation(
			GuardPostAssetData, GuardPost, EmptyGuardPostContext));
	TestEqual(TEXT("An intentionally static guard post is valid"),
		Validator->ValidateLoadedAsset_Implementation(
			GuardPostAssetData, GuardPost, EmptyGuardPostContext),
		EDataValidationResult::Valid);

	GuardPost->PatrolRoute.SetNum(1);
	FDataValidationContext SingleNodePatrolContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A misleading one-node patrol route is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			GuardPostAssetData, GuardPost, SingleNodePatrolContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("One-node patrol route emits a validation error"),
		SingleNodePatrolContext.GetNumErrors() > 0u);

	GuardPost->PatrolRoute.SetNum(2);
	FDataValidationContext PatrolReadyContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A two-node reusable patrol route is valid"),
		Validator->ValidateLoadedAsset_Implementation(
			GuardPostAssetData, GuardPost, PatrolReadyContext),
		EDataValidationResult::Valid);

	UTerritoryProductionProfile* ProductionProfile =
		NewObject<UTerritoryProductionProfile>();
	const FAssetData ProductionAssetData(ProductionProfile);
	FDataValidationContext EmptyProductionContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestTrue(TEXT("Modern validator accepts production profiles"),
		Validator->CanValidateAsset_Implementation(
			ProductionAssetData, ProductionProfile, EmptyProductionContext));
	TestEqual(TEXT("A production profile without rules is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			ProductionAssetData, ProductionProfile, EmptyProductionContext),
		EDataValidationResult::Invalid);

	FTerritoryProductionRule ProductionRule;
	ProductionRule.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate ProductionOutput;
	ProductionOutput.ItemClass = UNarrativeItem::StaticClass();
	ProductionOutput.QuantityPerCycle = 1;
	ProductionRule.Outputs.Add(ProductionOutput);
	ProductionProfile->Rules.Add(ProductionRule);
	FDataValidationContext ValidProductionContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A complete production profile passes editor validation"),
		Validator->ValidateLoadedAsset_Implementation(
			ProductionAssetData, ProductionProfile, ValidProductionContext),
		EDataValidationResult::Valid);

	UTerritoryPlaceDefinition* Place = NewObject<UTerritoryPlaceDefinition>();
	Place->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Place->DisplayName = FText::FromString(TEXT("Blacksmith"));
	Place->StableTerritoryGUID = FGuid::NewGuid();
	Place->TerritoryActorClass = ATerritoryProperty::StaticClass();
	FTerritoryGuardPostTemplate& EmbeddedPost =
		Place->GuardPosts.AddDefaulted_GetRef();
	EmbeddedPost.GuardPostID = TEXT("FrontDoor");
	EmbeddedPost.StableGuardPostGUID = FGuid::NewGuid();
	EmbeddedPost.ActorClass = ATerritoryGuardSpawnPoint::StaticClass();
	const FAssetData PlaceAssetData(Place);
	FDataValidationContext ValidPlaceContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestTrue(TEXT("Modern validator accepts modular Place definitions"),
		Validator->CanValidateAsset_Implementation(
			PlaceAssetData, Place, ValidPlaceContext));
	TestEqual(TEXT("A complete embedded guard post passes definition validation"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, ValidPlaceContext),
		EDataValidationResult::Valid);

	FTerritoryStateConfig& ClaimedConfig = Place->StateConfigs.FindChecked(
		ETerritoryState::Claimed);
	ClaimedConfig.EntryEvents.Add(
		NewObject<UTerritoryScheduleEnemyWaveEvent>(Place));
	UTerritorySetDiplomacyEvent* PrematurePeace =
		NewObject<UTerritorySetDiplomacyEvent>(Place);
	PrematurePeace->NewState = EDiplomacyState::None;
	ClaimedConfig.EntryEvents.Add(PrematurePeace);
	FDataValidationContext ContradictoryWaveContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A Wave plus immediate peace remains structurally valid"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, ContradictoryWaveContext),
		EDataValidationResult::Valid);
	TArray<FText> ContradictoryWaveWarnings;
	TArray<FText> ContradictoryWaveErrors;
	ContradictoryWaveContext.SplitIssues(
		ContradictoryWaveWarnings, ContradictoryWaveErrors);
	TestTrue(TEXT("A Wave plus immediate peace explains why deployment will cancel"),
		ContradictoryWaveWarnings.ContainsByPredicate(
			[](const FText& Warning)
			{
				return Warning.ToString().Contains(
					TEXT("peace-like event will cancel the Wave"));
			}));
	ClaimedConfig.EntryEvents.Reset();

	FTerritoryStateAudioConfig& PlaceAudio = Place->StateConfigs.FindChecked(
		ETerritoryState::Contested).Audio;
	PlaceAudio.bOverrideNarrativeMusic = true;
	FDataValidationContext MissingMusicThemeContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A music override without a theme is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, MissingMusicThemeContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("A missing Territory music theme emits an error"),
		MissingMusicThemeContext.GetNumErrors() > 0u);

	PlaceAudio.MusicTheme = FGameplayTag::RequestGameplayTag(
		TEXT("Music.Combat"), false);
	PlaceAudio.StateEffectVolume = 5.f;
	FDataValidationContext InvalidStateEffectContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("An out-of-range Territory state effect is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, InvalidStateEffectContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("An invalid Territory state effect emits an error"),
		InvalidStateEffectContext.GetNumErrors() > 0u);
	PlaceAudio.StateEffectVolume = 1.f;
	PlaceAudio.MusicSetOverride = NewObject<UTaggedMusicSet>(Place);
	FDataValidationContext MissingMusicTrackContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A Tagged Music Set without the selected theme is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, MissingMusicTrackContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("A missing Tagged Music Set theme row emits an error"),
		MissingMusicTrackContext.GetNumErrors() > 0u);
	PlaceAudio = FTerritoryStateAudioConfig();

	EmbeddedPost.ReserveTotalRetryLimit = 0;
	FDataValidationContext InvalidEmbeddedPostContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("An invalid embedded reserve policy is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			PlaceAssetData, Place, InvalidEmbeddedPostContext),
		EDataValidationResult::Invalid);
	EmbeddedPost.ReserveTotalRetryLimit = 10;

	UTerritoryDistrictDefinition* District =
		NewObject<UTerritoryDistrictDefinition>();
	UTerritoryCityDefinition* City = NewObject<UTerritoryCityDefinition>();
	District->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	District->StableTerritoryGUID = FGuid::NewGuid();
	District->TerritoryActorClass = ATerritoryDistrict::StaticClass();
	City->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach"), false);
	City->StableTerritoryGUID = District->StableTerritoryGUID;
	City->TerritoryActorClass = ATerritoryCity::StaticClass();
	District->Places.Add(Place);
	City->Districts.Add(District);
	City->RefreshHierarchyLinks();
	const FAssetData CityAssetData(City);
	FDataValidationContext DuplicateHierarchyGuidContext(
		false, EDataValidationUsecase::Script, NoAssociatedAssets);
	TestEqual(TEXT("A duplicate save GUID in one City hierarchy is rejected"),
		Validator->ValidateLoadedAsset_Implementation(
			CityAssetData, City, DuplicateHierarchyGuidContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Duplicate hierarchy identity emits an error"),
		DuplicateHierarchyGuidContext.GetNumErrors() > 0u);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryQuestTerminalStateValidation,
	"TerritoryFramework.Editor.DataValidation.NarrativeQuestTerminalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryQuestTerminalStateValidation::RunTest(const FString& Parameters)
{
	UQuestBlueprint* Blueprint = NewObject<UQuestBlueprint>();
	TestNotNull(TEXT("Narrative Quest Blueprint creates its Quest Template"),
		Blueprint->QuestTemplate);
	if (!Blueprint->QuestTemplate) return false;

	UQuestState* Start = NewObject<UQuestState>(Blueprint->QuestTemplate);
	UQuestState* Intermediate = NewObject<UQuestState>(Blueprint->QuestTemplate);
	UQuestState* Success = NewObject<UQuestState>(Blueprint->QuestTemplate);
	UQuestBranch* FirstBranch = NewObject<UQuestBranch>(Blueprint->QuestTemplate);
	UQuestBranch* CaptureBranch = NewObject<UQuestBranch>(Blueprint->QuestTemplate);
	UTerritoryCaptureTask* CaptureTask = NewObject<UTerritoryCaptureTask>(CaptureBranch);

	Start->StateNodeType = EStateNodeType::Regular;
	Intermediate->StateNodeType = EStateNodeType::Success;
	Success->StateNodeType = EStateNodeType::Success;
	FirstBranch->DestinationState = Intermediate;
	CaptureBranch->DestinationState = Success;
	CaptureBranch->QuestTasks.Add(CaptureTask);
	Start->Branches.Add(FirstBranch);
	Intermediate->Branches.Add(CaptureBranch);
	Blueprint->QuestTemplate->AddState(Start);
	Blueprint->QuestTemplate->AddState(Intermediate);
	Blueprint->QuestTemplate->AddState(Success);
	Blueprint->QuestTemplate->AddBranch(FirstBranch);
	Blueprint->QuestTemplate->AddBranch(CaptureBranch);
	Blueprint->QuestTemplate->SetQuestStartState(Start);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	TestFalse(TEXT("A Success state with a later Territory objective is invalid"),
		UTerritoryDataValidator::ValidateQuest(Blueprint, Errors, Warnings));
	TestTrue(TEXT("The validation message explains duplicate completion"),
		Errors.ContainsByPredicate([](const FString& Error)
		{
			return Error.Contains(TEXT("complete the same quest again"));
		}));

	Intermediate->StateNodeType = EStateNodeType::Regular;
	FirstBranch->QuestTasks.Add(NewObject<UTerritoryCaptureTask>(FirstBranch));
	Errors.Reset();
	Warnings.Reset();
	TestTrue(TEXT("A Regular intermediate state followed by one final Success is valid"),
		UTerritoryDataValidator::ValidateQuest(Blueprint, Errors, Warnings));
	return true;
}

#endif

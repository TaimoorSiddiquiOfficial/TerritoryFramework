#include "TerritoryFrameworkEditor.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Assets/TerritoryAssetTypeActions.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryVolume.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Navigation/TerritoryRoadTrafficActors.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Tales/TerritoryQuestCascadeEditorLibrary.h"
#include "TerritoryDefinitionEditorLibrary.h"
#include "Vehicles/Mass/MassVehicleSpawner.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawnerTypes.h"
#include "MassEntityConfigAsset.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/UnrealType.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "QuestBlueprint.h"
#include "Styling/AppStyle.h"
#include "Story/STerritoryStoryOutcomePanel.h"
#include "Tales/STerritoryQuestMissionSummaryPanel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr TCHAR TerritoryNarrativeTaskSearchPath[] =
		TEXT("/TerritoryFramework/Tales/Tasks/");

	/**
	 * Narrative discovers task Blueprint assets from an editor setting rather than
	 * from native UNarrativeTask classes. Plugin DefaultEngine.ini files do not
	 * participate in the project's Engine config hierarchy, so register our
	 * content-only path on the settings CDO without changing Narrative or the
	 * host project's config file.
	 */
	bool RegisterNarrativeTaskSearchPath()
	{
		FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("NarrativeQuestEditor"));
		UClass* SettingsClass = FindObject<UClass>(
			nullptr, TEXT("/Script/NarrativeQuestEditor.QuestEditorSettings"));
		if (!SettingsClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[TerritoryNarrativeTask] Narrative Quest Editor settings class is unavailable; Territory tasks will not appear in its picker."));
			return false;
		}

		FArrayProperty* SearchPathsProperty = FindFProperty<FArrayProperty>(
			SettingsClass, TEXT("QuestTaskSearchPaths"));
		FStrProperty* PathProperty = SearchPathsProperty
			? CastField<FStrProperty>(SearchPathsProperty->Inner) : nullptr;
		UObject* Settings = SettingsClass->GetDefaultObject();
		if (!SearchPathsProperty || !PathProperty || !Settings)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[TerritoryNarrativeTask] Narrative QuestTaskSearchPaths has an unexpected shape; Territory tasks will not appear in its picker."));
			return false;
		}

		FScriptArrayHelper Paths(SearchPathsProperty,
			SearchPathsProperty->ContainerPtrToValuePtr<void>(Settings));
		for (int32 Index = 0; Index < Paths.Num(); ++Index)
		{
			if (PathProperty->GetPropertyValue(Paths.GetRawPtr(Index)).Equals(
				TerritoryNarrativeTaskSearchPath, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const int32 NewIndex = Paths.AddValue();
		PathProperty->SetPropertyValue(Paths.GetRawPtr(NewIndex),
			TerritoryNarrativeTaskSearchPath);
		return true;
	}

	/**
	 * Narrative's AMassVehicleSpawner constructor creates its generator with no
	 * explicit Outer. Blueprint instances can consequently retain an archetype
	 * reference to a private object owned by the Blueprint CDO, and Unreal then
	 * refuses to save the containing map. Recreate only the placed actor's
	 * generator as a clean instance owned by that actor; Narrative source remains
	 * untouched and the configured generator class/proportion are preserved.
	 */
	bool RepairMassVehicleSpawnGeneratorOwnership(AMassVehicleSpawner* Spawner)
	{
		if (!IsValid(Spawner)) return false;

		FArrayProperty* SpawnGeneratorsProperty = FindFProperty<FArrayProperty>(
			Spawner->GetClass(), TEXT("SpawnDataGenerators"));
		if (!SpawnGeneratorsProperty) return false;

		FScriptArrayHelper SpawnGenerators(SpawnGeneratorsProperty,
			SpawnGeneratorsProperty->ContainerPtrToValuePtr<void>(Spawner));
		bool bRepaired = false;
		for (int32 Index = 0; Index < SpawnGenerators.Num(); ++Index)
		{
			FMassSpawnDataGenerator* Generator =
				reinterpret_cast<FMassSpawnDataGenerator*>(
					SpawnGenerators.GetRawPtr(Index));
			if (!Generator) continue;

			UClass* GeneratorClass = Generator->GeneratorClass.Get();
			if (!GeneratorClass && Generator->GeneratorInstance)
			{
				GeneratorClass = Generator->GeneratorInstance->GetClass();
			}
			if (!GeneratorClass || !GeneratorClass->IsChildOf(
				UMassEntitySpawnDataGeneratorBase::StaticClass()))
			{
				continue;
			}

			const UObject* CurrentArchetype = Generator->GeneratorInstance
				? Generator->GeneratorInstance->GetArchetype() : nullptr;
			const bool bHasPrivateBlueprintArchetype = CurrentArchetype
				&& CurrentArchetype->GetOuter()
				&& CurrentArchetype->GetOuter()->HasAnyFlags(RF_ClassDefaultObject)
				&& CurrentArchetype->GetOuter()->GetClass()->ClassGeneratedBy;
			if (Generator->GeneratorInstance
				&& Generator->GeneratorInstance->GetOuter() == Spawner
				&& !bHasPrivateBlueprintArchetype)
			{
				continue;
			}

			Generator->GeneratorInstance =
				NewObject<UMassEntitySpawnDataGeneratorBase>(Spawner,
					GeneratorClass, NAME_None, RF_Transactional);
			bRepaired = Generator->GeneratorInstance != nullptr || bRepaired;
		}
		return bRepaired;
	}

	class FTerritoryDefinitionDetails final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FTerritoryDefinitionDetails>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			TArray<TWeakObjectPtr<UObject>> Objects;
			DetailBuilder.GetObjectsBeingCustomized(Objects);
			UTerritoryDefinition* SelectedDefinition = nullptr;
			TWeakObjectPtr<UTerritoryPlaceDefinition> SelectedPlace;
			bool bContainsNonPlace = false;
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (!SelectedDefinition)
				{
					SelectedDefinition = Cast<UTerritoryDefinition>(Object.Get());
				}
				if (!SelectedPlace.IsValid())
				{
					SelectedPlace = Cast<UTerritoryPlaceDefinition>(Object.Get());
				}
				if (Object.IsValid() && !Object->IsA<UTerritoryPlaceDefinition>())
				{
					bContainsNonPlace = true;
				}
			}
			if (SelectedDefinition)
			{
				IDetailCategoryBuilder& StoryOutcome = DetailBuilder.EditCategory(
					TEXT("00 Story Outcome (Read Only)"),
					FText::FromString(TEXT("Story Outcome (Read Only)")),
					ECategoryPriority::Important);
				StoryOutcome.InitiallyCollapsed(false);
				StoryOutcome.AddCustomRow(FText::FromString(TEXT("Story Outcome")))
				.WholeRowContent()
				[
					SNew(STerritoryStoryOutcomePanel)
					.Definition(SelectedDefinition)
				];
			}
			if (SelectedPlace.IsValid())
			{
				IDetailCategoryBuilder& Capture = DetailBuilder.EditCategory(
					TEXT("06 Capture"), FText::FromString(TEXT("Capture")));
				Capture.AddCustomRow(FText::FromString(TEXT("Active Capture Mode")))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Active Capture Mode")))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.ValueContent()
				.MinDesiredWidth(400.f)
				[
					SNew(STextBlock)
					.Text_Lambda([SelectedPlace]()
					{
						const UTerritoryPlaceDefinition* Place = SelectedPlace.Get();
						if (!Place) return FText::GetEmpty();
						if (Place->bStoryCaptureFromBounds)
						{
							return FText::FromString(
								TEXT("Story Bounds — multi-floor contest + explicit handover; physical automatic progress is off."));
						}
						if (Place->CapturePoint.bEnabled
							&& Place->CapturePoint.bAutomaticCapture)
						{
							return FText::FromString(
								TEXT("Physical Automatic — domination/multiplayer progress inside the Capture Point."));
						}
						if (Place->CapturePoint.bEnabled)
						{
							return FText::FromString(
								TEXT("Manual Capture Point — visible/interactable point; Narrative or server logic owns capture."));
						}
						return FText::FromString(
							TEXT("Explicit Story/Server — no physical point; a Narrative Event, owner handover, or server action captures."));
					})
					.AutoWrapText(true)
					.ToolTipText(FText::FromString(
						TEXT("This is the effective capture authority. Story Bounds and Physical Automatic are mutually exclusive.")))
				];
			}
			if (!bContainsNonPlace) return;

			// These legacy serialized fields remain loadable for bounded migration, but
			// City/District authors cannot edit them. Runtime and validation enforce the
			// same Place-only boundary.
			const FName PlaceOnlyProperties[] = {
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialOwningFaction),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialState),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, MaxConcurrentAttackers),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PeriodicIncome),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardUpkeepPerCycle),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardRecruitmentCost),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultStealthProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, bStoryCaptureFromBounds),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CapturePoint),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultGuardDefinition),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FactionGuardDefinitions),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialGuardCount),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PostCaptureGarrisonPolicy),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardBehavior),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardPosts),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefenderDiedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, AllDefendersDefeatedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackApproaches),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardQuality),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FortificationStrength),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, NearbyAlliedSupport)
			};
			for (const FName PropertyName : PlaceOnlyProperties)
			{
				DetailBuilder.HideProperty(PropertyName, UTerritoryDefinition::StaticClass());
			}
		}
	};

	class FTerritoryQuestCascadeRecipeDetails final
		: public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FTerritoryQuestCascadeRecipeDetails>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			TArray<TWeakObjectPtr<UObject>> Objects;
			DetailBuilder.GetObjectsBeingCustomized(Objects);
			TWeakObjectPtr<UTerritoryQuestCascadeRecipe> Recipe;
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (UTerritoryQuestCascadeRecipe* Candidate =
					Cast<UTerritoryQuestCascadeRecipe>(Object.Get()))
				{
					Recipe = Candidate;
					break;
				}
			}
			if (!Recipe.IsValid()) return;

			IDetailCategoryBuilder& MissionSummary = DetailBuilder.EditCategory(
				TEXT("00 Mission Logic (Read Only)"),
				FText::FromString(TEXT("Mission Logic (Read Only)")),
				ECategoryPriority::Important);
			MissionSummary.InitiallyCollapsed(false);
			MissionSummary.AddProperty(DetailBuilder.GetProperty(
				GET_MEMBER_NAME_CHECKED(UTerritoryQuestCascadeRecipe,
					NarrativeQuestGraph)));
			MissionSummary.AddCustomRow(FText::FromString(TEXT("Mission Logic")))
			.WholeRowContent()
			[
				SNew(STerritoryQuestMissionSummaryPanel)
				.Recipe(Recipe)
			];

			IDetailCategoryBuilder& Generate = DetailBuilder.EditCategory(
				TEXT("00 Create Narrative Quest"),
				FText::FromString(TEXT("Create Narrative Quest")),
				ECategoryPriority::Important);
			Generate.InitiallyCollapsed(false);
			Generate.AddCustomRow(FText::FromString(TEXT("Explanation")))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(
					TEXT("Creates a new, editable Narrative Quest beside this recipe. "
						"Tasks on one branch are ALL required; separate branches are alternative routes. "
						"The recipe is never used as a second runtime.")))
			];
			Generate.AddCustomRow(FText::FromString(TEXT("Generate Quest")))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Create New Narrative Quest From This Recipe")))
				.ToolTipText(FText::FromString(
					TEXT("Validates the recipe, creates a unique NQ_ asset in this folder, "
						"builds its normal Narrative graph, compiles it, and opens it.")))
				.OnClicked_Lambda([Recipe]()
				{
					FTerritoryQuestCascadeBuildReport Report =
						UTerritoryQuestCascadeEditorLibrary::CreateQuestBesideRecipe(
							Recipe.Get());
					for (const FText& Error : Report.Errors)
					{
						UE_LOG(LogTemp, Error,
							TEXT("[TerritoryQuestCascade] %s"), *Error.ToString());
					}
					for (const FText& Warning : Report.Warnings)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[TerritoryQuestCascade] %s"), *Warning.ToString());
					}
					if (Report.bSucceeded && Report.QuestAsset && GEditor)
					{
						UE_LOG(LogTemp, Display,
							TEXT("[TerritoryQuestCascade] Created %s with %d states, %d branches, %d tasks, %d functional condition gates, and %d automatic checkpoints."),
							*Report.QuestPackageName, Report.CreatedStates,
							Report.CreatedBranches, Report.CreatedTasks,
							Report.CreatedConditionGates,
							Report.CreatedCheckpointEvents);
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
							->OpenEditorForAsset(Report.QuestAsset);
					}
					return FReply::Handled();
				})
			];
			Generate.AddCustomRow(FText::FromString(TEXT("Migrate Quest Conditions")))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(FText::FromString(
					TEXT("Remove Unsupported Quest-Node Conditions")))
				.ToolTipText(FText::FromString(
					TEXT("Moves State and Branch condition rows from the selected Narrative Quest into hidden Wait For Narrative Conditions Tasks, then clears the unsupported Quest-node arrays. Event conditions are unchanged.")))
				.OnClicked_Lambda([Recipe]()
				{
					UClass* QuestClass = Recipe.IsValid()
						? Recipe->NarrativeQuestGraph.LoadSynchronous() : nullptr;
					UQuestBlueprint* QuestBlueprint = QuestClass
						? Cast<UQuestBlueprint>(QuestClass->ClassGeneratedBy) : nullptr;
					FTerritoryQuestCascadeBuildReport Report =
						UTerritoryQuestCascadeEditorLibrary::
						MigrateQuestNodeConditionsToGateTasks(QuestBlueprint);
					for (const FText& Error : Report.Errors)
					{
						UE_LOG(LogTemp, Error,
							TEXT("[TerritoryQuestCascade] %s"), *Error.ToString());
					}
					for (const FText& Warning : Report.Warnings)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[TerritoryQuestCascade] %s"), *Warning.ToString());
					}
					if (Report.bSucceeded)
					{
						UE_LOG(LogTemp, Display,
							TEXT("[TerritoryQuestCascade] Removed %d unsupported Quest-node condition rows and copied %d missing requirements into functional gate Tasks."),
							Report.RemovedQuestNodeConditions,
							Report.CopiedConditions);
					}
					return FReply::Handled();
				})
			];
		}
	};
}

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkEditorModule"

void FTerritoryFrameworkEditorModule::StartupModule()
{
	RegisterTerritoryAssetTypes();
	const bool bNarrativeTasksRegistered = RegisterNarrativeTaskSearchPath();
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
		TEXT("PropertyEditor"));
	// PropertyEditor queries registered layouts for both the concrete class and
	// its registered parents. Registering this same customization for the base
	// and all three derived Definition classes therefore inserts the complete
	// Story Outcome panel twice. One base registration is inherited by Place,
	// District, and City and still receives the concrete selected objects.
	PropertyEditor.RegisterCustomClassLayout(
		UTerritoryDefinition::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(
			&FTerritoryDefinitionDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(
		UTerritoryQuestCascadeRecipe::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(
			&FTerritoryQuestCascadeRecipeDetails::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
	EnsureVehicleRoadsConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.EnsureVehicleRoads"),
		TEXT("Create or update ZoneGraph roads and Blueprint Territory Road Guides for every enabled Narrative Vehicle approach in the loaded editor level."),
		FConsoleCommandDelegate::CreateRaw(
			this, &FTerritoryFrameworkEditorModule::EnsureVehicleRoadsForLoadedLevel),
		ECVF_Default);
	NormalizeClaimedDiplomacyConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.NormalizeClaimedDiplomacy"),
		TEXT("Migrate owner-relative Claimed state diplomacy rules in the loaded editor level to Neutral / No Treaty."),
		FConsoleCommandDelegate::CreateRaw(
			this, &FTerritoryFrameworkEditorModule::NormalizeClaimedDiplomacyForLoadedLevel),
		ECVF_Default);
	MigrateFactionSignatureVehiclesConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.MigrateFactionSignatureVehicles"),
		TEXT("Copy the first authored vehicle-approach Blueprint into each blank faction Signature Vehicle in loaded Territory profiles. Save All after review."),
		FConsoleCommandDelegate::CreateRaw(this,
			&FTerritoryFrameworkEditorModule::MigrateFactionSignatureVehiclesForLoadedLevel),
		ECVF_Default);
	// UEditorValidator subclasses are auto-registered by the DataValidation system
	// when the module loads — no manual registration needed
	UE_LOG(LogTemp, Log,
		TEXT("TerritoryFrameworkEditor module loaded (auto-validators registered, Narrative tasks %s)"),
		bNarrativeTasksRegistered ? TEXT("registered") : TEXT("unavailable"));
}

void FTerritoryFrameworkEditorModule::ShutdownModule()
{
	UnregisterTerritoryAssetTypes();
	if (EnsureVehicleRoadsConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			EnsureVehicleRoadsConsoleCommand, false);
		EnsureVehicleRoadsConsoleCommand = nullptr;
	}
	if (NormalizeClaimedDiplomacyConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			NormalizeClaimedDiplomacyConsoleCommand, false);
		NormalizeClaimedDiplomacyConsoleCommand = nullptr;
	}
	if (MigrateFactionSignatureVehiclesConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			MigrateFactionSignatureVehiclesConsoleCommand, false);
		MigrateFactionSignatureVehiclesConsoleCommand = nullptr;
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(
			UTerritoryDefinition::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomClassLayout(
			UTerritoryQuestCascadeRecipe::StaticClass()->GetFName());
	}
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module unloaded"));
}

void FTerritoryFrameworkEditorModule::RegisterTerritoryAssetTypes()
{
	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	TerritoryAssetCategory = AssetTools.FindAdvancedAssetCategory(
		FName(TEXT("TerritoryFramework")));
	if (TerritoryAssetCategory == EAssetTypeCategories::Misc)
	{
		TerritoryAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
			FName(TEXT("TerritoryFramework")),
			LOCTEXT("TerritoryFrameworkAssetCategory", "Territory Framework"));
	}

	auto Register = [this, &AssetTools](
		UClass* AssetClass,
		const FText& Name,
		const FText& Description,
		const FText& SubMenu,
		const FColor& Color)
	{
		TSharedPtr<IAssetTypeActions> Action =
			MakeShared<FTerritoryAssetTypeActions>(
				AssetClass,
				Name,
				Description,
				SubMenu,
				TerritoryAssetCategory,
				Color);
		AssetTools.RegisterAssetTypeActions(Action.ToSharedRef());
		RegisteredAssetTypeActions.Add(MoveTemp(Action));
	};

	const FText WorldMenu = LOCTEXT("TerritoryWorldAssetSubMenu", "World & Hierarchy");
	const FText CombatMenu = LOCTEXT("TerritoryCombatAssetSubMenu", "Combat & Defence");
	const FText EconomyMenu = LOCTEXT("TerritoryEconomyAssetSubMenu", "Economy");
	const FText StealthMenu = LOCTEXT("TerritoryStealthAssetSubMenu", "Stealth & Disguise");
	const FText DiplomacyMenu = LOCTEXT("TerritoryDiplomacyAssetSubMenu", "AI & Diplomacy");
	const FText StoryMenu = LOCTEXT("TerritoryStoryAssetSubMenu", "Story & Quests");

	Register(
		UTerritoryPlaceDefinition::StaticClass(),
		LOCTEXT("TerritoryPlaceDefinitionAsset", "Territory Place Definition"),
		LOCTEXT("TerritoryPlaceDefinitionDescription",
			"Creates one capturable location, such as a farm, blacksmith, checkpoint, or police station."),
		WorldMenu,
		FColor(50, 184, 159));
	Register(
		UTerritoryDistrictDefinition::StaticClass(),
		LOCTEXT("TerritoryDistrictDefinitionAsset", "Territory District Definition"),
		LOCTEXT("TerritoryDistrictDefinitionDescription",
			"Groups Place Definitions and calculates district control from those places."),
		WorldMenu,
		FColor(45, 156, 191));
	Register(
		UTerritoryCityDefinition::StaticClass(),
		LOCTEXT("TerritoryCityDefinitionAsset", "Territory City Definition"),
		LOCTEXT("TerritoryCityDefinitionDescription",
			"Groups District Definitions and calculates city control from those districts."),
		WorldMenu,
		FColor(55, 123, 181));
	Register(
		UTerritoryCounterAttackProfile::StaticClass(),
		LOCTEXT("TerritoryCounterAttackProfileAsset", "Territory Counter-Attack Profile"),
		LOCTEXT("TerritoryCounterAttackProfileDescription",
			"Configures faction forces, schedules, reinforcement vehicles, difficulty, and recapture behavior."),
		CombatMenu,
		FColor(216, 103, 52));
	Register(
		UTerritoryGuardPostDefinition::StaticClass(),
		LOCTEXT("TerritoryGuardPostDefinitionAsset", "Territory Guard Post Definition"),
		LOCTEXT("TerritoryGuardPostDefinitionDescription",
			"Creates reusable guard capacity, spawn, patrol, and garrison placement settings."),
		CombatMenu,
		FColor(185, 81, 65));
	Register(
		UTerritoryProductionProfile::StaticClass(),
		LOCTEXT("TerritoryProductionProfileAsset", "Territory Production Profile"),
		LOCTEXT("TerritoryProductionProfileDescription",
			"Defines money or Narrative Item production, required inputs, cycle timing, and ownership gates."),
		EconomyMenu,
		FColor(213, 174, 62));
	Register(
		UTerritoryStealthProfile::StaticClass(),
		LOCTEXT("TerritoryStealthProfileAsset", "Territory Stealth Profile"),
		LOCTEXT("TerritoryStealthProfileDescription",
			"Defines detection, exposure, suspicious actions, gunfire, and stealth-breaking rules."),
		StealthMenu,
		FColor(116, 105, 190));
	Register(
		UTerritoryDisguiseProfile::StaticClass(),
		LOCTEXT("TerritoryDisguiseProfileAsset", "Territory Disguise Profile"),
		LOCTEXT("TerritoryDisguiseProfileDescription",
			"Defines which faction a disguise represents and how suspicion can expose the player."),
		StealthMenu,
		FColor(148, 96, 184));
	Register(
		UTerritoryDiplomacyDialogueProfile::StaticClass(),
		LOCTEXT("TerritoryDiplomacyDialogueProfileAsset", "Territory Diplomacy Dialogue Profile"),
		LOCTEXT("TerritoryDiplomacyDialogueProfileDescription",
			"Selects friendly, neutral, suspicious, and hostile dialogue from the current faction relationship."),
		DiplomacyMenu,
		FColor(191, 88, 141));
	Register(
		UTerritoryQuestCascadeRecipe::StaticClass(),
		LOCTEXT("TerritoryQuestCascadeRecipeAsset", "Territory Quest Cascade Recipe"),
		LOCTEXT("TerritoryQuestCascadeRecipeDescription",
			"Builds a normal Narrative Quest from reusable states, route conditions, tasks, events, dialogue settings, and alternative endings."),
		StoryMenu,
		FColor(78, 171, 204));
}

void FTerritoryFrameworkEditorModule::UnregisterTerritoryAssetTypes()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		IAssetTools& AssetTools =
			FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}
	RegisteredAssetTypeActions.Reset();
	TerritoryAssetCategory = EAssetTypeCategories::Misc;
}

void FTerritoryFrameworkEditorModule::MigrateFactionSignatureVehiclesForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryVehicleMigration] Load a non-PIE editor level first."));
		return;
	}

	int32 UpdatedForces = 0;
	TSet<UTerritoryCounterAttackProfile*> VisitedProfiles;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		UTerritoryCounterAttackProfile* Profile = IsValid(Territory)
			? Territory->GetCounterAttackProfile() : nullptr;
		if (!IsValid(Profile) || VisitedProfiles.Contains(Profile)) continue;
		VisitedProfiles.Add(Profile);

		TSoftClassPtr<ANarrativeVehicleBase> ApproachFallback;
		for (const FTerritoryAssaultApproach& Approach :
			Territory->GetCounterAttackApproaches())
		{
			if (Approach.bEnabled
				&& Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle
				&& !Approach.VehicleClass.IsNull())
			{
				ApproachFallback = Approach.VehicleClass;
				break;
			}
		}
		if (ApproachFallback.IsNull()) continue;

		for (FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
		{
			if (!Force.Faction.IsValid() || !Force.SignatureVehicleClass.IsNull()) continue;
			Profile->Modify();
			Force.SignatureVehicleClass = ApproachFallback;
			Profile->MarkPackageDirty();
			++UpdatedForces;
			UE_LOG(LogTemp, Display,
				TEXT("[TerritoryVehicleMigration] %s now uses %s as the signature car in %s."),
				*Force.Faction.ToString(), *ApproachFallback.ToString(),
				*Profile->GetPathName());
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryVehicleMigration] Updated %d faction force(s) in %d profile(s). Review the cars, then Save All."),
		UpdatedForces, VisitedProfiles.Num());
}

void FTerritoryFrameworkEditorModule::NormalizeClaimedDiplomacyForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryDiplomacyMigration] Load a non-PIE editor level first."));
		return;
	}

	int32 UpdatedEvents = 0;
	TSet<UTerritoryDefinition*> VisitedDefinitions;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!IsValid(Territory)) continue;

		// Runtime State Configs are deliberately transient clones. Migrate the
		// authoritative Definition template so the correction survives reload,
		// then rebuild the actor clone below.
		UTerritoryDefinition* Definition = Territory->GetTerritoryDefinition();
		if (!IsValid(Definition) || VisitedDefinitions.Contains(Definition)) continue;
		VisitedDefinitions.Add(Definition);

		FTerritoryStateConfig* ClaimedConfig =
			Definition->StateConfigs.Find(ETerritoryState::Claimed);
		if (!ClaimedConfig) continue;

		// Older Place assets sometimes retained the Contested=War rule but lost
		// the matching Claimed=Neutral cleanup event. Repair that pair only when
		// the author has clearly opted into state-driven diplomacy.
		UTerritorySetDiplomacyEvent* ExistingClaimedDiplomacy = nullptr;
		for (const TObjectPtr<UNarrativeEvent>& Event : ClaimedConfig->EntryEvents)
		{
			if (UTerritorySetDiplomacyEvent* Typed =
				Cast<UTerritorySetDiplomacyEvent>(Event))
			{
				ExistingClaimedDiplomacy = Typed;
				break;
			}
		}
		const UTerritorySetDiplomacyEvent* ContestedWarTemplate = nullptr;
		if (const FTerritoryStateConfig* ContestedConfig =
			Definition->StateConfigs.Find(ETerritoryState::Contested))
		{
			for (const TObjectPtr<UNarrativeEvent>& Event : ContestedConfig->EntryEvents)
			{
				const UTerritorySetDiplomacyEvent* Typed =
					Cast<UTerritorySetDiplomacyEvent>(Event);
				if (Typed && Typed->NewState == EDiplomacyState::War)
				{
					ContestedWarTemplate = Typed;
					break;
				}
			}
		}
		if (!ExistingClaimedDiplomacy && ContestedWarTemplate)
		{
			Definition->Modify();
			UTerritorySetDiplomacyEvent* PeaceEvent =
				NewObject<UTerritorySetDiplomacyEvent>(Definition, NAME_None,
					RF_Transactional);
			PeaceEvent->FactionASource =
				ETerritoryDiplomacyFactionSource::CurrentOwningFaction;
			PeaceEvent->FactionBSource =
				ETerritoryDiplomacyFactionSource::TransitionOpposingFaction;
			PeaceEvent->FactionA = ContestedWarTemplate->FactionA;
			PeaceEvent->FactionB = ContestedWarTemplate->FactionB;
			PeaceEvent->NewState = EDiplomacyState::None;
			PeaceEvent->bApplyWhenStateStartsActive = true;
			PeaceEvent->bRequireContainingTerritoryOwner = true;
			PeaceEvent->bPreserveOtherActiveTerritoryWars = true;
			ClaimedConfig->EntryEvents.Add(PeaceEvent);
			Definition->MarkPackageDirty();
			++UpdatedEvents;
		}

		for (const TObjectPtr<UNarrativeEvent>& EntryEvent :
			ClaimedConfig->EntryEvents)
		{
			UTerritorySetDiplomacyEvent* DiplomacyEvent =
				Cast<UTerritorySetDiplomacyEvent>(EntryEvent);
			if (!DiplomacyEvent
				|| DiplomacyEvent->FactionASource !=
					ETerritoryDiplomacyFactionSource::CurrentOwningFaction
				|| DiplomacyEvent->FactionBSource !=
					ETerritoryDiplomacyFactionSource::TransitionOpposingFaction)
			{
				continue;
			}

			const bool bNeedsMigration =
				DiplomacyEvent->NewState != EDiplomacyState::None
				|| !DiplomacyEvent->bApplyWhenStateStartsActive
				|| !DiplomacyEvent->bRequireContainingTerritoryOwner
				|| !DiplomacyEvent->bPreserveOtherActiveTerritoryWars;
			if (bNeedsMigration)
			{
				Definition->Modify();
				DiplomacyEvent->Modify();
				DiplomacyEvent->NewState = EDiplomacyState::None;
				DiplomacyEvent->bApplyWhenStateStartsActive = true;
				DiplomacyEvent->bRequireContainingTerritoryOwner = true;
				DiplomacyEvent->bPreserveOtherActiveTerritoryWars = true;
				Definition->MarkPackageDirty();
				++UpdatedEvents;
			}
		}
	}

	if (UpdatedEvents > 0)
	{
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && VisitedDefinitions.Contains((*It)->GetTerritoryDefinition()))
			{
				(*It)->ApplyTerritoryDefinition();
			}
		}
	}
	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryDiplomacyMigration] Normalized %d owner-relative Claimed diplomacy event template(s) in %s. Save All to persist the changed Territory Definitions."),
		UpdatedEvents, *World->GetName());
}

void FTerritoryFrameworkEditorModule::EnsureVehicleRoadsForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryRoadSetup] Load a non-PIE editor level first."));
		return;
	}

	int32 Attempted = 0;
	int32 Succeeded = 0;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!IsValid(Territory)) continue;
		for (const FTerritoryAssaultApproach& Approach :
			Territory->GetCounterAttackApproaches())
		{
			if (!Approach.bEnabled || Approach.EntryType !=
				ETerritoryAssaultEntryType::NarrativeVehicle)
			{
				continue;
			}
			++Attempted;
			FText Failure;
			if (UTerritoryDefinitionEditorLibrary::EnsureStraightVehicleApproachRoad(
				Territory, Approach.ApproachID, Failure))
			{
				++Succeeded;
			}
			else
			{
				UE_LOG(LogTemp, Error,
					TEXT("[TerritoryRoadSetup] %s / %s failed: %s"),
					*Territory->GetTerritoryTag().ToString(),
					*Approach.ApproachID.ToString(), *Failure.ToString());
			}
		}
	}

	FBox CombinedRoadBounds(ForceInit);
	TArray<ATerritoryRoadGuide*> RoadGuides;
	for (TActorIterator<ATerritoryRoadGuide> It(World); It; ++It)
	{
		if (!IsValid(*It)) continue;
		RoadGuides.Add(*It);
		CombinedRoadBounds += It->GetComponentsBoundingBox(true);
	}
	if (CombinedRoadBounds.IsValid)
	{
		CombinedRoadBounds = CombinedRoadBounds.ExpandBy(
			FVector(2500.f, 2500.f, 1500.f));
	}

	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	TArray<AQuestRoadControls*> TrafficControllers;
	for (TActorIterator<AQuestRoadControls> It(World); It; ++It)
	{
		if (IsValid(*It)) TrafficControllers.Add(*It);
	}
	if (TrafficControllers.IsEmpty() && Settings)
	{
		UClass* ControlsClass =
			Settings->RoadTrafficControlsBlueprintClass.LoadSynchronous();
		if (ControlsClass && ControlsClass->IsChildOf(
			ATerritoryRoadTrafficControls::StaticClass()))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = World->PersistentLevel;
			SpawnParams.ObjectFlags = RF_Transactional;
			if (AQuestRoadControls* Controls = World->SpawnActor<AQuestRoadControls>(
				ControlsClass, FTransform::Identity, SpawnParams))
			{
				Controls->SetActorLabel(TEXT("Territory Mission Traffic Controls"));
				TrafficControllers.Add(Controls);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadSetup] Create and configure BP_TerritoryRoadTrafficControls as a Blueprint child of TerritoryRoadTrafficControls."));
		}
	}

	TArray<AMassVehicleSpawner*> TrafficSpawners;
	for (TActorIterator<AMassVehicleSpawner> It(World); It; ++It)
	{
		if (IsValid(*It)) TrafficSpawners.Add(*It);
	}
	if (TrafficSpawners.IsEmpty() && Settings)
	{
		UClass* SpawnerClass =
			Settings->RoadTrafficSpawnerBlueprintClass.LoadSynchronous();
		if (SpawnerClass && SpawnerClass->IsChildOf(
			AMassVehicleSpawner::StaticClass()))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = World->PersistentLevel;
			SpawnParams.ObjectFlags = RF_Transactional;
			if (AMassVehicleSpawner* Spawner = World->SpawnActor<AMassVehicleSpawner>(
				SpawnerClass, FTransform::Identity, SpawnParams))
			{
				Spawner->SetActorLabel(TEXT("Territory Mission Traffic Spawner"));
				TrafficSpawners.Add(Spawner);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadSetup] Create and configure BP_TerritoryRoadTrafficSpawner as a Blueprint child of Narrative MassVehicleSpawner."));
		}
	}
	if (TrafficSpawners.Num() == 1 && Settings)
	{
		AMassVehicleSpawner* Spawner = TrafficSpawners[0];
		Spawner->Modify();
		if (RepairMassVehicleSpawnGeneratorOwnership(Spawner))
		{
			UE_LOG(LogTemp, Display,
				TEXT("[TerritoryRoadSetup] Re-owned Narrative's Mass spawn generator to the placed traffic spawner so the level can save safely."));
		}
		if (FArrayProperty* EntityTypesProperty = FindFProperty<FArrayProperty>(
			Spawner->GetClass(), TEXT("EntityTypes")))
		{
			FScriptArrayHelper EntityTypes(EntityTypesProperty,
				EntityTypesProperty->ContainerPtrToValuePtr<void>(Spawner));
			EntityTypes.EmptyValues();
			const int32 NewIndex = EntityTypes.AddValue();
			if (FMassSpawnedEntityType* Type = reinterpret_cast<FMassSpawnedEntityType*>(
				EntityTypes.GetRawPtr(NewIndex)))
			{
				Type->EntityConfig = Settings->DefaultRoadTrafficEntityConfig;
				Type->Proportion = 1.f;
			}
		}
		if (FIntProperty* CountProperty = FindFProperty<FIntProperty>(
			Spawner->GetClass(), TEXT("Count")))
		{
			CountProperty->SetPropertyValue_InContainer(Spawner, 0);
		}
		if (FBoolProperty* AutoSpawnProperty = FindFProperty<FBoolProperty>(
			Spawner->GetClass(), TEXT("bAutoSpawnOnBeginPlay")))
		{
			AutoSpawnProperty->SetPropertyValue_InContainer(Spawner, false);
		}
		Spawner->MarkPackageDirty();
	}

	if (TrafficControllers.Num() == 1)
	{
		if (ATerritoryRoadTrafficControls* TerritoryControls =
			Cast<ATerritoryRoadTrafficControls>(TrafficControllers[0]);
			TerritoryControls && CombinedRoadBounds.IsValid)
		{
			TerritoryControls->Modify();
			TerritoryControls->SetMissionTrafficWorldBounds(CombinedRoadBounds);
		}
		for (ATerritoryRoadGuide* Guide : RoadGuides)
		{
			Guide->Modify();
			Guide->NarrativeTrafficControls = TrafficControllers[0];
			Guide->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TerritoryRoadSetup] Found %d QuestRoadControls actors. Narrative traffic linking requires exactly one shared world controller; road splines were still created."),
			TrafficControllers.Num());
	}

	World->MarkPackageDirty();
	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryRoadSetup] Completed %d/%d Narrative vehicle roads in %s with %d shared traffic controller(s) and %d Mass traffic spawner(s). Save the level and build ZoneGraph."),
		Succeeded, Attempted, *World->GetName(), TrafficControllers.Num(),
		TrafficSpawners.Num());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkEditorModule, TerritoryFrameworkEditor)

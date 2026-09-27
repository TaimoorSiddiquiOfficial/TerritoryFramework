#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cinematics/TerritoryCinematicLightRig.h"

#include "AI/TerritoryDiplomacyDialogue.h"
#include "AssetToolsModule.h"
#include "Assets/TerritoryAssetFactories.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Combat/TerritoryFloorCombatPolicy.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryWorldState.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Factories/BlueprintFactory.h"
#include "IAssetTools.h"
#include "IPlacementModeModule.h"
#include "ToolMenus.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Tales/TerritorySituationCondition.h"
#include "Tales/TerritoryDialogueRecipe.h"
#include "Social/TerritoryConversationProfile.h"
#include "Social/TerritoryConversationMeetingPoint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTerritoryAssetCreationMenuTest,
	"TerritoryFramework.Editor.Authoring.AssetCreationMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryAssetCreationMenuTest::RunTest(const FString& Parameters)
{
	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const uint32 Category = AssetTools.FindAdvancedAssetCategory(
		FName(TEXT("TerritoryFramework")));
	TestTrue(
		TEXT("Territory Framework has its own advanced Content Browser category"),
		Category != EAssetTypeCategories::Misc);
	if (!IsRunningCommandlet())
	{
		IPlacementModeModule* Placement = FModuleManager::GetModulePtr<IPlacementModeModule>(TEXT("PlacementMode"));
		if (TestNotNull(TEXT("Territory actor placement module is available"), Placement))
		{
			TArray<TSharedPtr<FPlaceableItem>> Items;
			Placement->GetItemsForCategory(TEXT("TerritoryFramework"), Items);
			TestTrue(TEXT("Floor geometry can be placed without hunting through class lists"), Items.ContainsByPredicate([](const TSharedPtr<FPlaceableItem>& Item)
			{
				return Item.IsValid() && Item->AssetData.GetAsset() == ATerritoryFloorVolume::StaticClass();
			}));
			TestTrue(TEXT("Conversation meeting points are directly placeable"), Items.ContainsByPredicate([](const TSharedPtr<FPlaceableItem>& Item)
			{
				return Item.IsValid() && Item->AssetData.GetAsset() == ATerritoryConversationMeetingPoint::StaticClass();
			}));
		}
		UToolMenu* Tools = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
		const FToolMenuSection* Section = Tools ? Tools->FindSection(TEXT("TerritoryFramework")) : nullptr;
		TestTrue(TEXT("Native Tools menu exposes the shared Territory authoring workflow"),
			Section && Section->FindEntry(TEXT("TerritoryFramework")));
	}
	const auto ContainsSubMenu = [](const TArray<FText>& Menus, const TCHAR* Expected)
	{
		for (const FText& Menu : Menus)
		{
			if (Menu.ToString().Equals(Expected, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	};

	struct FExpectedEntry
	{
		UClass* AssetClass;
		UClass* FactoryClass;
		const TCHAR* DisplayName;
		const TCHAR* SubMenu;
	};

	const FExpectedEntry Entries[] = {
		{ UTerritoryCinematicLightRigProfile::StaticClass(),
			UTerritoryCinematicLightRigProfileFactory::StaticClass(),
			TEXT("Territory Cinematic Light Rig Profile"), TEXT("Story & Quests") },
		{ UTerritoryPlaceDefinition::StaticClass(),
			UTerritoryPlaceDefinitionFactory::StaticClass(),
			TEXT("Territory Place Definition"), TEXT("World & Hierarchy") },
		{ UTerritoryDistrictDefinition::StaticClass(),
			UTerritoryDistrictDefinitionFactory::StaticClass(),
			TEXT("Territory District Definition"), TEXT("World & Hierarchy") },
		{ UTerritoryCityDefinition::StaticClass(),
			UTerritoryCityDefinitionFactory::StaticClass(),
			TEXT("Territory City Definition"), TEXT("World & Hierarchy") },
		{ UTerritoryCounterAttackProfile::StaticClass(),
			UTerritoryCounterAttackProfileFactory::StaticClass(),
			TEXT("Territory Counter-Attack Profile"), TEXT("Combat & Defence") },
		{ UTerritoryGuardPostDefinition::StaticClass(),
			UTerritoryGuardPostDefinitionFactory::StaticClass(),
			TEXT("Territory Guard Post Definition"), TEXT("Combat & Defence") },
		{ UTerritoryFloorCombatPolicy::StaticClass(),
			UTerritoryFloorCombatPolicyFactory::StaticClass(),
			TEXT("Territory Floor Combat Policy"), TEXT("Combat & Defence") },
		{ UTerritoryProductionProfile::StaticClass(),
			UTerritoryProductionProfileFactory::StaticClass(),
			TEXT("Territory Production Profile"), TEXT("Economy") },
		{ UTerritoryStealthProfile::StaticClass(),
			UTerritoryStealthProfileFactory::StaticClass(),
			TEXT("Territory Stealth Profile"), TEXT("Stealth & Disguise") },
		{ UTerritoryDisguiseProfile::StaticClass(),
			UTerritoryDisguiseProfileFactory::StaticClass(),
			TEXT("Territory Disguise Profile"), TEXT("Stealth & Disguise") },
		{ UTerritoryDiplomacyDialogueProfile::StaticClass(),
			UTerritoryDiplomacyDialogueProfileFactory::StaticClass(),
			TEXT("Territory Diplomacy Dialogue Profile"), TEXT("AI & Diplomacy") },
		{ UTerritoryQuestCascadeRecipe::StaticClass(),
			UTerritoryQuestCascadeRecipeFactory::StaticClass(),
			TEXT("Territory Quest Cascade Recipe"), TEXT("Story & Quests") },
		{ UTerritorySituationProfile::StaticClass(),
			UTerritorySituationProfileFactory::StaticClass(),
			TEXT("Territory Situation Profile"), TEXT("Story & Quests") },
		{ UTerritoryDialogueRecipe::StaticClass(),
			UTerritoryDialogueRecipeFactory::StaticClass(),
			TEXT("Territory Conditional Dialogue Recipe"), TEXT("Story & Quests") },
		{ UTerritoryConversationProfile::StaticClass(),
			UTerritoryConversationProfileFactory::StaticClass(),
			TEXT("Territory Conversation Profile"), TEXT("Story & Quests") }
	};

	for (const FExpectedEntry& Entry : Entries)
	{
		const TWeakPtr<IAssetTypeActions> Action =
			AssetTools.GetAssetTypeActionsForClass(Entry.AssetClass);
		TestTrue(
			*FString::Printf(TEXT("%s has registered asset actions"), Entry.DisplayName),
			Action.IsValid());
		if (!Action.IsValid())
		{
			continue;
		}

		const TSharedPtr<IAssetTypeActions> PinnedAction = Action.Pin();
		TestEqual(
			*FString::Printf(TEXT("%s has a clear menu name"), Entry.DisplayName),
			PinnedAction->GetName().ToString(),
			FString(Entry.DisplayName));
		TestTrue(
			*FString::Printf(TEXT("%s belongs to Territory Framework"), Entry.DisplayName),
			(PinnedAction->GetCategories() & Category) != 0);
		TestTrue(
			*FString::Printf(TEXT("%s has its expected submenu"), Entry.DisplayName),
			ContainsSubMenu(PinnedAction->GetSubMenus(), Entry.SubMenu));

		UFactory* Factory = NewObject<UFactory>(
			GetTransientPackage(), Entry.FactoryClass);
		TestNotNull(
			*FString::Printf(TEXT("%s factory exists"), Entry.DisplayName),
			Factory);
		if (!Factory)
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("%s is visible in Add/New"), Entry.DisplayName),
			Factory->ShouldShowInNewMenu());
		TestTrue(
			*FString::Printf(TEXT("%s factory supports the exact class"), Entry.DisplayName),
			Factory->GetSupportedClass() == Entry.AssetClass);

		const FName ObjectName = MakeUniqueObjectName(
			GetTransientPackage(), Entry.AssetClass,
			FName(*FString::Printf(TEXT("Test_%s"), *Entry.AssetClass->GetName())));
		UObject* Created = Factory->FactoryCreateNew(
			Entry.AssetClass,
			GetTransientPackage(),
			ObjectName,
			RF_Transient,
			nullptr,
			GWarn);
		TestTrue(
			*FString::Printf(TEXT("%s factory creates the exact asset"), Entry.DisplayName),
			Created && Created->GetClass() == Entry.AssetClass);
	}

	struct FExpectedBlueprintEntry
	{
		UClass* ParentClass;
		UClass* FactoryClass;
		const TCHAR* DisplayName;
		const TCHAR* SubMenu;
	};

	const FExpectedBlueprintEntry BlueprintEntries[] = {
		{ ATerritoryProperty::StaticClass(),
			UTerritoryPlaceActorBlueprintFactory::StaticClass(),
			TEXT("Territory Place Actor Blueprint"), TEXT("World Actors") },
		{ ATerritoryDistrict::StaticClass(),
			UTerritoryDistrictActorBlueprintFactory::StaticClass(),
			TEXT("Territory District Actor Blueprint"), TEXT("World Actors") },
		{ ATerritoryCity::StaticClass(),
			UTerritoryCityActorBlueprintFactory::StaticClass(),
			TEXT("Territory City Actor Blueprint"), TEXT("World Actors") },
		{ ATerritoryWorldState::StaticClass(),
			UTerritoryWorldStateBlueprintFactory::StaticClass(),
			TEXT("Territory World State Blueprint"), TEXT("World Actors") },
		{ ATerritoryFloorVolume::StaticClass(),
			UTerritoryFloorVolumeBlueprintFactory::StaticClass(),
			TEXT("Territory Floor Volume Blueprint"), TEXT("World Actors") },
		{ ATerritoryConversationMeetingPoint::StaticClass(),
			UTerritoryConversationMeetingPointBlueprintFactory::StaticClass(),
			TEXT("Territory Conversation Meeting Point Blueprint"), TEXT("Interaction & Navigation Actors") },
		{ ATerritoryCapturePoint::StaticClass(),
			UTerritoryCapturePointBlueprintFactory::StaticClass(),
			TEXT("Territory Capture Point Blueprint"), TEXT("Interaction & Navigation Actors") },
		{ ATerritoryGuardSpawnPoint::StaticClass(),
			UTerritoryGuardSpawnPointBlueprintFactory::StaticClass(),
			TEXT("Territory Guard Spawn Point Blueprint"), TEXT("Guard & Combat Actors") },
		{ ATerritoryStoryOwnerSpawner::StaticClass(),
			UTerritoryStoryOwnerSpawnerBlueprintFactory::StaticClass(),
			TEXT("Territory Story Owner Spawner Blueprint"), TEXT("Interaction & Navigation Actors") },
		{ ATerritoryDistrictManagementPoint::StaticClass(),
			UTerritoryDistrictManagementPointBlueprintFactory::StaticClass(),
			TEXT("Territory Management Point Blueprint"), TEXT("Interaction & Navigation Actors") },
		{ ATerritoryRoadGuide::StaticClass(),
			UTerritoryRoadGuideBlueprintFactory::StaticClass(),
			TEXT("Territory Road Guide Blueprint"), TEXT("Interaction & Navigation Actors") },
		{ ATerritoryGuardCharacter::StaticClass(),
			UTerritoryGuardCharacterBlueprintFactory::StaticClass(),
			TEXT("Territory Guard Character Blueprint"), TEXT("Guard & Combat Actors") },
		{ ATerritoryAssaultCharacter::StaticClass(),
			UTerritoryAssaultCharacterBlueprintFactory::StaticClass(),
			TEXT("Territory Assault Character Blueprint"), TEXT("Guard & Combat Actors") }
	};

	for (const FExpectedBlueprintEntry& Entry : BlueprintEntries)
	{
		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>(
			GetTransientPackage(), Entry.FactoryClass);
		TestNotNull(
			*FString::Printf(TEXT("%s factory exists"), Entry.DisplayName),
			Factory);
		if (!Factory)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s has a clear menu name"), Entry.DisplayName),
			Factory->GetDisplayName().ToString(),
			FString(Entry.DisplayName));
		TestTrue(
			*FString::Printf(TEXT("%s belongs to Territory Framework"), Entry.DisplayName),
			(Factory->GetMenuCategories() & Category) != 0);
		TestTrue(
			*FString::Printf(TEXT("%s has its expected submenu"), Entry.DisplayName),
			ContainsSubMenu(Factory->GetMenuCategorySubMenus(), Entry.SubMenu));
		TestTrue(
			*FString::Printf(TEXT("%s uses the exact parent class"), Entry.DisplayName),
			Factory->ParentClass == Entry.ParentClass);
		TestTrue(
			*FString::Printf(TEXT("%s skips the broad class picker"), Entry.DisplayName),
			Factory->bSkipClassPicker);
	}

	return true;
}

#endif

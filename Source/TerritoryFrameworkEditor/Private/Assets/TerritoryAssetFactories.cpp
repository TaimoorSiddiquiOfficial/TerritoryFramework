#include "Assets/TerritoryAssetFactories.h"

#include "AI/TerritoryDiplomacyDialogue.h"
#include "AssetToolsModule.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryStealthProfile.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Economy/TerritoryProductionProfile.h"
#include "IAssetTools.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"

#define LOCTEXT_NAMESPACE "TerritoryAssetFactories"

UTerritoryAssetFactoryBase::UTerritoryAssetFactoryBase()
{
	bCreateNew = true;
	bEditAfterNew = true;
}

void UTerritoryAssetFactoryBase::InitializeFor(UClass* AssetClass)
{
	SupportedClass = AssetClass;
}

UObject* UTerritoryAssetFactoryBase::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	UClass* ClassToCreate = SupportedClass;
	if (!ClassToCreate || !ClassToCreate->IsChildOf<UDataAsset>())
	{
		return nullptr;
	}

	return NewObject<UObject>(
		InParent,
		ClassToCreate,
		Name,
		Flags | RF_Transactional);
}

UTerritoryPlaceDefinitionFactory::UTerritoryPlaceDefinitionFactory()
{
	InitializeFor(UTerritoryPlaceDefinition::StaticClass());
}

UTerritoryDistrictDefinitionFactory::UTerritoryDistrictDefinitionFactory()
{
	InitializeFor(UTerritoryDistrictDefinition::StaticClass());
}

UTerritoryCityDefinitionFactory::UTerritoryCityDefinitionFactory()
{
	InitializeFor(UTerritoryCityDefinition::StaticClass());
}

UTerritoryCounterAttackProfileFactory::UTerritoryCounterAttackProfileFactory()
{
	InitializeFor(UTerritoryCounterAttackProfile::StaticClass());
}

UTerritoryGuardPostDefinitionFactory::UTerritoryGuardPostDefinitionFactory()
{
	InitializeFor(UTerritoryGuardPostDefinition::StaticClass());
}

UTerritoryProductionProfileFactory::UTerritoryProductionProfileFactory()
{
	InitializeFor(UTerritoryProductionProfile::StaticClass());
}

UTerritoryStealthProfileFactory::UTerritoryStealthProfileFactory()
{
	InitializeFor(UTerritoryStealthProfile::StaticClass());
}

UTerritoryDisguiseProfileFactory::UTerritoryDisguiseProfileFactory()
{
	InitializeFor(UTerritoryDisguiseProfile::StaticClass());
}

UTerritoryDiplomacyDialogueProfileFactory::UTerritoryDiplomacyDialogueProfileFactory()
{
	InitializeFor(UTerritoryDiplomacyDialogueProfile::StaticClass());
}

UTerritoryQuestCascadeRecipeFactory::UTerritoryQuestCascadeRecipeFactory()
{
	InitializeFor(UTerritoryQuestCascadeRecipe::StaticClass());
}

UTerritoryBlueprintFactoryBase::UTerritoryBlueprintFactoryBase()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bSkipClassPicker = true;
	SupportedClass = UBlueprint::StaticClass();
}

void UTerritoryBlueprintFactoryBase::InitializeBlueprintFor(
	UClass* InParentClass,
	FText InDisplayName,
	FText InToolTip,
	FText InSubMenu)
{
	ParentClass = InParentClass;
	FactoryDisplayName = MoveTemp(InDisplayName);
	FactoryToolTip = MoveTemp(InToolTip);
	FactorySubMenus.Reset();
	FactorySubMenus.Add(MoveTemp(InSubMenu));
}

FText UTerritoryBlueprintFactoryBase::GetDisplayName() const
{
	return FactoryDisplayName;
}

uint32 UTerritoryBlueprintFactoryBase::GetMenuCategories() const
{
	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	return AssetTools.FindAdvancedAssetCategory(FName(TEXT("TerritoryFramework")));
}

const TArray<FText>& UTerritoryBlueprintFactoryBase::GetMenuCategorySubMenus() const
{
	return FactorySubMenus;
}

FText UTerritoryBlueprintFactoryBase::GetToolTip() const
{
	return FactoryToolTip;
}

UTerritoryPlaceActorBlueprintFactory::UTerritoryPlaceActorBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryProperty::StaticClass(),
		LOCTEXT("TerritoryPlaceActorBlueprint", "Territory Place Actor Blueprint"),
		LOCTEXT("TerritoryPlaceActorBlueprintTip",
			"Creates the level actor for one Place. Assign a Territory Place Definition; do not duplicate settings on the actor."),
		LOCTEXT("TerritoryWorldActorSubMenu", "World Actors"));
}

UTerritoryDistrictActorBlueprintFactory::UTerritoryDistrictActorBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryDistrict::StaticClass(),
		LOCTEXT("TerritoryDistrictActorBlueprint", "Territory District Actor Blueprint"),
		LOCTEXT("TerritoryDistrictActorBlueprintTip",
			"Creates the level actor for a District whose control is calculated from its Places."),
		LOCTEXT("TerritoryWorldActorSubMenu", "World Actors"));
}

UTerritoryCityActorBlueprintFactory::UTerritoryCityActorBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryCity::StaticClass(),
		LOCTEXT("TerritoryCityActorBlueprint", "Territory City Actor Blueprint"),
		LOCTEXT("TerritoryCityActorBlueprintTip",
			"Creates the level actor for a City whose control is calculated from its Districts."),
		LOCTEXT("TerritoryWorldActorSubMenu", "World Actors"));
}

UTerritoryCapturePointBlueprintFactory::UTerritoryCapturePointBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryCapturePoint::StaticClass(),
		LOCTEXT("TerritoryCapturePointBlueprint", "Territory Capture Point Blueprint"),
		LOCTEXT("TerritoryCapturePointBlueprintTip",
			"Creates the optional physical capture-zone actor used by multiplayer or hold-the-zone capture."),
		LOCTEXT("TerritoryInteractionActorSubMenu", "Interaction & Navigation Actors"));
}

UTerritoryGuardSpawnPointBlueprintFactory::UTerritoryGuardSpawnPointBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryGuardSpawnPoint::StaticClass(),
		LOCTEXT("TerritoryGuardSpawnPointBlueprint", "Territory Guard Spawn Point Blueprint"),
		LOCTEXT("TerritoryGuardSpawnPointBlueprintTip",
			"Creates a definition-bound spawn and patrol slot for defenders or counterattack staging."),
		LOCTEXT("TerritoryCombatActorSubMenu", "Guard & Combat Actors"));
}

UTerritoryStoryOwnerSpawnerBlueprintFactory::UTerritoryStoryOwnerSpawnerBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryStoryOwnerSpawner::StaticClass(),
		LOCTEXT("TerritoryStoryOwnerSpawnerBlueprint", "Territory Story Owner Spawner Blueprint"),
		LOCTEXT("TerritoryStoryOwnerSpawnerBlueprintTip",
			"Creates the Narrative NPC spawner used for dialogue-based Place handover after defenders are defeated."),
		LOCTEXT("TerritoryInteractionActorSubMenu", "Interaction & Navigation Actors"));
}

UTerritoryDistrictManagementPointBlueprintFactory::UTerritoryDistrictManagementPointBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryDistrictManagementPoint::StaticClass(),
		LOCTEXT("TerritoryDistrictManagementPointBlueprint", "Territory Management Point Blueprint"),
		LOCTEXT("TerritoryDistrictManagementPointBlueprintTip",
			"Creates an in-world POI where the owning faction can open district commands and upgrades."),
		LOCTEXT("TerritoryInteractionActorSubMenu", "Interaction & Navigation Actors"));
}

UTerritoryRoadGuideBlueprintFactory::UTerritoryRoadGuideBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryRoadGuide::StaticClass(),
		LOCTEXT("TerritoryRoadGuideBlueprint", "Territory Road Guide Blueprint"),
		LOCTEXT("TerritoryRoadGuideBlueprintTip",
			"Creates a spline route for vehicle reinforcement, counterattack, traffic, and chase missions."),
		LOCTEXT("TerritoryInteractionActorSubMenu", "Interaction & Navigation Actors"));
}

UTerritoryGuardCharacterBlueprintFactory::UTerritoryGuardCharacterBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryGuardCharacter::StaticClass(),
		LOCTEXT("TerritoryGuardCharacterBlueprint", "Territory Guard Character Blueprint"),
		LOCTEXT("TerritoryGuardCharacterBlueprintTip",
			"Creates a Narrative NPC defender that patrols, respects diplomacy, and protects assigned Territory."),
		LOCTEXT("TerritoryCombatActorSubMenu", "Guard & Combat Actors"));
}

UTerritoryAssaultCharacterBlueprintFactory::UTerritoryAssaultCharacterBlueprintFactory()
{
	InitializeBlueprintFor(
		ATerritoryAssaultCharacter::StaticClass(),
		LOCTEXT("TerritoryAssaultCharacterBlueprint", "Territory Assault Character Blueprint"),
		LOCTEXT("TerritoryAssaultCharacterBlueprintTip",
			"Creates a Narrative NPC attacker that can travel, fight defenders, pursue targets, and take over a Place."),
		LOCTEXT("TerritoryCombatActorSubMenu", "Guard & Combat Actors"));
}

#undef LOCTEXT_NAMESPACE

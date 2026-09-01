#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/Factory.h"
#include "TerritoryAssetFactories.generated.h"

/**
 * Shared implementation for the named Territory Framework entries in the
 * Content Browser's Add/New menu. Concrete factories deliberately create one
 * exact authoring type so designers never have to search the generic Data
 * Asset class picker.
 */
UCLASS(Abstract, hidecategories=Object)
class UTerritoryAssetFactoryBase : public UFactory
{
	GENERATED_BODY()

public:
	UTerritoryAssetFactoryBase();

	virtual UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;

protected:
	void InitializeFor(UClass* AssetClass);
};

UCLASS(hidecategories=Object)
class UTerritoryPlaceDefinitionFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryPlaceDefinitionFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryDistrictDefinitionFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryDistrictDefinitionFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryCityDefinitionFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryCityDefinitionFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryCounterAttackProfileFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryCounterAttackProfileFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryGuardPostDefinitionFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryGuardPostDefinitionFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryProductionProfileFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryProductionProfileFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryStealthProfileFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryStealthProfileFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryDisguiseProfileFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryDisguiseProfileFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryDiplomacyDialogueProfileFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryDiplomacyDialogueProfileFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryQuestCascadeRecipeFactory : public UTerritoryAssetFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryQuestCascadeRecipeFactory();
};

/**
 * Creates a Blueprint with an exact Territory parent class and skips Unreal's
 * broad parent-class picker. These entries are for reusable level actors; all
 * gameplay configuration still comes from the Definition/Profile assets.
 */
UCLASS(Abstract, hidecategories=Object)
class UTerritoryBlueprintFactoryBase : public UBlueprintFactory
{
	GENERATED_BODY()

public:
	UTerritoryBlueprintFactoryBase();

	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;
	virtual FText GetToolTip() const override;

protected:
	void InitializeBlueprintFor(
		UClass* InParentClass,
		FText InDisplayName,
		FText InToolTip,
		FText InSubMenu);

private:
	FText FactoryDisplayName;
	FText FactoryToolTip;
	TArray<FText> FactorySubMenus;
};

UCLASS(hidecategories=Object)
class UTerritoryPlaceActorBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryPlaceActorBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryDistrictActorBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryDistrictActorBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryCityActorBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryCityActorBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryCapturePointBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryCapturePointBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryGuardSpawnPointBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryGuardSpawnPointBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryStoryOwnerSpawnerBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryStoryOwnerSpawnerBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryDistrictManagementPointBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryDistrictManagementPointBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryRoadGuideBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryRoadGuideBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryGuardCharacterBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryGuardCharacterBlueprintFactory();
};

UCLASS(hidecategories=Object)
class UTerritoryAssaultCharacterBlueprintFactory : public UTerritoryBlueprintFactoryBase
{
	GENERATED_BODY()
public:
	UTerritoryAssaultCharacterBlueprintFactory();
};

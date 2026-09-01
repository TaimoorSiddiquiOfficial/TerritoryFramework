#include "Assets/TerritoryAssetTypeActions.h"

FTerritoryAssetTypeActions::FTerritoryAssetTypeActions(
	UClass* InSupportedClass,
	FText InName,
	FText InDescription,
	FText InSubMenu,
	uint32 InCategory,
	FColor InColor)
	: SupportedClass(InSupportedClass)
	, Name(MoveTemp(InName))
	, Description(MoveTemp(InDescription))
	, Category(InCategory)
	, Color(InColor)
{
	SubMenus.Add(MoveTemp(InSubMenu));
}

FText FTerritoryAssetTypeActions::GetName() const
{
	return Name;
}

FColor FTerritoryAssetTypeActions::GetTypeColor() const
{
	return Color;
}

UClass* FTerritoryAssetTypeActions::GetSupportedClass() const
{
	return SupportedClass;
}

uint32 FTerritoryAssetTypeActions::GetCategories()
{
	return Category;
}

const TArray<FText>& FTerritoryAssetTypeActions::GetSubMenus() const
{
	return SubMenus;
}

FText FTerritoryAssetTypeActions::GetAssetDescription(
	const FAssetData& AssetData) const
{
	return Description;
}

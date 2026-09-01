#pragma once

#include "AssetTypeActions_Base.h"

/** Named and grouped Content Browser presentation for one Territory asset. */
class FTerritoryAssetTypeActions final : public FAssetTypeActions_Base
{
public:
	FTerritoryAssetTypeActions(
		UClass* InSupportedClass,
		FText InName,
		FText InDescription,
		FText InSubMenu,
		uint32 InCategory,
		FColor InColor);

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;

private:
	UClass* SupportedClass = nullptr;
	FText Name;
	FText Description;
	TArray<FText> SubMenus;
	uint32 Category = EAssetTypeCategories::Misc;
	FColor Color = FColor::White;
};

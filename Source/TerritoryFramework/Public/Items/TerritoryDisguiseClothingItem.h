#pragma once

#include "CoreMinimal.h"
#include "Items/EquippableItem.h"
#include "TerritoryDisguiseClothingItem.generated.h"

class UTerritoryDisguiseProfile;

/**
 * Narrative Pro clothing that also supplies a temporary Territory identity.
 * Create a Blueprint child, assign its clothing mesh and Disguise Profile, then
 * add it to Narrative inventory exactly like any other clothing item.
 */
UCLASS(BlueprintType, Blueprintable,
	meta=(DisplayName="Territory Disguise Clothing Item"))
class TERRITORYFRAMEWORK_API UTerritoryDisguiseClothingItem
	: public UEquippableItem_Clothing
{
	GENERATED_BODY()

public:
	UTerritoryDisguiseClothingItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item - Equippable | Disguise",
		meta=(ToolTip="Perceived faction, quality, clearance, and exposure rules. Equipping activates it; unequipping removes it. The wearer's true Narrative faction is never changed."))
	TObjectPtr<UTerritoryDisguiseProfile> DisguiseProfile;

protected:
	virtual void HandleEquip_Implementation() override;
	virtual void HandleUnequip_Implementation(const FGameplayTag& OldSlot) override;
};

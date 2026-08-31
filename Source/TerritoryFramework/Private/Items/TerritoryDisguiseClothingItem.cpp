#include "Items/TerritoryDisguiseClothingItem.h"

#include "Core/TerritoryDisguiseProfile.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "UnrealFramework/NarrativeCharacter.h"

UTerritoryDisguiseClothingItem::UTerritoryDisguiseClothingItem()
{
	// Narrative's Equipment Gameplay Effect already knows how to apply this as a
	// SetByCaller modifier. Designers can lower it for imperfect uniforms.
	StealthRating = 100.f;
}

void UTerritoryDisguiseClothingItem::HandleEquip_Implementation()
{
	Super::HandleEquip_Implementation();
	ANarrativeCharacter* Wearer = GetOwningNarrativeCharacter();
	if (!Wearer || !Wearer->HasAuthority() || !DisguiseProfile) return;
	if (UWorld* WearerWorld = Wearer->GetWorld())
	{
		if (UTerritoryDisguiseSubsystem* Disguises =
			WearerWorld->GetSubsystem<UTerritoryDisguiseSubsystem>())
		{
			Disguises->ActivateDisguise(Wearer, DisguiseProfile, this);
		}
	}
}

void UTerritoryDisguiseClothingItem::HandleUnequip_Implementation(
	const FGameplayTag& OldSlot)
{
	ANarrativeCharacter* Wearer = GetOwningNarrativeCharacter();
	if (Wearer && Wearer->HasAuthority())
	{
		if (UWorld* WearerWorld = Wearer->GetWorld())
		{
			if (UTerritoryDisguiseSubsystem* Disguises =
				WearerWorld->GetSubsystem<UTerritoryDisguiseSubsystem>())
			{
				Disguises->RemoveDisguise(Wearer, this);
			}
		}
	}
	Super::HandleUnequip_Implementation(OldSlot);
}

#include "Cinematics/TerritoryShotRoleTags.h"

#include "NativeGameplayTags.h"

namespace TerritoryShotRoleTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Establishing, "Territory.Cinematic.Shot.Establishing",
		"Open the location, show geography, and establish both speakers before coverage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TwoShot, "Territory.Cinematic.Shot.TwoShot",
		"Default dialogue master shot; keeps speaker geography readable and edits safe.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(OverShoulder, "Territory.Cinematic.Shot.OverShoulder",
		"Dialogue coverage with foreground depth; alternate screen side without crossing the line.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MediumCloseUp, "Territory.Cinematic.Shot.MediumCloseUp",
		"Primary speaking coverage; natural facial emphasis while preserving shoulders and gesture.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CloseUp, "Territory.Cinematic.Shot.CloseUp",
		"Emotional story beat; reserve the shallow depth of field for important lines.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Reaction, "Territory.Cinematic.Shot.Reaction",
		"Silent listener response and player reply selection; creates editorial rhythm.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Insert, "Territory.Cinematic.Shot.Insert",
		"Story prop, hand action, weapon, contract, or Territory handover detail.");

	FGameplayTag FromLegacyRole(const ETerritoryDialogueShotRole Role)
	{
		switch (Role)
		{
		case ETerritoryDialogueShotRole::Establishing:  return Establishing;
		case ETerritoryDialogueShotRole::TwoShot:       return TwoShot;
		case ETerritoryDialogueShotRole::OverShoulder:  return OverShoulder;
		case ETerritoryDialogueShotRole::MediumCloseUp: return MediumCloseUp;
		case ETerritoryDialogueShotRole::CloseUp:       return CloseUp;
		case ETerritoryDialogueShotRole::Reaction:      return Reaction;
		case ETerritoryDialogueShotRole::Insert:        return Insert;
		}
		return FGameplayTag();
	}
}

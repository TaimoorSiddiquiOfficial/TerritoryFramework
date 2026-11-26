#include "Skills/TerritoryUpgradeSkill.h"

UTerritoryUpgradeSkill::UTerritoryUpgradeSkill(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkillDisplayName = NSLOCTEXT("TerritoryUpgradeSkill", "Name", "Territory Upgrades");
	SkillDescription = NSLOCTEXT("TerritoryUpgradeSkill", "Description",
		"Unlock Place upgrade tiers for your faction: Upgrade I, then II, then III.");

	FPerkConfig TierOne;
	TierOne.Perk = UTerritoryUpgradePerk_I::StaticClass();
	TierOne.PerkCords = FVector2D(0.f, 0.f);
	TierOne.LinkedTo.Add(UTerritoryUpgradePerk_II::StaticClass());

	FPerkConfig TierTwo;
	TierTwo.Perk = UTerritoryUpgradePerk_II::StaticClass();
	TierTwo.PerkCords = FVector2D(160.f, 0.f);
	TierTwo.LinkedTo.Add(UTerritoryUpgradePerk_III::StaticClass());

	FPerkConfig TierThree;
	TierThree.Perk = UTerritoryUpgradePerk_III::StaticClass();
	TierThree.PerkCords = FVector2D(320.f, 0.f);

	Perks.Add(TierOne);
	Perks.Add(TierTwo);
	Perks.Add(TierThree);
}

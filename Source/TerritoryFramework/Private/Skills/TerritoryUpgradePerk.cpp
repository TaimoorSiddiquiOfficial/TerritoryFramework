#include "Skills/TerritoryUpgradePerk.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Subsystems/TerritoryUpgradeSubsystem.h"

UTerritoryUpgradePerk::UTerritoryUpgradePerk(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxLevels = 1;
	PerkDisplayName = NSLOCTEXT("TerritoryUpgradePerk", "Name", "Territory Upgrade");
	PerkDescription = NSLOCTEXT("TerritoryUpgradePerk", "Description",
		"Unlocks a Place upgrade tier for your faction.");
}

void UTerritoryUpgradePerk::SetPerkLevel_Implementation(const int32 NewPerkLevel)
{
	Super::SetPerkLevel_Implementation(NewPerkLevel);

	// -1 is the vendor's "not purchased" state. Only a server-side purchase may widen the
	// faction's allowed Place tier; a client-local BuyPerk never writes authority state.
	if (NewPerkLevel < 0)
	{
		return;
	}
	USkillTreeComponent* SkillTree = GetOwningComponent();
	AActor* Owner = SkillTree ? SkillTree->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || Owner->GetNetMode() == NM_Client)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn)
	{
		OwnerPawn = Cast<APlayerState>(Owner) ? Cast<APlayerState>(Owner)->GetPawn() : nullptr;
	}
	const FGameplayTag Faction = OwnerPawn
		? UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, OwnerPawn) : FGameplayTag();
	UWorld* World = GetWorld();
	UTerritoryUpgradeSubsystem* Upgrades = World
		? World->GetSubsystem<UTerritoryUpgradeSubsystem>() : nullptr;
	UE_LOG(LogTerritory, Log,
		TEXT("[UpgradePerk] %s level %d: owner=%s authority=%d pawn=%s faction=%s subsystem=%s net=%d"),
		*GetName(), NewPerkLevel, *GetNameSafe(Owner), Owner->HasAuthority() ? 1 : 0,
		*GetNameSafe(OwnerPawn), *Faction.ToString(), *GetNameSafe(Upgrades),
		World ? static_cast<int32>(World->GetNetMode()) : -1);
	if (!Faction.IsValid() || !Upgrades)
	{
		return;
	}
	Upgrades->SetFactionTier(Faction,
		FMath::Max(Upgrades->GetFactionTier(Faction), FMath::Max(1, UnlockedTier)));
}

UTerritoryUpgradePerk_I::UTerritoryUpgradePerk_I(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnlockedTier = 1;
	PrerequisitePerk = nullptr;
	PerkDisplayName = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeI", "Upgrade I — Unlock");
	PerkDescription = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeIDesc",
		"Unlocks Place upgrade level 1 for your faction.");
}

UTerritoryUpgradePerk_II::UTerritoryUpgradePerk_II(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnlockedTier = 2;
	PrerequisitePerk = UTerritoryUpgradePerk_I::StaticClass();
	PerkDisplayName = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeII", "Upgrade II");
	PerkDescription = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeIIDesc",
		"Unlocks Place upgrade level 2 for your faction.");
}

UTerritoryUpgradePerk_III::UTerritoryUpgradePerk_III(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnlockedTier = 3;
	PrerequisitePerk = UTerritoryUpgradePerk_II::StaticClass();
	PerkDisplayName = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeIII", "Upgrade III");
	PerkDescription = NSLOCTEXT("TerritoryUpgradePerk", "UpgradeIIIDesc",
		"Unlocks Place upgrade level 3 for your faction.");
}

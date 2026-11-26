#include "Interaction/TerritoryUpgradeBroker.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "SkillTrees/TreeSkill.h"
#include "Skills/TerritoryUpgradePerk.h"
#include "Skills/TerritoryUpgradeSkill.h"
#include "Subsystems/TerritoryUpgradeSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	/** The vendor component has no public accessor for its instanced skills; read the UPROPERTY. */
	UTreeSkill* FindSkillForPerk(const USkillTreeComponent* SkillTree,
		const TSubclassOf<UTreePerk>& PerkClass)
	{
		if (!SkillTree || !PerkClass)
		{
			return nullptr;
		}
		const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(
			USkillTreeComponent::StaticClass(), TEXT("SkillTreeSkills"));
		const FObjectPropertyBase* InnerProperty = ArrayProperty
			? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!ArrayProperty || !InnerProperty)
		{
			return nullptr;
		}
		FScriptArrayHelper Helper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(SkillTree));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			UTreeSkill* Skill = Cast<UTreeSkill>(
				InnerProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
			if (!Skill)
			{
				continue;
			}
			const bool bContainsPerk = Skill->Perks.ContainsByPredicate(
				[&PerkClass](const FPerkConfig& Config) { return Config.Perk == PerkClass; });
			if (bContainsPerk)
			{
				return Skill;
			}
		}
		return nullptr;
	}

	// CanBuyPerk/BuyPerk are protected on the vendor component, so the server broker calls them
	// through their UFunction signatures instead of touching vendor headers.
	struct FCanBuyPerkParams
	{
		TSubclassOf<UTreePerk> Perk;
		FText OutCantBuyReason;
		bool ReturnValue = false;
	};

	struct FBuyPerkParams
	{
		TSubclassOf<UTreePerk> Perk;
		UTreeSkill* OwnerSkill = nullptr;
		bool ReturnValue = false;
	};

	bool CallCanBuyPerk(USkillTreeComponent* SkillTree, TSubclassOf<UTreePerk> Perk,
		FText& OutReason)
	{
		UFunction* Function = SkillTree ? SkillTree->FindFunction(TEXT("CanBuyPerk")) : nullptr;
		if (!Function)
		{
			OutReason = NSLOCTEXT("TerritoryUpgradeBroker", "MissingCanBuy", "Skill tree cannot validate perks.");
			return false;
		}
		FCanBuyPerkParams Params;
		Params.Perk = Perk;
		SkillTree->ProcessEvent(Function, &Params);
		OutReason = Params.OutCantBuyReason;
		return Params.ReturnValue;
	}

	bool CallBuyPerk(USkillTreeComponent* SkillTree, TSubclassOf<UTreePerk> Perk, UTreeSkill* Skill)
	{
		UFunction* Function = SkillTree ? SkillTree->FindFunction(TEXT("BuyPerk")) : nullptr;
		if (!Function)
		{
			return false;
		}
		FBuyPerkParams Params;
		Params.Perk = Perk;
		Params.OwnerSkill = Skill;
		SkillTree->ProcessEvent(Function, &Params);
		return Params.ReturnValue;
	}

	bool HasPurchasedPerk(const USkillTreeComponent* SkillTree, TSubclassOf<UTreePerk> PerkClass)
	{
		const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(
			USkillTreeComponent::StaticClass(), TEXT("PurchasedPerks"));
		const FObjectPropertyBase* InnerProperty = ArrayProperty
			? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!ArrayProperty || !InnerProperty)
		{
			return false;
		}
		FScriptArrayHelper Helper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(SkillTree));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const UTreePerk* Perk = Cast<UTreePerk>(
				InnerProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
			if (Perk && Perk->GetClass() == PerkClass && Perk->PerkLevel >= 0)
			{
				return true;
			}
		}
		return false;
	}

	bool EnsureTerritorySkill(USkillTreeComponent* SkillTree)
	{
		const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(
			USkillTreeComponent::StaticClass(), TEXT("SkillTreeSkills"));
		const FObjectPropertyBase* InnerProperty = ArrayProperty
			? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!ArrayProperty || !InnerProperty)
		{
			return false;
		}
		FScriptArrayHelper Helper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(SkillTree));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			if (Cast<UTerritoryUpgradeSkill>(
				InnerProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index))))
			{
				return true;
			}
		}
		UTerritoryUpgradeSkill* Skill = NewObject<UTerritoryUpgradeSkill>(
			SkillTree, UTerritoryUpgradeSkill::StaticClass());
		if (!Skill)
		{
			return false;
		}
		const int32 Index = Helper.AddValue();
		InnerProperty->SetObjectPropertyValue(Helper.GetRawPtr(Index), Skill);
		return true;
	}
}

ATerritoryUpgradeBroker::ATerritoryUpgradeBroker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	bNetUseOwnerRelevancy = true;
	bAlwaysRelevant = false;
}

bool ATerritoryUpgradeBroker::RequestUpgradeTier(int32 Tier)
{
	if (Tier < 1 || Tier > 3)
	{
		return false;
	}
	const TSubclassOf<UTerritoryUpgradePerk> PerkClass =
		Tier == 1 ? TSubclassOf<UTerritoryUpgradePerk>(UTerritoryUpgradePerk_I::StaticClass())
		: Tier == 2 ? TSubclassOf<UTerritoryUpgradePerk>(UTerritoryUpgradePerk_II::StaticClass())
		: TSubclassOf<UTerritoryUpgradePerk>(UTerritoryUpgradePerk_III::StaticClass());
	if (HasAuthority())
	{
		ServerBuyPerk_Implementation(PerkClass);
		return true;
	}
	ServerBuyPerk(PerkClass);
	return true;
}

FGameplayTag ATerritoryUpgradeBroker::GetOwnerFaction() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn) : FGameplayTag();
}

int32 ATerritoryUpgradeBroker::GetOwnerFactionTier() const
{
	const FGameplayTag Faction = GetOwnerFaction();
	if (!Faction.IsValid() || !GetWorld())
	{
		return 0;
	}
	const UTerritoryUpgradeSubsystem* Upgrades =
		GetWorld()->GetSubsystem<UTerritoryUpgradeSubsystem>();
	return Upgrades ? Upgrades->GetFactionTier(Faction) : 0;
}

bool ATerritoryUpgradeBroker::ServerBuyPerk_Validate(TSubclassOf<UTerritoryUpgradePerk> PerkClass)
{
	return PerkClass && PerkClass->IsChildOf(UTerritoryUpgradePerk::StaticClass());
}

void ATerritoryUpgradeBroker::ServerBuyPerk_Implementation(TSubclassOf<UTerritoryUpgradePerk> PerkClass)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}
	APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>();
	USkillTreeComponent* SkillTree = PlayerState
		? PlayerState->FindComponentByClass<USkillTreeComponent>() : nullptr;
	if (!SkillTree || !PerkClass)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[UpgradePerk] %s has no skill-tree component."),
			*GetNameSafe(Controller));
		return;
	}

	const UTerritoryUpgradePerk* PerkDefaults =
		Cast<UTerritoryUpgradePerk>(PerkClass->GetDefaultObject());
	if (PerkDefaults && PerkDefaults->PrerequisitePerk
		&& !HasPurchasedPerk(SkillTree, PerkDefaults->PrerequisitePerk))
	{
		UE_LOG(LogTerritory, Log, TEXT("[UpgradePerk] %s requires %s before %s."),
			*GetNameSafe(Controller), *GetNameSafe(PerkDefaults->PrerequisitePerk.Get()),
			*GetNameSafe(PerkClass.Get()));
		return;
	}
	if (!FindSkillForPerk(SkillTree, PerkClass) && !EnsureTerritorySkill(SkillTree))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[UpgradePerk] Could not ensure the Territory skill on %s."),
			*GetNameSafe(Controller));
		return;
	}

	FText CannotBuyReason;
	if (!CallCanBuyPerk(SkillTree, PerkClass, CannotBuyReason))
	{
		UE_LOG(LogTerritory, Log, TEXT("[UpgradePerk] %s cannot buy %s: %s"),
			*GetNameSafe(Controller), *GetNameSafe(PerkClass.Get()), *CannotBuyReason.ToString());
		return;
	}

	UTreeSkill* OwnerSkill = FindSkillForPerk(SkillTree, PerkClass);
	if (!OwnerSkill)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UpgradePerk] %s is not authored in any skill on %s."),
			*GetNameSafe(PerkClass.Get()), *GetNameSafe(Controller));
		return;
	}

	if (!CallBuyPerk(SkillTree, PerkClass, OwnerSkill))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[UpgradePerk] BuyPerk failed for %s on %s."),
			*GetNameSafe(PerkClass.Get()), *GetNameSafe(Controller));
		return;
	}

	UE_LOG(LogTerritory, Log, TEXT("[UpgradePerk] %s bought %s for skill %s."),
		*GetNameSafe(Controller), *GetNameSafe(PerkClass.Get()),
		*GetNameSafe(OwnerSkill));
}

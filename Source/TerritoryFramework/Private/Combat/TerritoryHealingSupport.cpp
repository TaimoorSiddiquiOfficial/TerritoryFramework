// Territory Framework — automatic consumption of a Narrative healing item.
//
// Design contract (see Docs/NARRATIVE_DAMAGE_ENGINE.md):
//   * Narrative owns damage, healing and inventory. Territory only decides *when* an item may
//     be used and reports the outcome.
//   * The item's heal comes from its own Narrative gameplay effect (SetByCaller.Heal on
//     GE_Item_Instant) applied by UNarrativeGameplayEffectItem::Use, and the stack is removed by
//     Narrative's bConsumeOnUse path inside UNarrativeInventoryComponent::UseItem.
//   * Every gate is authority-side, bounded and re-validated; a client call reports ClientOnly
//     instead of pretending the use happened.

#include "Combat/TerritoryHealingSupport.h"

#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "UnrealFramework/NarrativeCharacter.h"

#define LOCTEXT_NAMESPACE "TerritoryHealingSupport"

namespace
{
	FText HealStatusText(const ETerritoryHealConsumeStatus Status)
	{
		switch (Status)
		{
		case ETerritoryHealConsumeStatus::Consumed:
			return LOCTEXT("HealConsumed", "Consumed healing item.");
		case ETerritoryHealConsumeStatus::Disabled:
			return LOCTEXT("HealDisabled", "Automatic healing item consumption is disabled.");
		case ETerritoryHealConsumeStatus::InvalidCharacter:
			return LOCTEXT("HealInvalidCharacter", "No Narrative character with an ability system and inventory.");
		case ETerritoryHealConsumeStatus::ClientOnly:
			return LOCTEXT("HealClientOnly", "Automatic healing item consumption is server-authoritative.");
		case ETerritoryHealConsumeStatus::Dead:
			return LOCTEXT("HealDead", "Owner is dead.");
		case ETerritoryHealConsumeStatus::ItemNotConfigured:
			return LOCTEXT("HealNoItem", "No healing item class configured.");
		case ETerritoryHealConsumeStatus::HealthAboveThreshold:
			return LOCTEXT("HealAboveThreshold", "Health is above the auto consume threshold.");
		case ETerritoryHealConsumeStatus::ItemMissing:
			return LOCTEXT("HealItemMissing", "Healing item is not in the inventory.");
		case ETerritoryHealConsumeStatus::ItemBusy:
			return LOCTEXT("HealItemBusy", "Healing item cannot be used yet.");
		case ETerritoryHealConsumeStatus::Ready:
			return LOCTEXT("HealReady", "Ready to consume a healing item.");
		case ETerritoryHealConsumeStatus::CooldownActive:
			return LOCTEXT("HealCooldown", "Auto consume cooldown is still running.");
		case ETerritoryHealConsumeStatus::UseRejected:
			return LOCTEXT("HealUseRejected", "Narrative rejected the item use.");
		default:
			break;
		}
		return FText::GetEmpty();
	}

	void SetHealStatus(FTerritoryHealConsumeResult& Result, const ETerritoryHealConsumeStatus Status)
	{
		Result.Status = Status;
		Result.bConsumed = Status == ETerritoryHealConsumeStatus::Consumed;
		Result.FailureReason = (Status == ETerritoryHealConsumeStatus::Ready
			|| Status == ETerritoryHealConsumeStatus::Consumed)
			? FText::GetEmpty() : HealStatusText(Status);
	}

	void UpdateHealDebugString(FTerritoryHealConsumeResult& Result)
	{
		const FString ItemName = Result.ItemClass
			? Result.ItemClass->GetName() : FString(TEXT("<none>"));
		switch (Result.Status)
		{
		case ETerritoryHealConsumeStatus::Consumed:
			Result.DebugString = FString::Printf(
				TEXT("AutoConsumeHealItem %s: consumed at %.0f%% health -> %.0f%% (%d left)"),
				*ItemName, Result.HealthFractionBefore * 100.f, Result.HealthFractionAfter * 100.f,
				Result.QuantityAfter);
			break;
		case ETerritoryHealConsumeStatus::CooldownActive:
			Result.DebugString = FString::Printf(
				TEXT("AutoConsumeHealItem %s: %.1fs cooldown remaining at %.0f%% health"),
				*ItemName, Result.CooldownRemainingSeconds, Result.HealthFractionBefore * 100.f);
			break;
		case ETerritoryHealConsumeStatus::HealthAboveThreshold:
			Result.DebugString = FString::Printf(
				TEXT("AutoConsumeHealItem %s: holding item at %.0f%% health"),
				*ItemName, Result.HealthFractionBefore * 100.f);
			break;
		case ETerritoryHealConsumeStatus::Ready:
			Result.DebugString = FString::Printf(
				TEXT("AutoConsumeHealItem %s: ready at %.0f%% health (%d in inventory)"),
				*ItemName, Result.HealthFractionBefore * 100.f, Result.QuantityAfter);
			break;
		default:
			Result.DebugString = FString::Printf(TEXT("AutoConsumeHealItem %s: %s"),
				*ItemName, *Result.FailureReason.ToString());
			break;
		}
	}
}

bool TerritoryHealingSupport::TryGetNarrativeHealthFraction(
	const ANarrativeCharacter* Character, float& OutHealthFraction)
{
	OutHealthFraction = 0.f;
	if (!Character)
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* AbilitySystem = Character->GetNarrativeAbilitySystemComponent();
	if (!AbilitySystem)
	{
		return false;
	}

	// Narrative owns these attributes; Territory only reads them.
	const float MaxHealth = AbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return false;
	}

	OutHealthFraction = FMath::Clamp(
		AbilitySystem->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) / MaxHealth,
		0.f, 1.f);
	return true;
}

void TerritoryHealingSupport::EvaluateAutoConsumeHealItem(
	const ANarrativeCharacter* Character,
	const TSubclassOf<UNarrativeItem> HealItemClass,
	const float HealthFractionThreshold,
	const float LastConsumeWorldTime,
	const float CooldownSeconds,
	FTerritoryHealConsumeResult& OutResult)
{
	OutResult = FTerritoryHealConsumeResult();
	OutResult.ItemClass = HealItemClass;

	if (!Character)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::InvalidCharacter);
		UpdateHealDebugString(OutResult);
		return;
	}

	const UNarrativeAbilitySystemComponent* AbilitySystem = Character->GetNarrativeAbilitySystemComponent();
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!AbilitySystem || !Inventory)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::InvalidCharacter);
		UpdateHealDebugString(OutResult);
		return;
	}

	if (!HealItemClass)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ItemNotConfigured);
		UpdateHealDebugString(OutResult);
		return;
	}

	// Narrative already decided the death state; never heal a corpse.
	if (AbilitySystem->IsDead())
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::Dead);
		UpdateHealDebugString(OutResult);
		return;
	}

	float HealthFraction = 0.f;
	if (!TryGetNarrativeHealthFraction(Character, HealthFraction))
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::InvalidCharacter);
		UpdateHealDebugString(OutResult);
		return;
	}
	OutResult.HealthFractionBefore = HealthFraction;
	OutResult.HealthFractionAfter = HealthFraction;

	if (HealthFraction > HealthFractionThreshold)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::HealthAboveThreshold);
		UpdateHealDebugString(OutResult);
		return;
	}

	const UWorld* World = Character->GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	if (CooldownSeconds > 0.f && LastConsumeWorldTime > 0.f
		&& (Now - LastConsumeWorldTime) < CooldownSeconds)
	{
		OutResult.CooldownRemainingSeconds = FMath::Max(0.f, CooldownSeconds - (Now - LastConsumeWorldTime));
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::CooldownActive);
		UpdateHealDebugString(OutResult);
		return;
	}

	if (!Inventory->HasItem(HealItemClass, 1, false))
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ItemMissing);
		UpdateHealDebugString(OutResult);
		return;
	}

	UNarrativeItem* Item = Inventory->FindItemOfClass(HealItemClass, false);
	if (!Item || !Item->CanUse())
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ItemBusy);
		UpdateHealDebugString(OutResult);
		return;
	}

	// Mirror Narrative's own recharge window so UseItem is not called before it can succeed.
	if (Item->UseRechargeDuration > 0.f && (Now - Item->GetLastUseTime()) <= Item->UseRechargeDuration)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ItemBusy);
		UpdateHealDebugString(OutResult);
		return;
	}

	OutResult.QuantityAfter = Item->GetQuantity();
	SetHealStatus(OutResult, ETerritoryHealConsumeStatus::Ready);
	UpdateHealDebugString(OutResult);
}

bool TerritoryHealingSupport::AutoConsumeHealItemFromInventory(
	ANarrativeCharacter& Character,
	const TSubclassOf<UNarrativeItem> HealItemClass,
	const float HealthFractionThreshold,
	const float CooldownSeconds,
	float& InOutLastConsumeWorldTime,
	FTerritoryHealConsumeResult& OutResult)
{
	OutResult = FTerritoryHealConsumeResult();
	OutResult.ItemClass = HealItemClass;

	// Narrative applies item effects on the server; a client call would only queue a
	// ServerUseItem RPC, so report the limitation instead of claiming a consume.
	if (!Character.HasAuthority())
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ClientOnly);
		UpdateHealDebugString(OutResult);
		UE_LOG(LogTerritory, Verbose, TEXT("%s"), *OutResult.DebugString);
		return false;
	}

	EvaluateAutoConsumeHealItem(&Character, HealItemClass, HealthFractionThreshold,
		InOutLastConsumeWorldTime, CooldownSeconds, OutResult);
	if (OutResult.Status != ETerritoryHealConsumeStatus::Ready)
	{
		UE_LOG(LogTerritory, Verbose, TEXT("%s"), *OutResult.DebugString);
		return false;
	}

	UNarrativeInventoryComponent* Inventory = Character.GetInventoryComponent();
	UNarrativeItem* Item = Inventory ? Inventory->FindItemOfClass(HealItemClass, false) : nullptr;
	if (!Inventory || !IsValid(Item))
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::ItemMissing);
		UpdateHealDebugString(OutResult);
		UE_LOG(LogTerritory, Verbose, TEXT("%s"), *OutResult.DebugString);
		return false;
	}

	// Narrative applies the item's SetByCaller gameplay effect and, when bConsumeOnUse is set,
	// removes the stack in the same authoritative call. Territory never writes Health.
	if (!Inventory->UseItem(Item))
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::UseRejected);
		UpdateHealDebugString(OutResult);
		UE_LOG(LogTerritory, Warning, TEXT("%s"), *OutResult.DebugString);
		return false;
	}

	const UWorld* World = Character.GetWorld();
	InOutLastConsumeWorldTime = FMath::Max(World ? World->GetTimeSeconds() : 0.f, KINDA_SMALL_NUMBER);

	// The stack may have left the inventory with the same call, so re-resolve before reading.
	UNarrativeItem* Remaining = Inventory->FindItemOfClass(HealItemClass, false);
	OutResult.QuantityAfter = IsValid(Remaining) ? Remaining->GetQuantity() : 0;
	TryGetNarrativeHealthFraction(&Character, OutResult.HealthFractionAfter);

	SetHealStatus(OutResult, ETerritoryHealConsumeStatus::Consumed);
	UpdateHealDebugString(OutResult);
	UE_LOG(LogTerritory, Log, TEXT("%s"), *OutResult.DebugString);
	return true;
}

bool TerritoryHealingSupport::AutoConsumeHealItemFromInventory(
	ANarrativeCharacter& Character,
	float& InOutLastConsumeWorldTime,
	FTerritoryHealConsumeResult& OutResult)
{
	OutResult = FTerritoryHealConsumeResult();

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (!Settings || !Settings->bEnableAutoConsumeHealItem)
	{
		SetHealStatus(OutResult, ETerritoryHealConsumeStatus::Disabled);
		UpdateHealDebugString(OutResult);
		return false;
	}

	OutResult.ItemClass = Settings->AutoConsumeHealItemClass;
	return AutoConsumeHealItemFromInventory(Character, Settings->AutoConsumeHealItemClass,
		Settings->AutoConsumeHealthFraction, Settings->AutoConsumeCooldownSeconds,
		InOutLastConsumeWorldTime, OutResult);
}

// Blueprint contract. The project's player pawn (BP_TerritoryPlayerCharacter) and any NPC
// Blueprint can call these nodes; the C++ callers (guards, counterattack participants) use the
// namespace above directly.

bool UTerritoryHealingLibrary::CanAutoConsumeHealItem(
	const ANarrativeCharacter* Character,
	const TSubclassOf<UNarrativeItem> HealItemClass,
	const float HealthFractionThreshold,
	const float LastConsumeWorldTime,
	const float CooldownSeconds,
	FTerritoryHealConsumeResult& OutResult)
{
	TerritoryHealingSupport::EvaluateAutoConsumeHealItem(Character, HealItemClass,
		HealthFractionThreshold, LastConsumeWorldTime, CooldownSeconds, OutResult);
	return OutResult.Status == ETerritoryHealConsumeStatus::Ready;
}

bool UTerritoryHealingLibrary::AutoConsumeHealItemFromInventory(
	ANarrativeCharacter* Character,
	const TSubclassOf<UNarrativeItem> HealItemClass,
	const float HealthFractionThreshold,
	const float CooldownSeconds,
	float& InOutLastConsumeWorldTime,
	FTerritoryHealConsumeResult& OutResult)
{
	if (!IsValid(Character))
	{
		OutResult = FTerritoryHealConsumeResult();
		OutResult.ItemClass = HealItemClass;
		OutResult.Status = ETerritoryHealConsumeStatus::InvalidCharacter;
		OutResult.FailureReason = LOCTEXT("HealNoCharacterBP", "No Narrative character supplied.");
		OutResult.DebugString = TEXT("AutoConsumeHealItem: no character");
		return false;
	}

	return TerritoryHealingSupport::AutoConsumeHealItemFromInventory(*Character, HealItemClass,
		HealthFractionThreshold, CooldownSeconds, InOutLastConsumeWorldTime, OutResult);
}

bool UTerritoryHealingLibrary::AutoConsumeHealItemFromInventoryUsingSettings(
	ANarrativeCharacter* Character,
	float& InOutLastConsumeWorldTime,
	FTerritoryHealConsumeResult& OutResult)
{
	if (!IsValid(Character))
	{
		OutResult = FTerritoryHealConsumeResult();
		OutResult.Status = ETerritoryHealConsumeStatus::InvalidCharacter;
		OutResult.FailureReason = LOCTEXT("HealNoCharacterBP2", "No Narrative character supplied.");
		OutResult.DebugString = TEXT("AutoConsumeHealItem: no character");
		return false;
	}

	return TerritoryHealingSupport::AutoConsumeHealItemFromInventory(*Character,
		InOutLastConsumeWorldTime, OutResult);
}

#undef LOCTEXT_NAMESPACE

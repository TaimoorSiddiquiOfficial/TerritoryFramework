// Territory Framework — automatic consumption of a Narrative healing item.
//
// Territory owns no damage, healing or inventory math. Narrative's
// UNarrativeGameplayEffectItem applies the item's SetByCaller gameplay effect
// (for example SetByCaller.Heal on GE_Item_Instant) when the item is used, and
// Narrative's own bConsumeOnUse path removes the stack afterwards. These helpers
// only decide *when* that server-authoritative use is allowed, and report a
// structured result (success, old state, new state, failure reason).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "TerritoryHealingSupport.generated.h"

class ANarrativeCharacter;
class UNarrativeAbilitySystemComponent;
class UNarrativeInventoryComponent;
class UNarrativeItem;

/** Outcome of one automatic heal-item consume attempt. */
UENUM(BlueprintType)
enum class ETerritoryHealConsumeStatus : uint8
{
	/** The item was used; Narrative applied its SetByCaller effect and consumed the stack. */
	Consumed            UMETA(DisplayName = "Consumed"),
	/** Auto consume is switched off in Territory settings. */
	Disabled            UMETA(DisplayName = "Auto Consume Disabled"),
	/** No Narrative character, ability system component or inventory component. */
	InvalidCharacter    UMETA(DisplayName = "Invalid Character Or Narrative Components"),
	/** Called on a client: Narrative's item use is server-authoritative. */
	ClientOnly          UMETA(DisplayName = "Not Authority"),
	/** The Narrative ability system component reports the character as dead. */
	Dead                UMETA(DisplayName = "Owner Is Dead"),
	/** No healing item class configured. */
	ItemNotConfigured   UMETA(DisplayName = "No Healing Item Configured"),
	/** Health is above the configured threshold, so the item is kept. */
	HealthAboveThreshold UMETA(DisplayName = "Health Above Threshold"),
	/** The healing item is not in the inventory. */
	ItemMissing         UMETA(DisplayName = "Healing Item Not In Inventory"),
	/** Item cannot be used yet (busy, hidden, or still inside Narrative's recharge window). */
	ItemBusy            UMETA(DisplayName = "Item Cannot Be Used Yet"),
	/** Evaluation only: every gate passed, a consume would be attempted. */
	Ready               UMETA(DisplayName = "Ready To Consume"),
	/** Territory's auto-consume cooldown is still running for this character. */
	CooldownActive      UMETA(DisplayName = "Auto Consume Cooldown Active"),
	/** Narrative rejected UNarrativeInventoryComponent::UseItem. */
	UseRejected         UMETA(DisplayName = "Narrative Rejected Item Use")
};

/**
 * Structured result of one automatic heal-item consume attempt.
 *
 * Mirrors the framework contract: success flag, the state before and after the
 * mutation, and a machine readable reason when nothing happened.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryHealConsumeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	ETerritoryHealConsumeStatus Status = ETerritoryHealConsumeStatus::InvalidCharacter;

	/** True only when Narrative accepted the item use. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	bool bConsumed = false;

	/** Item class the attempt used (after settings fallback resolution). */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	TSubclassOf<UNarrativeItem> ItemClass;

	/** Health fraction before the consume (0-1). */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	float HealthFractionBefore = 0.f;

	/** Health fraction after Narrative applied the heal (0-1). Never authored by Territory. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	float HealthFractionAfter = 0.f;

	/** Remaining stack quantity after the consume (0 when the stack was consumed). */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	int32 QuantityAfter = 0;

	/** Seconds still left on Territory's auto-consume cooldown, when the attempt was blocked by it. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	float CooldownRemainingSeconds = 0.f;

	/** Player/designer readable reason when the attempt did not consume. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	FText FailureReason;

	/** One line summary for logs and the Territory combat debug string. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Combat|Auto Heal")
	FString DebugString;

	bool WasConsumed() const { return bConsumed; }
};

namespace TerritoryHealingSupport
{
	/**
	 * Narrative health fraction of a character (Health / MaxHealth, clamped 0-1).
	 * Returns false when the character has no Narrative attribute set or MaxHealth is zero.
	 * Side-effect free.
	 */
	TERRITORYFRAMEWORK_API bool TryGetNarrativeHealthFraction(
		const ANarrativeCharacter* Character, float& OutHealthFraction);

	/**
	 * Side-effect free evaluation: would an automatic heal-item consume be allowed right now?
	 * Fills OutResult with the status, threshold comparison and failure reason. Never mutates
	 * the inventory, attributes, or cooldown state.
	 */
	TERRITORYFRAMEWORK_API void EvaluateAutoConsumeHealItem(
		const ANarrativeCharacter* Character,
		TSubclassOf<UNarrativeItem> HealItemClass,
		float HealthFractionThreshold,
		float LastConsumeWorldTime,
		float CooldownSeconds,
		FTerritoryHealConsumeResult& OutResult);

	/**
	 * Server-authoritative consume of one healing item from the character's Narrative inventory.
	 *
	 * Uses UNarrativeInventoryComponent::UseItem, so Narrative applies the item's
	 * SetByCaller gameplay effect (SetByCaller.Heal) and removes the stack through its own
	 * bConsumeOnUse path. Territory never writes Health. Returns true only when Narrative
	 * accepted the use.
	 *
	 * @param InOutLastConsumeWorldTime Caller-owned cooldown stamp, updated on success.
	 */
	TERRITORYFRAMEWORK_API bool AutoConsumeHealItemFromInventory(
		ANarrativeCharacter& Character,
		TSubclassOf<UNarrativeItem> HealItemClass,
		float HealthFractionThreshold,
		float CooldownSeconds,
		float& InOutLastConsumeWorldTime,
		FTerritoryHealConsumeResult& OutResult);

	/**
	 * Server-authoritative consume using UTerritoryDeveloperSettings defaults
	 * (bEnableAutoConsumeHealItem, AutoConsumeHealItemClass, AutoConsumeHealthFraction,
	 * AutoConsumeCooldownSeconds).
	 */
	TERRITORYFRAMEWORK_API bool AutoConsumeHealItemFromInventory(
		ANarrativeCharacter& Character,
		float& InOutLastConsumeWorldTime,
		FTerritoryHealConsumeResult& OutResult);
}

/**
 * Blueprint contract for automatic healing item consumption.
 *
 * Blueprints get the same gates as C++ callers. The mutation nodes are server-authoritative;
 * on a client they report ETerritoryHealConsumeStatus::ClientOnly into the result instead of
 * pretending the item was used.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryHealingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Side-effect free: would this character consume a Narrative healing item right now?
	 * Returns true when every gate passes and OutResult.Status is Ready.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat|Auto Heal",
		meta = (DisplayName = "Can Auto Consume Healing Item"))
	static bool CanAutoConsumeHealItem(
		const ANarrativeCharacter* Character,
		TSubclassOf<UNarrativeItem> HealItemClass,
		float HealthFractionThreshold,
		float LastConsumeWorldTime,
		float CooldownSeconds,
		FTerritoryHealConsumeResult& OutResult);

	/**
	 * Server-authoritative: consume one configured healing item from the character's Narrative
	 * inventory when its health is at or below the threshold. Narrative applies the item's
	 * SetByCaller heal and removes the stack through bConsumeOnUse; Territory never writes health.
	 * Keep InOutLastConsumeWorldTime in a character variable so the cooldown survives calls.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat|Auto Heal",
		meta = (DisplayName = "Auto Consume Healing Item From Inventory"))
	static bool AutoConsumeHealItemFromInventory(
		ANarrativeCharacter* Character,
		TSubclassOf<UNarrativeItem> HealItemClass,
		float HealthFractionThreshold,
		float CooldownSeconds,
		UPARAM(ref) float& InOutLastConsumeWorldTime,
		FTerritoryHealConsumeResult& OutResult);

	/**
	 * Server-authoritative convenience node using the Territory|Combat|Auto Heal project
	 * settings (enable flag, item class, health fraction and cooldown).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat|Auto Heal",
		meta = (DisplayName = "Auto Consume Healing Item (Settings Defaults)"))
	static bool AutoConsumeHealItemFromInventoryUsingSettings(
		ANarrativeCharacter* Character,
		UPARAM(ref) float& InOutLastConsumeWorldTime,
		FTerritoryHealConsumeResult& OutResult);
};

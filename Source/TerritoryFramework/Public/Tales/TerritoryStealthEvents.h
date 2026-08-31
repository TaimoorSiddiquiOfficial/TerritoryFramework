#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryStealthProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryStealthEvents.generated.h"

class UTerritoryDisguiseProfile;

UENUM(BlueprintType)
enum class ETerritoryDisguiseEventAction : uint8
{
	Compromise UMETA(DisplayName="Compromise Disguise"),
	Restore UMETA(DisplayName="Restore Disguise")
};

/** Quest event that temporarily enables/disables the active Territory stealth profile. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Territory Stealth Infiltration Override"))
class TERRITORYFRAMEWORK_API UTerritorySetStealthOverrideEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetStealthOverrideEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TerritoryToModify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	bool bEnableInfiltration = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Return control to the active Territory Stealth Profile instead of storing an override."))
	bool bClearOverride = false;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Story event that confirms the explicit target and follows the profile escalation scope. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Reveal Territory Infiltrator"))
class TERRITORYFRAMEWORK_API UTerritoryRevealInfiltratorEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryRevealInfiltratorEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TerritoryToModify;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Clears exposure after an escape, disguise, cease-search objective, or story reset. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Clear Territory Exposure"))
class TERRITORYFRAMEWORK_API UTerritoryClearExposureEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryClearExposureEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TerritoryToModify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	bool bResetSuspicion = true;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Creates anonymous investigation evidence without exposing the target. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Report Territory Distraction"))
class TERRITORYFRAMEWORK_API UTerritoryReportDistractionEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryReportDistractionEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TerritoryToModify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="World offset from the explicit Narrative target. Easy example: use 0,0,0 to make the nearest guards investigate the player's current position."))
	FVector LocationOffset = FVector::ZeroVector;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Activates a perceived identity without changing Narrative faction membership. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Activate Territory Disguise"))
class TERRITORYFRAMEWORK_API UTerritoryActivateDisguiseEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryActivateDisguiseEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Useful for a cutscene or dialogue reward. For normal inventory play, use Territory Disguise Clothing so equip and unequip happen automatically."))
	TObjectPtr<UTerritoryDisguiseProfile> DisguiseProfile;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Remove Territory Disguise"))
class TERRITORYFRAMEWORK_API UTerritoryRemoveDisguiseEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryRemoveDisguiseEvent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Burns or restores the cover identity for one faction without changing diplomacy. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Territory Disguise Cover State"))
class TERRITORYFRAMEWORK_API UTerritorySetDisguiseCoverEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetDisguiseCoverEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	ETerritoryDisguiseEventAction Action = ETerritoryDisguiseEventAction::Compromise;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction that recognizes the player. Empty means every faction."))
	FGameplayTag ObserverFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Optional context for Gameplay Event payload and debugging."))
	FGameplayTag TerritoryContext;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Runs the current Place's deterministic quality and clearance check. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Perform Territory Disguise Identity Check"))
class TERRITORYFRAMEWORK_API UTerritoryDisguiseIdentityCheckEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryDisguiseIdentityCheckEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Optional exact checkpoint Territory. Empty uses the Place containing the player."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Optional checking faction. Empty uses the Territory owner."))
	FGameplayTag ObserverFaction;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

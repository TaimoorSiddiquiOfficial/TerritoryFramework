#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryStealthProfile.generated.h"

class UTerritoryInvestigationActivity;

/** Per-player awareness inside a stealth-enabled Territory. Presence is not exposure. */
UENUM(BlueprintType)
enum class ETerritoryExposureState : uint8
{
	Undetected UMETA(DisplayName="Undetected",
		ToolTip="The player is inside, but no guard has enough evidence to identify them."),
	Suspicious UMETA(DisplayName="Suspicious / Investigating",
		ToolTip="Guards have weak evidence and may investigate, but Territory conflict has not begun."),
	Exposed UMETA(DisplayName="Exposed",
		ToolTip="A guard has confirmed the player. Story bounds may now register them as a contester.")
};

/** Evidence understood by the Territory layer. Narrative remains the source of perception data. */
UENUM(BlueprintType)
enum class ETerritoryStealthEvidence : uint8
{
	None UMETA(DisplayName="None"),
	Sight UMETA(DisplayName="Guard Sight"),
	FireSeen UMETA(DisplayName="Fired While Seen"),
	Gunshot UMETA(DisplayName="Unseen Gunshot"),
	BulletImpact UMETA(DisplayName="Unseen Bullet Impact"),
	Damage UMETA(DisplayName="Confirmed Damage"),
	DefenderKilledSeen UMETA(DisplayName="Defender Kill Seen"),
	Corpse UMETA(DisplayName="Unseen Defender Death / Corpse"),
	ThrowableDistraction UMETA(DisplayName="Throwable Distraction"),
	Scripted UMETA(DisplayName="Story Event")
};

/** How far confirmed exposure should escalate. Faction War preserves the existing contest flow. */
UENUM(BlueprintType)
enum class ETerritoryStealthEscalationScope : uint8
{
	LocalAlarm UMETA(DisplayName="Local Alarm Only",
		ToolTip="Guards investigate, but the Territory is not registered as Contested."),
	TerritoryConflict UMETA(DisplayName="Territory Conflict",
		ToolTip="Register the exposed player as a Territory contester. Diplomacy is decided by State Events."),
	FactionWar UMETA(DisplayName="Faction War Through State Event",
		ToolTip="Register the player as a contester, then let the Contested State Event declare War.")
};

/** Read-only result for UI, Narrative conditions, debugging, and tests. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryInfiltrationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryExposureState ExposureState = ETerritoryExposureState::Undetected;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryStealthEvidence LastEvidence = ETerritoryStealthEvidence::None;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Suspicion = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector LastEvidenceLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector EstimatedSourceDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	float LastEvidenceWorldTime = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	int32 ConfirmingObserverCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	bool bInsideTerritory = false;
};

/**
 * Reusable stealth policy selected by a Territory Definition and optionally overridden per State Config.
 * It adapts Narrative sight/hearing/damage into Territory suspicion; it does not replace Narrative AI.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Stealth Profile"))
class TERRITORYFRAMEWORK_API UTerritoryStealthProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTerritoryStealthProfile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Infiltration",
		meta=(ToolTip="When enabled, entering story bounds registers an undetected infiltrator instead of immediately starting Contested. Easy example: use this for a rescue quest inside an enemy Place."))
	bool bAllowStealthInfiltration = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Infiltration")
	ETerritoryStealthEscalationScope EscalationScope =
		ETerritoryStealthEscalationScope::FactionWar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Effective Narrative sight at or above this value immediately confirms the player."))
	float ImmediateSightExposureThreshold = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Sight below this value is ignored. Narrative's normal attack goal uses 0.2 as its useful minimum."))
	float MinimumSightEvidence = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Multiplies Narrative's sight strength before Stealth Rating is applied."))
	float GuardDetectionMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Suspicion added per second while partial sight remains active."))
	float SightSuspicionGainPerSecond = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Suspicion removed per second after every guard loses sight."))
	float SuspicionDecayPerSecond = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="100.0",
			ToolTip="Narrative Stealth Rating is interpreted on this scale. A rating of 50 on a scale of 100 halves effective sight."))
	float MaximumStealthRating = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight")
	bool bRespectNarrativeInvisibleTag = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bFireWhileSeenExposes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bFireWhileUnseenStartsInvestigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bDamageImmediatelyExposes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bSeenDefenderKillExposes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bUnseenDefenderDeathStartsInvestigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float GunshotSuspicion = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float BulletImpactSuspicion = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float CorpseSuspicion = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float ThrowableDistractionSuspicion = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0", ClampMax="8"))
	int32 MaximumInvestigators = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="100.0", Units="cm"))
	float InvestigationRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0.5", Units="s"))
	float InvestigationDuration = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="10.0", Units="cm"))
	float InvestigationAcceptanceRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0.0", ClampMax="2.0", Units="s",
			ToolTip="Muzzle and impact sounds from the same actor inside this window are treated as one missed shot."))
	float ShotCorrelationWindow = 0.3f;

	/** Optional Blueprint child can add Narrative dialogue or a custom search Behaviour Tree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation")
	TSubclassOf<UTerritoryInvestigationActivity> InvestigationActivityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(Categories="Territory.Event.Stealth",
			ToolTip="Sent to the exposed player's Ability System. A temporary stealth ability may listen for this event and cancel itself; passive equipment is not removed."))
	FGameplayTag BreakStealthGameplayEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration")
	bool bSendBreakStealthGameplayEvent = true;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

#pragma once

#include "CoreMinimal.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryStoryEvents.generated.h"

class ANarrativePlayerState;
class ULevelSequence;
class UTerritoryCinematicLightRigProfile;

/**
 * Changes the exact Narrative quest player's saved faction membership.
 * This is political identity, not a disguise and not Territory ownership.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Narrative Player Factions",
		ToolTip="Change the exact player saved Narrative memberships. Replace removes old memberships; Add keeps them. Primary Faction selects the membership used by Territory ownership and accounts. Disguise is a separate system."))
class TERRITORYFRAMEWORK_API UTerritorySetNarrativePlayerFactionsEvent
	: public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetNarrativePlayerFactionsEvent(
		const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction memberships applied to the exact Narrative target player. Easy example: replace Police with Heroes after the Regime betrays the player."))
	FGameplayTagContainer NewFactions;

	/** Optional political membership to put first in Narrative's saved faction list. Empty keeps the existing order. This must belong to the final memberships. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions", ToolTip="Choose the faction the player represents when owning, capturing and managing places. Example: add Rebels and choose Rebels as Primary Faction while keeping a second membership. Leave empty to keep the current order."))
	FGameplayTag PrimaryFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="True replaces every existing player faction. False adds memberships. Both modes commit once through Narrative Player State. Use Primary Faction to deliberately choose which political membership comes first."))
	bool bReplaceExistingFactions = true;

	/** Native helper used by execution and regression tests. */
	bool ApplyToPlayerState(ANarrativePlayerState* PlayerState) const;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target,
		APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UENUM(BlueprintType)
enum class ETerritoryHierarchyStoryOperation : uint8
{
	ClaimForFaction UMETA(DisplayName="Claim Entire Hierarchy For Faction"),
	ClearToUnclaimed UMETA(DisplayName="Clear Entire Hierarchy To Unclaimed"),
	Lock UMETA(DisplayName="Lock Entire Hierarchy"),
	Unlock UMETA(DisplayName="Unlock Entire Hierarchy")
};

/**
 * Applies one story decision to a loaded City, District, or Place hierarchy.
 * Ownership changes are committed only on independent leaf Places; the existing
 * unanimity reducer remains the sole authority that derives District and City owner.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Apply Territory Hierarchy Story Override",
		ToolTip="Apply an explicit story decision to loaded descendants. Each Place commits through existing authority; parents are derived from their children. This is not an atomic transaction across the whole city. Unloaded descendants are not changed."))
class TERRITORYFRAMEWORK_API UTerritoryHierarchyStoryOverrideEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryHierarchyStoryOverrideEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Root City, District, or Place. Easy example: choose Haven Reach to change every currently loaded District and Place below it after a betrayal quest."))
	FGameplayTag RootTerritory;

	/** Choose whether the loaded hierarchy is claimed, cleared, locked or unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	ETerritoryHierarchyStoryOperation Operation =
		ETerritoryHierarchyStoryOperation::ClaimForFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			EditCondition="Operation == ETerritoryHierarchyStoryOperation::ClaimForFaction",
			EditConditionHides,
			ToolTip="Exact Narrative faction that receives every independent Place. District and City ownership is then derived from their children."))
	FGameplayTag ClaimingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Recommended for a deliberate story override. Bypasses Place lock, diplomacy, and state conditions, but never bypasses server authority or the hierarchy reducer."))
	bool bForceStoryOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(EditCondition="Operation == ETerritoryHierarchyStoryOperation::Lock",
			EditConditionHides,
			ToolTip="Reason shown by locked Territory UI. Example: Complete The Governor's Trial."))
	FText LockReason;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Schedules one finite physical enemy assault through the existing counterattack authority.
 * The inherited Narrative Event -> Conditions array is evaluated before scheduling. A
 * Strategic Counterattack may later repeat only when the selected force profile permits
 * a finite/unlimited schedule and its cooldown, Narrative time, quest, diplomacy, staging,
 * route, and budget rules still pass. Story Pursuit never repeats automatically.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wave of Enemies (Schedule Territory Assault)",
		ToolTip="Request one finite physical force through the counterattack scheduler. Choose strategic counterattack, story pursuit, or owner reinforcements before handover. Immediate removes the preparation wait; route, budget and policy checks can still reject it."))
class TERRITORYFRAMEWORK_API UTerritoryScheduleEnemyWaveEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryScheduleEnemyWaveEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Claimed Territory the enemy will physically attack."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="When enabled, diplomacy, force power, supply, budgets, and deterministic priority choose the best configured attacker."))
	bool bChooseBestEligibleAttacker = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Exact attacker, or preferred tie-break when Best Eligible Attacker is enabled. Example: Narrative.Factions.Bandits."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Strategic Counterattack starts one finite battle and may later follow the force profile's One Assault, Finite Series, or Unlimited Schedule policy. Story Pursuit / Boss Chase is a deliberate Tales exception, requires force-profile permission, and never repeats automatically."))
	ETerritoryAssaultLaunchMode LaunchMode =
		ETerritoryAssaultLaunchMode::StrategicCounterattack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Narrative.Factions",
		EditCondition="LaunchMode == ETerritoryAssaultLaunchMode::StoryReinforcements", EditConditionHides,
		ToolTip="Optional exact opponent. Empty uses the explicit Narrative target pawn's faction, falling back to the Tales owner. The sender must still own the Place and be at war with this faction."))
	FGameplayTag OpposingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(
		ToolTip="Stable story encounter ID, for example Blacksmith_PostCapture. Supported in every launch mode and required for owner reinforcements. Use the same ID in an assault condition or quest task so another battle cannot count as this one. Named strategic and owner-reinforcement victories survive history trimming. Owner reinforcements also reject a completed encounter; guard other repeatable Wave events with conditions."))
	FName ScenarioID;

	/**
	 * Special story beat: deploy now after admission. An explicit Narrative Wave
	 * does not require the automatic strategy layer's secure District,
	 * Reinforcements capability, or counter-Quest gate. Immediate also skips profile
	 * grace, time window, probability, warning delay, and player proximity. Authority,
	 * opposing ownership, diplomacy, finite force, budgets, approaches, and routes
	 * remain mandatory.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(DisplayName="Start Counterattack Immediately",
			ToolTip="Launch the physical force immediately after validation. Use for an authored ambush or Quest climax. Leave off for the normal strategic schedule."))
	bool bStartImmediately = false;

	/** Native admission used by this event and persistent Narrative admission tasks.
	 * Rechecks conditions and returns the scheduler's refusal without logging retries. */
	bool TryScheduleWave(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent, FText& OutFailureReason,
		bool bLogFailure = false);

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Easy Tales event for a one-shot boss pursuit. Narrative owns mounting, driving, arrival,
 * dismount and combat. Territory supplies the finite scenario, route direction, optional
 * capture policy and outcome. Use it for hunters arriving by car or a capo escaping by car.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Start Territory Boss Chase",
		ToolTip="Start a finite story pursuit using the existing attack profile and Narrative NPCs. Choose who chases whom in Pursuit Options. The default is one enemy and no territory capture; route and spawn checks still apply."))
class TERRITORYFRAMEWORK_API UTerritoryStartBossChaseEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryStartBossChaseEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Claimed Place where the boss force will pursue the player. Its Territory Definition supplies the vehicle and foot approaches."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Exact pursuing faction. Configure that faction's Attacker Definition as the boss or boss-force NPC Definition in the target's counterattack profile."))
	FGameplayTag PursuingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ShowOnlyInnerProperties,
			ToolTip="Reusable story options. Enemy Chases Player drives into the Place and hands off to combat. Player Chases Enemy reverses the authored vehicle route and records Target Escaped if the capo reaches the exit."))
	FTerritoryStoryPursuitOptions PursuitOptions;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Cancels matching durable assaults; active physical attackers are optional and never become a hidden ownership roll. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Cancel Territory Enemy Waves",
		ToolTip="Cancel matching finite assault records on the server. Optional faction and story ID narrow the selection. By default only preparing forces are cancelled. Include Active also withdraws deployed attackers from capture."))
class TERRITORYFRAMEWORK_API UTerritoryCancelEnemyWavesEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryCancelEnemyWavesEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Optional exact story encounter ID from the Wave event. Example: BlacksmithRetake cancels only that encounter. Empty preserves the old behavior and allows all story IDs."))
	FName ScenarioID;

	/** Selects records only; the counterattack subsystem remains the cancellation authority. */
	bool MatchesAssault(const FTerritoryAssaultRecord& Assault) const;

	/** Territory targeted by this operation or result. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions", ToolTip="Optional attacker filter. Leave empty to cancel waves from every faction."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="False cancels only grace, warning, and waiting records. True also retires living attackers from an active assault."))
	bool bIncludePhysicallyActiveAssaults = false;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Uses the existing atomic staffing/currency mutation; it does not grant free guards. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Territory Guard Assignment Target",
		ToolTip="Request a new guard assignment target using the exact Narrative requester. Ownership, capacity and payment rules still apply. This does not grant free guards or immediately fill every post."))
class TERRITORYFRAMEWORK_API UTerritorySetGarrisonTargetEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetGarrisonTargetEvent(const FObjectInitializer& ObjectInitializer);

	/** Territory targeted by this operation or result. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="0", ToolTip="Exact desired staffing target. Increasing it charges the explicit Narrative target's inventory."))
	int32 DesiredGuards = 1;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Purchases exactly one existing Property upgrade through the normal Narrative currency path. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Purchase One Territory Property Upgrade",
		ToolTip="Buy one upgrade for a loaded Property using the exact Narrative requester. Requires ownership, an eligible upgrade and payment. Districts and Cities cannot be upgraded by this event."))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePropertyEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryUpgradePropertyEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Loaded Property to upgrade by exactly one level."))
	FGameplayTag TargetProperty;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Executes one atomic input/output recipe against the explicit target's Narrative inventory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Execute Territory Resource Recipe",
		ToolTip="Execute a finite recipe batch using Narrative inventory and faction storage. Requires an explicit requester and valid resources. Recipe failures do not grant output."))
class TERRITORYFRAMEWORK_API UTerritoryExecuteResourceRecipeEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryExecuteResourceRecipeEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions", ToolTip="Faction that owns the explicit Narrative inventory account."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Semantic source recorded in the production result. It does not bypass ownership or capture."))
	FGameplayTag SourceTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Atomic Narrative inventory inputs and outputs. Example: consume medicine supplies and produce one relief package."))
	FTerritoryProductionRule Recipe;

	/** Requested Place upgrade level used by the selected scripted operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(ClampMin="0"))
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="1", ToolTip="Finite number of recipe batches executed in one validated transaction."))
	int32 BatchCount = 1;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Plays one authored Level Sequence as a Territory cutscene, for an explicitly resolved audience.
 *
 * Place it in Defender Died, All Defenders Defeated, or a Floor's Cleared Events, so "the third
 * defender steps out of the stairwell" and "floor 2 is clear" become cutscene cues. Narrative stays
 * the presentation authority: this event only starts a sequence through the vendor's replicated,
 * audience-filtered player factory and hands the camera back when the sequence ends.
 *
 * Input suppression is a data flag, not an API call. The engine enters and leaves cinematic mode
 * from the sequence player's own OnStartedPlaying/OnStopped, so this event must never call
 * SetCinematicMode itself: the release would not be symmetric and the player would be left without
 * control. Pause At End is forced off when the cutscene runs, because ULevelSequencePlayer fires
 * OnStopped only on a real stop - a sequence that merely pauses fires OnPause, never releases
 * cinematic mode, and would leave the player permanently unable to move.
 *
 * This event also owns the actor's lifetime, because nobody else does: the vendor factory returns
 * the actor through OutActor and never destroys it. Teardown is delegated to
 * UTerritoryCutsceneTeardownComponent, which this event attaches to the actor it just created, so
 * the actor is destroyed once its sequence has genuinely stopped rather than leaking for the rest
 * of the session. See that component for why it binds OnStop rather than trusting OnFinished.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Play Territory Cutscene",
		ToolTip="Play one Level Sequence as a cutscene for the exact Narrative target player, or for the players of Audience Faction when this event runs with no player context. Server-only; Narrative owns presentation. Movement and look are released when the sequence ends, so the player always gets control back."))
class TERRITORYFRAMEWORK_API UTerritoryPlayCutsceneEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryPlayCutsceneEvent(const FObjectInitializer& ObjectInitializer);

	/** The sequence played as the cutscene. Empty does nothing and reports why. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(AllowedClasses="/Script/LevelSequence.LevelSequence",
			ToolTip="Level Sequence asset played as the cutscene. Example: a short shot of defenders walking down the stairwell."))
	TSoftObjectPtr<ULevelSequence> CutsceneSequence;

	/** Playback settings. Pause At End is overridden to false when the cutscene runs; see the class comment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ShowOnlyInnerProperties,
			ToolTip="Narrative playback settings for this cutscene. Pause At End is ignored: a paused sequence never fires OnStopped, which is the engine's only path that releases cinematic input suppression."))
	FNarrativeSequencePlaybackSettings PlaybackSettings;

	/**
	 * Net relevancy radius for the sequence actor. Zero makes it always relevant to the resolved
	 * audience, which is the correct default: the audience is already filtered by the explicit
	 * player list, and a distance check on top of it would silently drop a viewer who walked away.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="0.0", Units="cm",
			ToolTip="Leave at 0 so every resolved viewer receives the cutscene regardless of distance. A non-zero radius additionally culls viewers who are far from the spawn location."))
	float RelevancyDist = 0.f;

	/** Fallback audience used only when this event runs without an explicit player context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Faction whose players watch this cutscene when the event runs with no player context, such as a defender defeat with no instigator. Matched against Narrative faction identity exactly as capture context resolution does. Leave empty to play for nobody rather than guess."))
	FGameplayTag AudienceFaction;

	/** Optional per-viewer cinematic light rig. Empty leaves lighting entirely to Narrative. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Optional Territory light rig to follow the sequence for each viewer. Leave empty to leave lighting authority with Narrative."))
	TObjectPtr<UTerritoryCinematicLightRigProfile> LightRigProfile;

	/**
	 * How long the sequence actor survives after the cutscene stops, before it is destroyed.
	 *
	 * The destruction is replicated, so this grace is what keeps a client whose own copy of the
	 * sequence is a fraction of a second behind from having its cutscene cut off mid-shot. It is
	 * measured from the stop, not from the start, so an authored PlayRate or a skipped sequence
	 * cannot make it fire early. Zero destroys as soon as the sequence stops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="0.0", Units="s",
			ToolTip="Seconds the cutscene actor is kept alive after its sequence stops, so a client a moment behind is not cut off mid-shot. Raise it for a high-latency audience; 0 destroys immediately."))
	float TeardownGraceSeconds = 1.f;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

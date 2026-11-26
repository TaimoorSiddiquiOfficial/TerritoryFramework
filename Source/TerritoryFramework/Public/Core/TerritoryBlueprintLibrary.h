#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "TerritoryBlueprintLibrary.generated.h"

class ATerritoryVolume;
class UTerritoryRegistrySubsystem;
class UTerritoryControlSubsystem;
class UTerritoryEconomySubsystem;
class UTerritoryCombatDirector;
class UTerritoryDiplomacySubsystem;
class ATerritoryCity;
class ATerritoryDistrict;
class AAIController;
class ANarrativeNPCCharacter;
class ANarrativeNPCController;
class UNPCGoalItem;
class UNPCGoalGenerator;
struct FTerritoryInfiltrationSnapshot;

/**
 * Global Blueprint function library for territory queries.
 *
 * All functions are static (no instance needed). Most are BlueprintPure for
 * use in any graph (no exec pin). Use WorldContextObject from any actor to
 * get the subsystem.
 *
 * Quick start:
 *   GetTerritoryControl(MyActor)->IsTerritoryContested(Tag) -> bool
 *   GetTerritoryAtLocation(MyActor, Location) -> ATerritoryVolume*
 *   GetTerritoryEconomy(MyActor)->GetActorCurrency(Requester) -> int32
 *   TerritoryBlueprintLibrary::IsSameFaction(FactionA, FactionB) -> bool
 */
enum class ETerritoryStealthEvidence : uint8;

UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════════
	// Subsystem Access (BlueprintPure — no exec pin)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the territory registry subsystem (all territory registrations + spatial lookups). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Registry"))
	static UTerritoryRegistrySubsystem* GetTerritoryRegistry(const UObject* WorldContextObject);

	/** Get the territory control subsystem (capture flow, attackers, progress tracking). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Control"))
	static UTerritoryControlSubsystem* GetTerritoryControl(const UObject* WorldContextObject);

	/** Get the territory economy subsystem (faction treasuries, transactions, income ticks). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Economy"))
	static UTerritoryEconomySubsystem* GetTerritoryEconomy(const UObject* WorldContextObject);

	/** Get the combat director subsystem (attack slot management for NPCs). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Combat Director"))
	static UTerritoryCombatDirector* GetTerritoryCombatDirector(const UObject* WorldContextObject);

	/** Get the diplomacy subsystem (treaties, reputation, war/peace/alliance). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Diplomacy"))
	static UTerritoryDiplomacySubsystem* GetTerritoryDiplomacy(const UObject* WorldContextObject);

	/** Get the evidence ledger subsystem (durable physical facts about the campaign). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Evidence Ledger"))
	static UTerritoryEvidenceLedgerSubsystem* GetTerritoryEvidenceLedger(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Territory Queries (BlueprintPure)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Returns the territory volume at a world location. Null if no territory contains it.
	 * Uses spatial index for O(1) lookup.
	 *
	 * Example:
	 *   Volume = GetTerritoryAtLocation(Self, PlayerLocation)
	 *   if (Volume != None) { ShowTerritoryUI(Volume); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory At Location"))
	static ATerritoryVolume* GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	/**
	 * Returns a territory by its tag. Null if no territory has that tag.
	 * Tag example: Territory.Marketplace, Territory.Harbor, Territory.Castle
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory By Tag"))
	static ATerritoryVolume* GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	/** Returns all registered territories in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Territories"))
	static TArray<ATerritoryVolume*> GetAllTerritories(const UObject* WorldContextObject);

	/** Returns all territories owned by the given faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territories By Faction"))
	static TArray<ATerritoryVolume*> GetTerritoriesByFaction(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns all child territories of a parent tag (districts of a city, properties of a district). */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Child Territories"))
	static TArray<ATerritoryVolume*> GetChildTerritories(const UObject* WorldContextObject, const FGameplayTag& ParentTag);

	/** Return the number of Territories included in the current query. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Count"))
	static int32 GetTerritoryCount(const UObject* WorldContextObject);

	/** Count Territories owned by the exact Narrative faction in the available Territory data. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Territory Count"))
	static int32 GetFactionTerritoryCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Stealth Contest Admission
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Single authority for the stealth profile's "who may start a contest inside a Place" rule.
	 *
	 * True when the profile does not require a wielded weapon, or a war already exists between the
	 * Place owner and the pawn's faction, or the pawn is actually wielding a weapon. An equipped but
	 * holstered weapon is not a threat. The story-bounds gate and the stealth exposure escalation
	 * both call this, so a holstered visitor is warned instead of contested.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Does Pawn Admit Story Contest",
			ToolTip="Stealth profile rule: profile flag off, an existing war, or a wielded weapon admits the contest."))
	static bool DoesPawnAdmitStoryContest(const AActor* Pawn, const AActor* TerritoryActor,
		const UTerritoryStealthProfile* Profile, bool bTreatAsAggressor = false);

	/** True when this pawn's faction is at war with the faction owning the given Territory actor. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Is Pawn At War With Territory Owner"))
	static bool IsPawnAtWarWithTerritoryOwner(const AActor* Pawn, const AActor* TerritoryActor);

	/**
	 * True when this evidence proves the target attacked rather than merely being seen: fired while
	 * seen, an unseen gunshot or bullet impact, confirmed damage, a seen defender kill, or a story
	 * scripted exposure. Such a target is a legitimate combat target whatever it is holding.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Is Aggressive Stealth Evidence"))
	static bool IsAggressiveStealthEvidence(ETerritoryStealthEvidence Evidence);

	/**
	 * Aggression plus freshness: a clue older than WindowSeconds stops admitting the player to combat
	 * and to the story-bounds contest. WindowSeconds <= 0 keeps the legacy "last evidence wins"
	 * behaviour. The contest gate, the guard engagement decision and the attack-goal gate all call
	 * this, so a guard and the Place can never disagree about whether a player is a threat.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(WorldContext="WorldContextObject",
			DisplayName="Is Recent Aggressive Stealth Evidence",
			ToolTip="True when the last evidence is aggressive and no older than Window Seconds. Stale gunfire no longer keeps a holstered visitor attackable."))
	static bool IsRecentAggressiveStealthEvidence(const UObject* WorldContextObject,
		const FTerritoryInfiltrationSnapshot& Snapshot, float WindowSeconds);

	/**
	 * Deterministic half of Is Recent Aggressive Stealth Evidence, exposed for tests and previews.
	 * NowSeconds and LastEvidenceWorldTime must come from the same clock.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Is Recent Aggressive Stealth Evidence At Time"))
	static bool IsRecentAggressiveStealthEvidenceAtTime(
		const FTerritoryInfiltrationSnapshot& Snapshot, float NowSeconds, float WindowSeconds);

	/**
	 * Goal-generator gate for the guard attack goal.
	 *
	 * Call it at the top of a goal generator's "Try Add Attack Goal From Actor" before the parent
	 * implementation runs. It resolves the generator's owning guard, asks that guard's stealth
	 * observer whether this perceived actor must still be ignored (undetected, below the profile's
	 * sight threshold), and returns false in that case. Parent filters (attitude, sense, class)
	 * remain intact because the caller passes their result through bParentAllows.
	 *
	 * Fails open: a generator without a Territory guard, a non-stealth Place, or a missing
	 * observer returns bParentAllows unchanged.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Evaluate Territory Attack Goal Gate",
			ToolTip="True when the parent generator may add its attack goal. False while stealth still hides the target, so the guard investigates instead of attacking."))
	static bool EvaluateAttackGoalGate(UNPCGoalGenerator* Generator,
		AActor* PerceivedActor, bool bParentAllows = true);

	/** True when the pawn's Narrative weapon is in hand (a wield slot is populated), not merely equipped. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Is Pawn Wielding Weapon"))
	static bool IsPawnWieldingWeapon(const AActor* Pawn);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Authored Floors
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * One authored floor's guard read model, the same replicated values the Territory's own
	 * garrison snapshot carries. Returns false when this Territory declares no such floor, so a
	 * widget can tell "no such floor" apart from "a floor holding nothing" - the struct is left
	 * untouched in that case rather than zeroed into a floor that does not exist.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards|Floor",
		meta=(DisplayName="Get Territory Floor Guards"))
	static bool GetTerritoryFloorGuards(const ATerritoryVolume* Territory,
		int32 FloorIndex, FTerritoryFloorSnapshot& OutFloor);

	/**
	 * Whether an authored floor has nothing left to send: a post stands on it, nobody is alive,
	 * nobody is deploying and no reserve remains. A floor between reinforcement waves is not
	 * cleared, and a floor that never held a defender is not cleared either.
	 *
	 * This is the one rule the per-floor story objective and the floor-cleared event both read.
	 * Widgets must call this rather than restating it from the raw counts, so a HUD can never
	 * disagree with the quest that completes on it.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards|Floor",
		meta=(DisplayName="Is Territory Floor Cleared"))
	static bool IsTerritoryFloorCleared(const FTerritoryFloorSnapshot& Floor);

	/**
	 * Whether a floor's counts are complete: every post the Definition authors for it was observed
	 * standing. False means **unknown, not zero** - the floor's post actors are outside the loaded
	 * set, so its counts say nothing about whether anyone is defending it.
	 *
	 * A widget must render this as "unknown" rather than "not cleared". IsTerritoryFloorCleared is
	 * already false whenever this is false, so the two never disagree about a cleared floor; this
	 * exists so the presentation layer can tell an empty floor from an unloaded one.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards|Floor",
		meta=(DisplayName="Are Territory Floor Guard Counts Known"))
	static bool AreTerritoryFloorGuardCountsKnown(const FTerritoryFloorSnapshot& Floor);

	/**
	 * Whether a Territory's whole-garrison counts are complete: every post the Definition authors
	 * was observed standing. False means **unknown, not zero**, and the whole-Place
	 * AllDefendersDefeated objective requires it, so a fully streamed-out Place cannot satisfy a
	 * "defenders defeated" quest it has not fought.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards",
		meta=(DisplayName="Are Territory Guard Counts Known"))
	static bool AreTerritoryGuardCountsKnown(const ATerritoryVolume* Territory);

	// ═══════════════════════════════════════════════════════════════════════════════
	// State-driven Command Capabilities
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Combines capabilities granted by every Territory currently owned by the
	 * faction. Example: a claimed radio District grants Reinforcements; when the
	 * faction loses it, this result no longer contains that capability.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Command Capabilities"))
	static FGameplayTagContainer GetFactionCommandCapabilities(
		const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns currently held Territories that supply the exact capability. */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Command Capability Sources"))
	static TArray<ATerritoryVolume*> GetFactionCommandCapabilitySources(
		const UObject* WorldContextObject, const FGameplayTag& FactionTag,
		const FGameplayTag& Capability);

	/** True when at least one loaded Territory authors this capability in any state. */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Command Capability Used"))
	static bool IsCommandCapabilityUsed(
		const UObject* WorldContextObject, const FGameplayTag& Capability);

	/**
	 * Backward-compatible capability gate. If no Territory authors the capability,
	 * the action keeps its legacy availability. Once a source is authored, the
	 * faction must currently hold an active grant.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(WorldContext="WorldContextObject", DisplayName="Can Faction Use Command Capability"))
	static bool CanFactionUseCommandCapability(
		const UObject* WorldContextObject, const FGameplayTag& FactionTag,
		const FGameplayTag& Capability, FText& OutFailureReason);

	/** Fast check — true if any territory contains the location. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Territory At Location"))
	static bool IsTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Economy Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Deprecated compatibility query. TerritoryFramework does not maintain a faction wallet. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Gold", DeprecatedFunction,
			DeprecationMessage="Use TerritoryEconomy->GetActorCurrency(Requester) instead."))
	static int32 GetFactionGold(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns the sum of periodic income across all territories owned by faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Income"))
	static int32 GetFactionIncome(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns tags of all factions with any territory ownership. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Factions"))
	static TArray<FGameplayTag> GetAllFactions(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Capture Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Read whether the selected Territory is Unclaimed, Contested, or Claimed. This does not change its owner or unlock it. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory State"))
	static ETerritoryState GetTerritoryState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	/** Return current capture progress from 0 to 1. A value of 0.5 means half complete. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Capture Progress"))
	static float GetCaptureProgress(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	/** Loaded actor first, then the existing WorldState directory. False means the row is not ready. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture", meta=(WorldContext="WorldContextObject"))
	static bool TryGetTerritoryCaptureState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag,
		ETerritoryState& OutState, FGameplayTag& OutContestingFaction, float& OutProgress);

	/**
	 * Immediately transfers ownership of territory to faction. Server-only.
	 * Spawns guards for new owner, despawns for old owner.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Force Capture Territory", DevelopmentOnly))
	static void ForceCaptureTerritory(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag, const FGameplayTag& FactionTag);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Diplomacy Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Read the current treaty between two Narrative factions, such as war, peace, or alliance. This does not change their relationship. */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Treaty State"))
	static EDiplomacyState GetTreatyState(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/** True if factions are in Alliance or higher (War=false, Peace=false). */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Allied"))
	static bool IsAllied(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/** True if factions are at War. */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Is At War"))
	static bool IsAtWar(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Utility
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Pure faction equality check. Returns true if both tags have the same GameplayTag value.
	 * No world context needed — purely tag comparison.
	 *
	 * Example:
	 *   if (UTerritoryBlueprintLibrary::IsSameFaction(FactionA, FactionB)) { ... }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Utility", meta=(DisplayName="Is Same Faction"))
	static bool IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/**
	 * Narrative Pro's canonical SetByCaller tag for an additive AttackDamage modifier.
	 * Use this instead of creating a Territory-specific damage tag. The eventual hit
	 * remains governed by Narrative's AttackDamage, AttackRating, Armor, material,
	 * faction-attitude, and friendly-fire rules.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Narrative Pro|Damage",
		meta=(DisplayName="Get Narrative Attack Damage Set By Caller Tag"))
	static FGameplayTag GetNarrativeAttackDamageSetByCallerTag();

	/** Converts a gameplay tag into UI-friendly text without exposing the full hierarchy. */
	UFUNCTION(BlueprintPure, Category="Territory|UI", meta=(DisplayName="Get Friendly Tag Display Name"))
	static FText GetFriendlyTagDisplayName(const FGameplayTag& Tag);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Narrative AI Lifecycle Safety
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * True only while a Narrative goal generator still has a live Activity Component,
	 * possessed controller, and AI Perception Component. This deliberately becomes
	 * false during death, unpossession, streaming removal, and PIE teardown.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|AI|Safety",
		meta=(DisplayName="Can Safely Refresh Perceived Actors"))
	static bool CanSafelyRefreshPerceivedActors(
		const UObject* GoalGenerator, const AAIController* OwnerController);

	/**
	 * Check the current guard or assault rules before scoring a Narrative attack goal.
	 * In ScoreGoalItem, return zero when this is false so boarding, travel, escape,
	 * peace and unloaded targets cannot start combat. Zero keeps the goal available
	 * for later; Narrative still scores weapons, perception and goal lifetime.
	 * Ordinary Narrative NPCs keep their existing combat policy. Server query only.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|AI|Combat",
		meta=(DisplayName="Can Score Territory Combat Goal"))
	static bool CanScoreTerritoryCombatGoal(
		const ANarrativeNPCController* OwnerController, const UNPCGoalItem* Goal);

	/**
	 * Adapter for project-owned children of Narrative's GoalGenerator_Attack.
	 * Call this from the child's RefreshPerceivedActors override. The inherited
	 * implementation runs only when its cached controller and perception component
	 * are safe; otherwise the late callback is ignored without touching Narrative.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|AI|Safety",
		meta=(DisplayName="Refresh Parent Perceived Actors Safely"))
	static bool RefreshParentPerceivedActorsSafely(
		UObject* GoalGenerator, AAIController* OwnerController);

	/**
	 * Update a remote NPC's death presentation from its replicated Narrative ASC.
	 * Use on the Remote branch of a child NPC's HandleDeath override; the Authority
	 * branch calls the Narrative parent. This never changes health, saves, AI goals
	 * or ragdoll state. Narrative's separate ragdoll replication still owns the body.
	 * For late join, also call next tick after BeginPlay and CharacterVisualInitialized.
	 * BP_TerritoryStoryNPC includes these initialization paths.
	 * Returns false for authority actors, missing ASCs or actors being removed.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|AI|Safety",
		meta=(DisplayName="Update Narrative NPC Client Death Presentation"))
	static bool UpdateNarrativeNPCClientDeathPresentation(ANarrativeNPCCharacter* NPC);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Narrative Pro Faction Bridge
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Returns the actor's live Narrative factions, including an intentionally empty
	 * membership. The optional player fallback is only for actors without Narrative's team interface.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Actor Factions"))
	static FGameplayTagContainer GetActorFactions(const UObject* WorldContextObject, AActor* Actor);

	/** True if actor belongs to the specified faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Actor In Faction"))
	static bool IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag);

	/** True for a child of Narrative.Factions. The root tag itself is not a faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions")
	static bool IsNarrativeFactionTag(const FGameplayTag& Tag);

	/** True for a faction identity that can own places, excluding Narrative's universal attitude tags. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions")
	static bool IsPoliticalFactionTag(const FGameplayTag& Tag);

	/** Returns the first political Narrative membership. Set Narrative Player Factions can explicitly put the chosen Primary Faction first; Narrative saves and replicates that order. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Actor Primary Faction"))
	static FGameplayTag GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor);

	/**
	 * Tests whether two Narrative team agents share at least one faction tag, matching
	 * Narrative Pro's UArsenalStatics::IsSameTeam semantics.
	 * No WorldContext needed — uses actor's world internally.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Factions", meta=(DisplayName="Are Actors Allied"))
	static bool AreActorsAllied(AActor* A, AActor* B);

	// ═══════════════════════════════════════════════════════════════════════════════
	// City / District Queries
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Returns all registered city volumes in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Cities"))
	static TArray<ATerritoryCity*> GetAllCities(const UObject* WorldContextObject);

	/** Returns all registered district volumes in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Districts"))
	static TArray<ATerritoryDistrict*> GetAllDistricts(const UObject* WorldContextObject);

	/** Returns the city that contains the given district. Null if district has no parent. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get City For District"))
	static ATerritoryCity* GetCityForDistrict(const UObject* WorldContextObject, ATerritoryDistrict* District);

	/** True if a faction owns all districts in a city (city fully captured). */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Does Faction Control City"))
	static bool DoesFactionControlCity(const UObject* WorldContextObject, ATerritoryCity* City, const FGameplayTag& FactionTag);

	/** Count Cities owned by the exact Narrative faction in the available Territory data. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction City Count"))
	static int32 GetFactionCityCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/**
	 * Current unlocked Claimed Districts, including Definition-backed World
	 * Partition rows. Contested, partial, locked, and unclaimed Districts do not count.
	 *
	 * This is the convenience count, and it answers 0 when it has nothing to count. Code that makes
	 * a gameplay or story decision must use TryGetFactionDistrictCount instead: on a client that has
	 * not received the authoritative directory yet, 0 here means "not known", not "holds none".
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction District Count"))
	static int32 GetFactionDistrictCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/**
	 * Readiness-aware claimed-District count.
	 *
	 * Returns false and leaves OutCount at 0 when the answer is genuinely unknown: a client whose
	 * authoritative strategic directory has not arrived yet. Callers that gate story, capture or
	 * assault decisions must refuse the decision on false rather than compare the unknown value,
	 * because comparing it would read another faction's holdings as empty.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Try Get Faction District Count"))
	static bool TryGetFactionDistrictCount(const UObject* WorldContextObject,
		const FGameplayTag& FactionTag, int32& OutCount);

	/** Returns all districts marked as capitols (bIsCapital=true). */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Capital Districts"))
	static TArray<ATerritoryDistrict*> GetCapitalDistricts(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Dialogue State
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Maps a Territory diplomacy relationship onto the dialogue-state tag that a state machine
	 * (or any other server authority) should apply to an NPC's Ability System. Ambient tagged
	 * dialogue entries read that tag through their Required/Blocked tag filters.
	 *
	 * Read-only: this never changes a treaty or a Narrative attitude.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Get Dialogue State Tag"))
	static FGameplayTag GetDialogueStateTag(EDiplomacyState Relationship, bool bSameFaction);

	/**
	 * Plays one resolved tagged-dialogue line natively on the listener's Tales component - the same
	 * call Narrative's own NPCInteractable makes.
	 *
	 * Narrative's ANarrativeNPCCharacter::ExecutePlayTaggedDialogue is a BlueprintImplementableEvent
	 * with no implementation in this project, so a tagged line that goes through the vendor path would
	 * only be stamped (LastPlayTime) and never heard. Territory NPC Blueprints can forward that event
	 * here, and Territory's own requests fall back to it, so both routes share one playback rule.
	 *
	 * Listener = DialogueInstigator, else the nearest player pawn within the row's MaxDistance
	 * (MaxDistance <= 0 means unlimited). Server-authoritative: returns false on a client, with no
	 * dialogue class, no Tales component, or while that listener is already inside a dialogue.
	 * bKeepMovementDuringLine (default true) keeps the listener moving and fighting: a tagged line is
	 * an in-gameplay bark, so the dialogue must not root the player the way a cutscene does.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Dialogue",
		meta=(DisplayName="Play Tagged Dialogue Line"))
	static bool PlayTaggedDialogueLine(const FTaggedDialogue& Row, AActor* Speaker,
		AActor* DialogueInstigator = nullptr, bool bKeepMovementDuringLine = true);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Debug Helpers
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Prints debug info for one territory to screen + log (owner, state, guards, income). */
	UFUNCTION(BlueprintCallable, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject", DisplayName="Print Territory Debug", DevelopmentOnly))
	static void PrintTerritoryDebug(const UObject* WorldContextObject, ATerritoryVolume* Territory, float Duration = 5.f);

	/** Prints debug info for ALL territories to screen + log. */
	UFUNCTION(BlueprintCallable, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject", DisplayName="Print All Territory Debug", DevelopmentOnly))
	static void PrintAllTerritoryDebug(const UObject* WorldContextObject, float Duration = 5.f);
};

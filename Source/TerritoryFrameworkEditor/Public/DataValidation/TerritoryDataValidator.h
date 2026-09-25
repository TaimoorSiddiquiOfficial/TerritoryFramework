#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GameplayTagContainer.h"
#include "TerritoryDataValidator.generated.h"

class ATerritoryGuardSpawnPoint;
class ATerritoryVolume;
class ULevel;
class UNPCDefinition;
class UWorld;
class UTerritoryCounterAttackProfile;
class UTerritoryProductionProfile;
class UTerritoryDefinition;
class UQuestBlueprint;
class UDialogue;

/**
 * Editor data validator for TerritoryFramework assets.
 * Hooks into Unreal's Data Validation system (Validate Packages / CI).
 *
 * Checks for:
 * - Duplicate territory tags
 * - Missing/invalid territory GUIDs
 * - Duplicate GUIDs
 * - Missing parent territory references
 * - Self-referencing parent (cycle)
 * - Incorrect parent class type (Property→City, District→Property, etc.)
 * - Empty territory tags or display names
 * - Invalid faction prefixes
 * - Multiple TerritoryWorldState/SavableData actors
 * - Negative economy configuration
 * - Invalid production recipes, item classes, quantities, or duplicate rules
 * - Missing bounds shape
 * - Guard config mismatch (BT without NPC definition, etc.)
 * - Duplicate territory display names
 * - Missing parent tag on districts/properties
 * - Orphaned guard spawn points (no parent territory)
 * - Patrol nodes that leave their own Place (guards wandering outside the territory bound)
 * - Guard posts whose authored deployment is blocked, and floors that can never be staffed
 * - City with parent tag set (cities are top-level)
 */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDataValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UTerritoryDataValidator();

	// UEditorValidatorBase interface (UE 5.5+ native validation path).
	virtual bool CanValidateAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) override;

	// Manual validation API (callable from editor utilities)
	static bool ValidateLevel(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static bool ValidateWorld(UWorld* World, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static bool ValidateTerritory(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static bool ValidateDefinition(UTerritoryDefinition* Definition,
		TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static bool ValidateQuest(UQuestBlueprint* QuestBlueprint,
		TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	/** Validate Narrative shot coverage and every referenced Level Sequence binding. */
	static bool ValidateDialogue(UDialogue* Dialogue,
		TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

private:
	static void CheckDuplicateTags(ULevel* Level, TArray<FString>& OutErrors);
	static void CheckDuplicateGUIDs(ULevel* Level, TArray<FString>& OutErrors);
	static void CheckHierarchyIntegrity(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckSingletonActors(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckEconomyConfig(ATerritoryVolume* Territory, TArray<FString>& OutWarnings);
	static void CheckDuplicateDisplayNames(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckGuardConfig(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckCounterAttackConfig(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckBoundsShape(ATerritoryVolume* Territory, TArray<FString>& OutWarnings);
	static void CheckOrphanedSpawnPoints(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckMissingParentTags(ULevel* Level, TArray<FString>& OutWarnings);
	/**
	 * Report placed posts whose serialized Definition binding cannot supply a row.
	 *
	 * BeginPlay calls ApplyTerritoryDefinition, which returns false when the bound Definition has no
	 * row matching the post's Guard Post ID; BeginPlay then logs an error and returns before any
	 * spawn. Such a post therefore produces no guard at all, while still holding a Place, a patrol
	 * route and a guard definition — so no other check sees anything wrong with it.
	 *
	 * Both halves of the binding are serialized, which is what makes this answerable before Play.
	 * It is also the only editor-time route to a post's floor or owner: ApplyTerritoryDefinition is
	 * what writes OwnerTerritoryTag and FloorIndex, so both stay transient.
	 */
	static void CheckGuardPostBindings(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckPatrolContainment(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckGuardDeploymentFeasibility(ULevel* Level, TArray<FString>& OutWarnings);

	/**
	 * Every guard post in a level, with the Place lookups that resolution needs. Built once per
	 * level so the orphan check and both placement checks share one answer to "which Place does
	 * this post belong to" instead of three orderings that can drift apart.
	 */
	struct FSpawnPointPlaceIndex
	{
		TArray<ATerritoryVolume*> Territories;
		TArray<ATerritoryGuardSpawnPoint*> Posts;
		/** Place by stable territory tag, taken from the level's own actors. */
		TMap<FGameplayTag, ATerritoryVolume*> TerritoriesByTag;
		/** Post -> owning territory, for posts listed in a Territory's authored typed array. */
		TMap<const AActor*, ATerritoryVolume*> TypedArrayOwner;
	};

	static FSpawnPointPlaceIndex BuildSpawnPointPlaceIndex(ULevel* Level);

	/** How a guard post came to be (or failed to be) attached to a Place. */
	enum class EPostPlaceResolution : uint8
	{
		/** Listed in the Territory's authored typed array. */
		TypedArray,
		/** Bound by its stable owner tag. */
		OwnerTag,
		/** Bound by placement or patrol overlap with a Place. */
		Containment,
		/** Owner tag names a City or District; guard posts belong to Places only. */
		AggregateOwnerTarget,
		/** Owner tag names no loaded territory. */
		UnresolvedOwnerTag,
		/** No binding and no containment hit. */
		Orphaned
	};

	struct FPostPlaceResolution
	{
		/** Null exactly when Reason is one of the three unresolved cases. */
		ATerritoryVolume* Owner = nullptr;
		EPostPlaceResolution Reason = EPostPlaceResolution::Orphaned;
	};

	/**
	 * Resolve the Place a post belongs to, in the order the runtime uses
	 * (TerritoryGuardSpawnPoint.cpp:414-430): the authored typed array, then the owner tag through
	 * the level's territories, then placement/patrol overlap.
	 *
	 * The owner tag is read twice on purpose. `OwnerTerritoryTag` is transient — it is copied from
	 * the post's bound Definition at BeginPlay (TerritoryGuardSpawnPoint.cpp:282) — so a map that
	 * has never been played may have no tag there at all, while the binding it would be copied from
	 * is serialized. The bound Definition's tag is therefore checked second, and it can only ever
	 * add resolutions: when the transient copy exists the two hold the same value, because the
	 * Definition is what writes it. A valid transient tag that a runtime territory reference
	 * overrode still wins, which is why the transient copy is checked first.
	 *
	 * Pure on purpose. CheckOrphanedSpawnPoints owns every message about a post having no Place,
	 * so this cannot report one itself — if it did, each of the three checks calling it would
	 * repeat the same warning for the same post. Callers that only need the Place ignore Reason.
	 */
	static FPostPlaceResolution ResolvePostTerritory(
		const ATerritoryGuardSpawnPoint* Post, const FSpawnPointPlaceIndex& Index);

	/**
	 * Every guard definition that could spawn this post, following the runtime's cascade
	 * (TerritoryVolume.cpp:3275-3305): the post's inline override, then its guard-post data asset,
	 * then the territory's declarations. Either override wins outright and yields one candidate,
	 * because neither override is faction-dependent.
	 *
	 * The territory's declarations are gathered as *all* of them rather than resolved the way the
	 * runtime resolves them, and that difference is the whole point of this function.
	 * ResolveGuardDefinition(GetOwningFaction()) answers with the entry matching the owning faction,
	 * which is runtime state and is invalid with Play stopped — so it falls through to the Place's
	 * default, which a Place that staffs its floors per faction leaves null on purpose. Resolving
	 * that way reports no guard class for every post on such a Place, and the deployment check then
	 * calls its correctly authored floors unstaffable. It did exactly that on HopDistrictTest, whose
	 * Blacksmith Place declares two per-faction definitions and no default.
	 *
	 * Callers must treat the result as a conjunction: a post is refused only when every candidate
	 * refuses it, because which candidate the runtime picks is not knowable before Play.
	 *
	 * Deliberately not shared with CheckGuardConfig: that check validates each authored definition
	 * on its own terms, which is a different question from which definitions could end up used.
	 */
	static void GatherCandidateGuardDefinitions(
		const ATerritoryVolume* Territory, const ATerritoryGuardSpawnPoint* SpawnPoint,
		TArray<UNPCDefinition*>& OutDefinitions);

	/**
	 * The floor this post is authored on, read from the Definition row it is bound to.
	 *
	 * `ATerritoryGuardSpawnPoint::FloorIndex` is transient — ApplyTerritoryDefinition copies it
	 * from the row at BeginPlay, and its own comment says "The Definition owns the assignment, so
	 * this stays transient" — so on a map that has never been played every post still reads its
	 * default zero. Zero is a *valid* floor, so the stale value is indistinguishable from a real
	 * one and a check reading it would report correctly authored upper floors as unstaffable. The
	 * row is the authority the copy comes from and the binding is serialized, so read the row and
	 * fall back to the post's own value only when there is no row to read.
	 */
	static int32 ResolveAuthoredPostFloor(const ATerritoryGuardSpawnPoint* SpawnPoint);
};

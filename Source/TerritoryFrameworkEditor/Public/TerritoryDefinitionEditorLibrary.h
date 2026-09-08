#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryDefinitionEditorLibrary.generated.h"

class UTerritoryCityDefinition;
class UTerritoryDefinition;
class UGameplayEffect;
class ATerritoryVolume;
class UWorld;
class UBlueprint;

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryDefinitionSyncReport
{
	GENERATED_BODY()

	/** Existing helper actors updated by the editor build operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	int32 UpdatedActors = 0;

	/** Helper actors created by the editor build operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	int32 CreatedActors = 0;

	/** Problems that must be fixed before this operation or asset can be considered valid. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	TArray<FString> Errors;

	/** Non-blocking issues to review before relying on the result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	TArray<FString> Warnings;

	/** Whether the requested operation completed successfully; also inspect warnings and detailed results. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	bool bSucceeded = false;
};

/** Editor-only builder for a complete City -> District -> Place definition tree. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDefinitionEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Copy selected project assets into new Territory plugin packages and update references between the copies. Existing destination assets are never overwritten. Source assets stay unchanged. */
	UFUNCTION(BlueprintCallable, Category="Territory|Content|Editor")
	static bool CopyProjectContentToPlugin(const TMap<FString, FString>& PackageDestinations, FText& OutFailureReason);

	/** Replace a fixed BeginPlay delay with a lifecycle-safe Narrative GameplayHUD gate. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Pro|Editor",
		meta=(DisplayName="Migrate Narrative Controller HUD Readiness"))
	static bool MigrateNarrativeControllerHUDReadiness(
		UBlueprint* ControllerBlueprint,
		FText& OutFailureReason);

	/**
	 * Updates actors already linked by definition/tag and optionally creates missing
	 * Blueprint actors from each template. Runtime state is not changed in PIE.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Definition|Editor",
		meta=(DisplayName="Synchronize Territory City In Current Level"))
	static FTerritoryDefinitionSyncReport SynchronizeCityInCurrentLevel(
		UTerritoryCityDefinition* CityDefinition,
		FTransform NewCityAnchor,
		bool bCreateMissingActors,
		bool bMoveExistingActorsToDefinitionTransforms);

	/**
	 * Rewrites the AttackDamage SetByCaller modifier in a project Gameplay Effect
	 * to Narrative Pro's canonical SetByCaller.AttackDamage tag. This is an
	 * editor migration/helper; it never changes Narrative Pro content.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Pro|Editor",
		meta=(DisplayName="Align Attack Damage Effect With Narrative Pro"))
	static bool AlignAttackDamageEffectWithNarrativePro(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		FText& OutFailureReason);

	/**
	 * Creates or updates a straight ZoneGraph Road spline from one authored
	 * NarrativeVehicle spawn to its drop-off, then rebuilds ZoneGraph data.
	 * Use this for simple unobstructed streets; use normal ZoneShape editing for
	 * curved roads, intersections, bridges, or obstacle avoidance.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Counter Attack|Editor",
		meta=(DisplayName="Ensure Straight Vehicle Approach Road"))
	static bool EnsureStraightVehicleApproachRoad(
		ATerritoryVolume* Territory,
		FName ApproachID,
		FText& OutFailureReason);

	/** DataAsset-first form used by commandlets and unloaded editor workflows. */
	UFUNCTION(BlueprintCallable, Category="Territory|Counter Attack|Editor",
		meta=(DisplayName="Ensure Straight Vehicle Approach Road From Definition"))
	static bool EnsureStraightVehicleApproachRoadFromDefinition(
		UWorld* World,
		UTerritoryDefinition* TerritoryDefinition,
		FName ApproachID,
		FText& OutFailureReason);
};

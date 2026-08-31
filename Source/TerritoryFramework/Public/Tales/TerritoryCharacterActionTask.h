#pragma once

#include "CoreMinimal.h"
#include "NarrativeActorProvider.h"
#include "Tales/QuestTask.h"
#include "TerritoryCharacterActionTask.generated.h"

class ACharacter;

/** Discrete movement and traversal actions missing from Narrative's distance-based Move task. */
UENUM(BlueprintType)
enum class ETerritoryCharacterActionObjective : uint8
{
	Jump UMETA(DisplayName="Jump", ToolTip="Count Narrative Character jump events."),
	ReachJumpApex UMETA(DisplayName="Reach Jump Apex", ToolTip="Count jumps that reach their highest point. The Character Movement Component must enable Notify Apex."),
	Land UMETA(DisplayName="Land", ToolTip="Count valid Character landing events."),
	Crouch UMETA(DisplayName="Crouch", ToolTip="Count transitions from standing to crouched."),
	Uncrouch UMETA(DisplayName="Stand Up", ToolTip="Count transitions from crouched to standing."),
	StartSprint UMETA(DisplayName="Start Sprinting", ToolTip="Count transitions into Narrative sprinting."),
	StopSprint UMETA(DisplayName="Stop Sprinting", ToolTip="Count transitions out of Narrative sprinting."),
	StartSlowWalk UMETA(DisplayName="Start Slow Walking", ToolTip="Count transitions into Narrative slow walk."),
	StopSlowWalk UMETA(DisplayName="Stop Slow Walking", ToolTip="Count transitions out of Narrative slow walk."),
	StartSwimming UMETA(DisplayName="Enter Water and Swim", ToolTip="Count transitions into Unreal swimming movement."),
	StopSwimming UMETA(DisplayName="Leave Swimming", ToolTip="Count transitions out of Unreal swimming movement."),
	StartFalling UMETA(DisplayName="Begin Falling", ToolTip="Count transitions into falling. This includes ledges as well as jumps."),
	StartClimbing UMETA(DisplayName="Start Climbing", ToolTip="Count transitions into Narrative climb movement."),
	StopClimbing UMETA(DisplayName="Stop Climbing", ToolTip="Count transitions out of Narrative climb movement."),
	EnterCover UMETA(DisplayName="Enter Cover", ToolTip="Count Narrative cover-entry events."),
	ExitCover UMETA(DisplayName="Exit Cover", ToolTip="Count Narrative cover-exit events."),
	Hurdle UMETA(DisplayName="Hurdle an Obstacle", ToolTip="Count Narrative Hurdle traversal actions."),
	Mantle UMETA(DisplayName="Mantle an Obstacle", ToolTip="Count Narrative Mantle traversal actions."),
	Vault UMETA(DisplayName="Vault an Obstacle", ToolTip="Count Narrative Vault traversal actions.")
};

/**
 * Community Narrative Task for jump, crouch, sprint, swimming, climbing, cover,
 * and traversal tutorials. Narrative's built-in Move task remains the authority
 * for distance travelled.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Character Movement Action Task",
		ToolTip="Narrative Task that observes real Character and Narrative movement transitions without changing movement."))
class TERRITORYFRAMEWORK_API UTerritoryCharacterActionTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	/** Optional Character provider. Empty follows the pawn that owns this Narrative quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|Subject",
		meta=(ToolTip="Optional Narrative Actor Provider for another Character. Easy example: Find NPC can watch an escort climb. Empty watches the quest player's pawn."))
	TObjectPtr<UNarrativeActorProvider> SubjectProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Movement",
		meta=(ToolTip="Movement action to count. Use Narrative's built-in Move task when the objective is distance travelled."))
	ETerritoryCharacterActionObjective Objective =
		ETerritoryCharacterActionObjective::Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Movement",
		meta=(ToolTip="For positive state objectives such as Crouch, Sprint, Swim, Climb, or Cover, count an already-active state when the task starts. Stop/exit objectives always require a real transition."))
	bool bCountInitialState = false;

	/** Read-only current-state query. Event-only actions such as Jump and Land return false. */
	UFUNCTION(BlueprintPure, Category="Community Task|Preview",
		meta=(ToolTip="Checks the current movement state without moving the Character or changing quest progress."))
	bool IsActionStateSatisfiedBy(const ACharacter* Character) const;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	ACharacter* ResolveCharacter() const;
	void BindCharacter(ACharacter* Character);
	void UnbindCharacter();
	void SnapshotPolledStates(const ACharacter* Character);
	void CountAction();

	UFUNCTION() void HandleProviderActorReady(AActor* Actor);
	UFUNCTION() void HandleJumped();
	UFUNCTION() void HandleReachedJumpApex();
	UFUNCTION() void HandleLanded(const FHitResult& Hit);
	UFUNCTION() void HandleMovementModeChanged(ACharacter* Character,
		EMovementMode PreviousMode, uint8 PreviousCustomMode);
	UFUNCTION() void HandleEnteredCover();
	UFUNCTION() void HandleExitedCover();

	UPROPERTY() TWeakObjectPtr<ACharacter> CachedCharacter;
	bool bPreviousCrouched = false;
	bool bPreviousSprinting = false;
	bool bPreviousSlowWalking = false;
	bool bPreviousTraversalPlaying = false;
};

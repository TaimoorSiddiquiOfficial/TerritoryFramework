#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tales/TalesComponent.h"
#include "TimerManager.h"
#include "TerritoryDialogueLifecycleComponent.generated.h"

class ANarrativePlayerController;

/** Server-side repair for Native's rejected solo-dialogue replacement path.
 * Uses the existing Tales session and reliable exit message; owns no saved or replicated state.
 */
UCLASS(ClassGroup=(Territory), meta=(DisplayName="Territory Dialogue Lifecycle"))
class TERRITORYFRAMEWORK_API UTerritoryDialogueLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTerritoryDialogueLifecycleComponent();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	static UTerritoryDialogueLifecycleComponent* FindOrCreate(ANarrativePlayerController* Controller);

private:
	friend class FTFTerritoryRemoteDialogueLifecycle;
	UPROPERTY(Transient)
	TObjectPtr<UTalesComponent> Tales;
	FTimerHandle ReconcileTimer;
	TWeakObjectPtr<UWorld> TimerWorld;

	void Unbind();
	void CancelReconciliation();
	void ReconcileReplacement();
	UFUNCTION()
	void HandleDialogueBegan(UDialogue* Dialogue);
	UFUNCTION()
	void HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue,
		EExitDialogueReason Reason);
};

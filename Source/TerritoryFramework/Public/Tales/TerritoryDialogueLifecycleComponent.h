#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tales/TalesComponent.h"
#include "TimerManager.h"
#include "TerritoryDialogueLifecycleComponent.generated.h"

class ANarrativePlayerController;
class UQuest;

/** Server-side observer of Native's Tales lifecycle for one Tales component.
 *
 * Two jobs live here on purpose. The first is the original one: a server-side repair for
 * Native's rejected personal or party dialogue replacement, using the existing Tales session and
 * reliable exit message. The second is evidence: a quest that reached its success state and a
 * dialogue that ran out of lines are durable physical facts about the character that owns this
 * Tales component, so they are recorded on the ledger. Both jobs need the same thing - the
 * Native delegate bound on the authority, and the owning actor for attribution - and a second
 * observer component per Tales component would duplicate that plumbing to gain nothing, so the
 * two delegate bindings share one observer.
 *
 * Owns no saved or replicated state: the ledger owns the records, this component only reports.
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
	/** One observer per Native Tales component, including multiple parties on one actor. */
	static UTerritoryDialogueLifecycleComponent* FindOrCreateForTales(UTalesComponent* Source);

private:
	friend class FTFTerritoryRemoteDialogueLifecycle;
	friend class FTFTerritoryPartyDialogueLifecycle;
	TWeakObjectPtr<UTalesComponent> SourceTales;
	UPROPERTY(Transient)
	TObjectPtr<UTalesComponent> Tales;
	FTimerHandle ReconcileTimer;
	TWeakObjectPtr<UWorld> TimerWorld;

	/** What a Tales fact can be attributed to: the owning character's durable identity, when it
	 * has one, and the faction it currently acts for. A player character carries no editor GUID,
	 * so its rows stand on the faction alone and the ledger still accepts them as attributable.
	 */
	struct FTalesFactAttribution
	{
		FGuid SubjectId;
		FGameplayTag InstigatorFaction;
	};

	FTalesFactAttribution ResolveFactAttribution() const;
	void RecordDialogueCompletedEvidence(const UDialogue& Dialogue);
	void RecordQuestCompletedEvidence(const UQuest& Quest);

	void Unbind();
	void CancelReconciliation();
	void ReconcileReplacement();
	UFUNCTION()
	void HandleJoinedParty(UNarrativePartyComponent* NewParty, UNarrativePartyComponent* LeftParty);
	UFUNCTION()
	void HandleDialogueBegan(UDialogue* Dialogue);
	UFUNCTION()
	void HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue,
		EExitDialogueReason Reason);
	UFUNCTION()
	void HandleQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);
};

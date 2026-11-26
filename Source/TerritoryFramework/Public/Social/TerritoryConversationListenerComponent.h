#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tales/Dialogue.h"
#include "TerritoryConversationListenerComponent.generated.h"

class ATerritoryConversationMeetingPoint;
class ATerritoryGuardCharacter;
class UTalesComponent;
class UTerritoryAmbientDialogue;

/** Author this replicated component on the Narrative player controller. It transports exact
 * roles before Native Tales starts; it owns neither story progress nor dialogue playback. */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryConversationListenerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTerritoryConversationListenerComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** Claim this listener for an authored meeting before either guard has walked over. A meeting
	 * stages for up to Profile->MeetingTimeout before Native Tales starts, and during that window
	 * the listener owns no dialogue at all, so every ambient tagged line (a hostile guard's Taunt,
	 * a Territory warning) would otherwise be free to take the listener's Tales component first and
	 * cancel the meeting with "Listener began another dialogue.". Ambient callers must ask
	 * HasPreparedConversation()/IsReserved() before they start a line on this listener. */
	bool Reserve(ATerritoryConversationMeetingPoint* Meeting, const FGuid& Session, double Duration);
	void Unreserve(const FGuid& Session);
	UFUNCTION(BlueprintPure, Category="Territory|Conversation")
	bool IsReserved() const;
	// Not Blueprint-exposed on purpose: UHT rejects const object pointer parameters.
	bool IsReservedFor(const ATerritoryConversationMeetingPoint* Meeting) const;
	bool Prepare(ATerritoryConversationMeetingPoint* Meeting, UTalesComponent* Listener,
		const FGuid& Session, TSubclassOf<UTerritoryAmbientDialogue> Dialogue,
		ATerritoryGuardCharacter* First, ATerritoryGuardCharacter* Second);
	bool BeginPrepared(const FGuid& Session);
	void Release(const FGuid& Session);
	bool ResolvePlayParams(UTalesComponent* Listener, const UClass* DialogueClass, FDialoguePlayParams& Params) const;
	bool BindInitializedDialogue(UTalesComponent* Tales, UTerritoryAmbientDialogue* Dialogue);
	UDialogue* GetPreparedDialogue(const FGuid& Session) const;
	bool IsStartingNativeDialogue(const FGuid& Session) const { return bStartingNativeDialogue && SessionID == Session; }
	static bool IsSupportedListener(UTalesComponent* Listener, const UActorComponent* Adapter);
	UFUNCTION(BlueprintPure, Category="Territory|Conversation")
	bool HasPreparedConversation() const { return SessionID.IsValid(); }
private:
	friend class FTFTerritoryConversationTransport;
	UPROPERTY(Transient) TWeakObjectPtr<ATerritoryConversationMeetingPoint> PendingMeeting;
	UPROPERTY(Transient) TSubclassOf<UTerritoryAmbientDialogue> PreparedClass;
	UPROPERTY(Transient) TObjectPtr<ATerritoryGuardCharacter> FirstGuard;
	UPROPERTY(Transient) TObjectPtr<ATerritoryGuardCharacter> SecondGuard;
	UPROPERTY(Transient) TWeakObjectPtr<UTerritoryAmbientDialogue> PreparedDialogue;
	// Reservation is separate from SessionID on purpose: SessionID means "roles were transported and
	// acknowledged", while a reservation only means "an authored meeting already owns this listener".
	// Prepare() must stay legal while a reservation is held.
	UPROPERTY(Transient) TWeakObjectPtr<ATerritoryConversationMeetingPoint> ReservedMeeting;
	FGuid ReservedSession;
	double ReservedUntil = 0.;
	FGuid SessionID;
	bool bAcknowledged = false;
	bool bStartingNativeDialogue = false;
	double ExpiresAt = 0.;
	UTalesComponent* GetPersonalTales() const;
	void ResetContext();
	UFUNCTION(Client, Reliable)
	void ClientPrepare(FGuid Session, TSubclassOf<UTerritoryAmbientDialogue> Dialogue,
		ATerritoryGuardCharacter* First, ATerritoryGuardCharacter* Second);
	UFUNCTION(Server, Reliable)
	void ServerAcknowledge(FGuid Session, bool bReady);
	UFUNCTION(Client, Reliable)
	void ClientRelease(FGuid Session);
	UFUNCTION(Server, Reliable)
	void ServerAbandon(FGuid Session);
};

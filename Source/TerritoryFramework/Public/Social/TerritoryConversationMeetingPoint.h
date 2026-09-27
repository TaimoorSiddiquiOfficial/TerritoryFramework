#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "TerritoryConversationMeetingPoint.generated.h"

class ATerritoryVolume;
class ATerritoryGuardCharacter;
class UTerritoryConversationProfile;
class UTerritoryConversationGoal;
class UTerritoryConversationListenerComponent;
class UTalesComponent;
class UDialogue;
class UNarrativeAbilitySystemComponent;

UENUM(BlueprintType)
enum class ETerritoryConversationPhase : uint8 { Idle, Staging, PreparingListener, Playing };

/** Server-owned temporary coordination around an authored Place. Never recruits assault goals.
 * Caller supplies the personal Tales listener; there is no global player discovery or autoplay. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryConversationMeetingPoint : public AActor
{
	GENERATED_BODY()
public:
	ATerritoryConversationMeetingPoint();
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Conversation")
	TObjectPtr<ATerritoryVolume> Territory;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation")
	TObjectPtr<UTerritoryConversationProfile> Profile;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation", meta=(MakeEditWidget))
	FVector FirstOffset = FVector(0.f, -120.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation", meta=(MakeEditWidget))
	FVector SecondOffset = FVector(0.f, 120.f, 0.f);
	UPROPERTY(Transient, BlueprintReadOnly, Category="Conversation")
	ETerritoryConversationPhase Phase = ETerritoryConversationPhase::Idle;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Conversation")
	TObjectPtr<ATerritoryGuardCharacter> FirstGuard;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Conversation")
	TObjectPtr<ATerritoryGuardCharacter> SecondGuard;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Conversation")
	FString LastStopReason;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Conversation")
	bool TryStartConversation(UTalesComponent* ExplicitListener, FString& Failure);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Conversation")
	void CancelConversation(const FString& Reason);
	virtual void Tick(float DeltaSeconds) override;
	bool OwnsGoal(const UTerritoryConversationGoal* Goal, const ATerritoryGuardCharacter* Guard) const;
	void ListenerPrepared(const FGuid& Session, bool bReady);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class FTFTerritoryConversationCoordination;
	UPROPERTY(Transient) TObjectPtr<UTerritoryConversationGoal> FirstGoal;
	UPROPERTY(Transient) TObjectPtr<UTerritoryConversationGoal> SecondGoal;
	UPROPERTY(Transient) TWeakObjectPtr<UTalesComponent> Listener;
	UPROPERTY(Transient) TWeakObjectPtr<UTerritoryConversationListenerComponent> ListenerAdapter;
	UPROPERTY(Transient) TWeakObjectPtr<UDialogue> PlayingDialogue;
	FGuid SessionID;
	FGameplayTag OwnerAtStart;
	double Deadline = 0.;
	double NextAllowedTime = 0.;
	bool bCancelling = false;
	bool bStartingConversation = false;
	FString DeferredStopReason;
	bool IsEligible(ATerritoryGuardCharacter* Guard, const FGameplayTagContainer& RequiredTags) const;
	bool IsSessionValid() const;
	bool AddMeetingGoal(ATerritoryGuardCharacter* Guard, const FVector& Location, UTerritoryConversationGoal*& OutGoal);
	void BindGuard(ATerritoryGuardCharacter* Guard, bool bBind);
	UFUNCTION() void OnBeginLoad();
	UFUNCTION() void OnGuardDamaged(UNarrativeAbilitySystemComponent* Source, float Damage, const FGameplayEffectSpec& Spec);
	UFUNCTION() void OnGuardDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
};

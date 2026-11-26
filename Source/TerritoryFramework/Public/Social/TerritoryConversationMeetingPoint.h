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
	/**
	 * Clears combat and interaction trace responses on every primitive component.
	 *
	 * A trigger volume is not geometry. OverlapAllDynamic only overrides the eight built-in channels,
	 * so every custom game trace channel falls back to the engine default of Block. Narrative traces
	 * weapons and projectiles on GameTraceChannel2 and GameTraceChannel6, and a line trace returns the
	 * first BLOCKING hit, so a player standing inside the trigger became the first hit for melee swing
	 * sweeps, firearm traces and projectile lines: every attack registered on the trigger instead of
	 * the enemy behind it. Measured in PIE, a raycast on GameTraceChannel2 from a player inside the
	 * sphere hit `ListenerRange` at distance 0.
	 *
	 * Call this again after adding a primitive component at runtime.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Conversation")
	void ApplyNonCombatCollision();
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
	/** Rate-limits the staging arrival diagnostic so a long walk cannot flood the log. */
	double NextStagingLogTime = 0.;
	/** Rate-limits stalled-walk re-issues and bounds them per guard. */
	double NextNudgeTime = 0.;
	int32 FirstNudgeCount = 0;
	int32 SecondNudgeCount = 0;
	bool bCancelling = false;
	bool bStartingConversation = false;
	FString DeferredStopReason;
	bool IsEligible(ATerritoryGuardCharacter* Guard, const FGameplayTagContainer& RequiredTags) const;
	bool IsSessionValid() const;
	bool AddMeetingGoal(ATerritoryGuardCharacter* Guard, const FVector& Location, UTerritoryConversationGoal*& OutGoal);
	void NudgeStalledWalker(ATerritoryGuardCharacter* Guard, UTerritoryConversationGoal* Goal, int32& InOutCount);
	void BindGuard(ATerritoryGuardCharacter* Guard, bool bBind);
	UFUNCTION() void OnBeginLoad();
	UFUNCTION() void OnGuardDamaged(UNarrativeAbilitySystemComponent* Source, float Damage, const FGameplayEffectSpec& Spec);
	UFUNCTION() void OnGuardDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
};

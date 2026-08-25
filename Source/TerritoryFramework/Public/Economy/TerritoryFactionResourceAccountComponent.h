#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryFactionResourceAccountComponent.generated.h"

/**
 * Routes a faction to an existing Narrative inventory. This component never owns
 * item quantities and can be placed on a persistent player, leader, or depot actor.
 */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Territory Faction Resource Account"))
class TERRITORYFRAMEWORK_API UTerritoryFactionResourceAccountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryFactionResourceAccountComponent();

	/** Must exactly match one of the owning actor's Narrative faction tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources")
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources")
	bool bAutoRegister = true;

	/** Accounts on PlayerControllers may not have a possessed pawn during BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources",
		meta=(ClampMin="0.1", Units="s", EditCondition="bAutoRegister"))
	float RegistrationRetryInterval = 1.f;

	/** Bounds startup retries so a misconfigured account cannot schedule forever. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources",
		meta=(ClampMin="1", EditCondition="bAutoRegister"))
	int32 MaxRegistrationAttempts = 30;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Resources")
	bool RegisterResourceAccount();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Resources")
	void UnregisterResourceAccount();

	UFUNCTION(BlueprintPure, Category="Territory|Resources")
	bool IsResourceAccountRegistered() const { return bRegistered; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bRegistered = false;
	int32 RegistrationAttempts = 0;
	FTimerHandle RegistrationRetryTimer;

	void TryAutoRegister();
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryFactionResourceAccountComponent.generated.h"

class ANarrativeCharacter;
class APawn;

UENUM(BlueprintType)
enum class ETerritoryResourceAccountBinding : uint8
{
	FixedFaction UMETA(DisplayName="Fixed Faction Storage"),
	OwnerPrimaryFaction UMETA(DisplayName="Follow Owner's Political Faction")
};

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

	/** Fixed storage stays with its authored faction. Follow Owner moves the routing when Narrative membership changes; inventory contents are never transferred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources")
	ETerritoryResourceAccountBinding BindingMode = ETerritoryResourceAccountBinding::FixedFaction;

	/** Exact Narrative faction served by fixed storage. Use Follow Owner for a story player who may leave this faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources",
		meta=(Categories="Narrative.Factions", EditCondition="BindingMode == ETerritoryResourceAccountBinding::FixedFaction", EditConditionHides))
	FGameplayTag Faction;

	/** Highest priority wins. Equal priorities block storage until the conflict is resolved. Example: give the faction depot 100 and player accounts 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Resources")
	int32 AccountPriority = 0;

	/** Register this faction resource account with the economy subsystem when the component begins play. */
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

	/** Register this component's eligible Narrative inventory as the faction's resource account on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Resources")
	bool RegisterResourceAccount();

	/** Remove this component's resource-account registration without deleting its inventory. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Resources")
	void UnregisterResourceAccount();

	/** Check whether this component's Narrative inventory is currently registered as faction storage. */
	UFUNCTION(BlueprintPure, Category="Territory|Resources")
	bool IsResourceAccountRegistered() const;

	/** The faction this account currently requests. An empty result means the owner's political faction is not resolved. */
	UFUNCTION(BlueprintPure, Category="Territory|Resources")
	FGameplayTag GetResourceAccountFaction() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Client read model only. The economy's selected account remains authoritative. */
	UPROPERTY(Replicated)
	bool bRegistered = false;
	UPROPERTY(Replicated)
	FGameplayTag RegistrationFaction;
	bool bWantsRegistration = false;
	bool bEndingPlay = false;
	int32 RegistrationAttempts = 0;
	FTimerHandle RegistrationRetryTimer;
	FTimerHandle IdentityObservationTimer;
	TWeakObjectPtr<ANarrativeCharacter> BoundCharacter;
	FGameplayTagContainer ObservedFactions;
	FGameplayTag ObservedDesiredFaction;

	void TryAutoRegister();
	void BindIdentitySources();
	void RefreshRegistrationStatus();
	void RemoveCurrentRegistration();
	void ObserveIdentity();
	UFUNCTION()
	void OnNarrativeIdentityChanged();
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION()
	void OnResourceAccountChanged(FGameplayTag ChangedFaction);
};

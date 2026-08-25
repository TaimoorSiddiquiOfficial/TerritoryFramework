#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "UI/TerritoryLiveEventTypes.h"
#include "TerritoryPlayerManagementComponent.generated.h"

class ATerritoryDistrictManagementPoint;
class ATerritoryDistrict;
class ATerritoryVolume;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTerritoryGuardPurchaseResult,
	ATerritoryVolume*, Territory, bool, bSuccess, FText, Message, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryAssaultNotification,
	const FTerritoryAssaultRecord&, Assault);

/** Owned client-to-server bridge for district management actions. */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryPlayerManagementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryPlayerManagementComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ensures the owned bridge exists on a player controller for framework-only projects. */
	static UTerritoryPlayerManagementComponent* FindOrCreateForPlayerController(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category="Territory|Management")
	FOnTerritoryGuardPurchaseResult OnGuardPurchaseResult;

	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryAssaultNotification OnAssaultNotification;

	/** Owning-client copy of each relevant authoritative assault-state transition. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryCounterHappened OnCounterHappened;

	/** Fired after an authoritative replicated transition is added to this local player's feed. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Live Events")
	FOnTerritoryLiveEventAdded OnLiveEventAdded;

	UPROPERTY(BlueprintAssignable, Category="Territory|Live Events")
	FOnTerritoryLiveEventsChanged OnLiveEventsChanged;

	/** Newest-first session history. Expired entries are muted, then retired after the retention window. */
	UFUNCTION(BlueprintPure, Category="Territory|Live Events")
	TArray<FTerritoryLiveEvent> GetLiveEvents(bool bIncludeExpired = true) const;

	UFUNCTION(BlueprintCallable, Category="Territory|Live Events")
	void ClearExpiredLiveEvents();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(ClampMin="1", ClampMax="200"))
	int32 MaxLiveEventHistory = 40;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(ClampMin="1.0"))
	float LiveEventActiveDuration = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(ClampMin="30.0"))
	float ExpiredEventRetentionDuration = 300.f;

	/** Server-side targeted notification route for this owning controller only. */
	void SendAssaultNotification(const FTerritoryAssaultRecord& Assault);
	void SendCounterHappened(const FTerritoryCounterAttackStateEvent& Event);

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="0"))
	float PurchaseCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="1"))
	int32 MaxGuardPurchaseCount = 10;

	/** Hard RPC input bound; the selected territory still enforces its authored capacity. */
	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="1"))
	int32 MaxGuardTargetCount = 100;

	/** Sets an absolute garrison staffing target through a nearby district command point. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestSetGuardTarget(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount);

	/** Sets an absolute target remotely from the journal for an owned district or child property. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestSetGuardTargetForTerritory(ATerritoryVolume* Territory, int32 NewDesiredGuardCount);

	/**
	 * Deploys existing reserves to an owned garrison without increasing its saved
	 * staffing target. The server rechecks ownership, state, reserve posts, and the
	 * Reinforcements command capability before executing.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestSendReinforcements(ATerritoryVolume* Territory, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	/** Remote management action used by the territory journal for owned districts. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuardsForDistrict(ATerritoryDistrict* District, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestRemoveGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	/** Remote management action used by the territory journal for owned districts. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestRemoveGuardsForDistrict(ATerritoryDistrict* District, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

private:
	UPROPERTY(Transient)
	TArray<FTerritoryLiveEvent> LiveEvents;

	TMap<TWeakObjectPtr<ATerritoryVolume>, ETerritoryState> ObservedTerritoryStates;

	void BindLiveEventSources();
	void UnbindLiveEventSources();
	void BindTerritoryLiveEvents(ATerritoryVolume* Territory);
	void UnbindTerritoryLiveEvents(ATerritoryVolume* Territory);
	void AddLiveEvent(ETerritoryLiveEventType Type, const FGameplayTag& TerritoryTag,
		const FText& Headline, const FText& Detail, bool bCanSetWaypoint,
		float ActiveDuration = -1.f);
	FText ResolveTerritoryName(const FGameplayTag& TerritoryTag) const;
	FGameplayTag ResolveViewerFaction() const;

	UFUNCTION()
	void HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryOwnershipChanged(ATerritoryVolume* Territory,
		FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void HandleTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSetGuardTarget(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	bool ServerRequestSetGuardTarget_Validate(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSetGuardTargetForTerritory(ATerritoryVolume* Territory,
		int32 NewDesiredGuardCount, int32 RequestId);
	bool ServerRequestSetGuardTargetForTerritory_Validate(ATerritoryVolume* Territory,
		int32 NewDesiredGuardCount, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSendReinforcements(ATerritoryVolume* Territory,
		int32 Count, int32 RequestId);
	bool ServerRequestSendReinforcements_Validate(ATerritoryVolume* Territory,
		int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	bool ServerRequestPurchaseGuards_Validate(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPurchaseGuardsForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	bool ServerRequestPurchaseGuardsForDistrict_Validate(ATerritoryDistrict* District, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRemoveGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	bool ServerRequestRemoveGuards_Validate(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRemoveGuardsForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	bool ServerRequestRemoveGuardsForDistrict_Validate(ATerritoryDistrict* District, int32 Count, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveAssaultNotification(const FTerritoryAssaultRecord& Assault);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCounterHappened(const FTerritoryCounterAttackStateEvent& Event);

	void PerformPurchase(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	void PerformPurchaseForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	void PerformRemove(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	void PerformRemoveForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	void PerformSetGuardTarget(ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	void PerformSendReinforcements(ATerritoryVolume* Territory, int32 Count, int32 RequestId);
	void PerformSetGuardTargetAtManagementPoint(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	bool CanManageDistrict(ATerritoryDistrict* District, APawn* Pawn, FText& OutFailureReason) const;
	bool CanManageTerritory(ATerritoryVolume* Territory, APawn* Pawn, FText& OutFailureReason) const;
	bool IsTerritoryManagedByDistrict(ATerritoryDistrict* District, ATerritoryVolume* Territory) const;
	APawn* GetManagingPawn() const;

	float LastPurchaseRequestTime = -BIG_NUMBER;
	int32 NextRequestId = 0;
	int32 LastServerRequestId = 0;
};

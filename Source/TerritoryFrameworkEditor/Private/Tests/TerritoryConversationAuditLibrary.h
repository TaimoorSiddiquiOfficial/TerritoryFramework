#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryConversationAuditLibrary.generated.h"

class ATerritoryConversationMeetingPoint;
class ATerritoryGuardCharacter;
class UTalesComponent;
class AActor;

/** Editor-only audit entry points dispatch outside Python's forced-local RPC scope.
 * A true return means scheduled, not that admission/damage succeeded. */
UCLASS()
class UTerritoryConversationAuditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Read-only legacy NetworkGUID in this actor's PIE NetDriver. Empty means no assigned identity,
	 * invalid/non-PIE actor, or unsupported Iris driver. Never assigns a GUID or changes replication. */
	UFUNCTION(BlueprintPure, Category="Territory|Tests")
	static FString GetActorNetworkIdentityForPIE(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Territory|Tests")
	static bool ScheduleConversationForPIE(ATerritoryConversationMeetingPoint* Meeting, UTalesComponent* ExplicitListener);
	UFUNCTION(BlueprintCallable, Category="Territory|Tests")
	static bool ScheduleConversationCancelForPIE(ATerritoryConversationMeetingPoint* Meeting);
	/** Applies an instant test effect through Native's Damage meta attribute, including invulnerability, Health and real OnDamagedBy/death processing. Never writes saves. */
	UFUNCTION(BlueprintCallable, Category="Territory|Tests")
	static bool ScheduleGuardDamageForPIE(ATerritoryGuardCharacter* Guard, AActor* ExactSource, float Damage);
};

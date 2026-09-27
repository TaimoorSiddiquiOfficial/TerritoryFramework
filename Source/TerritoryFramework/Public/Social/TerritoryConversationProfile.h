#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryConversationProfile.generated.h"

class UTerritoryAmbientDialogue;

/** Two loaded defenders, one explicit personal Tales listener. No persistent session state. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryConversationProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation")
	TSubclassOf<UTerritoryAmbientDialogue> Dialogue;
	/** Optional ASC tags required for each role; faction identity still comes from Narrative. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation")
	FGameplayTagContainer FirstRoleTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Conversation")
	FGameplayTagContainer SecondRoleTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="100", Units="cm"))
	float RecruitRadius = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="100", Units="cm"))
	float ListenerRadius = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="10", Units="cm"))
	float ArrivalRadius = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="1", ClampMax="120", Units="s"))
	float MeetingTimeout = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="1", ClampMax="15", Units="s"))
	float PreparationTimeout = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="1", ClampMax="300", Units="s"))
	float MaximumDialogueDuration = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float Cooldown = 60.f;
	bool Validate(FString& Failure) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

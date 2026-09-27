#pragma once

#include "CoreMinimal.h"
#include "Tales/Dialogue.h"
#include "TerritoryAmbientDialogue.generated.h"

/** Native dialogue with exact meeting roles and an unrestrained, non-speaking listener. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryAmbientDialogue : public UDialogue
{
	GENERATED_BODY()
public:
	UTerritoryAmbientDialogue();
	/** Independent role IDs survive Native's NPC-definition-derived SpeakerID editor normalization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Roles")
	FName FirstRoleID = TEXT("FirstGuard");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Roles")
	FName SecondRoleID = TEXT("SecondGuard");
	UFUNCTION(BlueprintCallable, Category="Territory|Roles")
	void NormalizeMeetingRoles();
	static bool ValidateDialogueClass(TSubclassOf<UTerritoryAmbientDialogue> Class, FString& Failure);
	bool ValidateGraph(const UDialogue* Graph, FString& Failure) const;
	virtual bool Initialize(UTalesComponent* InitializingComp, const FDialoguePlayParams Params) override;
	virtual AActor* LinkSpeakerAvatar_Implementation(const FSpeakerInfo& Speaker, int32 SpeakerIndex) override;
	virtual void InitSpeakerAvatars() override;
	/** Read the exact transported role actors without falling back to a definition lookup. */
	UFUNCTION(BlueprintPure, Category="Territory|Roles")
	AActor* GetMeetingRoleActor(FName RoleID) const;
	/** Explicit story listener, including while Native speaker cleanup temporarily clears its presentation pawn cache. */
	UFUNCTION(BlueprintPure, Category="Territory|Roles")
	APawn* GetListeningPawn() const;
	virtual void OnEndDialogue() override;
	virtual void BlendingOutFinished() override;
	virtual void PlayDialogueSequence(UNarrativeDialogueSequence* Sequence, AActor* Speaker, AActor* Listener) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
	void ApplyAmbientPresentation();
};

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tales/TalesComponent.h"
#include "TerritoryCinematicPresentationSubsystem.generated.h"

class AActor;
class APlayerController;
class UActorComponent;
class UDialogue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTerritoryCinematicPresentationChanged, bool, bIsActive);

/**
 * Local-player presentation bridge between Narrative dialogue and Territory UI.
 * It also pins participant meshes, MetaHuman LODSync, and Groom components to LOD 0
 * for the duration of a dialogue, then restores every previous value exactly.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCinematicPresentationSubsystem
	: public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** Returns the presentation subsystem associated with a local controller. */
	UFUNCTION(BlueprintPure, Category="Territory|Cinematics")
	static UTerritoryCinematicPresentationSubsystem* GetForPlayerController(
		const APlayerController* PlayerController);

	/** True from Narrative OnDialogueBegan through the final OnDialogueFinished. */
	UFUNCTION(BlueprintPure, Category="Territory|Cinematics")
	bool IsNarrativeCinematicActive() const { return ActiveDialogue != nullptr; }

	/** Adds a late-spawned dialogue visual to the temporary quality override. */
	UFUNCTION(BlueprintCallable, Category="Territory|Cinematics")
	void RegisterCinematicSubject(AActor* Subject);

	UPROPERTY(BlueprintAssignable, Category="Territory|Cinematics")
	FOnTerritoryCinematicPresentationChanged OnPresentationChanged;

private:
	enum class EComponentOverrideType : uint8
	{
		LODSync,
		Groom,
		SkinnedMesh
	};

	struct FComponentLODOverride
	{
		TWeakObjectPtr<UActorComponent> Component;
		EComponentOverrideType Type = EComponentOverrideType::SkinnedMesh;
		int32 PreviousLOD = 0;
	};

	UPROPERTY(Transient)
	TObjectPtr<UTalesComponent> BoundTalesComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDialogue> ActiveDialogue;

	TArray<FComponentLODOverride> ComponentLODOverrides;

	void BindToController(APlayerController* PlayerController);
	void UnbindFromTalesComponent();
	void RefreshDialogueSubjects(UDialogue* Dialogue);
	void RestoreComponentLODs();
	bool HasOverrideFor(const UActorComponent* Component) const;

	UFUNCTION()
	void HandleDialogueBegan(UDialogue* Dialogue);

	UFUNCTION()
	void HandleDialogueFinished(UDialogue* Dialogue,
		const bool bStartingNewDialogue, const EExitDialogueReason Reason);

	UFUNCTION()
	void HandleNPCDialogueLineStarted(UDialogue* Dialogue,
		class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine,
		const FSpeakerInfo& Speaker);

	UFUNCTION()
	void HandlePlayerDialogueLineStarted(UDialogue* Dialogue,
		class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);
};

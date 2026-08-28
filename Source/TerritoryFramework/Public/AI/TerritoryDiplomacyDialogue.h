#pragma once

#include "CoreMinimal.h"
#include "AI/NPCInteractable.h"
#include "Components/ActorComponent.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Engine/DataAsset.h"
#include "TerritoryDiplomacyDialogue.generated.h"

class UDialogue;

/**
 * Dialogue choices for one Territory NPC family.
 *
 * Easy example: a Bandit definition can keep DBP_Bandit as its Narrative default,
 * while this profile uses a respectful greeting for Alliance and Trade Agreement.
 * When a betrayal changes the relationship to War, the hostile dialogue is used.
 * Empty slots safely fall back to the Narrative NPCDefinition dialogue.
 */
UCLASS(BlueprintType, meta=(DisplayName="Territory Diplomacy Dialogue Profile"))
class TERRITORYFRAMEWORK_API UTerritoryDiplomacyDialogueProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue",
		meta=(ToolTip="Dialogue for an interactor who shares an exact Narrative faction tag with the NPC."))
	TSubclassOf<UDialogue> SameFactionDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue",
		meta=(ToolTip="Dialogue when no treaty exists. Example: cautious but not hateful."))
	TSubclassOf<UDialogue> NeutralDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> CeasefireDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> NonAggressionDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> TradeAgreementDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> AllianceDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue",
		meta=(ToolTip="Dialogue used while the NPC and interactor factions are at War."))
	TSubclassOf<UDialogue> WarDialogue;

	/** Returns the configured slot, or Default Dialogue when that slot is empty. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Get Diplomacy Dialogue"))
	TSubclassOf<UDialogue> GetDialogueForRelationship(EDiplomacyState Relationship,
		bool bSameFaction, TSubclassOf<UDialogue> DefaultDialogue) const;
};

/** Exact Narrative faction to dialogue-profile mapping for a shared guard class. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryFactionDialogueProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Dialogue",
		meta=(Categories="Narrative.Factions",
			ToolTip="Exact current NPC faction that selects this profile."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Dialogue",
		meta=(ToolTip="Relationship dialogue profile for this exact faction."))
	TObjectPtr<UTerritoryDiplomacyDialogueProfile> DialogueProfile;
};

/**
 * Per-NPC resolver that reads the existing Territory diplomacy subsystem. It does
 * not create another faction database and it does not modify Narrative Pro source.
 */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Territory Diplomacy Dialogue"))
class TERRITORYFRAMEWORK_API UTerritoryDiplomacyDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryDiplomacyDialogueComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Dialogue",
		meta=(ToolTip="Optional fallback profile after exact faction mappings. Leave empty on a guard Blueprint shared by several factions so an unmapped faction keeps its own Narrative NPCDefinition dialogue."))
	TObjectPtr<UTerritoryDiplomacyDialogueProfile> DialogueProfile;

	/**
	 * First exact faction match wins. Easy example: map Bandits to a Bandit profile;
	 * a Hero using the same pawn Blueprint is not matched and keeps the Hero definition dialogue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Dialogue",
		meta=(TitleProperty="Faction",
			ToolTip="Optional per-faction profiles for a pawn class shared by several Narrative NPCDefinitions."))
	TArray<FTerritoryFactionDialogueProfile> FactionDialogueProfiles;

	/** Returns the first exact faction profile, then the optional fallback profile. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Get Dialogue Profile For Owner"))
	UTerritoryDiplomacyDialogueProfile* GetDialogueProfileForOwner() const;

	/**
	 * Resolves the strongest relationship across all exact faction pairs. War wins
	 * over friendly treaties, which prevents a multi-faction actor from receiving
	 * friendly dialogue while any one of its factions is actively hostile.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Resolve Dialogue Relationship"))
	EDiplomacyState ResolveRelationshipForInteractor(const APawn* Interactor,
		bool& bOutSameFaction) const;

	/** Returns the profile choice, or Default Dialogue if no override is available. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Resolve Dialogue For Interactor"))
	TSubclassOf<UDialogue> ResolveDialogueForInteractor(const APawn* Interactor,
		TSubclassOf<UDialogue> DefaultDialogue) const;
};

/**
 * Upgrade-safe Narrative adapter. It selects a dialogue for the specific
 * interacting pawn, then delegates all talk/loot/busy/ragdoll behavior to
 * UNPCInteractable. The selection is restored immediately, avoiding a shared
 * permanent Dialogue mutation when multiplayer players belong to different factions.
 */
UCLASS(meta=(DisplayName="Territory Diplomacy NPC Interactable"))
class TERRITORYFRAMEWORK_API UTerritoryDiplomacyInteractable : public UNPCInteractable
{
	GENERATED_BODY()

public:
	UTerritoryDiplomacyInteractable(const FObjectInitializer& ObjectInitializer);

	virtual bool Interact(APawn* Interactor,
		class UNarrativeInteractionComponent* InteractionComp) override;
	virtual bool CanInteract_Implementation(APawn* Interactor,
		class UNarrativeInteractionComponent* InteractionComp,
		FText& OutErrorText) override;
};

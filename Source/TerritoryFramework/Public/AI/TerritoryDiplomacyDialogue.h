#pragma once

#include "CoreMinimal.h"
#include "AI/NPCInteractable.h"
#include "Components/ActorComponent.h"
#include "Core/TerritoryDialogueTags.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryDiplomacyDialogue.generated.h"

class UAbilitySystemComponent;
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

	/** Dialogue selected during a ceasefire; empty uses the NPC Definition fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> CeasefireDialogue;

	/** Dialogue selected for a non-aggression relationship; empty uses the NPC Definition fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> NonAggressionDialogue;

	/** Dialogue selected for a trade agreement; empty uses the NPC Definition fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> TradeAgreementDialogue;

	/** Dialogue selected for an alliance; empty uses the NPC Definition fallback. */
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

	/** Runtime cache populated exclusively from the owning Territory Definition. */
	UPROPERTY(Transient)
	TObjectPtr<UTerritoryDiplomacyDialogueProfile> DialogueProfile;

	/**
	 * First exact faction match wins. Easy example: map Bandits to a Bandit profile;
	 * a Hero using the same pawn Blueprint is not matched and keeps the Hero definition dialogue.
	 */
	UPROPERTY(Transient)
	TArray<FTerritoryFactionDialogueProfile> FactionDialogueProfiles;

	/** Runtime binding used by Territory Definitions and other DataAsset authorities. */
	void SetDialogueProfiles(UTerritoryDiplomacyDialogueProfile* InFallbackProfile,
		const TArray<FTerritoryFactionDialogueProfile>& InFactionProfiles);

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

	/**
	 * Ambient dialogue state. The same relationship this component already resolves for interaction
	 * dialogue is mirrored onto the owner's Ability System as a `Territory.DialogueState.*` tag, which
	 * is what Narrative's tagged dialogue entries (`FTaggedDialogue::RequiredTags` / `BlockedTags`) read.
	 * Story/state machines ask through here instead of building a second relationship resolver, and a
	 * second diplomacy/faction database is never created.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Dialogue",
		meta=(DisplayName="Apply Dialogue State From Interactor"))
	ETerritoryDialogueState ApplyDialogueStateFromInteractor(const APawn* Interactor);

	/** Server-only. Replaces the previously applied state tag on the owner's Ability System. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Dialogue",
		meta=(DisplayName="Apply Dialogue State"))
	bool ApplyDialogueState(ETerritoryDialogueState NewState);

	/** Server-only. Removes the applied state tag (also runs on EndPlay). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Dialogue",
		meta=(DisplayName="Clear Dialogue State"))
	void ClearDialogueState();

	/** Applied dialogue state. A read-model only; the diplomacy subsystem stays authoritative. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Get Dialogue State"))
	ETerritoryDialogueState GetDialogueState() const { return DialogueState; }

	/** Tag currently applied to the owner, or an invalid tag when nothing is applied. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Get Active Dialogue State Tag"))
	FGameplayTag GetActiveDialogueStateTag() const { return AppliedStateTag; }

	/**
	 * Server-only. Requests a Narrative tagged dialogue on the owning NPC (for example
	 * `Narrative.TaggedDialogue.Greet`). Narrative's own gate still applies: required/blocked tags,
	 * the entry cooldown and the NPC Blueprint's listener check.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Dialogue",
		meta=(DisplayName="Request Tagged Dialogue"))
	bool RequestTaggedDialogue(FGameplayTag Tag, AActor* DialogueInstigator);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Tag currently applied to the owner's Ability System. */
	UPROPERTY(Transient)
	FGameplayTag AppliedStateTag;

	/** Mirrors AppliedStateTag for Blueprint reads. */
	UPROPERTY(Transient)
	ETerritoryDialogueState DialogueState = ETerritoryDialogueState::Neutral;

	/** The owner's Ability System, or null when the owner has none. */
	UAbilitySystemComponent* ResolveOwnerAbilitySystem() const;
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

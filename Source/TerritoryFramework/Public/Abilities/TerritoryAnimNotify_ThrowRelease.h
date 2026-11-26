#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "TerritoryAnimNotify_ThrowRelease.generated.h"

/**
 * Sends a Gameplay Event at an exact frame of a throw montage.
 *
 * Drop it into any montage and pick the tag an ability is waiting for. Nothing here is hard-wired
 * to one ability: the ability supplies the tag it listens to (Throw Release Event Tag), a Blueprint
 * child of that ability can change it, and the same notify asset works for a stone, a bottle or a
 * grenade. Designers get the timing without touching C++, and artists get a different release frame
 * by moving the notify.
 *
 * Server-only throwing abilities (Territory's distraction runs ServerOnly) need the event on the
 * authority only, so bAuthorityOnly defaults to true.
 */
UCLASS(meta = (DisplayName = "Territory Throw Release"))
class TERRITORYFRAMEWORK_API UTerritoryAnimNotify_ThrowRelease final : public UAnimNotify
{
	GENERATED_BODY()

public:
	UTerritoryAnimNotify_ThrowRelease();

	/** Event sent to the owning actor's ability system at this frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Throw",
		meta = (Categories = "GameplayEvent",
			ToolTip = "Event tag the throw ability waits for. Territory's distraction ability listens to GameplayEvent.Distraction.Release by default; a Blueprint child may listen to its own tag and this notify can send that one instead."))
	FGameplayTag EventTag;

	/**
	 * When true the event is only sent by the authority. Keep it on for server-only throw abilities
	 * so a cosmetic client montage cannot spawn a projectile.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Throw",
		meta = (ToolTip = "Send only on the authority. Server-only abilities (Territory distractions) need this; a client-predicted throw would turn it off."))
	bool bAuthorityOnly = true;

	/** Send the event only when the montage is actually playing (skips preview and editor playback). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Throw")
	bool bRequirePlayingMontage = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};

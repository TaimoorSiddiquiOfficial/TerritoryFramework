#include "Abilities/TerritoryAnimNotify_ThrowRelease.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TerritoryStealthTags.h"
#include "GameFramework/Actor.h"

UTerritoryAnimNotify_ThrowRelease::UTerritoryAnimNotify_ThrowRelease()
{
	// Sensible designer default: Territory's distraction ability listens to this tag out of the box.
	EventTag = TerritoryStealthTags::DistractionReleaseEvent;
}

void UTerritoryAnimNotify_ThrowRelease::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !EventTag.IsValid())
	{
		return;
	}
	if (bAuthorityOnly && !Owner->HasAuthority())
	{
		return;
	}
	// Editor preview and sequencer playback should not fire gameplay events for a server-only throw.
	if (bRequirePlayingMontage)
	{
		const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
		if (!AnimInstance || !AnimInstance->IsAnyMontagePlaying())
		{
			return;
		}
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

FString UTerritoryAnimNotify_ThrowRelease::GetNotifyName_Implementation() const
{
	return EventTag.IsValid()
		? FString::Printf(TEXT("Territory Throw Release (%s)"), *EventTag.ToString())
		: TEXT("Territory Throw Release");
}

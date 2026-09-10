#include "AI/TerritoryDeathCollisionState.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

void FTerritoryDeathCollisionState::FComponentSettings::Apply(UPrimitiveComponent* Current)
{
	if (!IsValid(Current)) return;
	if (Component.Get() != Current)
	{
		Restore();
		Component = Current;
		Pawn = Current->GetCollisionResponseToChannel(ECC_Pawn);
		Camera = Current->GetCollisionResponseToChannel(ECC_Camera);
		StepUp = Current->CanCharacterStepUpOn;
	}
	Current->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Current->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Current->CanCharacterStepUpOn = ECB_No;
}

void FTerritoryDeathCollisionState::FComponentSettings::Restore()
{
	if (UPrimitiveComponent* Previous = Component.Get())
	{
		Previous->SetCollisionResponseToChannel(ECC_Pawn, Pawn);
		Previous->SetCollisionResponseToChannel(ECC_Camera, Camera);
		Previous->CanCharacterStepUpOn = StepUp;
	}
	Component.Reset();
}

void FTerritoryDeathCollisionState::Refresh(ANarrativeNPCCharacter& Character)
{
	if (Character.IsActorBeingDestroyed()) return;
	const UNarrativeAbilitySystemComponent* ASC = Character.GetNarrativeAbilitySystemComponent();
	if (!IsValid(ASC)) return;
	if (ASC->IsDead())
	{
		// Leave world collision, physics mode, and Narrative's loot trace unchanged.
		Capsule.Apply(Character.GetCapsuleComponent());
		Mesh.Apply(Character.GetMesh());
	}
	else
	{
		Capsule.Restore();
		Mesh.Restore();
	}
}

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

class ANarrativeNPCCharacter;

/** Local corpse collision settings. Narrative still owns death and ragdoll simulation. */
struct TERRITORYFRAMEWORK_API FTerritoryDeathCollisionState
{
	/** Read the current Native death flag. Restore the exact previous settings on revival. */
	void Refresh(ANarrativeNPCCharacter& Character);

private:
	struct FComponentSettings
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionResponse Pawn = ECR_Ignore;
		ECollisionResponse Camera = ECR_Ignore;
		ECanBeCharacterBase StepUp = ECB_No;

		void Apply(UPrimitiveComponent* Current);
		void Restore();
	};

	// Not campaign state: components and their collision settings are rebuilt on spawn.
	FComponentSettings Capsule;
	FComponentSettings Mesh;
};

#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeEvent.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryCaptureEvent.generated.h"

UENUM(BlueprintType)
enum class ETerritoryCaptureFactionSource : uint8
{
	ExplicitFaction UMETA(DisplayName="Explicit Faction",
		ToolTip="Use Capturing Faction below. Best for a fixed story handover."),
	NarrativeTargetFaction UMETA(DisplayName="Narrative Target / Player Faction",
		ToolTip="Resolve the exact current Narrative faction from the dialogue or quest target. Use this when player choices can change faction."),
	ControllerPawnFaction UMETA(DisplayName="Player Controller Pawn Faction",
		ToolTip="Resolve the exact current Narrative faction from the controller's possessed pawn.")
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryCaptureEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryCaptureEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory"))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag CapturingFaction;

	/** Where the handover gets its new owner. Existing assets keep Explicit Faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	ETerritoryCaptureFactionSource CapturingFactionSource =
		ETerritoryCaptureFactionSource::ExplicitFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	bool bForceCapture = false;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
};

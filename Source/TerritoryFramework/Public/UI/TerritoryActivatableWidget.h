#pragma once

#include "CoreMinimal.h"
#include "NarrativeActivatableWidget.h"
#include "TerritoryActivatableWidget.generated.h"

class APlayerController;
class UWidget;

/** Narrative gameplay-HUD menu base for Territory tabs and modal screens. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryActivatableWidget : public UNarrativeActivatableWidget
{
	GENERATED_BODY()

public:
	UTerritoryActivatableWidget();

	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void CloseTerritoryWidget();

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	APlayerController* GetTerritoryPlayerController() const;

protected:
	/** CommonUI-auditable named focus target. If absent, the first enabled Narrative button is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI|CommonUI")
	FName DesiredFocusTargetName;

	/** Legacy name retained so existing Territory widget defaults migrate without breaking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI|CommonUI")
	FName InitialFocusWidgetName;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;
};

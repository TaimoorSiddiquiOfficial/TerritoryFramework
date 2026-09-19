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

	/** Close this Territory screen through the existing CommonUI screen stack. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void CloseTerritoryWidget();

	/** Return the player controller that owns this Territory widget. */
	UFUNCTION(BlueprintPure, Category="Territory|UI")
	APlayerController* GetTerritoryPlayerController() const;

	/**
	 * True when a widget can actually receive focus on screen.
	 *
	 * A widget is reachable when it is enabled and neither it nor any ancestor is collapsed or
	 * hidden. The ancestor walk is the part that matters: collapsing a panel leaves each child's own
	 * visibility at "Visible", so a check that looks only at the widget itself will happily hand focus
	 * to a button the player cannot see. Easy example: a Waypoint button inside a collapsed detail
	 * pane still reports itself visible, and focusing it leaves the player with nothing highlighted.
	 *
	 * HitTestInvisible and SelfHitTestInvisible are treated as reachable — they block the mouse but
	 * are still drawn, and a gamepad-driven focus path never hit-tests.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|CommonUI")
	static bool IsFocusTargetReachable(const UWidget* Widget);

protected:
	/** CommonUI-auditable named focus target. If absent, the first enabled Narrative button is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI|CommonUI")
	FName DesiredFocusTargetName;

	/** Legacy name retained so existing Territory widget defaults migrate without breaking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI|CommonUI")
	FName InitialFocusWidgetName;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;
};

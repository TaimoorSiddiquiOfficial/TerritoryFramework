#include "UI/TerritoryActivatableWidget.h"

#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/NarrativeCommonButtonBase.h"

UTerritoryActivatableWidget::UTerritoryActivatableWidget()
{
	SetIsFocusable(true);
	bDeactivateOnBack = true;
	bFocusDesiredTargetOnActivate = true;
	InputConfig = ENarrativeWidgetInputMode::Menu;
}

void UTerritoryActivatableWidget::CloseTerritoryWidget()
{
	DeactivateWidget();
}

APlayerController* UTerritoryActivatableWidget::GetTerritoryPlayerController() const
{
	return GetOwningPlayer();
}

bool UTerritoryActivatableWidget::IsFocusTargetReachable(const UWidget* Widget)
{
	if (!Widget || !Widget->GetIsEnabled())
	{
		return false;
	}

	// Collapsed and Hidden take a widget's descendants off the screen. Every other visibility mode
	// is still drawn. The chain is walked because a child keeps its own "Visible" value while an
	// ancestor is collapsed, so asking the widget about itself alone answers the wrong question.
	for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
	{
		const ESlateVisibility CurrentVisibility = Current->GetVisibility();
		if (CurrentVisibility == ESlateVisibility::Collapsed
			|| CurrentVisibility == ESlateVisibility::Hidden)
		{
			return false;
		}
	}

	return true;
}

UWidget* UTerritoryActivatableWidget::NativeGetDesiredFocusTarget() const
{
	const FName ConfiguredFocusTarget = !DesiredFocusTargetName.IsNone()
		? DesiredFocusTargetName
		: InitialFocusWidgetName;

	if (!ConfiguredFocusTarget.IsNone())
	{
		if (UWidget* ExplicitTarget = GetWidgetFromName(ConfiguredFocusTarget))
		{
			if (IsFocusTargetReachable(ExplicitTarget))
			{
				return ExplicitTarget;
			}
		}
	}

	if (WidgetTree)
	{
		TArray<UWidget*> Widgets;
		WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UNarrativeCommonButtonBase* Button = Cast<UNarrativeCommonButtonBase>(Widget))
			{
				if (IsFocusTargetReachable(Button))
				{
					return Button;
				}
			}
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

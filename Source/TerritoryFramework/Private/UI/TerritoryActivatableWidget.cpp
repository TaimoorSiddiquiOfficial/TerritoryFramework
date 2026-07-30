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

UWidget* UTerritoryActivatableWidget::NativeGetDesiredFocusTarget() const
{
	const FName ConfiguredFocusTarget = !DesiredFocusTargetName.IsNone()
		? DesiredFocusTargetName
		: InitialFocusWidgetName;

	if (!ConfiguredFocusTarget.IsNone())
	{
		if (UWidget* ExplicitTarget = GetWidgetFromName(ConfiguredFocusTarget))
		{
			if (ExplicitTarget->GetIsEnabled() && ExplicitTarget->IsVisible())
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
				if (Button->GetIsEnabled() && Button->IsVisible())
				{
					return Button;
				}
			}
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

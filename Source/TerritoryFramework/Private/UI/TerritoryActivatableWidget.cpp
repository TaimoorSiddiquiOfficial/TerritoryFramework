#include "UI/TerritoryActivatableWidget.h"

#include "GameFramework/PlayerController.h"

void UTerritoryActivatableWidget::CloseTerritoryWidget()
{
	DeactivateWidget();
}

APlayerController* UTerritoryActivatableWidget::GetTerritoryPlayerController() const
{
	return GetOwningPlayer();
}

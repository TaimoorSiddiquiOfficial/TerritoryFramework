#include "UI/TerritoryInfoWidget.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTerritoryInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// If a tag was set before construct, resolve and bind
	if (BoundTerritoryTag.IsValid())
	{
		ResolveTerritoryFromTag();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DisplayRefreshTimerHandle, this,
			&UTerritoryInfoWidget::RefreshTerritoryDisplay, 0.5f, true);
	}
}

void UTerritoryInfoWidget::NativeDestruct()
{
	UnbindDelegates();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DisplayRefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UTerritoryInfoWidget::BindToTerritory(const FGameplayTag& TerritoryTag)
{
	UnbindDelegates();
	BoundTerritoryTag = TerritoryTag;
	ResolveTerritoryFromTag();
}

void UTerritoryInfoWidget::BindToTerritoryAtPlayer()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn()) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryAtLocation(PC->GetPawn()->GetActorLocation());
	// P2-N18: Skip rebind if territory hasn't changed
	if (Territory == BoundTerritory.Get()) return;

	if (Territory)
	{
		UnbindDelegates();
		BoundTerritory = Territory;
		BoundTerritoryTag = Territory->GetTerritoryTag();
		BindDelegates();
		RefreshTerritoryDisplay();
		OnTerritoryBound(Territory);
	}
}

void UTerritoryInfoWidget::UnbindFromTerritory()
{
	UnbindDelegates();
	BoundTerritory = nullptr;
	BoundTerritoryTag = FGameplayTag();
}

ATerritoryVolume* UTerritoryInfoWidget::GetBoundTerritory() const
{
	return BoundTerritory.IsValid() ? BoundTerritory.Get() : nullptr;
}

void UTerritoryInfoWidget::ResolveTerritoryFromTag()
{
	UWorld* World = GetWorld();
	if (!World || !BoundTerritoryTag.IsValid()) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(BoundTerritoryTag);
	if (Territory)
	{
		UnbindDelegates();
		BoundTerritory = Territory;
		BindDelegates();
		RefreshTerritoryDisplay();
		OnTerritoryBound(Territory);
	}
}

void UTerritoryInfoWidget::BindDelegates()
{
	if (BoundTerritory.IsValid())
	{
		BoundTerritory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryInfoWidget::HandleControlChanged);
		BoundTerritory->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryInfoWidget::HandleStateChanged);
		BoundTerritory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryInfoWidget::HandleControlChanged);
		BoundTerritory->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryInfoWidget::HandleStateChanged);
	}
}

void UTerritoryInfoWidget::UnbindDelegates()
{
	if (BoundTerritory.IsValid())
	{
		BoundTerritory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryInfoWidget::HandleControlChanged);
		BoundTerritory->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryInfoWidget::HandleStateChanged);
	}
}

void UTerritoryInfoWidget::HandleControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	RefreshTerritoryDisplay();
	OnTerritoryOwnershipChanged(OldOwner, NewOwner);
}

void UTerritoryInfoWidget::HandleStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState)
{
	RefreshTerritoryDisplay();
	OnTerritoryStateChanged(NewState);
}

void UTerritoryInfoWidget::RefreshTerritoryDisplay()
{
	ATerritoryVolume* Territory = GetBoundTerritory();
	if (!Territory) return;

	if (TerritoryNameText) TerritoryNameText->SetText(Territory->GetTerritoryDisplayName());
	if (TerritoryOwnerText)
	{
		TerritoryOwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Territory->GetOwningFaction()));
	}
	if (TerritoryStateText)
	{
		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		TerritoryStateText->SetText(StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(Territory->GetTerritoryState()))
			: FText::GetEmpty());
	}
	if (TerritoryGuardCountText)
	{
		TerritoryGuardCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			Territory->GetSpawnedGuardCount(), Territory->GetMaxGuardCount())));
	}
	if (TerritoryCaptureProgress) TerritoryCaptureProgress->SetPercent(Territory->GetControlProgress());
}

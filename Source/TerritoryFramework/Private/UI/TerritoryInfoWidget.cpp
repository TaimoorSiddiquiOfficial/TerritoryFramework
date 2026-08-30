#include "UI/TerritoryInfoWidget.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

	ATerritoryVolume* SpatialTerritory = Registry->GetTerritoryAtLocation(
		PC->GetPawn()->GetActorLocation());
	ATerritoryVolume* Territory =
		UTerritoryUIBlueprintLibrary::GetVisibleTerritoryAtLocation(
			this, PC->GetPawn()->GetActorLocation());
	// P2-N18: Skip rebind if territory hasn't changed
	if (Territory == BoundTerritory.Get()) return;

	if (Territory)
	{
		if (const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
			Settings && Settings->ShouldDebugUI())
		{
			UE_LOG(LogTerritory, Log,
				TEXT("[UI] %s player location %s selected spatial=%s, visible=%s (%s, political state %d); overlapping candidates=%d"),
				*GetName(), *PC->GetPawn()->GetActorLocation().ToString(),
				SpatialTerritory ? *SpatialTerritory->GetTerritoryTag().ToString()
					: TEXT("None"),
				*Territory->GetTerritoryTag().ToString(),
				Territory->IsLocked() ? TEXT("Locked") : TEXT("Unlocked"),
				static_cast<int32>(Territory->GetTerritoryState()),
				Registry->GetTerritoriesAtLocation(
					PC->GetPawn()->GetActorLocation()).Num());
		}
		UnbindDelegates();
		BoundTerritory = Territory;
		BoundTerritoryTag = Territory->GetTerritoryTag();
		BindDelegates();
		RefreshTerritoryDisplay();
		OnTerritoryBound(Territory);
	}
	else if (BoundTerritory.IsValid())
	{
		UnbindFromTerritory();
	}
}

void UTerritoryInfoWidget::UnbindFromTerritory()
{
	UnbindDelegates();
	BoundTerritory = nullptr;
	BoundTerritoryTag = FGameplayTag();
	ClearTerritoryDisplay();
	OnTerritoryUnbound();
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
	if (!Territory)
	{
		ClearTerritoryDisplay();
		return;
	}

	if (TerritoryNameText) TerritoryNameText->SetText(Territory->GetTerritoryDisplayName());
	if (TerritoryOwnerText)
	{
		TerritoryOwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Territory->GetOwningFaction()));
	}
	if (TerritoryStateText)
	{
		TerritoryStateText->SetText(UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
			Territory->GetTerritoryAvailability(), Territory->GetTerritoryState()));
	}
	if (TerritoryGuardCountText)
	{
		TerritoryGuardCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryInfo", "GuardCounts", "{0} active / {1} assigned / {2} max"),
			FText::AsNumber(Territory->GetSpawnedGuardCount()),
			FText::AsNumber(Territory->GetDesiredGuardCount()),
			FText::AsNumber(Territory->GetMaxGuardCount())));
	}
	if (TerritoryCaptureProgress) TerritoryCaptureProgress->SetPercent(Territory->GetControlProgress());
	bool bProductionTextSet = false;

	if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory))
	{
		FTerritoryDistrictOperationsView View;
		if (UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
			this, District, GetOwningPlayer(), View))
		{
			if (TerritoryAvailabilityText) TerritoryAvailabilityText->SetText(View.AvailabilityReason);
			if (TerritoryThreatText) TerritoryThreatText->SetText(View.ThreatSummary);
			if (TerritoryNetIncomeText)
			{
				TerritoryNetIncomeText->SetText(FText::Format(
					NSLOCTEXT("TerritoryInfo", "NetIncome", "Net per cycle: {0}"),
					FText::AsNumber(View.NetIncome)));
			}
			if (TerritoryProductionText)
			{
				TerritoryProductionText->SetText(FText::Format(
					NSLOCTEXT("TerritoryInfo", "DistrictProduction",
						"Production: {0} active | {1} blocked"),
					FText::AsNumber(View.ProducingSiteCount),
					FText::AsNumber(View.BlockedProductionSiteCount)));
				bProductionTextSet = true;
			}
		}
	}
	if (TerritoryProductionText && !bProductionTextSet)
	{
		FTerritoryProductionSiteOperationsView ProductionView;
		if (UTerritoryUIBlueprintLibrary::BuildProductionSiteOperationsView(
			this, Territory->GetTerritoryTag(), ProductionView))
		{
			TerritoryProductionText->SetText(FText::Format(
				NSLOCTEXT("TerritoryInfo", "PropertyProduction", "Production: {0}"),
				UTerritoryUIBlueprintLibrary::GetProductionStatusText(ProductionView.Status)));
		}
		else
		{
			TerritoryProductionText->SetText(FText::GetEmpty());
		}
	}
}

void UTerritoryInfoWidget::ClearTerritoryDisplay()
{
	if (TerritoryNameText) TerritoryNameText->SetText(FText::GetEmpty());
	if (TerritoryOwnerText) TerritoryOwnerText->SetText(FText::GetEmpty());
	if (TerritoryStateText) TerritoryStateText->SetText(FText::GetEmpty());
	if (TerritoryGuardCountText) TerritoryGuardCountText->SetText(FText::GetEmpty());
	if (TerritoryCaptureProgress) TerritoryCaptureProgress->SetPercent(0.f);
	if (TerritoryAvailabilityText) TerritoryAvailabilityText->SetText(FText::GetEmpty());
	if (TerritoryThreatText) TerritoryThreatText->SetText(FText::GetEmpty());
	if (TerritoryNetIncomeText) TerritoryNetIncomeText->SetText(FText::GetEmpty());
	if (TerritoryProductionText) TerritoryProductionText->SetText(FText::GetEmpty());
}

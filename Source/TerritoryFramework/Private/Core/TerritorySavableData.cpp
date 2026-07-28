#include "Core/TerritorySavableData.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "SaveSystemStatics.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATerritorySavableData::ATerritorySavableData()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATerritorySavableData::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (IsSuppressedByWorldState())
		{
			UE_LOG(LogTerritory, Error, TEXT("ATerritorySavableData %s is disabled because ATerritoryWorldState exists. Remove the deprecated actor."),
				*GetPathName());
			return;
		}
		if (!SavableDataGUID.IsValid())
		{
			SavableDataGUID = FGuid::NewGuid();
		}

		USaveSystemStatics::LoadSingleActor(this);
	}
}

#if WITH_EDITOR
void ATerritorySavableData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!SavableDataGUID.IsValid())
	{
		SavableDataGUID = FGuid::NewGuid();
	}
}

void ATerritorySavableData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// PIE world creation uses StaticDuplicateObject — must NOT regenerate GUID.
	// Only regenerate for actual editor duplication (user Ctrl+D).
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		SavableDataGUID = FGuid::NewGuid();
	}
}
#endif

FGuid ATerritorySavableData::GetActorGUID_Implementation() const { return SavableDataGUID; }
void ATerritorySavableData::SetActorGUID_Implementation(const FGuid& NewGUID) { SavableDataGUID = NewGUID; }
bool ATerritorySavableData::ShouldRespawn_Implementation() const { return false; }

void ATerritorySavableData::PrepareForSave_Implementation()
{
	if (IsSuppressedByWorldState()) return;
	SaveToSelf();
}

void ATerritorySavableData::Load_Implementation()
{
	if (IsSuppressedByWorldState()) return;
	LoadFromSelf();
}

bool ATerritorySavableData::IsSuppressedByWorldState() const
{
	if (!GetWorld()) return false;
	for (TActorIterator<ATerritoryWorldState> It(GetWorld()); It; ++It)
	{
		return true;
	}
	return false;
}

void ATerritorySavableData::SaveToSelf()
{
	if (IsSuppressedByWorldState()) return;
	// ─── Save Economy ───
	SavedTreasuries.Empty();
	if (UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		TArray<FGameplayTag> Factions = Economy->GetAllFactionsWithTreasury();
		for (const FGameplayTag& Faction : Factions)
		{
			SavedTreasuries.Add(Faction, Economy->GetFactionEconomy(Faction));
		}
		SavedTransactions = Economy->GetAllTransactionHistory();
	}

	// ─── Save Diplomacy ───
	if (UTerritoryDiplomacySubsystem* Diplomacy = GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		SavedTreaties = Diplomacy->GetAllTreaties();
		SavedDiplomacyHistory = Diplomacy->GetDiplomacyHistory();

		SavedReputation = Diplomacy->GetAllReputation();
	}
}

void ATerritorySavableData::LoadFromSelf()
{
	if (IsSuppressedByWorldState()) return;
	// ─── Load Economy ───
	if (UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->RestoreTreasuryState(SavedTreasuries);
		Economy->RestoreTransactionHistory(SavedTransactions);
	}

	// ─── Load Diplomacy ───
	if (UTerritoryDiplomacySubsystem* Diplomacy = GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		Diplomacy->RestorePersistentState(SavedTreaties, SavedReputation, SavedDiplomacyHistory);
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritorySavableData loaded: %d treasuries, %d treaties"),
		SavedTreasuries.Num(), SavedTreaties.Num());
}

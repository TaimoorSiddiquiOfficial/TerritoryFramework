#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/TerritoryDossierTypes.h"
#include "TerritoryDossierEntryObject.generated.h"

/**
 * One ledger fact wrapped for a UMG ListView.
 *
 * A UListView drives its rows through UObject items, so the Dossier owns this thin wrapper
 * instead of handing raw structs to the list. The wrapper never owns gameplay state: it holds
 * a copy of the presentation snapshot the Dossier widget already built from the ledger, which
 * keeps UTerritoryEvidenceLedgerSubsystem the single authority for the fact itself (AGENTS.md
 * §4.1) and lets a virtualised list show hundreds of rows without copying the ledger.
 *
 * HasDossierRecord() exists because a list rebuild can leave a stale entry alive for a frame:
 * a row must be able to tell "this entry carries a fact" from "this entry was emptied".
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryDossierEntryObject : public UObject
{
	GENERATED_BODY()

public:
	/** Copy a presentation snapshot into this entry. Called by the owning Dossier widget. */
	void SetDossierRecord(const FTerritoryDossierRecordView& InRecord) { Record = InRecord; }

	/** Presentation snapshot this entry carries, exactly as the ledger handed it over. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	FTerritoryDossierRecordView GetDossierRecord() const { return Record; }

	/**
	 * True when this entry carries a real fact. An entry with no valid evidence tag is empty,
	 * and a row showing one would be inventing history.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	bool HasDossierRecord() const { return Record.EvidenceTag.IsValid(); }

private:
	/**
	 * The snapshot itself. Transient because it is rebuilt from the ledger on every refresh;
	 * only the ledger is persisted (AGENTS.md §7 — saved state stays in its authority).
	 */
	UPROPERTY(Transient)
	FTerritoryDossierRecordView Record;
};

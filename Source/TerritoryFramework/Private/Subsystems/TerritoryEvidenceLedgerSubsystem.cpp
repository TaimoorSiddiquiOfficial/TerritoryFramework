#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

#include "Core/TerritoryTypes.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	/**
	 * A World Subsystem is authoritative in every non-client world. Mirrors the
	 * Territory Diplomacy gate: requiring an AuthGameMode made valid server startup,
	 * streaming, and automation contexts fail silently before a GameMode existed.
	 */
	bool CanMutateEvidence(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client;
	}

	/** Root of the evidence family. A fact must name an observable event, not a feeling. */
	FGameplayTag GetEvidenceFamilyRoot()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Territory.Evidence"), /*ErrorIfNotFound=*/false);
	}
}

FGuid UTerritoryEvidenceLedgerSubsystem::MakeEvidenceContextId(const FString& Key)
{
	// Namespaced so a producer's key can never collide with an unrelated deterministic
	// GUID recipe elsewhere in the plugin.
	return FGuid::NewDeterministicGuid(TEXT("TerritoryFramework.EvidenceContext|") + Key);
}

double UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(const UObject* WorldContextObject)
{
	// Producers that live on actors (not subsystems) still need the same campaign clock the
	// ledger stamps rows with, otherwise a producer's ContextId would use a different time base
	// than the row it is de-duplicated against. One recipe, one answer.
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World) return 0.0;

	const ANarrativeGameState* GameState = Cast<ANarrativeGameState>(World->GetGameState());
	return GameState ? static_cast<double>(GameState->GetAccumulatedTime()) : 0.0;
}

void UTerritoryEvidenceLedgerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EvidenceRecords.Reset();
	bEvidenceLedgerReady = false;
	TotalRecordedEvidence = 0;
	EvictedEvidence = 0;
}

void UTerritoryEvidenceLedgerSubsystem::Deinitialize()
{
	OnEvidenceRecorded.Clear();
	EvidenceRecords.Empty();
	bEvidenceLedgerReady = false;
	Super::Deinitialize();
}

void UTerritoryEvidenceLedgerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A server owns the ledger, so "no facts yet" is a real answer there. A client
	// replica must wait for ATerritoryWorldState to hydrate it; until then an empty
	// read means unknown, not absent (AGENTS.md §4.2).
	if (InWorld.GetNetMode() != NM_Client)
	{
		bEvidenceLedgerReady = true;
	}
}

FTerritoryEvidenceMutationResult UTerritoryEvidenceLedgerSubsystem::RecordEvidence(
	FGameplayTag EvidenceTag,
	FGuid SubjectId,
	FGameplayTag InstigatorFaction,
	FGameplayTag SourceTerritory,
	FGuid ContextId,
	int32 Weight)
{
	FTerritoryEvidenceRecord Evidence;
	Evidence.EvidenceTag = EvidenceTag;
	Evidence.SubjectId = SubjectId;
	Evidence.InstigatorFaction = InstigatorFaction;
	Evidence.SourceTerritory = SourceTerritory;
	Evidence.ContextId = ContextId;
	Evidence.Weight = Weight;
	return RecordEvidenceRecord(Evidence);
}

FTerritoryEvidenceMutationResult UTerritoryEvidenceLedgerSubsystem::RecordEvidenceRecord(
	const FTerritoryEvidenceRecord& Evidence)
{
	FTerritoryEvidenceMutationResult Result;

	if (!CanMutateEvidence(GetWorld()))
	{
		Result.FailureReason = NSLOCTEXT("TerritoryEvidence", "ServerOnly",
			"Evidence can only be recorded by the server.");
		return Result;
	}

	if (!Evidence.EvidenceTag.IsValid())
	{
		Result.FailureReason = NSLOCTEXT("TerritoryEvidence", "MissingTag",
			"Evidence requires a Territory.Evidence.* tag.");
		return Result;
	}

	const FGameplayTag EvidenceRoot = GetEvidenceFamilyRoot();
	if (!EvidenceRoot.IsValid() || !Evidence.EvidenceTag.MatchesTag(EvidenceRoot))
	{
		Result.FailureReason = NSLOCTEXT("TerritoryEvidence", "WrongTagFamily",
			"EvidenceTag must belong to the Territory.Evidence.* family.");
		return Result;
	}

	// An observation nobody is attached to cannot be read back usefully.
	if (!Evidence.SubjectId.IsValid() && !Evidence.InstigatorFaction.IsValid())
	{
		Result.FailureReason = NSLOCTEXT("TerritoryEvidence", "Unattributable",
			"Evidence requires a durable subject or an instigator faction.");
		return Result;
	}

	// Idempotency: a retried commit with the same context is a no-op, and reported as
	// such instead of as a failure so producers can retry safely.
	if (Evidence.ContextId.IsValid())
	{
		const int32 ExistingIndex = FindExistingRecordIndex(Evidence);
		if (ExistingIndex != INDEX_NONE)
		{
			Result.bAlreadyRecorded = true;
			Result.Record = EvidenceRecords[ExistingIndex];
			Result.FailureReason = NSLOCTEXT("TerritoryEvidence", "AlreadyRecorded",
				"This fact is already recorded for this context.");
			return Result;
		}
	}

	FTerritoryEvidenceRecord Committed = Evidence;
	Committed.Weight = FMath::Clamp(Evidence.Weight, 1, FMath::Max(1, MaxEvidenceWeight));
	if (Committed.GameTimeSeconds <= 0.0)
	{
		Committed.GameTimeSeconds = GetCampaignGameTime();
	}

	EvidenceRecords.Add(Committed);
	++TotalRecordedEvidence;
	EnforceRetention();

	Result.bSuccess = true;
	Result.Record = Committed;

	// §4.3 step 7: broadcast only after the commit succeeded.
	UE_LOG(LogTerritory, Verbose, TEXT("[Evidence] broadcasting %s to %d listener(s)"),
		*Committed.EvidenceTag.ToString(), OnEvidenceRecorded.GetAllObjects().Num());
	OnEvidenceRecorded.Broadcast(Committed);
	return Result;
}

void UTerritoryEvidenceLedgerSubsystem::RestorePersistentState(
	const TArray<FTerritoryEvidenceRecord>& InRecords)
{
	EvidenceRecords = InRecords;
	EnforceRetention();

	// A hydrated window is a complete answer for the rows it holds.
	bEvidenceLedgerReady = true;
}

bool UTerritoryEvidenceLedgerSubsystem::IsEvidenceLedgerReady() const
{
	return bEvidenceLedgerReady;
}

TArray<FTerritoryEvidenceRecord> UTerritoryEvidenceLedgerSubsystem::GetEvidenceRecords() const
{
	return EvidenceRecords;
}

TArray<FTerritoryEvidenceRecord> UTerritoryEvidenceLedgerSubsystem::GetEvidenceForSubject(FGuid SubjectId) const
{
	TArray<FTerritoryEvidenceRecord> Matches;
	if (!SubjectId.IsValid())
	{
		return Matches;
	}

	for (const FTerritoryEvidenceRecord& Record : EvidenceRecords)
	{
		if (Record.SubjectId == SubjectId)
		{
			Matches.Add(Record);
		}
	}
	return Matches;
}

TArray<FTerritoryEvidenceRecord> UTerritoryEvidenceLedgerSubsystem::GetEvidenceForTag(FGameplayTag EvidenceTag) const
{
	TArray<FTerritoryEvidenceRecord> Matches;
	if (!EvidenceTag.IsValid())
	{
		return Matches;
	}

	for (const FTerritoryEvidenceRecord& Record : EvidenceRecords)
	{
		if (Record.EvidenceTag == EvidenceTag)
		{
			Matches.Add(Record);
		}
	}
	return Matches;
}

TArray<FTerritoryEvidenceRecord> UTerritoryEvidenceLedgerSubsystem::GetEvidenceForTerritory(FGameplayTag SourceTerritory) const
{
	TArray<FTerritoryEvidenceRecord> Matches;
	if (!SourceTerritory.IsValid())
	{
		return Matches;
	}

	for (const FTerritoryEvidenceRecord& Record : EvidenceRecords)
	{
		if (Record.SourceTerritory == SourceTerritory)
		{
			Matches.Add(Record);
		}
	}
	return Matches;
}

int32 UTerritoryEvidenceLedgerSubsystem::GetEvidenceWeight(FGameplayTag EvidenceTag) const
{
	if (!EvidenceTag.IsValid())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FTerritoryEvidenceRecord& Record : EvidenceRecords)
	{
		if (Record.EvidenceTag == EvidenceTag)
		{
			Total += Record.Weight;
		}
	}
	return Total;
}

int32 UTerritoryEvidenceLedgerSubsystem::GetEvidenceWeightForSubject(FGameplayTag EvidenceTag, FGuid SubjectId) const
{
	if (!EvidenceTag.IsValid() || !SubjectId.IsValid())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FTerritoryEvidenceRecord& Record : EvidenceRecords)
	{
		if (Record.EvidenceTag == EvidenceTag && Record.SubjectId == SubjectId)
		{
			Total += Record.Weight;
		}
	}
	return Total;
}

int32 UTerritoryEvidenceLedgerSubsystem::GetRetainedEvidenceCount() const
{
	return EvidenceRecords.Num();
}

int32 UTerritoryEvidenceLedgerSubsystem::GetEvictedEvidenceCount() const
{
	return EvictedEvidence;
}

int32 UTerritoryEvidenceLedgerSubsystem::GetTotalRecordedEvidenceCount() const
{
	return TotalRecordedEvidence;
}

ANarrativeGameState* UTerritoryEvidenceLedgerSubsystem::GetNarrativeGameState() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	return Cast<ANarrativeGameState>(World->GetGameState());
}

double UTerritoryEvidenceLedgerSubsystem::GetCampaignGameTime() const
{
	return ResolveCampaignGameTime(this);
}

int32 UTerritoryEvidenceLedgerSubsystem::FindExistingRecordIndex(const FTerritoryEvidenceRecord& Evidence) const
{
	for (int32 Index = 0; Index < EvidenceRecords.Num(); ++Index)
	{
		const FTerritoryEvidenceRecord& Existing = EvidenceRecords[Index];
		if (Existing.ContextId == Evidence.ContextId
			&& Existing.EvidenceTag == Evidence.EvidenceTag
			&& Existing.SubjectId == Evidence.SubjectId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UTerritoryEvidenceLedgerSubsystem::EnforceRetention()
{
	const int32 Cap = FMath::Max(1, MaxEvidenceRecords);
	if (EvidenceRecords.Num() <= Cap)
	{
		return;
	}

	// Retention drops the oldest recorded rows. Insertion order, not the smallest
	// timestamp, decides, because a producer may replay a historical fact.
	const int32 Excess = EvidenceRecords.Num() - Cap;
	EvidenceRecords.RemoveAt(0, Excess);
	EvictedEvidence += Excess;

	UE_LOG(LogTerritory, Warning,
		TEXT("[Evidence] Ledger exceeded %d record(s); evicted %d oldest fact(s). Recorded %d, evicted %d."),
		Cap, Excess, TotalRecordedEvidence, EvictedEvidence);
}

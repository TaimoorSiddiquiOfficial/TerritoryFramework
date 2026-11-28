#include "Subsystems/TerritoryCollectibleSubsystem.h"

#include "Core/TerritoryCollectibleDefinition.h"
#include "Core/TerritoryCollectibleTags.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	/**
	 * A World Subsystem is authoritative in every non-client world. Mirrors the
	 * evidence ledger and diplomacy gates: requiring an AuthGameMode made valid server
	 * startup, streaming, and automation contexts fail silently before a GameMode existed.
	 */
	bool CanMutateCollectibles(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client;
	}
}

FGuid UTerritoryCollectibleSubsystem::MakeCollectibleContextId(const FString& Key)
{
	// Namespaced so a producer's key can never collide with an unrelated deterministic
	// GUID recipe elsewhere in the plugin.
	return FGuid::NewDeterministicGuid(TEXT("TerritoryFramework.CollectibleContext|") + Key);
}

void UTerritoryCollectibleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Findings.Reset();
	bFindingsKnown = false;
	TotalRecordedFindings = 0;
	EvictedFindings = 0;
	CachedCollectibleSet = nullptr;
	bCollectibleSetResolved = false;
}

void UTerritoryCollectibleSubsystem::Deinitialize()
{
	OnCollectibleFound.Clear();
	Findings.Empty();
	bFindingsKnown = false;
	CachedCollectibleSet = nullptr;
	bCollectibleSetResolved = false;
	Super::Deinitialize();
}

void UTerritoryCollectibleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A server owns the findings, so "nothing found yet" is a real answer there. A
	// client replica must wait for ATerritoryWorldState to hydrate it; until then an
	// empty read means unknown, not absent (AGENTS.md §4.2).
	if (InWorld.GetNetMode() != NM_Client)
	{
		bFindingsKnown = true;
	}
}

const UTerritoryCollectibleSet* UTerritoryCollectibleSubsystem::ResolveCollectibleSet() const
{
	if (bCollectibleSetResolved)
	{
		return CachedCollectibleSet.Get();
	}

	bCollectibleSetResolved = true;

	if (const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>())
	{
		if (!Settings->CollectibleSet.IsNull())
		{
			// Authored content, resolved once: the Dossier's denominator must not change
			// while the player is looking at the screen.
			CachedCollectibleSet = Settings->CollectibleSet.LoadSynchronous();
		}
	}

	if (!CachedCollectibleSet.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Collectibles] No collectible set is configured in Territory developer settings; ")
			TEXT("the Dossier can list findings but cannot report a campaign total."));
	}

	return CachedCollectibleSet.Get();
}

UTerritoryCollectibleSet* UTerritoryCollectibleSubsystem::GetCollectibleSet() const
{
	return const_cast<UTerritoryCollectibleSet*>(ResolveCollectibleSet());
}

FTerritoryCollectibleMutationResult UTerritoryCollectibleSubsystem::RecordCollectibleFound(
	FGameplayTag CollectibleTag,
	FGameplayTag SourceTerritory,
	FGuid DiscoveredByPlayerId,
	FGuid ContextId)
{
	FTerritoryCollectibleFinding Finding;
	Finding.CollectibleTag = CollectibleTag;
	Finding.SourceTerritory = SourceTerritory;
	Finding.DiscoveredByPlayerId = DiscoveredByPlayerId;
	Finding.ContextId = ContextId;
	return RecordCollectibleFinding(Finding);
}

FTerritoryCollectibleMutationResult UTerritoryCollectibleSubsystem::RecordCollectibleFinding(
	const FTerritoryCollectibleFinding& Finding)
{
	FTerritoryCollectibleMutationResult Result;

	if (!CanMutateCollectibles(GetWorld()))
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectibles", "ServerOnly",
			"Collectible findings can only be recorded by the server.");
		return Result;
	}

	if (!Finding.CollectibleTag.IsValid())
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectibles", "MissingTag",
			"A collectible finding requires a Territory.Collectible.* tag.");
		return Result;
	}

	const FGameplayTag CollectibleRoot = TerritoryCollectibleTags::Collectible.GetTag();
	if (!CollectibleRoot.IsValid() || !Finding.CollectibleTag.MatchesTag(CollectibleRoot))
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectibles", "WrongTagFamily",
			"The collectible tag must belong to the Territory.Collectible.* family.");
		return Result;
	}

	// A collectible is found once per campaign. The tag is the identity: recording the
	// same one twice would inflate the Dossier's counters, so a repeat is reported as
	// already collected instead of as a failure, and producers can retry safely.
	const int32 ExistingIndex = FindExistingFindingIndex(Finding);
	if (ExistingIndex != INDEX_NONE)
	{
		Result.bAlreadyCollected = true;
		Result.Finding = Findings[ExistingIndex];
		Result.FailureReason = NSLOCTEXT("TerritoryCollectibles", "AlreadyCollected",
			"The campaign has already found this collectible.");
		return Result;
	}

	FTerritoryCollectibleFinding Committed = Finding;
	if (Committed.GameTimeSeconds <= 0.0)
	{
		Committed.GameTimeSeconds = GetCampaignGameTime();
	}

	Findings.Add(Committed);
	++TotalRecordedFindings;
	EnforceRetention();

	Result.bSuccess = true;
	Result.Finding = Committed;

	// §4.3 step 7: broadcast only after the commit succeeded.
	UE_LOG(LogTerritory, Verbose, TEXT("[Collectibles] broadcasting %s to %d listener(s)"),
		*Committed.CollectibleTag.ToString(), OnCollectibleFound.GetAllObjects().Num());
	OnCollectibleFound.Broadcast(Committed);
	return Result;
}

void UTerritoryCollectibleSubsystem::RestorePersistentState(
	const TArray<FTerritoryCollectibleFinding>& InFindings)
{
	Findings = InFindings;
	EnforceRetention();

	// A hydrated window is a complete answer for the rows it holds.
	bFindingsKnown = true;
}

bool UTerritoryCollectibleSubsystem::IsCollectibleLedgerReady() const
{
	return bFindingsKnown;
}

bool UTerritoryCollectibleSubsystem::HasCollectibleBeenFound(FGameplayTag CollectibleTag) const
{
	if (!CollectibleTag.IsValid())
	{
		return false;
	}

	for (const FTerritoryCollectibleFinding& Finding : Findings)
	{
		if (Finding.CollectibleTag == CollectibleTag)
		{
			return true;
		}
	}
	return false;
}

bool UTerritoryCollectibleSubsystem::GetCollectibleFinding(
	FGameplayTag CollectibleTag,
	FTerritoryCollectibleFinding& OutFinding) const
{
	OutFinding = FTerritoryCollectibleFinding();
	if (!CollectibleTag.IsValid())
	{
		return false;
	}

	for (const FTerritoryCollectibleFinding& Finding : Findings)
	{
		if (Finding.CollectibleTag == CollectibleTag)
		{
			OutFinding = Finding;
			return true;
		}
	}
	return false;
}

TArray<FTerritoryCollectibleFinding> UTerritoryCollectibleSubsystem::GetCollectibleFindings() const
{
	return Findings;
}

int32 UTerritoryCollectibleSubsystem::GetFindingCount() const
{
	return Findings.Num();
}

int32 UTerritoryCollectibleSubsystem::GetEvictedFindingCount() const
{
	return EvictedFindings;
}

int32 UTerritoryCollectibleSubsystem::GetTotalRecordedFindingCount() const
{
	return TotalRecordedFindings;
}

FTerritoryCollectibleProgress UTerritoryCollectibleSubsystem::GetCollectibleProgress() const
{
	FTerritoryCollectibleProgress Progress;

	const UTerritoryCollectibleSet* Set = ResolveCollectibleSet();

	// Without an authored set there is no honest denominator, and without a hydrated
	// window there is no honest numerator. Both cases report unknown rather than zero,
	// so the UI shows a reason instead of "0 found" (AGENTS.md §4.2).
	if (!bFindingsKnown || !Set)
	{
		Progress.bProgressKnown = false;
		Progress.UnknownTagCount = Findings.Num();
		return Progress;
	}

	TArray<UTerritoryCollectibleDefinition*> Definitions;
	Set->LoadDefinitions(Definitions);

	// Authored tag -> does it count toward completion. One pass, so the denominator and
	// the numerator are read from the same snapshot of the authored set.
	TMap<FGameplayTag, bool> AuthoredCounting;
	int32 CountingTotal = 0;
	for (const UTerritoryCollectibleDefinition* Definition : Definitions)
	{
		if (!Definition || !Definition->CollectibleTag.IsValid())
		{
			continue;
		}

		if (!AuthoredCounting.Contains(Definition->CollectibleTag))
		{
			AuthoredCounting.Add(Definition->CollectibleTag, Definition->bCountsTowardCompletion);
			if (Definition->bCountsTowardCompletion)
			{
				++CountingTotal;
			}
		}
	}

	int32 Found = 0;
	int32 Unknown = 0;
	for (const FTerritoryCollectibleFinding& Finding : Findings)
	{
		const bool* bCounts = AuthoredCounting.Find(Finding.CollectibleTag);
		if (!bCounts)
		{
			++Unknown;
			continue;
		}

		// Bonus entries are found but deliberately do not move the completion counter.
		if (*bCounts)
		{
			++Found;
		}
	}

	Progress.bProgressKnown = true;
	Progress.TotalCount = CountingTotal;
	Progress.FoundCount = Found;
	Progress.UnknownTagCount = Unknown;
	return Progress;
}

TArray<FTerritoryCollectibleView> UTerritoryCollectibleSubsystem::BuildCollectibleViews() const
{
	TArray<FTerritoryCollectibleView> Views;

	const UTerritoryCollectibleSet* Set = ResolveCollectibleSet();
	if (Set)
	{
		TArray<UTerritoryCollectibleDefinition*> Definitions;
		Set->LoadDefinitions(Definitions);

		Views.Reserve(Definitions.Num());
		for (const UTerritoryCollectibleDefinition* Definition : Definitions)
		{
			if (!Definition || !Definition->CollectibleTag.IsValid())
			{
				continue;
			}

			FTerritoryCollectibleView View;
			View.CollectibleTag = Definition->CollectibleTag;
			View.CategoryTag = Definition->CategoryTag;
			View.DisplayName = Definition->DisplayName;
			View.Description = Definition->Description;
			View.Icon = Definition->Icon;

			FTerritoryCollectibleFinding Finding;
			if (GetCollectibleFinding(Definition->CollectibleTag, Finding))
			{
				View.bFound = true;
				View.FoundGameTimeSeconds = Finding.GameTimeSeconds;
				View.FoundTerritory = Finding.SourceTerritory;
			}

			Views.Add(View);
		}
	}

	// Findings outside the authored set still belong to the campaign: a tag that was
	// renamed or retired must not silently disappear from the player's record. They are
	// listed after the authored entries with no presentation data.
	for (const FTerritoryCollectibleFinding& Finding : Findings)
	{
		const bool bAlreadyListed = Views.ContainsByPredicate(
			[&Finding](const FTerritoryCollectibleView& View)
			{
				return View.CollectibleTag == Finding.CollectibleTag;
			});

		if (bAlreadyListed)
		{
			continue;
		}

		FTerritoryCollectibleView View;
		View.CollectibleTag = Finding.CollectibleTag;
		View.bFound = true;
		View.FoundGameTimeSeconds = Finding.GameTimeSeconds;
		View.FoundTerritory = Finding.SourceTerritory;
		Views.Add(View);
	}

	return Views;
}

const TArray<FTerritoryCollectibleFinding>& UTerritoryCollectibleSubsystem::GetPersistentState() const
{
	return Findings;
}

double UTerritoryCollectibleSubsystem::GetCampaignGameTime() const
{
	// Reuse the ledger's recipe rather than inventing a second campaign clock: a finding
	// and a fact stamped in the same frame must agree, and a restored save must keep the
	// same ordering.
	return UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(this);
}

int32 UTerritoryCollectibleSubsystem::FindExistingFindingIndex(const FTerritoryCollectibleFinding& Finding) const
{
	for (int32 Index = 0; Index < Findings.Num(); ++Index)
	{
		if (Findings[Index].CollectibleTag == Finding.CollectibleTag)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UTerritoryCollectibleSubsystem::EnforceRetention()
{
	const int32 Cap = FMath::Max(1, MaxCollectibleFindings);
	if (Findings.Num() <= Cap)
	{
		return;
	}

	// Retention drops the oldest rows. Insertion order, not the smallest timestamp,
	// decides, because a producer may replay a historical finding.
	const int32 Excess = Findings.Num() - Cap;
	Findings.RemoveAt(0, Excess);
	EvictedFindings += Excess;

	UE_LOG(LogTerritory, Warning,
		TEXT("[Collectibles] Findings exceeded %d row(s); evicted %d oldest finding(s). Recorded %d, evicted %d."),
		Cap, Excess, TotalRecordedFindings, EvictedFindings);
}

#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "TimerManager.h"

namespace
{
	FGuid MakeDiplomacyRecordID(const FDiplomacyEvent& Event)
	{
		return FGuid(GetTypeHash(Event.FactionA), GetTypeHash(Event.FactionB),
			GetTypeHash(static_cast<uint8>(Event.EventType)),
			GetTypeHash(Event.GameTime));
	}
}

UTerritoryPlayerManagementComponent::UTerritoryPlayerManagementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryPlayerManagementComponent::BeginPlay()
{
	Super::BeginPlay();
	BindLiveEventSources();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (bDiscoverPlacesOnEnter && PlayerController
		&& (PlayerController->HasAuthority() || PlayerController->IsLocalController())
		&& GetWorld())
	{
		PollTerritoryPOIDiscovery();
		GetWorld()->GetTimerManager().SetTimer(
			PlaceDiscoveryTimer, this,
			&UTerritoryPlayerManagementComponent::PollTerritoryPOIDiscovery,
			FMath::Max(0.10f, PlaceDiscoveryInterval), true);
	}
}

void UTerritoryPlayerManagementComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PlaceDiscoveryTimer);
	}
	LastPlayerPlace = nullptr;
	UnbindLiveEventSources();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryPlayerManagementComponent::PollTerritoryPOIDiscovery()
{
	RefreshTerritoryPOIDiscovery();
}

bool UTerritoryPlayerManagementComponent::RefreshTerritoryPOIDiscovery()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	UNarrativeNavigationComponent* NavigationComponent = PlayerController
		? PlayerController->FindComponentByClass<UNarrativeNavigationComponent>() : nullptr;
	if (!bDiscoverPlacesOnEnter || !PlayerController || !Pawn || !Registry
		|| !NavigationComponent
		|| (!PlayerController->HasAuthority() && !PlayerController->IsLocalController()))
	{
		LastPlayerPlace = nullptr;
		return false;
	}

	ATerritoryProperty* Place = Cast<ATerritoryProperty>(
		Registry->GetTerritoryAtLocation(Pawn->GetActorLocation()));
	if (!Place || !UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(
		PlayerController, Place) || !Place->GetTerritoryTag().IsValid())
	{
		LastPlayerPlace = nullptr;
		return false;
	}

	LastPlayerPlace = Place;
	const FGameplayTag POITag = Place->GetTerritoryTag();
	if (NavigationComponent->HasDiscoveredPOI(POITag)) return false;

	NavigationComponent->DiscoverPOI(POITag);
	return NavigationComponent->HasDiscoveredPOI(POITag);
}

void UTerritoryPlayerManagementComponent::PrepareForSave_Implementation()
{
	const int32 HistoryLimit = FMath::Max(1, MaxLiveEventHistory);
	if (LiveEvents.Num() > HistoryLimit)
	{
		LiveEvents.SetNum(HistoryLimit);
	}
}

void UTerritoryPlayerManagementComponent::Load_Implementation()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	NextIntelligenceSequence = 0;
	for (FTerritoryLiveEvent& Event : LiveEvents)
	{
		if (!Event.EventID.IsValid())
		{
			Event.EventID = FGuid::NewGuid();
		}
		NextIntelligenceSequence = FMath::Max(NextIntelligenceSequence, Event.Sequence);
		// Loaded history belongs in the databank. It must never replay a stale HUD alert.
		Event.CreatedRealTime = Now;
		Event.ActiveDuration = 0.f;
		Event.bExpired = true;
		Event.bWasHUDNotification = false;
	}
	PrepareForSave_Implementation();
	OnLiveEventsChanged.Broadcast();
}

void UTerritoryPlayerManagementComponent::BindLiveEventSources()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	if (UTerritoryEconomySubsystem* Economy =
		World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->OnTransactionRecorded.AddUniqueDynamic(
			this, &UTerritoryPlayerManagementComponent::HandleTransactionRecorded);
		Economy->OnFactionUpkeepDeficit.AddUniqueDynamic(
			this, &UTerritoryPlayerManagementComponent::HandleFactionUpkeepDeficit);
		Economy->OnProductionSettled.AddUniqueDynamic(
			this, &UTerritoryPlayerManagementComponent::HandleProductionSettled);
	}
	if (UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		Diplomacy->OnDiplomacyEvent.AddUniqueDynamic(
			this, &UTerritoryPlayerManagementComponent::HandleDiplomacyEvent);
	}
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return;

	Registry->OnTerritoryRegistered.AddUniqueDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryRegistered);
	Registry->OnTerritoryUnregistered.AddUniqueDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered);
	for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
	{
		BindTerritoryLiveEvents(Territory);
	}
}

void UTerritoryPlayerManagementComponent::UnbindLiveEventSources()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryEconomySubsystem* Economy =
			World->GetSubsystem<UTerritoryEconomySubsystem>())
		{
			Economy->OnTransactionRecorded.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleTransactionRecorded);
			Economy->OnFactionUpkeepDeficit.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleFactionUpkeepDeficit);
			Economy->OnProductionSettled.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleProductionSettled);
		}
		if (UTerritoryDiplomacySubsystem* Diplomacy =
			World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			Diplomacy->OnDiplomacyEvent.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleDiplomacyEvent);
		}
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered);
			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				UnbindTerritoryLiveEvents(Territory);
			}
		}
	}
	ObservedTerritoryStates.Empty();
	ObservedTerritoryCapabilities.Empty();
}

void UTerritoryPlayerManagementComponent::BindTerritoryLiveEvents(
	ATerritoryVolume* Territory)
{
	if (!IsValid(Territory)) return;
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	Territory->OnTerritoryOwnershipChanged.AddDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.AddDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	ObservedTerritoryStates.Add(Territory, Territory->GetTerritoryState());
	ObservedTerritoryCapabilities.Add(Territory,
		Territory->GetActiveCommandCapabilities());
}

void UTerritoryPlayerManagementComponent::UnbindTerritoryLiveEvents(
	ATerritoryVolume* Territory)
{
	if (!IsValid(Territory)) return;
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	ObservedTerritoryStates.Remove(Territory);
	ObservedTerritoryCapabilities.Remove(Territory);
}

void UTerritoryPlayerManagementComponent::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	BindTerritoryLiveEvents(Territory);
}

void UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	UnbindTerritoryLiveEvents(Territory);
}

FGameplayTag UTerritoryPlayerManagementComponent::ResolveViewerFaction() const
{
	return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		this, Cast<APlayerController>(GetOwner()));
}

FText UTerritoryPlayerManagementComponent::ResolveTerritoryName(
	const FGameplayTag& TerritoryTag) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			if (const ATerritoryVolume* Territory =
				Registry->GetTerritoryByTag(TerritoryTag))
			{
				const FText Name = Territory->GetTerritoryDisplayName();
				if (!Name.IsEmpty()) return Name;
			}
		}
	}
	return UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(TerritoryTag);
}

void UTerritoryPlayerManagementComponent::AddLiveEvent(
	ETerritoryLiveEventType Type, const FGameplayTag& TerritoryTag,
	const FText& Headline, const FText& Detail, bool bCanSetWaypoint,
	float ActiveDuration, ETerritoryIntelligenceCategory Category,
	ETerritoryIntelligenceSeverity Severity, FGameplayTag SourceFaction,
	FGameplayTag TargetFaction, FGameplayTagContainer CommandCapabilities,
	int64 IncomeDelta, int64 UpkeepDelta, int64 CurrencyDelta,
	bool bShowHUDNotification, FGuid SourceRecordID)
{
	UWorld* World = GetWorld();
	if (!World || !Cast<APlayerController>(GetOwner())
		|| !Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	for (const FTerritoryLiveEvent& Existing : LiveEvents)
	{
		if (Existing.Type == Type && Existing.TerritoryTag == TerritoryTag
			&& Existing.Headline.EqualTo(Headline)
			&& Now - Existing.CreatedRealTime < 0.75)
		{
			return;
		}
	}

	FTerritoryLiveEvent Event;
	Event.EventID = FGuid::NewGuid();
	Event.SourceRecordID = SourceRecordID;
	Event.Type = Type;
	Event.Category = Category;
	Event.Severity = Severity;
	Event.TerritoryTag = TerritoryTag;
	Event.TerritoryName = ResolveTerritoryName(TerritoryTag);
	Event.Headline = Headline;
	Event.Detail = Detail;
	Event.SourceFaction = SourceFaction;
	Event.TargetFaction = TargetFaction;
	Event.CommandCapabilities = MoveTemp(CommandCapabilities);
	Event.IncomeDelta = IncomeDelta;
	Event.UpkeepDelta = UpkeepDelta;
	Event.CurrencyDelta = CurrencyDelta;
	Event.Sequence = ++NextIntelligenceSequence;
	Event.CreatedRealTime = Now;
	Event.ActiveDuration = ActiveDuration >= 0.f
		? ActiveDuration : LiveEventActiveDuration;
	Event.bCanSetWaypoint = bCanSetWaypoint && TerritoryTag.IsValid();
	Event.bWasHUDNotification = bShowHUDNotification;
	LiveEvents.Insert(Event, 0);
	if (LiveEvents.Num() > FMath::Max(1, MaxLiveEventHistory))
	{
		LiveEvents.SetNum(FMath::Max(1, MaxLiveEventHistory));
	}

	OnLiveEventAdded.Broadcast(Event);
	OnLiveEventsChanged.Broadcast();
	if (bShowHUDNotification && (Severity == ETerritoryIntelligenceSeverity::Warning
		|| Severity == ETerritoryIntelligenceSeverity::Critical
		|| Severity == ETerritoryIntelligenceSeverity::Positive))
	{
		if (const ANarrativePlayerController* NarrativeController =
			Cast<ANarrativePlayerController>(GetOwner()))
		{
			if (UNarrativeGameplayHUD* HUD =
				NarrativeController->GetNarrativeGameplayHUD())
			{
				HUD->ShowNotification(Headline, FMath::Min(Event.ActiveDuration, 6.f));
			}
		}
	}
}

TArray<FTerritoryLiveEvent>
UTerritoryPlayerManagementComponent::GetLiveEvents(bool bIncludeExpired) const
{
	TArray<FTerritoryLiveEvent> Result;
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	for (FTerritoryLiveEvent Event : LiveEvents)
	{
		Event.bExpired = Event.IsExpiredAt(Now);
		if (Event.bExpired && ExpiredEventRetentionDuration >= 0.f
			&& Now - (Event.CreatedRealTime + FMath::Max(0.f, Event.ActiveDuration))
				> ExpiredEventRetentionDuration) continue;
		if (!bIncludeExpired && Event.bExpired) continue;
		Result.Add(MoveTemp(Event));
	}
	return Result;
}

TArray<FTerritoryLiveEvent>
UTerritoryPlayerManagementComponent::GetTerritoryIntelligence(
	ETerritoryIntelligenceFilter Filter, bool bIncludeArchived) const
{
	TArray<FTerritoryLiveEvent> Events = GetLiveEvents(bIncludeArchived);
	if (Filter == ETerritoryIntelligenceFilter::All
		|| Filter == ETerritoryIntelligenceFilter::Economy)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UTerritoryEconomySubsystem* Economy =
				World->GetSubsystem<UTerritoryEconomySubsystem>())
			{
				TSet<FGuid> IncludedRecords;
				for (const FTerritoryLiveEvent& Event : Events)
				{
					if (Event.SourceRecordID.IsValid())
					{
						IncludedRecords.Add(Event.SourceRecordID);
					}
				}
				const FGameplayTag ViewerFaction = ResolveViewerFaction();
				for (const FTerritoryTransaction& Transaction :
					Economy->GetTransactionHistory(ViewerFaction, MaxLiveEventHistory))
				{
					if (!Transaction.TransactionID.IsValid()
						|| IncludedRecords.Contains(Transaction.TransactionID)) continue;
					FTerritoryLiveEvent Event;
					Event.EventID = FGuid::NewGuid();
					Event.SourceRecordID = Transaction.TransactionID;
					Event.Type = Transaction.Amount > 0
						? ETerritoryLiveEventType::IncomeRecorded
						: ETerritoryLiveEventType::ExpenseRecorded;
					Event.Category = ETerritoryIntelligenceCategory::Economy;
					Event.Severity = Transaction.Amount > 0
						? ETerritoryIntelligenceSeverity::Positive
						: ETerritoryIntelligenceSeverity::Information;
					Event.TerritoryTag = Transaction.SourceTerritory;
					Event.TerritoryName = ResolveTerritoryName(Transaction.SourceTerritory);
					Event.SourceFaction = Transaction.Faction;
					Event.CurrencyDelta = Transaction.Amount;
					Event.Headline = FText::Format(Transaction.Amount > 0
						? NSLOCTEXT("TerritoryIntelligence", "ArchivedIncomeHeadline",
							"Income ledger: +{0}")
						: NSLOCTEXT("TerritoryIntelligence", "ArchivedExpenseHeadline",
							"Expense ledger: {0}"), FText::AsNumber(Transaction.Amount));
					Event.Detail = FText::Format(NSLOCTEXT("TerritoryIntelligence",
						"ArchivedTransactionDetail", "{0} Balance after transaction: {1}."),
						Transaction.Reason.IsEmpty()
							? NSLOCTEXT("TerritoryIntelligence", "ArchivedNoReason",
								"No additional reason supplied.")
							: FText::FromString(Transaction.Reason),
						FText::AsNumber(Transaction.BalanceAfter));
					Event.ActiveDuration = 0.f;
					Event.bExpired = true;
					Event.bCanSetWaypoint = Transaction.SourceTerritory.IsValid();
					Events.Add(MoveTemp(Event));
				}
			}
		}
	}
	if (Filter == ETerritoryIntelligenceFilter::All
		|| Filter == ETerritoryIntelligenceFilter::Conflict)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UTerritoryCounterAttackSubsystem* Counterattacks =
				World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
			{
				TSet<FGuid> IncludedRecords;
				for (const FTerritoryLiveEvent& Event : Events)
				{
					if (Event.SourceRecordID.IsValid())
					{
						IncludedRecords.Add(Event.SourceRecordID);
					}
				}
				const FGameplayTag ViewerFaction = ResolveViewerFaction();
				for (const FTerritoryAssaultRecord& Assault :
					Counterattacks->GetPersistentState())
				{
					if (!Assault.AssaultID.IsValid()
						|| IncludedRecords.Contains(Assault.AssaultID)
						|| (Assault.AttackingFaction != ViewerFaction
							&& Assault.DefendingFaction != ViewerFaction)
						|| (!bIncludeArchived && Assault.IsTerminal())) continue;

					FTerritoryLiveEvent Event;
					Event.EventID = FGuid::NewGuid();
					Event.SourceRecordID = Assault.AssaultID;
					Event.Category = ETerritoryIntelligenceCategory::Conflict;
					Event.TerritoryTag = Assault.TargetTerritory;
					Event.TerritoryName = ResolveTerritoryName(Assault.TargetTerritory);
					Event.SourceFaction = Assault.AttackingFaction;
					Event.TargetFaction = Assault.DefendingFaction;
					Event.bCanSetWaypoint = Assault.TargetTerritory.IsValid();
					Event.bExpired = Assault.IsTerminal();
					Event.ActiveDuration = Assault.IsTerminal() ? 0.f : -1.f;
					switch (Assault.State)
					{
					case ETerritoryAssaultState::Active:
						Event.Type = ETerritoryLiveEventType::CounterAttackActive;
						Event.Severity = ETerritoryIntelligenceSeverity::Critical;
						Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
							"ArchivedCounterActive", "Active counterattack: {0}"),
							Event.TerritoryName);
						break;
					case ETerritoryAssaultState::Defeated:
						Event.Type = ETerritoryLiveEventType::CounterAttackDefeated;
						Event.Severity = ETerritoryIntelligenceSeverity::Positive;
						Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
							"ArchivedCounterDefeated", "Counterattack defeated: {0}"),
							Event.TerritoryName);
						break;
					case ETerritoryAssaultState::Succeeded:
						Event.Type = ETerritoryLiveEventType::CounterAttackSucceeded;
						Event.Severity = ETerritoryIntelligenceSeverity::Critical;
						Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
							"ArchivedCounterSucceeded", "Counterattack captured: {0}"),
							Event.TerritoryName);
						break;
					case ETerritoryAssaultState::Cancelled:
						Event.Type = ETerritoryLiveEventType::CounterAttackCancelled;
						Event.Severity = ETerritoryIntelligenceSeverity::Information;
						Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
							"ArchivedCounterCancelled", "Counterattack cancelled: {0}"),
							Event.TerritoryName);
						break;
					default:
						Event.Type = ETerritoryLiveEventType::CounterAttackWarning;
						Event.Severity = ETerritoryIntelligenceSeverity::Warning;
						Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
							"ArchivedCounterPlanned", "Counterattack report: {0}"),
							Event.TerritoryName);
						break;
					}
					Event.Detail = FText::Format(NSLOCTEXT("TerritoryIntelligence",
						"ArchivedCounterDetail",
						"State: {0}. Planned: {1}; alive: {2}; reserve: {3}; killed: {4}; withdrawn: {5}. Resolution: {6}."),
						FText::FromString(UEnum::GetValueAsString(Assault.State)),
						FText::AsNumber(Assault.PlannedForce),
						FText::AsNumber(Assault.AliveForce),
						FText::AsNumber(Assault.PendingReserveForce),
						FText::AsNumber(Assault.KilledForce),
						FText::AsNumber(Assault.WithdrawnForce),
						FText::FromString(UEnum::GetValueAsString(Assault.Resolution)));
					Events.Add(MoveTemp(Event));
				}
			}
		}
	}
	if (Filter == ETerritoryIntelligenceFilter::All
		|| Filter == ETerritoryIntelligenceFilter::Diplomacy)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UTerritoryDiplomacySubsystem* Diplomacy =
				World->GetSubsystem<UTerritoryDiplomacySubsystem>())
			{
				TSet<FGuid> IncludedRecords;
				for (const FTerritoryLiveEvent& Event : Events)
				{
					if (Event.SourceRecordID.IsValid())
					{
						IncludedRecords.Add(Event.SourceRecordID);
					}
				}
				const FGameplayTag ViewerFaction = ResolveViewerFaction();
				for (const FDiplomacyEvent& DiplomacyEvent : Diplomacy->GetDiplomacyHistory())
				{
					const FGuid RecordID = MakeDiplomacyRecordID(DiplomacyEvent);
					if (IncludedRecords.Contains(RecordID)
						|| (DiplomacyEvent.FactionA != ViewerFaction
							&& DiplomacyEvent.FactionB != ViewerFaction)) continue;
					FTerritoryLiveEvent Event;
					Event.EventID = FGuid::NewGuid();
					Event.SourceRecordID = RecordID;
					Event.Type = ETerritoryLiveEventType::DiplomacyChanged;
					Event.Category = ETerritoryIntelligenceCategory::Diplomacy;
					Event.SourceFaction = DiplomacyEvent.FactionA;
					Event.TargetFaction = DiplomacyEvent.FactionB;
					Event.ActiveDuration = 0.f;
					Event.bExpired = true;
					Event.Severity = DiplomacyEvent.EventType == EDiplomacyEventType::DeclaredWar
						|| DiplomacyEvent.EventType == EDiplomacyEventType::BrokeCeasefire
						? ETerritoryIntelligenceSeverity::Critical
						: DiplomacyEvent.EventType == EDiplomacyEventType::BrokeAlliance
							|| DiplomacyEvent.EventType == EDiplomacyEventType::ExpiredTreaty
						? ETerritoryIntelligenceSeverity::Warning
						: ETerritoryIntelligenceSeverity::Positive;
					Event.Headline = FText::Format(NSLOCTEXT("TerritoryIntelligence",
						"ArchivedDiplomacyHeadline", "Diplomacy archive: {0}"),
						FText::FromString(UEnum::GetValueAsString(DiplomacyEvent.EventType)));
					Event.Detail = FText::Format(NSLOCTEXT("TerritoryIntelligence",
						"ArchivedDiplomacyDetail", "{0} / {1}. Territory hostility, capture, and counterattack rules use this relationship."),
						UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(DiplomacyEvent.FactionA),
						UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(DiplomacyEvent.FactionB));
					Events.Add(MoveTemp(Event));
				}
			}
		}
	}
	if (Filter == ETerritoryIntelligenceFilter::All)
	{
		return Events;
	}

	const ETerritoryIntelligenceCategory Category =
		Filter == ETerritoryIntelligenceFilter::Conflict ? ETerritoryIntelligenceCategory::Conflict
		: Filter == ETerritoryIntelligenceFilter::Economy ? ETerritoryIntelligenceCategory::Economy
		: Filter == ETerritoryIntelligenceFilter::Command ? ETerritoryIntelligenceCategory::Command
		: Filter == ETerritoryIntelligenceFilter::Production ? ETerritoryIntelligenceCategory::Production
		: Filter == ETerritoryIntelligenceFilter::Diplomacy ? ETerritoryIntelligenceCategory::Diplomacy
		: ETerritoryIntelligenceCategory::Control;
	TArray<FTerritoryLiveEvent> Filtered;
	for (const FTerritoryLiveEvent& Event : Events)
	{
		if (Event.Category == Category)
		{
			Filtered.Add(Event);
		}
	}
	return Filtered;
}

void UTerritoryPlayerManagementComponent::ClearExpiredLiveEvents()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	const int32 Removed = LiveEvents.RemoveAll(
		[Now](const FTerritoryLiveEvent& Event)
		{
			return Event.IsExpiredAt(Now);
		});
	if (Removed > 0) OnLiveEventsChanged.Broadcast();
}

void UTerritoryPlayerManagementComponent::GetTerritoryEconomicImpact(
	ATerritoryVolume* Territory, int64& OutIncome, int64& OutUpkeep) const
{
	OutIncome = 0;
	OutUpkeep = 0;
	if (!Territory) return;

	if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory))
	{
		FTerritoryDistrictOperationsView View;
		if (UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
			this, District, Cast<APlayerController>(GetOwner()), View))
		{
			OutIncome = View.PeriodicIncome;
			OutUpkeep = View.GuardUpkeep;
			return;
		}
	}
	OutIncome = Territory->GetPeriodicIncome();
	OutUpkeep = static_cast<int64>(Territory->GetGuardCost())
		* Territory->GetDesiredGuardCount();
}

void UTerritoryPlayerManagementComponent::AddCommandCapabilityChanges(
	ATerritoryVolume* Territory,
	const FGameplayTagContainer& PreviousCapabilities,
	const FGameplayTagContainer& CurrentCapabilities,
	bool bViewerGainedOwnership, bool bViewerLostOwnership)
{
	if (!Territory) return;
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	if (!ViewerFaction.IsValid()) return;

	FGameplayTagContainer Gained;
	FGameplayTagContainer Lost;
	TArray<FGameplayTag> Tags;
	CurrentCapabilities.GetGameplayTagArray(Tags);
	for (const FGameplayTag& Capability : Tags)
	{
		if ((bViewerGainedOwnership || !PreviousCapabilities.HasTagExact(Capability))
			&& UTerritoryBlueprintLibrary::GetFactionCommandCapabilitySources(
				this, ViewerFaction, Capability).Num() == 1)
		{
			Gained.AddTag(Capability);
		}
	}
	Tags.Reset();
	PreviousCapabilities.GetGameplayTagArray(Tags);
	for (const FGameplayTag& Capability : Tags)
	{
		if ((bViewerLostOwnership || !CurrentCapabilities.HasTagExact(Capability))
			&& UTerritoryBlueprintLibrary::GetFactionCommandCapabilitySources(
				this, ViewerFaction, Capability).IsEmpty())
		{
			Lost.AddTag(Capability);
		}
	}

	auto FormatCapabilities = [](const FGameplayTagContainer& Capabilities)
	{
		TArray<FGameplayTag> CapabilityTags;
		Capabilities.GetGameplayTagArray(CapabilityTags);
		TArray<FString> Names;
		for (const FGameplayTag& Capability : CapabilityTags)
		{
			Names.Add(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(
				Capability).ToString());
		}
		return FText::FromString(FString::Join(Names, TEXT(", ")));
	};

	if (!Gained.IsEmpty())
	{
		AddLiveEvent(ETerritoryLiveEventType::CommandCapabilityGained,
			Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "PerkGainedHeadline",
				"Strategic control unlocked: {0}"), FormatCapabilities(Gained)),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "PerkGainedDetail",
				"Holding {0} now supplies this faction-wide command. If the Territory is lost or leaves its granting state, the command is removed immediately."),
				Territory->GetTerritoryDisplayName()), true, 45.f,
			ETerritoryIntelligenceCategory::Command,
			ETerritoryIntelligenceSeverity::Positive,
			ViewerFaction, FGameplayTag(), Gained);
	}
	if (!Lost.IsEmpty())
	{
		AddLiveEvent(ETerritoryLiveEventType::CommandCapabilityLost,
			Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "PerkLostHeadline",
				"Strategic control lost: {0}"), FormatCapabilities(Lost)),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "PerkLostDetail",
				"{0} no longer supplies this command and no other held Territory provides it. Recapture or secure another configured source to restore access."),
				Territory->GetTerritoryDisplayName()), true, 60.f,
			ETerritoryIntelligenceCategory::Command,
			ETerritoryIntelligenceSeverity::Critical,
			FGameplayTag(), ViewerFaction, Lost);
	}
}

void UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged(
	ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!Territory || Territory->GetTerritoryState() == ETerritoryState::Locked) return;
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	const FText Name = Territory->GetTerritoryDisplayName();
	const FGameplayTagContainer PreviousCapabilities =
		ObservedTerritoryCapabilities.FindRef(Territory);
	const FGameplayTagContainer CurrentCapabilities =
		Territory->GetActiveCommandCapabilities();
	int64 Income = 0;
	int64 Upkeep = 0;
	GetTerritoryEconomicImpact(Territory, Income, Upkeep);
	if (ViewerFaction.IsValid() && NewOwner == ViewerFaction)
	{
		AddLiveEvent(ETerritoryLiveEventType::Captured, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CapturedHeadline", "{0} captured"), Name),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "CapturedImpactDetail",
				"Your faction now controls this Territory. Recurring impact: +{0} income and +{1} guard upkeep per cycle; net change {2}."),
				FText::AsNumber(Income), FText::AsNumber(Upkeep),
				FText::AsNumber(Income - Upkeep)), true, 45.f,
			ETerritoryIntelligenceCategory::Control,
			ETerritoryIntelligenceSeverity::Positive,
			NewOwner, OldOwner, FGameplayTagContainer(), Income, Upkeep);
		AddCommandCapabilityChanges(Territory, FGameplayTagContainer(),
			CurrentCapabilities, true, false);
	}
	else if (ViewerFaction.IsValid() && OldOwner == ViewerFaction)
	{
		AddLiveEvent(ETerritoryLiveEventType::Lost, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "LostHeadline", "{0} was lost"), Name),
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "LostImpactDetail",
				"Control changed to {0}. Recurring impact: -{1} income and -{2} guard upkeep per cycle; net change {3}. Any unique command perks supplied here are revoked."),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(NewOwner),
				FText::AsNumber(Income), FText::AsNumber(Upkeep),
				FText::AsNumber(-Income + Upkeep)), true, 60.f,
			ETerritoryIntelligenceCategory::Control,
			ETerritoryIntelligenceSeverity::Critical,
			NewOwner, OldOwner, FGameplayTagContainer(), -Income, -Upkeep);
		AddCommandCapabilityChanges(Territory, PreviousCapabilities,
			FGameplayTagContainer(), false, true);
	}
	ObservedTerritoryCapabilities.Add(Territory, CurrentCapabilities);
}

void UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	if (!Territory) return;
	const ETerritoryState PreviousState = ObservedTerritoryStates.FindRef(Territory);
	const FGameplayTagContainer PreviousCapabilities =
		ObservedTerritoryCapabilities.FindRef(Territory);
	const FGameplayTagContainer CurrentCapabilities =
		Territory->GetActiveCommandCapabilities();
	ObservedTerritoryStates.Add(Territory, NewState);
	ObservedTerritoryCapabilities.Add(Territory, CurrentCapabilities);
	const FText Name = Territory->GetTerritoryDisplayName();
	if (PreviousState == ETerritoryState::Locked && NewState != ETerritoryState::Locked)
	{
		AddLiveEvent(ETerritoryLiveEventType::Unlocked, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "UnlockedHeadline", "{0} unlocked"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "UnlockedDetail", "New Territory intel is available. Open Operations or set a waypoint."), true, 30.f,
			ETerritoryIntelligenceCategory::Control,
			ETerritoryIntelligenceSeverity::Information);
	}
	else if (NewState == ETerritoryState::Contested)
	{
		AddLiveEvent(ETerritoryLiveEventType::Contested, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "ContestedHeadline", "{0} is contested"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "ContestedDetail", "Physical attackers are applying capture pressure. Track the Territory to respond."), true, 45.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Critical);
	}
	else if (PreviousState == ETerritoryState::Contested
		&& NewState == ETerritoryState::Claimed)
	{
		AddLiveEvent(ETerritoryLiveEventType::Secured, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "SecuredHeadline", "{0} secured"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "SecuredDetail", "The contest ended and control is stable."), true, 30.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Positive);
	}
	if (Territory->GetOwningFaction() == ResolveViewerFaction())
	{
		AddCommandCapabilityChanges(Territory, PreviousCapabilities,
			CurrentCapabilities, false, false);
	}
}

void UTerritoryPlayerManagementComponent::HandleTransactionRecorded(
	const FTerritoryTransaction& Transaction)
{
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	if (!ViewerFaction.IsValid() || Transaction.Faction != ViewerFaction) return;

	const bool bIncome = Transaction.Amount > 0;
	const FText TerritoryName = Transaction.SourceTerritory.IsValid()
		? ResolveTerritoryName(Transaction.SourceTerritory)
		: NSLOCTEXT("TerritoryIntelligence", "FactionAccount", "Faction account");
	const FText Reason = Transaction.Reason.IsEmpty()
		? NSLOCTEXT("TerritoryIntelligence", "NoTransactionReason", "No additional reason supplied.")
		: FText::FromString(Transaction.Reason);
	AddLiveEvent(
		bIncome ? ETerritoryLiveEventType::IncomeRecorded
			: ETerritoryLiveEventType::ExpenseRecorded,
		Transaction.SourceTerritory,
		FText::Format(bIncome
			? NSLOCTEXT("TerritoryIntelligence", "IncomeHeadline", "Income recorded: +{0}")
			: NSLOCTEXT("TerritoryIntelligence", "ExpenseHeadline", "Expense recorded: {0}"),
			FText::AsNumber(Transaction.Amount)),
		FText::Format(NSLOCTEXT("TerritoryIntelligence", "TransactionDetail",
			"{0} • {1} Current balance: {2}."),
			TerritoryName, Reason, FText::AsNumber(Transaction.BalanceAfter)),
		Transaction.SourceTerritory.IsValid(), 12.f,
		ETerritoryIntelligenceCategory::Economy,
		bIncome ? ETerritoryIntelligenceSeverity::Positive
			: ETerritoryIntelligenceSeverity::Information,
		ViewerFaction, FGameplayTag(), FGameplayTagContainer(), 0, 0,
		Transaction.Amount, false, Transaction.TransactionID);
}

void UTerritoryPlayerManagementComponent::HandleFactionUpkeepDeficit(
	FGameplayTag Faction, int32 Deficit)
{
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	if (!ViewerFaction.IsValid() || Faction != ViewerFaction || Deficit <= 0) return;
	AddLiveEvent(ETerritoryLiveEventType::UpkeepDeficit, FGameplayTag(),
		NSLOCTEXT("TerritoryIntelligence", "UpkeepDeficitHeadline",
			"Guard upkeep could not be paid"),
		FText::Format(NSLOCTEXT("TerritoryIntelligence", "UpkeepDeficitDetail",
			"The faction account is short by {0}. Review unprofitable Districts, reduce staffing, or secure new income before the next cycle."),
			FText::AsNumber(Deficit)), false, 60.f,
		ETerritoryIntelligenceCategory::Economy,
		ETerritoryIntelligenceSeverity::Critical,
		ViewerFaction, FGameplayTag(), FGameplayTagContainer(), 0, 0,
		-Deficit, true);
}

void UTerritoryPlayerManagementComponent::HandleProductionSettled(
	const FTerritoryProductionResult& Result)
{
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	if (!ViewerFaction.IsValid() || Result.Faction != ViewerFaction) return;
	int32 InputCount = 0;
	int32 OutputCount = 0;
	for (const FTerritoryResourceAmount& Input : Result.InputsConsumed)
	{
		InputCount += FMath::Max(0, Input.Quantity);
	}
	for (const FTerritoryResourceAmount& Output : Result.OutputsProduced)
	{
		OutputCount += FMath::Max(0, Output.Quantity);
	}
	const FText RuleName = UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(
		Result.RuleTag);
	AddLiveEvent(
		Result.bSuccess ? ETerritoryLiveEventType::ProductionCompleted
			: ETerritoryLiveEventType::ProductionBlocked,
		Result.TerritoryTag,
		FText::Format(Result.bSuccess
			? NSLOCTEXT("TerritoryIntelligence", "ProductionCompleteHeadline",
				"Production completed: {0}")
			: NSLOCTEXT("TerritoryIntelligence", "ProductionBlockedHeadline",
				"Production blocked: {0}"), RuleName),
		Result.bSuccess
			? FText::Format(NSLOCTEXT("TerritoryIntelligence", "ProductionCompleteDetail",
				"Cycle {0} consumed {1} item units and produced {2}. The Narrative inventory remains the resource authority."),
				FText::AsNumber(Result.CycleIndex), FText::AsNumber(InputCount),
				FText::AsNumber(OutputCount))
			: FText::Format(NSLOCTEXT("TerritoryIntelligence", "ProductionBlockedDetail",
				"Cycle {0} produced nothing. Reason: {1}"),
				FText::AsNumber(Result.CycleIndex), Result.FailureReason),
		Result.TerritoryTag.IsValid(), Result.bSuccess ? 15.f : 45.f,
		ETerritoryIntelligenceCategory::Production,
		Result.bSuccess ? ETerritoryIntelligenceSeverity::Positive
			: ETerritoryIntelligenceSeverity::Warning,
		ViewerFaction, FGameplayTag(), FGameplayTagContainer(), 0, 0, 0,
		!Result.bSuccess);
}

void UTerritoryPlayerManagementComponent::HandleDiplomacyEvent(
	const FDiplomacyEvent& Event)
{
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	if (!ViewerFaction.IsValid()
		|| (Event.FactionA != ViewerFaction && Event.FactionB != ViewerFaction)) return;

	FText Action;
	ETerritoryIntelligenceSeverity Severity = ETerritoryIntelligenceSeverity::Information;
	switch (Event.EventType)
	{
	case EDiplomacyEventType::DeclaredWar:
		Action = NSLOCTEXT("TerritoryIntelligence", "DeclaredWar", "War declared");
		Severity = ETerritoryIntelligenceSeverity::Critical;
		break;
	case EDiplomacyEventType::DeclaredPeace:
		Action = NSLOCTEXT("TerritoryIntelligence", "DeclaredPeace", "Peace declared");
		Severity = ETerritoryIntelligenceSeverity::Positive;
		break;
	case EDiplomacyEventType::FormedAlliance:
		Action = NSLOCTEXT("TerritoryIntelligence", "FormedAlliance", "Alliance formed");
		Severity = ETerritoryIntelligenceSeverity::Positive;
		break;
	case EDiplomacyEventType::BrokeAlliance:
		Action = NSLOCTEXT("TerritoryIntelligence", "BrokeAlliance", "Alliance broken");
		Severity = ETerritoryIntelligenceSeverity::Warning;
		break;
	case EDiplomacyEventType::SignedTradeAgreement:
		Action = NSLOCTEXT("TerritoryIntelligence", "SignedTrade", "Trade agreement signed");
		Severity = ETerritoryIntelligenceSeverity::Positive;
		break;
	case EDiplomacyEventType::ExpiredTreaty:
		Action = NSLOCTEXT("TerritoryIntelligence", "TreatyExpired", "Treaty expired");
		Severity = ETerritoryIntelligenceSeverity::Warning;
		break;
	case EDiplomacyEventType::BrokeCeasefire:
		Action = NSLOCTEXT("TerritoryIntelligence", "CeasefireBroken", "Ceasefire broken");
		Severity = ETerritoryIntelligenceSeverity::Critical;
		break;
	case EDiplomacyEventType::SignedNonAggression:
		Action = NSLOCTEXT("TerritoryIntelligence", "NonAggressionSigned",
			"Non-aggression pact signed");
		Severity = ETerritoryIntelligenceSeverity::Positive;
		break;
	default:
		Action = NSLOCTEXT("TerritoryIntelligence", "DiplomacyUpdated", "Diplomacy updated");
		break;
	}
	AddLiveEvent(ETerritoryLiveEventType::DiplomacyChanged, FGameplayTag(),
		FText::Format(NSLOCTEXT("TerritoryIntelligence", "DiplomacyHeadline",
			"{0}: {1} / {2}"), Action,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Event.FactionA),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Event.FactionB)),
		NSLOCTEXT("TerritoryIntelligence", "DiplomacyDetail",
			"Narrative faction attitude changed. Capture eligibility, guard hostility, and future counterattack admission now use the new relationship."),
		false, 45.f, ETerritoryIntelligenceCategory::Diplomacy, Severity,
		Event.FactionA, Event.FactionB, FGameplayTagContainer(), 0, 0, 0,
		Severity != ETerritoryIntelligenceSeverity::Information,
		MakeDiplomacyRecordID(Event));
}

UTerritoryPlayerManagementComponent* UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(
	APlayerController* PlayerController)
{
	if (!PlayerController) return nullptr;
	if (UTerritoryPlayerManagementComponent* Existing =
		PlayerController->FindComponentByClass<UTerritoryPlayerManagementComponent>())
	{
		return Existing;
	}
	if (!PlayerController->HasAuthority())
	{
		// Runtime replicated components must be authored by the server. Creating a
		// same-named client-only bridge can prevent the authoritative component from
		// resolving correctly when it later replicates.
		return nullptr;
	}

	UTerritoryPlayerManagementComponent* Component =
		NewObject<UTerritoryPlayerManagementComponent>(PlayerController,
			TEXT("TerritoryPlayerManagement"), RF_Transient);
	if (!Component) return nullptr;

	PlayerController->AddInstanceComponent(Component);
	Component->SetIsReplicated(true);
	Component->RegisterComponent();
	PlayerController->ForceNetUpdate();
	return Component;
}

float UTerritoryPlayerManagementComponent::CalculateEspionageSuccessChance(
	int32 ControlledDistricts, int32 TotalUnlockedDistricts,
	int32 ActiveFriendlyGuards, int32 AssignedFriendlyGuards)
{
	const float ControlRatio = TotalUnlockedDistricts > 0
		? FMath::Clamp(static_cast<float>(FMath::Max(0, ControlledDistricts))
			/ static_cast<float>(TotalUnlockedDistricts), 0.f, 1.f)
		: 0.f;
	const float GarrisonReadiness = AssignedFriendlyGuards > 0
		? FMath::Clamp(static_cast<float>(FMath::Max(0, ActiveFriendlyGuards))
			/ static_cast<float>(AssignedFriendlyGuards), 0.f, 1.f)
		: 0.f;
	return FMath::Clamp(0.15f + 0.50f * ControlRatio
		+ 0.25f * GarrisonReadiness, 0.15f, 0.90f);
}

void UTerritoryPlayerManagementComponent::GetEspionageStrengthInputs(
	int32& OutControlledDistricts, int32& OutTotalUnlockedDistricts,
	int32& OutActiveFriendlyGuards, int32& OutAssignedFriendlyGuards) const
{
	OutControlledDistricts = 0;
	OutTotalUnlockedDistricts = 0;
	OutActiveFriendlyGuards = 0;
	OutAssignedFriendlyGuards = 0;
	const UWorld* World = GetWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	const FGameplayTag ViewerFaction = GetManagedFaction();
	if (!Registry || !ViewerFaction.IsValid()) return;

	for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
	{
		if (!IsValid(Territory)) continue;
		if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory))
		{
			if (UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(this, District))
			{
				++OutTotalUnlockedDistricts;
				OutControlledDistricts += District->GetTerritoryState()
					== ETerritoryState::Claimed
					&& District->GetOwningFaction() == ViewerFaction ? 1 : 0;
			}
		}
		if (Territory->GetTerritoryState() == ETerritoryState::Claimed
			&& Territory->GetOwningFaction() == ViewerFaction)
		{
			OutActiveFriendlyGuards += FMath::Max(0, Territory->GetDefenderCount());
			OutAssignedFriendlyGuards += FMath::Max(0, Territory->GetDesiredGuardCount());
		}
	}
}

float UTerritoryPlayerManagementComponent::GetEspionageSuccessChance() const
{
	int32 ControlledDistricts = 0;
	int32 TotalUnlockedDistricts = 0;
	int32 ActiveFriendlyGuards = 0;
	int32 AssignedFriendlyGuards = 0;
	GetEspionageStrengthInputs(ControlledDistricts, TotalUnlockedDistricts,
		ActiveFriendlyGuards, AssignedFriendlyGuards);
	return CalculateEspionageSuccessChance(ControlledDistricts,
		TotalUnlockedDistricts, ActiveFriendlyGuards, AssignedFriendlyGuards);
}

void UTerritoryPlayerManagementComponent::RequestEspionage(
	ATerritoryDistrict* District)
{
	if (!IsValid(District)) return;
	const int32 RequestId = ++NextEspionageRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastEspionageRequestTime < EspionageCooldown) return;
		LastEspionageRequestTime = Now;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformEspionage(District, RequestId);
	}
	else
	{
		ServerRequestEspionage(District, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestSetGuardTarget(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount)
{
	const int32 RequestId = ++NextRequestId;
	if (!ManagementPoint || !Territory || NewDesiredGuardCount < 0
		|| NewDesiredGuardCount > MaxGuardTargetCount)
	{
		OnGuardPurchaseResult.Broadcast(Territory, false,
			FText::FromString(TEXT("Garrison staffing target request is invalid.")), RequestId);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(Territory, false,
				FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformSetGuardTargetAtManagementPoint(ManagementPoint, Territory,
			NewDesiredGuardCount, RequestId);
	}
	else
	{
		ServerRequestSetGuardTarget(ManagementPoint, Territory, NewDesiredGuardCount, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestSetGuardTargetForTerritory(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount)
{
	const int32 RequestId = ++NextRequestId;
	if (!Territory || NewDesiredGuardCount < 0 || NewDesiredGuardCount > MaxGuardTargetCount)
	{
		OnGuardPurchaseResult.Broadcast(Territory, false,
			FText::FromString(TEXT("Garrison staffing target request is invalid.")), RequestId);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(Territory, false,
				FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
	}
	else
	{
		ServerRequestSetGuardTargetForTerritory(Territory, NewDesiredGuardCount, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestSendReinforcements(
	ATerritoryVolume* Territory, int32 Count)
{
	const int32 RequestId = ++NextRequestId;
	if (!Territory || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(Territory, false,
			NSLOCTEXT("TerritoryManagement", "InvalidReinforcementOrder",
				"Reinforcement order is invalid."), RequestId);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(Territory, false,
				NSLOCTEXT("TerritoryManagement", "ReinforcementCooldown",
					"Please wait before sending another command."), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformSendReinforcements(Territory, Count, RequestId);
	}
	else
	{
		ServerRequestSendReinforcements(Territory, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestPurchaseGuards(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	if (!ManagementPoint || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;

	// Anti-spam: ignore requests within cooldown window
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(ManagementPoint->ResolveDistrict(), false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		LastPurchaseRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastPurchaseRequestTime;
		PerformPurchase(ManagementPoint, Count, RequestId);
	}
	else
	{
		ServerRequestPurchaseGuards(ManagementPoint, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestPurchaseGuardsForDistrict(
	ATerritoryDistrict* District, int32 Count)
{
	if (!District || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(District, false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformPurchaseForDistrict(District, Count, RequestId);
	}
	else
	{
		ServerRequestPurchaseGuardsForDistrict(District, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestRemoveGuards(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	if (!ManagementPoint || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(ManagementPoint->ResolveDistrict(), false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformRemove(ManagementPoint, Count, RequestId);
	}
	else
	{
		ServerRequestRemoveGuards(ManagementPoint, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestRemoveGuardsForDistrict(
	ATerritoryDistrict* District, int32 Count)
{
	if (!District || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(District, false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformRemoveForDistrict(District, Count, RequestId);
	}
	else
	{
		ServerRequestRemoveGuardsForDistrict(District, Count, RequestId);
	}
}

bool UTerritoryPlayerManagementComponent::ServerRequestSetGuardTarget_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	return ManagementPoint && Territory && NewDesiredGuardCount >= 0
		&& NewDesiredGuardCount <= MaxGuardTargetCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestSetGuardTarget_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId) return;
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	PerformSetGuardTargetAtManagementPoint(ManagementPoint, Territory,
		NewDesiredGuardCount, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestSetGuardTargetForTerritory_Validate(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	return Territory && NewDesiredGuardCount >= 0
		&& NewDesiredGuardCount <= MaxGuardTargetCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestSetGuardTargetForTerritory_Implementation(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId) return;
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestSendReinforcements_Validate(
	ATerritoryVolume* Territory, int32 Count, int32 RequestId)
{
	return Territory && Count > 0 && Count <= MaxGuardPurchaseCount
		&& RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestSendReinforcements_Implementation(
	ATerritoryVolume* Territory, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			NSLOCTEXT("TerritoryManagement", "ServerReinforcementCooldown",
				"Please wait before sending another command."), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	PerformSendReinforcements(Territory, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuards_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	return ManagementPoint != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuards_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	if (!GetWorld()) return;
	if (RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr,
			false, FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;

	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), RequestId);
		return;
	}
	PerformPurchase(ManagementPoint, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuardsForDistrict_Validate(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	return District != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuardsForDistrict_Implementation(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), RequestId);
		return;
	}
	PerformPurchaseForDistrict(District, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestRemoveGuards_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	return ManagementPoint != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestRemoveGuards_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr,
			false, FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), RequestId);
		return;
	}
	PerformRemove(ManagementPoint, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestRemoveGuardsForDistrict_Validate(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	return District != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestRemoveGuardsForDistrict_Implementation(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), RequestId);
		return;
	}
	PerformRemoveForDistrict(District, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestEspionage_Validate(
	ATerritoryDistrict* District, int32 RequestId)
{
	return District != nullptr && RequestId > LastServerEspionageRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestEspionage_Implementation(
	ATerritoryDistrict* District, int32 RequestId)
{
	UWorld* World = GetWorld();
	if (!World || RequestId <= LastServerEspionageRequestId) return;
	LastServerEspionageRequestId = RequestId;
	const float Now = World->GetTimeSeconds();
	if (Now - LastEspionageRequestTime < EspionageCooldown) return;
	LastEspionageRequestTime = Now;
	PerformEspionage(District, RequestId);
}

bool UTerritoryPlayerManagementComponent::CanEspionageDistrict(
	ATerritoryDistrict* District, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	const UWorld* World = GetWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!IsValid(District) || !Registry
		|| Registry->GetTerritoryByTag(District->GetTerritoryTag()) != District)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEspionage", "TargetUnavailable",
			"That District is not currently available for reconnaissance.");
		return false;
	}
	if (District->GetTerritoryState() == ETerritoryState::Locked
		|| !UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(this, District))
	{
		OutFailureReason = NSLOCTEXT("TerritoryEspionage", "TargetLocked",
			"The story has not unlocked reconnaissance for that District.");
		return false;
	}
	const FGameplayTag ViewerFaction = GetManagedFaction();
	if (!ViewerFaction.IsValid())
	{
		OutFailureReason = NSLOCTEXT("TerritoryEspionage", "NoFaction",
			"The player has no Narrative faction for this operation.");
		return false;
	}
	if (District->GetTerritoryState() == ETerritoryState::Claimed
		&& District->GetOwningFaction() == ViewerFaction)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEspionage", "AlreadyOwned",
			"Your faction already controls that District.");
		return false;
	}
	return true;
}

void UTerritoryPlayerManagementComponent::PerformEspionage(
	ATerritoryDistrict* District, int32 RequestId)
{
	(void)RequestId;
	FText FailureReason;
	if (!CanEspionageDistrict(District, FailureReason))
	{
		if (IsValid(District))
		{
			ClientReceiveEspionageResult(District->GetTerritoryTag(), false,
				FGameplayTag(), 0);
		}
		return;
	}

	const float SuccessChance = GetEspionageSuccessChance();
	const bool bSuccess = FMath::FRand() <= SuccessChance;
	const FGameplayTag DefendingFaction = bSuccess
		? District->GetOwningFaction() : FGameplayTag();
	int32 ActiveDefenders = 0;
	if (bSuccess)
	{
		ActiveDefenders += FMath::Max(0, District->GetDefenderCount());
		for (ATerritoryVolume* Place : District->GetProperties())
		{
			if (IsValid(Place) && Place->GetOwningFaction() == DefendingFaction)
			{
				ActiveDefenders += FMath::Max(0, Place->GetDefenderCount());
			}
		}
	}
	ClientReceiveEspionageResult(District->GetTerritoryTag(), bSuccess,
		DefendingFaction, ActiveDefenders);
}

void UTerritoryPlayerManagementComponent::PerformPurchase(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();

	if (!ManagementPoint || !District || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	FText Result;
	if (!ManagementPoint->CanManage(Pawn, Result))
	{
		ClientReceiveGuardPurchaseResult(District, false, Result, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	PerformPurchaseForDistrict(District, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformSetGuardTargetAtManagementPoint(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();
	FText FailureReason;
	if (!ManagementPoint || !District || !Territory || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	if (!ManagementPoint->CanManage(Pawn, FailureReason)
		|| !CanManageTerritory(Territory, Pawn, FailureReason))
	{
		ClientReceiveGuardPurchaseResult(Territory, false, FailureReason, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	if (!IsTerritoryManagedByDistrict(District, Territory))
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("That garrison does not belong to this district.")), RequestId);
		return;
	}
	PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformSetGuardTarget(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	APawn* Pawn = GetManagingPawn();
	FText FailureReason;
	if (!CanManageTerritory(Territory, Pawn, FailureReason))
	{
		ClientReceiveGuardPurchaseResult(Territory, false, FailureReason, RequestId);
		return;
	}
	if (NewDesiredGuardCount < 0 || NewDesiredGuardCount > Territory->GetMaxGuardCount())
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("The staffing target exceeds this garrison's capacity.")), RequestId);
		return;
	}

	const FTerritoryGarrisonMutationResult Result =
		Territory->TrySetDesiredGuardCount(Pawn, NewDesiredGuardCount);
	ClientReceiveGuardPurchaseResult(Territory, Result.bSuccess, Result.Message, RequestId);
	if (Result.bSuccess)
	{
		ClientReceiveManagementIntelligence(Territory,
			ETerritoryLiveEventType::GarrisonChanged,
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "GarrisonPlanHeadline",
				"Garrison plan updated: {0}"), Territory->GetTerritoryDisplayName()),
			Result.Message);
	}
}

void UTerritoryPlayerManagementComponent::PerformSendReinforcements(
	ATerritoryVolume* Territory, int32 Count, int32 RequestId)
{
	APawn* Pawn = GetManagingPawn();
	if (!Territory || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			NSLOCTEXT("TerritoryManagement", "MissingReinforcementContext",
				"Garrison reinforcement context is unavailable."), RequestId);
		return;
	}
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			NSLOCTEXT("TerritoryManagement", "ServerInvalidReinforcementOrder",
				"Reinforcement order is invalid."), RequestId);
		return;
	}
	const FTerritoryGarrisonMutationResult Result =
		Territory->TrySendReinforcements(Pawn, Count);
	ClientReceiveGuardPurchaseResult(Territory, Result.bSuccess, Result.Message, RequestId);
	if (Result.bSuccess)
	{
		ClientReceiveManagementIntelligence(Territory,
			ETerritoryLiveEventType::ReinforcementDeployed,
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "ReinforcementHeadline",
				"Reserve deployed: {0}"), Territory->GetTerritoryDisplayName()),
			Result.Message);
	}
}

void UTerritoryPlayerManagementComponent::PerformPurchaseForDistrict(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
	{
	FText Result;
	bool bSuccess = false;
	APawn* Pawn = GetManagingPawn();
	if (!District || !Pawn)
	{
		Result = FText::FromString(TEXT("District management context is unavailable."));
	}
	else if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		Result = FText::FromString(TEXT("Guard purchase count is invalid."));
	}
	else if (!CanManageDistrict(District, Pawn, Result))
	{
		bSuccess = false;
	}
	else
	{
		bSuccess = District->TryPurchaseGuards(Pawn, Count, Result);
	}
	ClientReceiveGuardPurchaseResult(District, bSuccess, Result, RequestId);
	if (bSuccess && District)
	{
		ClientReceiveManagementIntelligence(District,
			ETerritoryLiveEventType::GarrisonChanged,
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "GuardsAddedHeadline",
				"Guards assigned: {0}"), District->GetTerritoryDisplayName()), Result);
	}
}

void UTerritoryPlayerManagementComponent::PerformRemove(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();
	if (!ManagementPoint || !District || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	FText Result;
	if (!ManagementPoint->CanManage(Pawn, Result))
	{
		ClientReceiveGuardPurchaseResult(District, false, Result, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	PerformRemoveForDistrict(District, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformRemoveForDistrict(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	FText Result;
	bool bSuccess = false;
	APawn* Pawn = GetManagingPawn();
	if (!District || !Pawn)
	{
		Result = FText::FromString(TEXT("District management context is unavailable."));
	}
	else if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		Result = FText::FromString(TEXT("Guard removal count is invalid."));
	}
	else if (!CanManageDistrict(District, Pawn, Result))
	{
		bSuccess = false;
	}
	else
	{
		bSuccess = District->TryRemoveGuards(Pawn, Count, Result);
	}
	ClientReceiveGuardPurchaseResult(District, bSuccess, Result, RequestId);
	if (bSuccess && District)
	{
		ClientReceiveManagementIntelligence(District,
			ETerritoryLiveEventType::GarrisonChanged,
			FText::Format(NSLOCTEXT("TerritoryIntelligence", "GuardsRemovedHeadline",
				"Guards withdrawn: {0}"), District->GetTerritoryDisplayName()), Result);
	}
}

bool UTerritoryPlayerManagementComponent::CanManageDistrict(
	ATerritoryDistrict* District, APawn* Pawn, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (!District || !Pawn)
	{
		OutFailureReason = FText::FromString(TEXT("District management context is unavailable."));
		return false;
	}
	if (District->GetTerritoryState() != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("Capture this district before managing it."));
		return false;
	}
	const FGameplayTag Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn);
	if (!Faction.IsValid() || Faction != District->GetOwningFaction())
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this district."));
		return false;
	}
	return true;
}

bool UTerritoryPlayerManagementComponent::CanManageTerritory(
	ATerritoryVolume* Territory, APawn* Pawn, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (!Territory || !Pawn)
	{
		OutFailureReason = FText::FromString(TEXT("Garrison management context is unavailable."));
		return false;
	}
	if (Territory->GetTerritoryState() != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("Capture this territory before managing its garrison."));
		return false;
	}
	const FGameplayTag Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn);
	if (!Faction.IsValid() || Faction != Territory->GetOwningFaction())
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this garrison."));
		return false;
	}
	return true;
}

bool UTerritoryPlayerManagementComponent::IsTerritoryManagedByDistrict(
	ATerritoryDistrict* District, ATerritoryVolume* Territory) const
{
	if (!District || !Territory) return false;
	if (District == Territory) return true;
	return District->GetProperties().Contains(Territory);
}

void UTerritoryPlayerManagementComponent::ClientReceiveGuardPurchaseResult_Implementation(
	ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId)
{
	OnGuardPurchaseResult.Broadcast(Territory, bSuccess, Message, RequestId);
}

void UTerritoryPlayerManagementComponent::ClientReceiveManagementIntelligence_Implementation(
	ATerritoryVolume* Territory, ETerritoryLiveEventType Type,
	const FText& Headline, const FText& Detail)
{
	AddLiveEvent(Type, Territory ? Territory->GetTerritoryTag() : FGameplayTag(),
		Headline, Detail, Territory != nullptr, 20.f,
		ETerritoryIntelligenceCategory::Command,
		ETerritoryIntelligenceSeverity::Positive,
		ResolveViewerFaction(), FGameplayTag(), FGameplayTagContainer(),
		0, 0, 0, false);
}

void UTerritoryPlayerManagementComponent::ClientReceiveEspionageResult_Implementation(
	FGameplayTag TerritoryTag, bool bSuccess, FGameplayTag DefendingFaction,
	int32 ActiveDefenders)
{
	const FText TerritoryName = ResolveTerritoryName(TerritoryTag);
	FText Detail;
	if (bSuccess)
	{
		const FText DefenderName = DefendingFaction.IsValid()
			? UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(DefendingFaction)
			: NSLOCTEXT("TerritoryEspionage", "NoDefendingFaction", "No faction");
		Detail = FText::Format(NSLOCTEXT("TerritoryEspionage", "SuccessDetail",
			"Defending faction: {0}. Active guards across the District: {1}."),
			DefenderName, FText::AsNumber(FMath::Max(0, ActiveDefenders)));
	}
	else
	{
		Detail = NSLOCTEXT("TerritoryEspionage", "FailureDetail",
			"Scouts returned without reliable intelligence. Control more Districts and keep your own assigned garrisons staffed before trying again.");
	}
	AddLiveEvent(bSuccess ? ETerritoryLiveEventType::EspionageSucceeded
			: ETerritoryLiveEventType::EspionageFailed,
		TerritoryTag,
		FText::Format(bSuccess
			? NSLOCTEXT("TerritoryEspionage", "SuccessHeadline",
				"Espionage report: {0}")
			: NSLOCTEXT("TerritoryEspionage", "FailureHeadline",
				"Espionage failed: {0}"), TerritoryName),
		Detail, true, LiveEventActiveDuration,
		ETerritoryIntelligenceCategory::Conflict,
		bSuccess ? ETerritoryIntelligenceSeverity::Positive
			: ETerritoryIntelligenceSeverity::Warning,
		GetManagedFaction(), DefendingFaction, FGameplayTagContainer(),
		0, 0, 0, false);
}

void UTerritoryPlayerManagementComponent::SendAssaultNotification(
	const FTerritoryAssaultRecord& Assault)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClientReceiveAssaultNotification(Assault);
	}
}

void UTerritoryPlayerManagementComponent::ClientReceiveAssaultNotification_Implementation(
	const FTerritoryAssaultRecord& Assault)
{
	OnAssaultNotification.Broadcast(Assault);
	AddLiveEvent(ETerritoryLiveEventType::CounterAttackWarning,
		Assault.TargetTerritory,
		FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterWarningHeadline",
			"Counterattack forming near {0}"), ResolveTerritoryName(Assault.TargetTerritory)),
		FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterWarningDetail",
			"{0} is preparing {1} finite attackers. The assault waits for the configured activation rules."),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.AttackingFaction),
			FText::AsNumber(Assault.PlannedForce)), true, 45.f,
		ETerritoryIntelligenceCategory::Conflict,
		ETerritoryIntelligenceSeverity::Warning,
		Assault.AttackingFaction, Assault.DefendingFaction,
		FGameplayTagContainer(), 0, 0, 0, true, Assault.AssaultID);
}

void UTerritoryPlayerManagementComponent::SendCounterHappened(
	const FTerritoryCounterAttackStateEvent& Event)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClientReceiveCounterHappened(Event);
	}
}

void UTerritoryPlayerManagementComponent::ClientReceiveCounterHappened_Implementation(
	const FTerritoryCounterAttackStateEvent& Event)
{
	OnCounterHappened.Broadcast(Event);
	const FGameplayTag Target = Event.Assault.TargetTerritory;
	const FText Name = ResolveTerritoryName(Target);
	switch (Event.NewState)
	{
	case ETerritoryAssaultState::ScheduledWarning:
	case ETerritoryAssaultState::WaitingForPlayerProximity:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackWarning, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterScheduledHeadline",
				"Attack warning: {0}"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "CounterScheduledDetail",
				"A hostile response is scheduled. Track the District to inspect its route and defence."), true, 45.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Warning,
			Event.Assault.AttackingFaction, Event.Assault.DefendingFaction,
			FGameplayTagContainer(), 0, 0, 0, true, Event.Assault.AssaultID);
		break;
	case ETerritoryAssaultState::Active:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackActive, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterActiveHeadline",
				"Counterattack active at {0}"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterActiveDetail",
				"{0} attackers remain alive and {1} remain in reserve."),
				FText::AsNumber(Event.Assault.AliveForce),
				FText::AsNumber(Event.Assault.PendingReserveForce)), true, 60.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Critical,
			Event.Assault.AttackingFaction, Event.Assault.DefendingFaction,
			FGameplayTagContainer(), 0, 0, 0, true, Event.Assault.AssaultID);
		break;
	case ETerritoryAssaultState::Defeated:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackDefeated, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterDefeatedHeadline",
				"Counterattack defeated at {0}"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterDefeatedDetail",
				"The finite force was removed after {0} casualties and {1} withdrawals."),
				FText::AsNumber(Event.Assault.KilledForce),
				FText::AsNumber(Event.Assault.WithdrawnForce)), true, 30.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Positive,
			Event.Assault.DefendingFaction, Event.Assault.AttackingFaction,
			FGameplayTagContainer(), 0, 0, 0, true, Event.Assault.AssaultID);
		break;
	case ETerritoryAssaultState::Succeeded:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackSucceeded, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterSucceededHeadline",
				"Enemy force took {0}"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "CounterSucceededDetail",
				"The physical capture completed. Track the Territory to organize a counter-offensive."), true, 60.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Critical,
			Event.Assault.AttackingFaction, Event.Assault.DefendingFaction,
			FGameplayTagContainer(), 0, 0, 0, true, Event.Assault.AssaultID);
		break;
	case ETerritoryAssaultState::Cancelled:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackCancelled, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterCancelledHeadline",
				"Attack on {0} cancelled"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterCancelledDetail",
				"The response ended before capture. Resolution: {0}."),
				FText::FromString(UEnum::GetValueAsString(Event.Resolution))), true, 20.f,
			ETerritoryIntelligenceCategory::Conflict,
			ETerritoryIntelligenceSeverity::Information,
			Event.Assault.AttackingFaction, Event.Assault.DefendingFaction,
			FGameplayTagContainer(), 0, 0, 0, false, Event.Assault.AssaultID);
		break;
	default:
		break;
	}
}

APawn* UTerritoryPlayerManagementComponent::GetManagingPawn() const
{
	if (const APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		return Controller->GetPawn();
	}
	return Cast<APawn>(GetOwner());
}

FGameplayTag UTerritoryPlayerManagementComponent::GetManagedFaction() const
{
	return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, GetManagingPawn());
}

#include "TerritoryFramework.h"

#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "Debug/TerritoryGameplayDebuggerCategory.h"
#include "GameplayDebugger.h"
#endif

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkModule"

#if !UE_BUILD_SHIPPING
namespace
{
	bool TryParseDebugDifficulty(const FString& Value,
		ENarrativeGameplayDifficulty& OutDifficulty)
	{
		if (Value.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = ENarrativeGameplayDifficulty::Easy;
			return true;
		}
		if (Value.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = ENarrativeGameplayDifficulty::Medium;
			return true;
		}
		if (Value.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = ENarrativeGameplayDifficulty::Hard;
			return true;
		}
		if (Value.Equals(TEXT("Insane"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = ENarrativeGameplayDifficulty::Insane;
			return true;
		}
		return false;
	}

	UWorld* ResolveAuthoritativeGameWorld(UWorld* RequestedWorld)
	{
		if (RequestedWorld && RequestedWorld->IsGameWorld()
			&& RequestedWorld->GetNetMode() != NM_Client)
		{
			return RequestedWorld;
		}

		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld()
					&& Candidate->GetNetMode() != NM_Client)
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	void StartDebugStoryPursuit(const TArray<FString>& Args, UWorld* RequestedWorld)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Usage: Territory.Debug.StartStoryPursuit <Territory.Tag> <Narrative.Factions.Attacker>"));
			return;
		}

		// A multiplayer PIE console can be focused on a client world. Always route
		// this mutation to the authoritative game world so one command is enough.
		UWorld* World = ResolveAuthoritativeGameWorld(RequestedWorld);

		if (!World)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadDebug] Start PIE before starting a story pursuit."));
			return;
		}

		const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(
			FName(*Args[0]), false);
		const FGameplayTag AttackingFaction = FGameplayTag::RequestGameplayTag(
			FName(*Args[1]), false);
		if (!TerritoryTag.IsValid() || !AttackingFaction.IsValid())
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadDebug] Invalid Territory or attacking-faction Gameplay Tag."));
			return;
		}

		ATerritoryVolume* Target = nullptr;
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTerritoryTag() == TerritoryTag)
			{
				Target = *It;
				break;
			}
		}

		UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
		FTerritoryStoryPursuitOptions StoryOptions;
		StoryOptions.bAllowsTerritoryCapture = true;
		StoryOptions.bUseStrategicDecisionRoll = true;
		StoryOptions.GracePeriodOverrideGameTime = -1.f;
		FText FailureReason;
		const bool bScheduled = Target && Counterattacks
			&& Counterattacks->TryScheduleAssaultWithReason(Target,
				AttackingFaction, ETerritoryAssaultLaunchMode::StoryPursuit,
				StoryOptions, FailureReason);
		UE_LOG(LogTemp, Display,
			TEXT("[TerritoryRoadDebug] Story pursuit target=%s attacker=%s result=%s reason=\"%s\" world=%s"),
			*Args[0], *Args[1], bScheduled ? TEXT("Scheduled") : TEXT("Rejected"),
			bScheduled ? TEXT("") : *FailureReason.ToString(),
			*World->GetName());
		if (!bScheduled && Target)
		{
			const UTerritoryCounterAttackProfile* Profile =
				Target->GetCounterAttackProfile();
			const FTerritoryFactionAssaultConfig* Force = Profile
				? Profile->FindFactionForce(AttackingFaction) : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("[TerritoryRoadDebug] Admission facts state=%d available=%s control=%d owner=%s profile=%s force=%s activeOrHistory=%d"),
				static_cast<int32>(Target->GetTerritoryState()),
				Target->IsAvailableForGameplay() ? TEXT("true") : TEXT("false"),
				static_cast<int32>(Target->GetControlMode()),
				*Target->GetOwningFaction().ToString(), *GetNameSafe(Profile),
				Force ? TEXT("configured") : TEXT("missing"),
				Counterattacks ? Counterattacks->GetAllAssaults().Num() : -1);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GStartTerritoryStoryPursuitCommand(
		TEXT("Territory.Debug.StartStoryPursuit"),
		TEXT("Development-only: start a Territory road pursuit in PIE. Args: <Territory.Tag> <Narrative.Factions.Attacker>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&StartDebugStoryPursuit));

	void PrepareAndStartDebugAssaultGate(const TArray<FString>& Args,
		UWorld* RequestedWorld)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Usage: Territory.Debug.StartAssaultGate <Territory.Tag> <Narrative.Factions.Attacker> <Narrative.Factions.Defender> [Immediate] [Easy|Medium|Hard|Insane]"));
			return;
		}

		UWorld* World = ResolveAuthoritativeGameWorld(RequestedWorld);
		if (!World)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryAssaultGate] Start PIE before preparing an assault gate."));
			return;
		}

		const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(
			FName(*Args[0]), false);
		const FGameplayTag AttackingFaction = FGameplayTag::RequestGameplayTag(
			FName(*Args[1]), false);
		const FGameplayTag DefendingFaction = FGameplayTag::RequestGameplayTag(
			FName(*Args[2]), false);
		if (!TerritoryTag.IsValid() || !AttackingFaction.IsValid()
			|| !DefendingFaction.IsValid()
			|| AttackingFaction == DefendingFaction)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryAssaultGate] Territory, attacker, and distinct defender Gameplay Tags are required."));
			return;
		}

		ATerritoryVolume* Target = nullptr;
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTerritoryTag() == TerritoryTag)
			{
				Target = *It;
				break;
			}
		}

		UTerritoryCounterAttackSubsystem* Counterattacks = World
			? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
		UTerritoryDiplomacySubsystem* Diplomacy = World
			? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
		if (!Target || !Counterattacks || !Diplomacy)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryAssaultGate] Could not resolve the target or required Territory subsystems."));
			return;
		}

		// This command is a transient non-shipping release gate. It deliberately
		// prepares the minimum valid political state so a clean PIE map can test
		// assault persistence without depending on a long authored capture prelude.
		Target->ForceSetOwningFaction(DefendingFaction);
		Target->ForceSetTerritoryState(ETerritoryState::Claimed);
		Diplomacy->DeclareWar(AttackingFaction, DefendingFaction);

		// A Claimed entry event may legitimately schedule its own strategic Wave.
		// Retire that transient auto-schedule before starting the exact gate mode,
		// otherwise the diagnostic command would reject itself as a duplicate.
		int32 CancelledPreparationAssaults = 0;
		for (const FTerritoryAssaultRecord& Existing :
			Counterattacks->GetAssaultsForTerritoryActor(Target))
		{
			if (!Existing.IsTerminal()
				&& Counterattacks->CancelAssault(Existing.AssaultID,
					ETerritoryAssaultResolution::ManuallyCancelled))
			{
				++CancelledPreparationAssaults;
			}
		}

		FTerritoryStoryPursuitOptions StoryOptions;
		StoryOptions.bAllowsTerritoryCapture = true;
		StoryOptions.bUseStrategicDecisionRoll = false;
		StoryOptions.GracePeriodOverrideGameTime = -1.f;
		const bool bStartImmediately = Args.Num() > 3
			&& (Args[3].Equals(TEXT("Immediate"), ESearchCase::IgnoreCase)
				|| Args[3].Equals(TEXT("true"), ESearchCase::IgnoreCase)
				|| Args[3] == TEXT("1"));
		UNarrativeGameUserSettings* UserSettings = GEngine
			? Cast<UNarrativeGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
		if (Args.Num() > 4)
		{
			ENarrativeGameplayDifficulty RequestedDifficulty;
			if (!TryParseDebugDifficulty(Args[4], RequestedDifficulty)
				|| !UserSettings)
			{
				UE_LOG(LogTemp, Error,
					TEXT("[TerritoryAssaultGate] Difficulty must be Easy, Medium, Hard, or Insane and Narrative Game User Settings must be active."));
				return;
			}
			// Development-only and deliberately transient. Do not save or apply the
			// setting: this gate must not change the player's authored preference.
			UserSettings->SetGameplayDifficulty(RequestedDifficulty);
		}
		const ENarrativeGameplayDifficulty ActiveDifficulty = UserSettings
			? UserSettings->GetGameplayDifficulty()
			: ENarrativeGameplayDifficulty::Easy;
		FText FailureReason;
		const bool bScheduled = Counterattacks->TryScheduleAssaultAdvancedWithReason(
			Target, AttackingFaction, ETerritoryAssaultLaunchMode::StoryPursuit,
			StoryOptions, bStartImmediately, FailureReason);
		UE_LOG(LogTemp, Display,
			TEXT("[TerritoryAssaultGate] result=%s target=%s attacker=%s defender=%s immediate=%s difficulty=%s preparationAssaultsCancelled=%d reason=\"%s\" world=%s"),
			bScheduled ? TEXT("Scheduled") : TEXT("Rejected"), *Args[0], *Args[1],
			*Args[2], bStartImmediately ? TEXT("true") : TEXT("false"),
			*StaticEnum<ENarrativeGameplayDifficulty>()->GetNameStringByValue(
				static_cast<int64>(ActiveDifficulty)),
			CancelledPreparationAssaults,
			bScheduled ? TEXT("") : *FailureReason.ToString(), *World->GetName());
	}

	FAutoConsoleCommandWithWorldAndArgs GTerritoryStartAssaultGateCommand(
		TEXT("Territory.Debug.StartAssaultGate"),
		TEXT("Development-only: prepare a claimed defender/War pair and start a deterministic assault persistence gate. Args: <Territory.Tag> <Attacker> <Defender> [Immediate] [Easy|Medium|Hard|Insane]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&PrepareAndStartDebugAssaultGate));

	void SaveReloadDebugActiveAssault(const TArray<FString>& Args,
		UWorld* RequestedWorld)
	{
		UWorld* World = ResolveAuthoritativeGameWorld(RequestedWorld);
		UTerritoryCounterAttackSubsystem* Counterattacks = World
			? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
		UNarrativeSaveSubsystem* SaveSubsystem = World
			? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
		ATerritoryWorldState* WorldState = World
			? ATerritoryWorldState::FindTerritoryWorldState(World) : nullptr;
		if (!World || !Counterattacks || !SaveSubsystem || !WorldState)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritorySaveReloadGate] Requires an authoritative PIE world, Narrative Save subsystem, and Territory World State."));
			return;
		}

		const FString SaveName = Args.IsEmpty() || Args[0].IsEmpty()
			? TEXT("TerritoryReleaseGate") : Args[0];
		const int32 SaveSlot = Args.Num() > 1
			? FMath::Max(0, FCString::Atoi(*Args[1])) : 0;
		const TArray<FTerritoryAssaultRecord> Before =
			Counterattacks->GetAllAssaults();
		TSet<FGuid> LiveAssaultIDs;
		for (const FTerritoryAssaultRecord& Assault : Before)
		{
			if (!Assault.IsTerminal())
			{
				LiveAssaultIDs.Add(Assault.AssaultID);
			}
		}
		if (LiveAssaultIDs.IsEmpty())
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritorySaveReloadGate] No live assault exists. Start one before running this gate."));
			return;
		}

		const bool bSaved = SaveSubsystem->Save(SaveName, SaveSlot);
		const bool bLoaded = bSaved && SaveSubsystem->Load(SaveName, SaveSlot);
		const TArray<FTerritoryAssaultRecord> After =
			Counterattacks->GetAllAssaults();
		TSet<FGuid> LiveAssaultIDsAfter;
		int32 LiveRecordsAfter = 0;
		int32 RestoredCount = 0;
		for (const FTerritoryAssaultRecord& Assault : After)
		{
			if (!Assault.IsTerminal())
			{
				++LiveRecordsAfter;
				LiveAssaultIDsAfter.Add(Assault.AssaultID);
				if (LiveAssaultIDs.Contains(Assault.AssaultID))
				{
					++RestoredCount;
				}
			}
		}

		const bool bPassed = bSaved && bLoaded
			&& RestoredCount == LiveAssaultIDs.Num()
			&& LiveRecordsAfter == LiveAssaultIDs.Num()
			&& LiveAssaultIDsAfter.Num() == LiveAssaultIDs.Num();
		const TCHAR* Result = bPassed ? TEXT("PASS") : TEXT("FAIL");
		const TCHAR* Saved = bSaved ? TEXT("true") : TEXT("false");
		const TCHAR* Loaded = bLoaded ? TEXT("true") : TEXT("false");
		const int32 HistoryAfter = After.Num() - LiveRecordsAfter;
		const int32 UnexpectedLive = FMath::Max(0,
			LiveAssaultIDsAfter.Num() - RestoredCount);
		const int32 DuplicateLiveIDs = FMath::Max(0,
			LiveRecordsAfter - LiveAssaultIDsAfter.Num());
		if (bPassed)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[TerritorySaveReloadGate] result=%s save=%s load=%s saveName=%s slot=%d liveBefore=%d liveRestored=%d liveAfter=%d historyAfter=%d totalAfter=%d unexpectedLive=%d duplicateLiveIDs=%d world=%s"),
				Result, Saved, Loaded, *SaveName, SaveSlot,
				LiveAssaultIDs.Num(), RestoredCount, LiveRecordsAfter, HistoryAfter,
				After.Num(), UnexpectedLive, DuplicateLiveIDs, *World->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritorySaveReloadGate] result=%s save=%s load=%s saveName=%s slot=%d liveBefore=%d liveRestored=%d liveAfter=%d historyAfter=%d totalAfter=%d unexpectedLive=%d duplicateLiveIDs=%d world=%s"),
				Result, Saved, Loaded, *SaveName, SaveSlot,
				LiveAssaultIDs.Num(), RestoredCount, LiveRecordsAfter, HistoryAfter,
				After.Num(), UnexpectedLive, DuplicateLiveIDs, *World->GetName());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GTerritorySaveReloadActiveAssaultCommand(
		TEXT("Territory.Debug.SaveReloadActiveAssault"),
		TEXT("Development-only: save to disk and reload while a Territory assault is live. Optional args: <SaveName> <Slot>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&SaveReloadDebugActiveAssault));
}
#endif

void FTerritoryFrameworkModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
	GameplayDebugger.RegisterCategory(TEXT("Territory"),
		IGameplayDebugger::FOnGetCategory::CreateStatic(
			&FGameplayDebuggerCategory_Territory::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebugger.NotifyCategoriesChanged();
#endif
}

void FTerritoryFrameworkModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
		GameplayDebugger.UnregisterCategory(TEXT("Territory"));
		GameplayDebugger.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkModule, TerritoryFramework)

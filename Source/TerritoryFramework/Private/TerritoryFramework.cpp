#include "TerritoryFramework.h"

#include "Core/TerritoryVolume.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
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
		UWorld* World = RequestedWorld;
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			World = nullptr;
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* Candidate = Context.World();
					if (Candidate && Candidate->IsGameWorld()
						&& Candidate->GetNetMode() != NM_Client)
					{
						World = Candidate;
						break;
					}
				}
			}
		}

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
		const bool bScheduled = Target && Counterattacks
			&& Counterattacks->ScheduleStoryPursuit(Target, AttackingFaction);
		UE_LOG(LogTemp, Display,
			TEXT("[TerritoryRoadDebug] Story pursuit target=%s attacker=%s result=%s world=%s"),
			*Args[0], *Args[1], bScheduled ? TEXT("Scheduled") : TEXT("Rejected"),
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

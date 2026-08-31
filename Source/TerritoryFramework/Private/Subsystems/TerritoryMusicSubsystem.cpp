#include "Subsystems/TerritoryMusicSubsystem.h"

#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Music/NarrativeMusicSubsystem.h"
#include "Music/TaggedMusicSet.h"
#include "Sound/SoundBase.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UObject/UnrealType.h"

namespace TerritoryMusicPrivate
{
	int32 GetTerritoryPriority(const ATerritoryVolume* Territory)
	{
		if (Territory && Territory->IsA<ATerritoryProperty>()) return 3;
		if (Territory && Territory->IsA<ATerritoryDistrict>()) return 2;
		if (Territory && Territory->IsA<ATerritoryCity>()) return 1;
		return Territory ? 0 : INDEX_NONE;
	}

	double GetBoundsVolume(const ATerritoryVolume* Territory)
	{
		if (!Territory) return TNumericLimits<double>::Max();
		const FVector Size = Territory->GetTerritoryBounds().GetSize();
		return static_cast<double>(Size.X) * static_cast<double>(Size.Y)
			* static_cast<double>(Size.Z);
	}

	FString BuildRuleKey(ETerritoryState State,
		const FTerritoryStateAudioConfig& Config)
	{
		return FString::Printf(TEXT("%d|%s|%s|%d"),
			static_cast<int32>(State),
			*Config.MusicSetOverride.ToSoftObjectPath().ToString(),
			*Config.MusicTheme.ToString(),
			Config.bImmediateThemeChange ? 1 : 0);
	}

	bool HasSameAssetPath(const UTaggedMusicSet* Set,
		const TSoftObjectPtr<UTaggedMusicSet>& SoftSet)
	{
		return Set && !SoftSet.IsNull()
			&& FSoftObjectPath(Set) == SoftSet.ToSoftObjectPath();
	}

	ETerritoryState GetPresentationState(const ATerritoryVolume* Territory)
	{
		if (!Territory) return ETerritoryState::Unclaimed;
		return Territory->IsAvailableForGameplay()
			? Territory->GetTerritoryState() : ETerritoryState::Locked;
	}
}

bool UTerritoryMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

void UTerritoryMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshAccumulator = RefreshInterval;
}

void UTerritoryMusicSubsystem::Deinitialize()
{
	ReleaseMusicRule();
	ObservedTerritory.Reset();
	MusicTerritory.Reset();
	EvaluatedWorld.Reset();
	Super::Deinitialize();
}

void UTerritoryMusicSubsystem::Tick(float DeltaTime)
{
	RefreshAccumulator += FMath::Max(0.f, DeltaTime);
	if (RefreshAccumulator < RefreshInterval) return;
	RefreshAccumulator = 0.f;
	RefreshNow();
}

TStatId UTerritoryMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTerritoryMusicSubsystem, STATGROUP_Tickables);
}

UWorld* UTerritoryMusicSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

bool UTerritoryMusicSubsystem::IsTickable() const
{
	return !IsTemplate() && GetGameInstance() != nullptr && !IsRunningDedicatedServer();
}

void UTerritoryMusicSubsystem::RefreshNow()
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World != EvaluatedWorld.Get())
	{
		ResetForWorld(World);
	}

	AActor* Listener = ResolveLocalListener();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Listener || !Registry)
	{
		RefreshObservedTerritory(nullptr);
		RefreshMusicTerritory(nullptr);
		MaintainMusicRule();
		return;
	}

	const FVector ListenerLocation = Listener->GetActorLocation();
	RefreshObservedTerritory(Registry->GetTerritoryAtLocation(ListenerLocation));
	RefreshMusicTerritory(ResolveMusicTerritory(ListenerLocation));
	MaintainMusicRule();
}

bool UTerritoryMusicSubsystem::IsMusicConfigUsable(
	const FTerritoryStateAudioConfig& Config)
{
	return Config.HasUsableMusicOverride();
}

AActor* UTerritoryMusicSubsystem::ResolveLocalListener() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!GameInstance || !World) return nullptr;

	// Narrative Music owns one soundtrack per GameInstance. Use the first valid
	// explicitly enumerated local player instead of a world-global controller guess.
	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		APlayerController* Controller = LocalPlayer
			? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Controller || !Controller->IsLocalController()) continue;
		if (APawn* Pawn = Controller->GetPawn()) return Pawn;
		if (APawn* Spectator = Controller->GetSpectatorPawn()) return Spectator;
	}
	return nullptr;
}

ATerritoryVolume* UTerritoryMusicSubsystem::ResolveMusicTerritory(
	const FVector& ListenerLocation) const
{
	UWorld* World = GetTickableGameObjectWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;

	ATerritoryVolume* Best = nullptr;
	int32 BestPriority = INDEX_NONE;
	double BestVolume = TNumericLimits<double>::Max();
	TArray<ATerritoryVolume*> Candidates = Registry->GetTerritoriesAtLocation(
		ListenerLocation);

	// A Place can inherit a soundtrack rule from its exact authored District or
	// City even when a parent's presentation volume does not overlap the Place.
	// This follows the Definition hierarchy instead of guessing tag prefixes.
	if (ATerritoryProperty* Place = Cast<ATerritoryProperty>(
		Registry->GetTerritoryAtLocation(ListenerLocation)))
	{
		if (ATerritoryDistrict* District = Place->GetOwningDistrict())
		{
			Candidates.AddUnique(District);
			Candidates.AddUnique(District->GetOwningCity());
		}
	}
	else if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(
		Registry->GetTerritoryAtLocation(ListenerLocation)))
	{
		Candidates.AddUnique(District->GetOwningCity());
	}

	for (ATerritoryVolume* Candidate : Candidates)
	{
		if (!IsValid(Candidate)) continue;
		const FTerritoryStateAudioConfig* Audio = FindStateAudio(
			Candidate, TerritoryMusicPrivate::GetPresentationState(Candidate));
		if (!Audio || !IsMusicConfigUsable(*Audio)) continue;

		const int32 Priority = TerritoryMusicPrivate::GetTerritoryPriority(Candidate);
		const double BoundsVolume = TerritoryMusicPrivate::GetBoundsVolume(Candidate);
		if (Priority > BestPriority
			|| (Priority == BestPriority && BoundsVolume < BestVolume))
		{
			Best = Candidate;
			BestPriority = Priority;
			BestVolume = BoundsVolume;
		}
	}
	return Best;
}

const FTerritoryStateAudioConfig* UTerritoryMusicSubsystem::FindStateAudio(
	const ATerritoryVolume* Territory, ETerritoryState State) const
{
	if (!IsValid(Territory)) return nullptr;
	const FTerritoryStateConfig* Config = Territory->GetStateConfigs().Find(State);
	return Config ? &Config->Audio : nullptr;
}

void UTerritoryMusicSubsystem::RefreshObservedTerritory(
	ATerritoryVolume* NewTerritory)
{
	const bool bSameTerritory = NewTerritory == ObservedTerritory.Get();
	const ETerritoryState NewState =
		TerritoryMusicPrivate::GetPresentationState(NewTerritory);
	const FTerritoryStateAudioConfig* NewAudioPtr = FindStateAudio(
		NewTerritory, NewState);
	const FTerritoryStateAudioConfig NewAudio = NewAudioPtr
		? *NewAudioPtr : FTerritoryStateAudioConfig();

	if (bHasObservedState)
	{
		if (bSameTerritory && NewTerritory && NewState != ObservedState)
		{
			PlayStateSound(ObservedAudio.StateExitedSound, ObservedAudio);
			PlayStateSound(NewAudio.StateEnteredSound, NewAudio);
		}
		else if (!bSameTerritory)
		{
			if (ObservedAudio.bPlayExitedSoundOnPlayerDeparture)
			{
				PlayStateSound(ObservedAudio.StateExitedSound, ObservedAudio);
			}
			if (NewTerritory && NewAudio.bPlayEnteredSoundOnPlayerArrival)
			{
				PlayStateSound(NewAudio.StateEnteredSound, NewAudio);
			}
		}
	}

	ObservedTerritory = NewTerritory;
	ObservedState = NewState;
	ObservedAudio = NewAudio;
	bHasObservedState = NewTerritory != nullptr;
}

void UTerritoryMusicSubsystem::RefreshMusicTerritory(ATerritoryVolume* NewTerritory)
{
	const ETerritoryState NewState =
		TerritoryMusicPrivate::GetPresentationState(NewTerritory);
	const FTerritoryStateAudioConfig* NewAudio = FindStateAudio(NewTerritory, NewState);
	if (!NewAudio || !IsMusicConfigUsable(*NewAudio))
	{
		NewTerritory = nullptr;
		NewAudio = nullptr;
	}

	const FString NewRuleKey = NewAudio
		? TerritoryMusicPrivate::BuildRuleKey(NewState, *NewAudio) : FString();
	if (MusicTerritory.Get() == NewTerritory && MusicState == NewState
		&& AppliedRuleKey == NewRuleKey)
	{
		return;
	}

	MusicTerritory = NewTerritory;
	MusicState = NewState;
	AppliedRuleKey = NewRuleKey;
	if (!NewTerritory || !NewAudio)
	{
		ReleaseMusicRule();
		return;
	}

	ApplyMusicRule(NewTerritory, *NewAudio);
}

void UTerritoryMusicSubsystem::ApplyMusicRule(ATerritoryVolume* Territory,
	const FTerritoryStateAudioConfig& Config)
{
	UNarrativeMusicSubsystem* Music = GetNarrativeMusic();
	if (!Music || !Territory || !IsMusicConfigUsable(Config)) return;

	if (!bOwnsMusicRule)
	{
		if (!bRestoringBaseline)
		{
			BaselineMusicSet = Music->GetActiveMusicSet();
			BaselineMusicTheme = Music->GetActiveTheme();
		}
		bRestoringBaseline = false;
		bOwnsMusicRule = true;
	}

	const TSoftObjectPtr<UTaggedMusicSet> PreviousTerritorySet =
		AppliedAudio.MusicSetOverride;
	AppliedAudio = Config;
	AppliedMusicTheme = Config.MusicTheme;
	bThemeApplyPending = true;
	bThemeAppliedByTerritory = false;
	if (!Config.MusicSetOverride.IsNull())
	{
		if (PreviousTerritorySet.ToSoftObjectPath()
			!= Config.MusicSetOverride.ToSoftObjectPath())
		{
			SetNarrativeMusicSet(Music, Config.MusicSetOverride);
		}
	}
	else
	{
		// A state with no set override means the pre-Territory world music set,
		// not a custom set left behind by the previous Territory row.
		if (!PreviousTerritorySet.IsNull())
		{
			if (!BaselineMusicSet.IsNull())
			{
				SetNarrativeMusicSet(Music, BaselineMusicSet);
			}
			else
			{
				ResetNarrativeMusicSet(Music);
			}
		}
	}

	// Theme application is completed by MaintainMusicRule after the selected
	// set is ready. It is attempted once, so a later quest or cinematic theme
	// is not continuously overwritten by a polling subsystem.
	MaintainMusicRule();
}

void UTerritoryMusicSubsystem::ReleaseMusicRule()
{
	if (!bOwnsMusicRule)
	{
		AppliedAudio = FTerritoryStateAudioConfig();
		AppliedMusicTheme = FGameplayTag();
		bThemeApplyPending = false;
		bThemeAppliedByTerritory = false;
		return;
	}

	UNarrativeMusicSubsystem* Music = GetNarrativeMusic();
	bool bStillOwnsNarrativeSelection = Music != nullptr;
	if (Music && !AppliedAudio.MusicSetOverride.IsNull())
	{
		UTaggedMusicSet* CurrentSet = Music->GetActiveMusicSet();
		const bool bTargetActive = TerritoryMusicPrivate::HasSameAssetPath(
			CurrentSet, AppliedAudio.MusicSetOverride);
		const bool bTargetStillPending = TerritoryMusicPrivate::HasSameAssetPath(
			CurrentSet, BaselineMusicSet) && bThemeApplyPending;
		const bool bTargetThemeActive = Music->GetActiveTheme().MatchesTagExact(
			AppliedMusicTheme);
		bStillOwnsNarrativeSelection = (bTargetActive
			&& (bTargetThemeActive || bThemeApplyPending
				|| !bThemeAppliedByTerritory))
			|| bTargetStillPending || (CurrentSet == nullptr && bThemeApplyPending);
	}
	else if (Music && AppliedMusicTheme.IsValid())
	{
		bStillOwnsNarrativeSelection = Music->GetActiveTheme().MatchesTagExact(
			AppliedMusicTheme);
	}

	if (Music && bStillOwnsNarrativeSelection)
	{
		UTaggedMusicSet* CurrentSet = Music->GetActiveMusicSet();
		if (!BaselineMusicSet.IsNull()
			&& !TerritoryMusicPrivate::HasSameAssetPath(CurrentSet, BaselineMusicSet))
		{
			SetNarrativeMusicSet(Music, BaselineMusicSet);
		}
		else if (BaselineMusicSet.IsNull())
		{
			ResetNarrativeMusicSet(Music);
		}
		bRestoringBaseline = BaselineMusicTheme.IsValid();
	}
	else
	{
		// Another local system (quest, cinematic, menu) changed Narrative Music.
		// Relinquish without overwriting its newer selection.
		bRestoringBaseline = false;
		BaselineMusicSet.Reset();
		BaselineMusicTheme = FGameplayTag();
	}

	bOwnsMusicRule = false;
	bThemeApplyPending = false;
	bThemeAppliedByTerritory = false;
	AppliedAudio = FTerritoryStateAudioConfig();
	AppliedMusicTheme = FGameplayTag();
}

void UTerritoryMusicSubsystem::MaintainMusicRule()
{
	UNarrativeMusicSubsystem* Music = GetNarrativeMusic();
	if (!Music) return;

	if (bOwnsMusicRule && bThemeApplyPending && AppliedMusicTheme.IsValid())
	{
		const bool bSetReady = AppliedAudio.MusicSetOverride.IsNull()
			? Music->GetActiveMusicSet() != nullptr
			: TerritoryMusicPrivate::HasSameAssetPath(
				Music->GetActiveMusicSet(), AppliedAudio.MusicSetOverride);
		if (bSetReady)
		{
			bThemeAppliedByTerritory = Music->GetActiveTheme().MatchesTagExact(
				AppliedMusicTheme);
			if (!bThemeAppliedByTerritory)
			{
				bThemeAppliedByTerritory = SetNarrativeTheme(Music,
					AppliedMusicTheme, AppliedAudio.bImmediateThemeChange)
					|| Music->GetActiveTheme().MatchesTagExact(AppliedMusicTheme);
				if (!bThemeAppliedByTerritory)
				{
					UE_LOG(LogTerritory, Warning,
						TEXT("[TerritoryMusic] Narrative Music did not accept theme %s for %s. Confirm the active Tagged Music Set contains this theme and no higher-priority music-with-sound override is active."),
						*AppliedMusicTheme.ToString(),
						*GetNameSafe(MusicTerritory.Get()));
				}
			}
			bThemeApplyPending = false;
		}
	}

	if (!bOwnsMusicRule && bRestoringBaseline)
	{
		const bool bSetReady = BaselineMusicSet.IsNull()
			? Music->GetActiveMusicSet() != nullptr
			: TerritoryMusicPrivate::HasSameAssetPath(
				Music->GetActiveMusicSet(), BaselineMusicSet);
		if (bSetReady)
		{
			if (!Music->GetActiveTheme().MatchesTagExact(BaselineMusicTheme))
			{
				SetNarrativeTheme(Music, BaselineMusicTheme, false);
			}
			if (Music->GetActiveTheme().MatchesTagExact(BaselineMusicTheme))
			{
				bRestoringBaseline = false;
				BaselineMusicSet.Reset();
				BaselineMusicTheme = FGameplayTag();
			}
		}
	}
}

void UTerritoryMusicSubsystem::PlayStateSound(
	const TSoftObjectPtr<USoundBase>& Sound,
	const FTerritoryStateAudioConfig& Config) const
{
	if (Sound.IsNull()) return;
	USoundBase* LoadedSound = Sound.LoadSynchronous();
	if (!LoadedSound) return;
	UGameplayStatics::PlaySound2D(GetGameInstance(), LoadedSound,
		FMath::Clamp(Config.StateEffectVolume, 0.f, 4.f),
		FMath::Clamp(Config.StateEffectPitch, 0.25f, 4.f));
}

void UTerritoryMusicSubsystem::ResetForWorld(UWorld* NewWorld)
{
	ObservedTerritory.Reset();
	MusicTerritory.Reset();
	ObservedAudio = FTerritoryStateAudioConfig();
	AppliedAudio = FTerritoryStateAudioConfig();
	ObservedState = ETerritoryState::Unclaimed;
	MusicState = ETerritoryState::Unclaimed;
	AppliedRuleKey.Reset();
	AppliedMusicTheme = FGameplayTag();
	BaselineMusicSet.Reset();
	BaselineMusicTheme = FGameplayTag();
	bHasObservedState = false;
	bOwnsMusicRule = false;
	bThemeApplyPending = false;
	bThemeAppliedByTerritory = false;
	bRestoringBaseline = false;
	EvaluatedWorld = NewWorld;
}

UNarrativeMusicSubsystem* UTerritoryMusicSubsystem::GetNarrativeMusic() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UNarrativeMusicSubsystem>() : nullptr;
}

bool UTerritoryMusicSubsystem::SetNarrativeTheme(
	UNarrativeMusicSubsystem* Music, FGameplayTag Theme, bool bImmediate)
{
	if (!Music || !Theme.IsValid()) return false;
	UFunction* Function = Music->FindFunction(
		GET_FUNCTION_NAME_CHECKED(UNarrativeMusicSubsystem, SetTheme));
	if (!Function) return false;

	struct FParameters
	{
		FGameplayTag Theme;
		bool bImmediate = false;
		bool ReturnValue = false;
	};
	FParameters Parameters;
	Parameters.Theme = Theme;
	Parameters.bImmediate = bImmediate;
	Music->ProcessEvent(Function, &Parameters);
	return Parameters.ReturnValue;
}

void UTerritoryMusicSubsystem::SetNarrativeMusicSet(
	UNarrativeMusicSubsystem* Music,
	const TSoftObjectPtr<UTaggedMusicSet>& MusicSet)
{
	if (!Music || MusicSet.IsNull()) return;
	UFunction* Function = Music->FindFunction(
		GET_FUNCTION_NAME_CHECKED(UNarrativeMusicSubsystem, OverrideMusicSet));
	if (!Function) return;

	struct FParameters
	{
		TSoftObjectPtr<UTaggedMusicSet> NewMusicSet;
		bool ReturnValue = false;
	};
	FParameters Parameters;
	Parameters.NewMusicSet = MusicSet;
	Music->ProcessEvent(Function, &Parameters);
}

void UTerritoryMusicSubsystem::ResetNarrativeMusicSet(
	UNarrativeMusicSubsystem* Music)
{
	if (!Music) return;
	UFunction* Function = Music->FindFunction(
		GET_FUNCTION_NAME_CHECKED(UNarrativeMusicSubsystem,
			ResetMusicSetToDefault));
	if (!Function) return;

	struct FParameters
	{
		bool ReturnValue = false;
	};
	FParameters Parameters;
	Music->ProcessEvent(Function, &Parameters);
}

#include "Subsystems/TerritoryMusicSubsystem.h"

#include "ArsenalSettings.h"
#include "Components/AudioComponent.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Music/NarrativeMusicSubsystem.h"
#include "Music/TaggedMusicSet.h"
#include "NarrativeWorldSettings.h"
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
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UTerritoryMusicSubsystem::HandleWorldCleanup);
	RefreshAccumulator = RefreshInterval;
}

void UTerritoryMusicSubsystem::Deinitialize()
{
	// Native owns world/audio teardown. Do not start a new async music load while
	// the GameInstance and its subsystems are being destroyed.
	ResetForWorld(nullptr);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	Super::Deinitialize();
}

void UTerritoryMusicSubsystem::SetAutomaticMusicEnabled(bool bEnabled)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World != EvaluatedWorld.Get()) ResetForWorld(World);
	if (bAutomaticMusicEnabled == bEnabled) return;
	bAutomaticMusicEnabled = bEnabled;
	// The caller is about to take ownership through Native's music APIs. Discard
	// our requests rather than restoring over that caller's queued scene theme.
	MusicTerritory.Reset();
	AppliedRuleKey.Reset();
	AppliedAudio = FTerritoryStateAudioConfig();
	AppliedMusicTheme = FGameplayTag();
	bOwnsMusicRule = false;
	bThemeApplyPending = false;
	bThemeAppliedByTerritory = false;
	RequestSourceTheme = FGameplayTag();
	SupersededQueuedTheme = FGameplayTag();
	RequestSourceMusicSet.Reset();
	DesiredMusicSet.Reset();
	ClearBaselineRestoration();
	if (bEnabled) RefreshNow();
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
		StopStateSounds();
		RefreshMusicTerritory(nullptr);
		MaintainMusicRule();
		return;
	}

	const FVector ListenerLocation = Listener->GetActorLocation();
	PlayStateSounds(RefreshObservedTerritory(
		ResolveMusicTerritory(ListenerLocation, false)));
	RefreshMusicTerritory(bAutomaticMusicEnabled
		? ResolveMusicTerritory(ListenerLocation) : nullptr);
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
	const FVector& ListenerLocation, bool bRequireMusicRule) const
{
	UWorld* World = GetTickableGameObjectWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;

	ATerritoryVolume* Best = nullptr;
	int32 BestPriority = INDEX_NONE;
	int32 BestAuthoredPriority = MIN_int32;
	double BestVolume = TNumericLimits<double>::Max();
	TArray<ATerritoryVolume*> Candidates = Registry->GetTerritoriesAtLocation(
		ListenerLocation);

	// A Place can inherit a soundtrack rule from its exact authored District or
	// City even when a parent's presentation volume does not overlap the Place.
	// This follows the Definition hierarchy instead of guessing tag prefixes.
	ATerritoryVolume* Containing = bRequireMusicRule
		? ResolveMusicTerritory(ListenerLocation, false) : nullptr;
	if (ATerritoryProperty* Place = Cast<ATerritoryProperty>(Containing))
	{
		if (ATerritoryDistrict* District = Place->GetOwningDistrict())
		{
			Candidates.AddUnique(District);
			Candidates.AddUnique(District->GetOwningCity());
		}
	}
	else if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Containing))
	{
		Candidates.AddUnique(District->GetOwningCity());
	}

	for (ATerritoryVolume* Candidate : Candidates)
	{
		if (!IsValid(Candidate)) continue;
		const FTerritoryStateAudioConfig* Audio = FindStateAudio(
			Candidate, TerritoryMusicPrivate::GetPresentationState(Candidate));
		if (bRequireMusicRule && (!Audio || !IsMusicConfigUsable(*Audio))) continue;
		const int32 AuthoredPriority = Audio ? Audio->MusicPriority : 0;

		const int32 Priority = TerritoryMusicPrivate::GetTerritoryPriority(Candidate);
		const double BoundsVolume = TerritoryMusicPrivate::GetBoundsVolume(Candidate);
		const bool bSamePriority = Priority == BestPriority
			&& AuthoredPriority == BestAuthoredPriority;
		const bool bStableTieWins = Best && bSamePriority && BoundsVolume == BestVolume
			&& (Candidate->GetTerritoryGUID().ToString() < Best->GetTerritoryGUID().ToString()
				|| (Candidate->GetTerritoryGUID() == Best->GetTerritoryGUID()
					&& Candidate->GetPathName() < Best->GetPathName()));
		if (!Best || Priority > BestPriority
			|| (Priority == BestPriority && AuthoredPriority > BestAuthoredPriority)
			|| (bSamePriority && BoundsVolume < BestVolume) || bStableTieWins)
		{
			Best = Candidate;
			BestPriority = Priority;
			BestAuthoredPriority = AuthoredPriority;
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

TArray<UTerritoryMusicSubsystem::FStateSoundRequest>
UTerritoryMusicSubsystem::RefreshObservedTerritory(
	ATerritoryVolume* NewTerritory)
{
	TArray<FStateSoundRequest> Sounds;
	const bool bSameTerritory = NewTerritory == ObservedTerritory.Get()
		&& !(bHasObservedState && !NewTerritory);
	const ETerritoryState NewState =
		TerritoryMusicPrivate::GetPresentationState(NewTerritory);
	const FTerritoryStateAudioConfig* NewAudioPtr = FindStateAudio(
		NewTerritory, NewState);
	const FTerritoryStateAudioConfig NewAudio = NewAudioPtr
		? *NewAudioPtr : FTerritoryStateAudioConfig();
	if (!bSameTerritory || (bHasObservedState && NewState != ObservedState))
	{
		// Cancel only this bridge's old cues, including loops authored in error.
		// Exit/entry cues for the new transition are subsequently serialized.
		StopStateSounds();
	}

	if (bHasObservedState && bSameTerritory && NewTerritory && NewState != ObservedState)
	{
		Sounds.Add({ ObservedAudio.StateExitedSound, ObservedAudio });
		Sounds.Add({ NewAudio.StateEnteredSound, NewAudio });
	}
	else if (!bSameTerritory)
	{
		if (bHasObservedState && ObservedAudio.bPlayExitedSoundOnPlayerDeparture)
		{
			Sounds.Add({ ObservedAudio.StateExitedSound, ObservedAudio });
		}
		// An arrival does not require a previous Territory. This also covers a
		// newly loaded place becoming available to the local listener.
		if (NewTerritory && NewAudio.bPlayEnteredSoundOnPlayerArrival)
		{
			Sounds.Add({ NewAudio.StateEnteredSound, NewAudio });
		}
	}

	ObservedTerritory = NewTerritory;
	ObservedState = NewState;
	ObservedAudio = NewAudio;
	bHasObservedState = NewTerritory != nullptr;
	return Sounds;
}

void UTerritoryMusicSubsystem::RefreshMusicTerritory(ATerritoryVolume* NewTerritory)
{
	if (!bAutomaticMusicEnabled) return;
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
	if (!bAutomaticMusicEnabled || !Music || !Territory || !IsMusicConfigUsable(Config)) return;
	const bool bWasRestoringSet = bRestoringBaseline
		&& RestoreSourceMusicSet.ToSoftObjectPath() != BaselineMusicSet.ToSoftObjectPath();
	FGameplayTag PreviousQueuedTheme = SupersededQueuedTheme;
	if (bOwnsMusicRule && bThemeAppliedByTerritory && Music->GetActiveTheme() != AppliedMusicTheme)
		PreviousQueuedTheme = AppliedMusicTheme;
	else if (bRestoringBaseline && bBaselineThemeRequested && Music->GetActiveTheme() != BaselineMusicTheme)
		PreviousQueuedTheme = BaselineMusicTheme;

	if (!bOwnsMusicRule)
	{
		if (!bRestoringBaseline)
		{
			BaselineMusicSet = Music->GetActiveMusicSet();
			if (BaselineMusicSet.IsNull()) BaselineMusicSet = ResolveDefaultMusicSet();
			BaselineMusicTheme = Music->GetActiveTheme();
			if (!BaselineMusicTheme.IsValid())
				BaselineMusicTheme = FGameplayTag::RequestGameplayTag(TEXT("Music.Ambient"), false);
		}
		bRestoringBaseline = false;
		bBaselineThemeRequested = false;
		RestoreSourceTheme = FGameplayTag();
		RestoreSourceMusicSet.Reset();
		ReleasedQueuedTheme = FGameplayTag();
		bWaitingForReleasedTheme = false;
		bOwnsMusicRule = true;
	}

	const TSoftObjectPtr<UTaggedMusicSet> PreviousDesiredSet = DesiredMusicSet;
	RequestSourceMusicSet = Music->GetActiveMusicSet();
	RequestSourceTheme = Music->GetActiveTheme();
	AppliedAudio = Config;
	DesiredMusicSet = Config.MusicSetOverride.IsNull() ? BaselineMusicSet : Config.MusicSetOverride;
	AppliedMusicTheme = Config.MusicTheme;
	SupersededQueuedTheme = Music->GetActiveTheme() == AppliedMusicTheme
		? PreviousQueuedTheme : FGameplayTag();
	bThemeApplyPending = true;
	bThemeAppliedByTerritory = false;
	if (!DesiredMusicSet.IsNull())
	{
		if (bWasRestoringSet || (PreviousDesiredSet.ToSoftObjectPath() != DesiredMusicSet.ToSoftObjectPath()
			&& (!TerritoryMusicPrivate::HasSameAssetPath(Music->GetActiveMusicSet(), DesiredMusicSet)
				|| !PreviousDesiredSet.IsNull())))
		{
			SetNarrativeMusicSet(Music, DesiredMusicSet);
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
	bool bStillOwnsNarrativeSelection = false;
	if (Music)
	{
		UTaggedMusicSet* CurrentSet = Music->GetActiveMusicSet();
		const bool bTargetSet = DesiredMusicSet.IsNull()
			? CurrentSet == RequestSourceMusicSet.Get()
			: TerritoryMusicPrivate::HasSameAssetPath(CurrentSet, DesiredMusicSet);
		const bool bSourceSet = CurrentSet == RequestSourceMusicSet.Get();
		const FGameplayTag CurrentTheme = Music->GetActiveTheme();
		// Native accepts a theme into its fade queue without changing ActiveTheme.
		// That is still our request while the visible selection is its source.
		bStillOwnsNarrativeSelection = (bTargetSet
			&& bThemeAppliedByTerritory
			&& (CurrentTheme == AppliedMusicTheme || CurrentTheme == RequestSourceTheme))
			|| (bThemeApplyPending && (bSourceSet || bTargetSet)
				&& CurrentTheme == RequestSourceTheme);
	}

	if (Music && bStillOwnsNarrativeSelection)
	{
		UTaggedMusicSet* CurrentSet = Music->GetActiveMusicSet();
		RestoreSourceMusicSet = CurrentSet;
		RestoreSourceTheme = Music->GetActiveTheme();
		ReleasedQueuedTheme = bThemeAppliedByTerritory
			&& RestoreSourceTheme != AppliedMusicTheme ? AppliedMusicTheme : FGameplayTag();
		bWaitingForReleasedTheme = false;
		bBaselineThemeRequested = false;
		if (!BaselineMusicSet.IsNull()
			&& (!TerritoryMusicPrivate::HasSameAssetPath(CurrentSet, BaselineMusicSet)
				|| DesiredMusicSet.ToSoftObjectPath() != BaselineMusicSet.ToSoftObjectPath()))
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
		ClearBaselineRestoration();
	}

	bOwnsMusicRule = false;
	bThemeApplyPending = false;
	bThemeAppliedByTerritory = false;
	AppliedAudio = FTerritoryStateAudioConfig();
	AppliedMusicTheme = FGameplayTag();
	DesiredMusicSet.Reset();
	RequestSourceMusicSet.Reset();
	RequestSourceTheme = FGameplayTag();
	SupersededQueuedTheme = FGameplayTag();
}

void UTerritoryMusicSubsystem::MaintainMusicRule()
{
	if (!bAutomaticMusicEnabled) return;
	UNarrativeMusicSubsystem* Music = GetNarrativeMusic();
	if (!Music) return;

	if (bOwnsMusicRule && bThemeApplyPending && AppliedMusicTheme.IsValid())
	{
		const bool bSetReady = DesiredMusicSet.IsNull()
			? Music->GetActiveMusicSet() != nullptr
			: TerritoryMusicPrivate::HasSameAssetPath(
				Music->GetActiveMusicSet(), DesiredMusicSet);
		if (bSetReady)
		{
			if (SupersededQueuedTheme.IsValid())
			{
				// Re-entry can request the still-playing theme while our previous
				// restoration is queued. Native cannot cancel that queue with its
				// active theme; reconcile once the known older request surfaces.
				if (Music->GetActiveTheme() == AppliedMusicTheme) return;
				if (Music->GetActiveTheme() != SupersededQueuedTheme)
				{
					bThemeApplyPending = false;
					SupersededQueuedTheme = FGameplayTag();
					return;
				}
				SupersededQueuedTheme = FGameplayTag();
			}
			bThemeAppliedByTerritory = Music->GetActiveTheme().MatchesTagExact(
				AppliedMusicTheme);
			if (!bThemeAppliedByTerritory)
			{
				RequestSourceTheme = Music->GetActiveTheme();
				RequestSourceMusicSet = Music->GetActiveMusicSet();
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
		// Active selection is the only ownership read model Native exposes.
		// A visibly newer set/theme wins even while our restore was waiting.
		UTaggedMusicSet* CurrentSet = Music->GetActiveMusicSet();
		const FGameplayTag CurrentTheme = Music->GetActiveTheme();
		const bool bExternalSet = CurrentSet && !BaselineMusicSet.IsNull()
			&& !TerritoryMusicPrivate::HasSameAssetPath(CurrentSet, BaselineMusicSet)
			&& !TerritoryMusicPrivate::HasSameAssetPath(CurrentSet, RestoreSourceMusicSet);
		const bool bExternalTheme = CurrentTheme.IsValid()
			&& CurrentTheme != BaselineMusicTheme && CurrentTheme != RestoreSourceTheme
			&& CurrentTheme != ReleasedQueuedTheme;
		if (bExternalSet || bExternalTheme)
		{
			ClearBaselineRestoration();
			return;
		}
		const bool bSetReady = BaselineMusicSet.IsNull()
			? Music->GetActiveMusicSet() != nullptr
			: TerritoryMusicPrivate::HasSameAssetPath(
				Music->GetActiveMusicSet(), BaselineMusicSet);
		if (bSetReady)
		{
			if (bWaitingForReleasedTheme && CurrentTheme == BaselineMusicTheme) return;
			if (!bBaselineThemeRequested
				&& (CurrentTheme != BaselineMusicTheme || ReleasedQueuedTheme.IsValid()))
			{
				// Accepted may mean queued, not audible yet. Never resubmit while
				// waiting: a later Native quest request can replace that queue.
				bBaselineThemeRequested = true;
				if (!SetNarrativeTheme(Music, BaselineMusicTheme, false))
				{
					if (CurrentTheme == BaselineMusicTheme && ReleasedQueuedTheme.IsValid())
					{
						// Native cannot replace a queue with the already-playing theme.
						// Wait for our queued theme to surface, then restore once. A
						// visibly different story theme still cancels this restoration.
						bBaselineThemeRequested = false;
						bWaitingForReleasedTheme = true;
						return;
					}
					// Already queued, a manual sound override, and invalid content all
					// return false. Native has no public discriminator; yield safely.
					ClearBaselineRestoration();
					return;
				}
			}
			if (Music->GetActiveTheme().MatchesTagExact(BaselineMusicTheme))
			{
				ClearBaselineRestoration();
			}
		}
	}
}

void UTerritoryMusicSubsystem::ClearBaselineRestoration()
{
	bRestoringBaseline = false;
	bBaselineThemeRequested = false;
	BaselineMusicSet.Reset();
	BaselineMusicTheme = FGameplayTag();
	RestoreSourceMusicSet.Reset();
	RestoreSourceTheme = FGameplayTag();
	ReleasedQueuedTheme = FGameplayTag();
	bWaitingForReleasedTheme = false;
}

void UTerritoryMusicSubsystem::PlayStateSounds(TArray<FStateSoundRequest> Sounds)
{
	StateSoundQueue.Append(MoveTemp(Sounds));
	if (StateSoundComponent) return;
	while (!StateSoundQueue.IsEmpty())
	{
		const FStateSoundRequest Request = StateSoundQueue[0];
		StateSoundQueue.RemoveAt(0);
		USoundBase* Sound = Request.Sound.LoadSynchronous();
		if (!Sound) continue;
		// This channel is for one-shots. Loops belong in Narrative music sets and
		// must not block every subsequent entry/exit cue indefinitely.
		if (Sound->IsLooping())
		{
			UE_LOG(LogTerritory, Warning, TEXT("[TerritoryMusic] Looping state cue %s ignored; author continuous ambience in a Narrative music set."), *GetNameSafe(Sound));
			continue;
		}
		StateSoundComponent = UGameplayStatics::CreateSound2D(GetGameInstance(), Sound,
			FMath::Clamp(Request.Config.StateEffectVolume, 0.f, 4.f),
			FMath::Clamp(Request.Config.StateEffectPitch, 0.25f, 4.f), 0.f, nullptr, false, false);
		if (!StateSoundComponent) continue;
		StateSoundComponent->OnAudioFinished.AddUniqueDynamic(this,
			&UTerritoryMusicSubsystem::HandleStateSoundFinished);
		StateSoundComponent->Play();
		break;
	}
}

void UTerritoryMusicSubsystem::StopStateSounds()
{
	StateSoundQueue.Reset();
	if (IsValid(StateSoundComponent))
	{
		StateSoundComponent->OnAudioFinished.RemoveDynamic(this,
			&UTerritoryMusicSubsystem::HandleStateSoundFinished);
		StateSoundComponent->Stop();
		StateSoundComponent->DestroyComponent();
	}
	StateSoundComponent = nullptr;
}

void UTerritoryMusicSubsystem::HandleStateSoundFinished()
{
	if (IsValid(StateSoundComponent))
	{
		StateSoundComponent->OnAudioFinished.RemoveDynamic(this,
			&UTerritoryMusicSubsystem::HandleStateSoundFinished);
		StateSoundComponent->DestroyComponent();
	}
	StateSoundComponent = nullptr;
	PlayStateSounds({});
}

void UTerritoryMusicSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
	if (World && (World == EvaluatedWorld.Get() || World == GetTickableGameObjectWorld()))
		ResetForWorld(nullptr);
}

TSoftObjectPtr<UTaggedMusicSet> UTerritoryMusicSubsystem::ResolveDefaultMusicSet() const
{
	const UWorld* World = GetTickableGameObjectWorld();
	const ANarrativeWorldSettings* Settings = World
		? Cast<ANarrativeWorldSettings>(World->GetWorldSettings()) : nullptr;
	if (Settings && Settings->DefaultMusicSetOverride) return Settings->DefaultMusicSetOverride.Get();
	return GetDefault<UArsenalSettings>()->DefaultMusicSet;
}

void UTerritoryMusicSubsystem::ResetForWorld(UWorld* NewWorld)
{
	StopStateSounds();
	ObservedTerritory.Reset();
	MusicTerritory.Reset();
	ObservedAudio = FTerritoryStateAudioConfig();
	AppliedAudio = FTerritoryStateAudioConfig();
	ObservedState = ETerritoryState::Unclaimed;
	MusicState = ETerritoryState::Unclaimed;
	AppliedRuleKey.Reset();
	AppliedMusicTheme = FGameplayTag();
	ClearBaselineRestoration();
	bHasObservedState = false;
	bOwnsMusicRule = false;
	bThemeApplyPending = false;
	bThemeAppliedByTerritory = false;
	RequestSourceTheme = FGameplayTag();
	SupersededQueuedTheme = FGameplayTag();
	RequestSourceMusicSet.Reset();
	DesiredMusicSet.Reset();
	bAutomaticMusicEnabled = true;
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
	// Narrative Pro exposes this function to reflection/Blueprint but does not
	// export its native symbol from NarrativeArsenal. A direct C++ call therefore
	// produces LNK2019 in external modules. Keep this narrow, signature-tested bridge.
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

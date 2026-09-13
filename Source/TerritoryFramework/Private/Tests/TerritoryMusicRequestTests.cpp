#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/AudioComponent.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryMusicTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Music/NarrativeMusicSubsystem.h"
#include "Music/TaggedMusicSet.h"
#include "Sound/SoundWave.h"
#include "Subsystems/TerritoryMusicSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryMusicRequestLifecycle,
	"TerritoryFramework.Audio.NarrativeMusic.Regression.QueuedRestoreAndStoryHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryMusicRequestLifecycle::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	UNarrativeMusicSubsystem* Native = GameInstance->GetSubsystem<UNarrativeMusicSubsystem>();
	UTerritoryMusicSubsystem* Territory = GameInstance->GetSubsystem<UTerritoryMusicSubsystem>();
	const auto Cleanup = [&]()
	{
		GameInstance->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	if (!TestNotNull(TEXT("Native music subsystem exists"), Native)
		|| !TestNotNull(TEXT("Territory music adapter exists"), Territory))
	{
		Cleanup();
		return false;
	}
	Territory->ResetForWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	TGuardValue<uint64> FrameCounter(GFrameCounter, GFrameCounter);
	const auto Advance = [&](float Seconds)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	// Headless tests exercise Native's real fade timers and queue without needing
	// an output device. The separate PIE test checks the actual master MetaSound.
	UTaggedMusicSet* Set = NewObject<UTaggedMusicSet>(GameInstance);
	USoundWave* Sound = NewObject<USoundWave>(Set);
	const FGameplayTag Baseline = TerritoryMusicTags::Unclaimed;
	const FGameplayTag PlaceTheme = TerritoryMusicTags::Contested;
	const FGameplayTag QuestTheme = TerritoryMusicTags::Claimed;
	FMapProperty* TracksProperty = FindFProperty<FMapProperty>(UTaggedMusicSet::StaticClass(), TEXT("MusicSets"));
	FObjectPropertyBase* SetProperty = FindFProperty<FObjectPropertyBase>(UNarrativeMusicSubsystem::StaticClass(), TEXT("CurrentMusicSet"));
	FObjectPropertyBase* AudioProperty = FindFProperty<FObjectPropertyBase>(UNarrativeMusicSubsystem::StaticClass(), TEXT("PrimaryAudioComponent"));
	FStructProperty* OverrideProperty = FindFProperty<FStructProperty>(UNarrativeMusicSubsystem::StaticClass(), TEXT("OverrideMusicSound"));
	if (!TestNotNull(TEXT("Native authoring map"), TracksProperty)
		|| !TestNotNull(TEXT("Native current-set fixture field"), SetProperty)
		|| !TestNotNull(TEXT("Native audio fixture field"), AudioProperty)
		|| !TestNotNull(TEXT("Native sound override fixture field"), OverrideProperty))
	{
		Cleanup();
		return false;
	}
	auto* Tracks = TracksProperty->ContainerPtrToValuePtr<TMap<FGameplayTag, FMusicTracksContainer>>(Set);
	for (FGameplayTag Tag : { Baseline, PlaceTheme, QuestTheme })
	{
		FMusicSound& Track = Tracks->FindOrAdd(Tag).MusicSounds.AddDefaulted_GetRef();
		Track.Music = Sound;
		Track.FadeInDuration = Track.FadeOutDuration = 3.f;
	}
	SetProperty->SetObjectPropertyValue_InContainer(Native, Set);
	UAudioComponent* Audio = NewObject<UAudioComponent>(GameInstance);
	AudioProperty->SetObjectPropertyValue_InContainer(Native, Audio);
	TestTrue(TEXT("Native accepts initial baseline"), Territory->SetNarrativeTheme(Native, Baseline, true));
	Advance(0.01f);
	Advance(0.02f);
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	FTerritoryStateAudioConfig Config;
	Config.bOverrideNarrativeMusic = true;
	Config.MusicTheme = PlaceTheme;
	Territory->ApplyMusicRule(Place, Config);
	TestEqual(TEXT("Territory requests its theme through Native"), Native->GetActiveTheme(), PlaceTheme);
	Territory->ReleaseMusicRule();
	Territory->MaintainMusicRule();
	TestTrue(TEXT("Baseline was submitted while Native was fading"), Territory->bBaselineThemeRequested);
	TestEqual(TEXT("Accepted queued baseline is not active yet"), Native->GetActiveTheme(), PlaceTheme);
	TestTrue(TEXT("A later quest theme replaces Native's pending theme"), Territory->SetNarrativeTheme(Native, QuestTheme, false));
	for (int32 Poll = 0; Poll < 20; ++Poll) Territory->MaintainMusicRule();
	Advance(0.01f);
	Advance(4.f);
	TestEqual(TEXT("Territory polling does not overwrite the queued quest theme"), Native->GetActiveTheme(), QuestTheme);
	Territory->MaintainMusicRule();
	TestFalse(TEXT("Visible external theme ends baseline observation"), Territory->bRestoringBaseline);
	Advance(4.f);

	// A visible external set wins before our waiting baseline request is sent.
	UTaggedMusicSet* ExternalSet = NewObject<UTaggedMusicSet>(GameInstance);
	Territory->BaselineMusicSet = Set;
	Territory->BaselineMusicTheme = Baseline;
	Territory->RestoreSourceMusicSet = Set;
	Territory->RestoreSourceTheme = QuestTheme;
	Territory->bRestoringBaseline = true;
	SetProperty->SetObjectPropertyValue_InContainer(Native, ExternalSet);
	Territory->MaintainMusicRule();
	TestFalse(TEXT("A newer visible music set cancels delayed restoration"), Territory->bRestoringBaseline);
	TestTrue(TEXT("External music set remains selected"), Native->GetActiveMusicSet() == ExternalSet);
	SetProperty->SetObjectPropertyValue_InContainer(Native, Set);

	// Native rejects themes during a sound override. Do not keep retrying until
	// that override ends and overwrite a newer scene decision.
	FMusicSound* Override = OverrideProperty->ContainerPtrToValuePtr<FMusicSound>(Native);
	Override->Music = Sound;
	Territory->BaselineMusicSet = Set;
	Territory->BaselineMusicTheme = Baseline;
	Territory->RestoreSourceMusicSet = Set;
	Territory->RestoreSourceTheme = QuestTheme;
	Territory->bRestoringBaseline = true;
	Territory->MaintainMusicRule();
	TestFalse(TEXT("Rejected restoration yields to Native's sound override"), Territory->bRestoringBaseline);
	Override->Music = nullptr;
	Territory->MaintainMusicRule();
	TestEqual(TEXT("Clearing an override does not trigger an old Territory retry"), Native->GetActiveTheme(), QuestTheme);

	// Explicit local story control also covers queued requests Native cannot expose.
	Territory->ApplyMusicRule(Place, Config);
	Territory->ReleaseMusicRule();
	Territory->SetAutomaticMusicEnabled(false);
	TestFalse(TEXT("Story handoff disables automatic music"), Territory->IsAutomaticMusicEnabled());
	TestFalse(TEXT("Story handoff discards pending baseline restoration"), Territory->bRestoringBaseline);
	Territory->ApplyMusicRule(Place, Config);
	Territory->MaintainMusicRule();
	TestFalse(TEXT("State changes cannot reacquire music while disabled"), Territory->bOwnsMusicRule);
	Territory->SetAutomaticMusicEnabled(true);
	TestTrue(TEXT("Story can return automatic music control"), Territory->IsAutomaticMusicEnabled());
	Territory->SetAutomaticMusicEnabled(false);
	Territory->ResetForWorld(World);
	TestTrue(TEXT("New-world presentation starts with automatic music enabled"), Territory->IsAutomaticMusicEnabled());
	TestFalse(TEXT("New world carries no pending audio restoration"), Territory->bRestoringBaseline);
	const UFunction* Handoff = Territory->FindFunction(TEXT("SetAutomaticMusicEnabled"));
	TestTrue(TEXT("Blueprint story handoff is local cosmetic API"), Handoff && Handoff->HasAnyFunctionFlags(FUNC_BlueprintCosmetic));
	TestFalse(TEXT("Story handoff adds no music RPC"), Handoff && Handoff->HasAnyFunctionFlags(FUNC_Net));
	Cleanup();
	return true;
}

#endif

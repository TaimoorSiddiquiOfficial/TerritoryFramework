#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tales/TerritoryCutsceneCondition.h"
#include "UnrealFramework/NarrativePlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneConditionPlayback,
	"TerritoryFramework.Presentation.Cinematics.CutsceneConditionAudienceAndPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneConditionPlayback::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Playback world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	World->InitializeActorsForPlay(FURL());
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	// No local-player registration/input processor is needed for a read query.
	auto* Viewer = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	auto* OtherViewer = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	ULevelSequence* Sequence = NewObject<ULevelSequence>(World);
	Sequence->Initialize();
	Sequence->GetMovieScene()->SetPlaybackRange(0, 24000);
	auto* Actor = World->SpawnActor<ANarrativeLevelSequenceActor>();
	Actor->OwnerControllers = { Viewer };
	Actor->SetSequence(Sequence);
	Actor->InitializePlayer();
	ULevelSequencePlayer* Player = Actor->GetSequencePlayer();
	// Native adds these entries to every authority controller on play, including
	// excluded viewers. Keep them through Stop to reproduce its stale server list.
	Viewer->CurrentSequences.Add(Actor);
	OtherViewer->CurrentSequences.Add(Actor);
	auto* Condition = NewObject<UTerritoryCutsceneCondition>();
	TestFalse(TEXT("Registered but stopped sequence is not a cutscene"),
		Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Player->Play();
	TestTrue(TEXT("Real Narrative sequence player is playing"), Player->IsPlaying());
	TestTrue(TEXT("Audience viewer passes expected true"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	TestFalse(TEXT("Excluded viewer fails even with Native registry entry"), Condition->CheckConditionInContext(nullptr, OtherViewer, nullptr));
	Condition->bIsInCutscene = false;
	TestTrue(TEXT("Excluded viewer passes expected false"), Condition->CheckConditionInContext(nullptr, OtherViewer, nullptr));
	TestFalse(TEXT("Missing context does not pass a negative condition"), Condition->CheckConditionInContext(nullptr, nullptr, nullptr));
	Condition->bIsInCutscene = true;
	Condition->Sequence = Sequence;
	TestTrue(TEXT("Exact playing sequence matches"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Condition->Sequence = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT("/Game/MissingCutscene.MissingCutscene")));
	TestFalse(TEXT("Unloaded different sequence is not replaced by any-sequence query"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Condition->Sequence.Reset();
	Player->Pause();
	TestFalse(TEXT("Paused does not satisfy default playing policy"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Condition->bIncludePaused = true;
	TestTrue(TEXT("Paused policy is explicit"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Player->Stop();
	TestFalse(TEXT("Stale registry entry after Stop is ignored"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Player->Play();
	Viewer->CurrentSequences.Reset();
	TestFalse(TEXT("Unregistered animation is never discovered through world scanning"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	Viewer->CurrentSequences.Add(Actor);
	Actor->Destroy();
	TestFalse(TEXT("Streamed/destroyed sequence is ignored"), Condition->CheckConditionInContext(nullptr, Viewer, nullptr));
	const UFunction* Query = UTerritoryCinematicPresentationSubsystem::StaticClass()->FindFunctionByName(TEXT("IsInCutscene"));
	TestTrue(TEXT("Blueprint query is pure"), Query && Query->HasAnyFunctionFlags(FUNC_BlueprintPure));
	TestFalse(TEXT("Playback query introduces no RPC"), Query && Query->HasAnyFunctionFlags(FUNC_Net));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TerritoryCinematicAudienceProbe.generated.h"

/**
 * A local controller that records every cinematic transition it is put through.
 *
 * The audience reconcile's whole observable effect is the sequence of SetCinematicMode calls a
 * controller receives, and nothing else in the engine or in Territory can report it: bCinematicMode is
 * a single bool, so "was suppressed and then released" and "was never touched" are the same reading
 * taken afterwards. Only a recording of the calls tells them apart, which is why this exists rather
 * than an assertion on the final flag.
 *
 * SetCinematicMode's five-argument overload is virtual, and it is the one the engine's own
 * ULevelSequencePlayer::EnableCinematicMode calls (LevelSequencePlayer.cpp:395), so an override here
 * sees exactly the calls production makes - the engine's and Territory's alike, with their real
 * argument values.
 */
UCLASS(Transient)
class TERRITORYFRAMEWORKEDITOR_API ATerritoryCinematicRecordingController : public APlayerController
{
	GENERATED_BODY()

public:
	/** One recorded transition, with every argument the caller passed. */
	struct FCall
	{
		bool bCinematicMode = false;
		bool bHidePlayer = false;
		bool bHideHud = false;
		bool bAffectsMovement = false;
		bool bAffectsTurning = false;
	};

	/** Every call in order, so a test can assert the last word as well as the whole history. */
	TArray<FCall> Calls;

	virtual void SetCinematicMode(bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD,
		bool bAffectsMovement, bool bAffectsTurning) override
	{
		FCall Call;
		Call.bCinematicMode = bInCinematicMode;
		Call.bHidePlayer = bHidePlayer;
		Call.bHideHud = bAffectsHUD;
		Call.bAffectsMovement = bAffectsMovement;
		Call.bAffectsTurning = bAffectsTurning;
		Calls.Add(Call);

		// The real path still runs. The point is to observe production, not to replace it: a probe
		// that swallowed the call would leave bCinematicMode false and the reconcile's own gate would
		// then release on every frame for the rest of the cutscene.
		Super::SetCinematicMode(bInCinematicMode, bHidePlayer, bAffectsHUD, bAffectsMovement, bAffectsTurning);
	}

	/** How many transitions were recorded at all. */
	int32 Num() const { return Calls.Num(); }

	/** How many transitions put this controller into cinematic mode. */
	int32 NumberEntering() const
	{
		int32 Count = 0;
		for (const FCall& Call : Calls)
		{
			if (Call.bCinematicMode) ++Count;
		}
		return Count;
	}

	/** How many transitions took it out. */
	int32 NumberLeaving() const
	{
		int32 Count = 0;
		for (const FCall& Call : Calls)
		{
			if (!Call.bCinematicMode) ++Count;
		}
		return Count;
	}

	/** The last transition, or null when nothing was ever recorded. */
	const FCall* Last() const { return Calls.IsEmpty() ? nullptr : &Calls.Last(); }
};

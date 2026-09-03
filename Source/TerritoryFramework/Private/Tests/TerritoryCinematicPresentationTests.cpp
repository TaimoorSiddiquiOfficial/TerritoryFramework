#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "Core/TerritoryDeveloperSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicDefaults,
	"TerritoryFramework.Presentation.Cinematics.SafeNarrativeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicDefaults::RunTest(const FString& Parameters)
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	TestTrue(TEXT("Territory capture HUD is hidden during Narrative dialogue"),
		Settings->bHideTerritoryHUDDuringNarrativeDialogue);
	TestTrue(TEXT("Dialogue participant LOD quality is enabled"),
		Settings->bUseCinematicParticipantLODDuringDialogue);
	TestTrue(TEXT("Attached MetaHuman visual actors receive cinematic LOD"),
		Settings->bIncludeAttachedActorsInCinematicLOD);

	const UTerritoryDialogueShot* Shot = NewObject<UTerritoryDialogueShot>();
	TestEqual(TEXT("Territory shots use a 2.39 presentation crop"),
		Shot->GetCinematicAspectRatio(), 2.39f);
	TestTrue(TEXT("Territory shots request Narrative HUD hiding"),
		Shot->GetPlaybackSettings().bHideHud);
	TestEqual(TEXT("Default shot intent is medium close-up"),
		Shot->GetShotRole(), ETerritoryDialogueShotRole::MediumCloseUp);
	TestTrue(TEXT("Cinematic lens override is enabled"),
		Shot->bApplyLensOverride);
	TestTrue(TEXT("Tracked focus smoothing is enabled"),
		Shot->bSmoothTrackingFocus);

	TestTrue(TEXT("Presentation bridge is an automatic local-player subsystem"),
		UTerritoryCinematicPresentationSubsystem::StaticClass()->IsChildOf(
			ULocalPlayerSubsystem::StaticClass()));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Social/TerritoryConversationMeetingPoint.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NarrativeArsenal.h"

/**
 * A meeting point is a coordination trigger, never geometry. Collision profile OverlapAllDynamic
 * only rewrites the eight built-in channels in BaseEngine.ini, so every custom game trace channel
 * keeps the engine default of Block. Narrative traces weapons on GameTraceChannel2 and projectiles
 * on GameTraceChannel6, and a line trace returns the first BLOCKING hit: a player standing inside
 * the trigger became the first hit for melee sweeps, firearm traces and projectile lines, so every
 * attack silently registered on the trigger instead of the enemy behind it. PIE confirmed it with a
 * GameTraceChannel2 raycast from inside the sphere hitting `ListenerRange` at distance 0.
 *
 * The pawn overlap that starts the meeting must survive, because the example Blueprint's
 * ActorBeginOverlap is the only entry point into TryStartConversation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryConversationMeetingCollisionTest,
	"TerritoryFramework.Social.Regression.MeetingPointDoesNotBlockCombatTraces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryConversationMeetingCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Meeting collision test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATerritoryConversationMeetingPoint* Meeting =
		World->SpawnActor<ATerritoryConversationMeetingPoint>(ATerritoryConversationMeetingPoint::StaticClass(), SpawnParams);
	if (!TestNotNull(TEXT("Meeting point spawned"), Meeting))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}

	// Mirror the example Blueprint exactly: a query-only sphere on OverlapAllDynamic, the profile that
	// provokes this defect because BaseEngine.ini leaves custom channels on the engine default (Block).
	USphereComponent* ListenerRange = NewObject<USphereComponent>(Meeting, TEXT("ListenerRange"));
	ListenerRange->SetupAttachment(Meeting->GetRootComponent());
	ListenerRange->SetSphereRadius(1000.f);
	ListenerRange->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ListenerRange->RegisterComponent();

	// Prove the setup really reproduces the bug before asserting the fix, otherwise the test could
	// pass while proving nothing.
	TestEqual(TEXT("Precondition: a custom trace channel defaults to Block"),
		ListenerRange->GetCollisionResponseToChannel(TraceChannel_NarrativeWeapon),
		ECR_Block);

	Meeting->ApplyNonCombatCollision();

	TestEqual(TEXT("Weapon traces ignore the trigger"),
		ListenerRange->GetCollisionResponseToChannel(TraceChannel_NarrativeWeapon), ECR_Ignore);
	TestEqual(TEXT("Projectile traces ignore the trigger"),
		ListenerRange->GetCollisionResponseToChannel(TraceChannel_NarrativeProjectile), ECR_Ignore);
	TestEqual(TEXT("Cover traces ignore the trigger"),
		ListenerRange->GetCollisionResponseToChannel(TraceChannel_NarrativeCover), ECR_Ignore);
	TestEqual(TEXT("Interaction traces ignore the trigger"),
		ListenerRange->GetCollisionResponseToChannel(TraceChannel_NarrativeInteraction), ECR_Ignore);

	// The meeting cannot start at all if this overlap response is lost.
	TestEqual(TEXT("Pawn overlap still triggers the meeting"),
		ListenerRange->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
	TestTrue(TEXT("Overlap events stay enabled"), ListenerRange->GetGenerateOverlapEvents());

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

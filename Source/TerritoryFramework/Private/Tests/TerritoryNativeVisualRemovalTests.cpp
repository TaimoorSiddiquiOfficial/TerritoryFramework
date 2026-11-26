#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNativeClientVisualRemoval,
 "TerritoryFramework.NativeIntegration.Regression.ClientVisualLeaderRemoval",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNativeClientVisualRemoval::RunTest(const FString& Parameters)
{
 USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
 if (!TestNotNull(TEXT("Engine skeletal fixture exists"), Asset)) return false;
 for (ENetRole Role : {ROLE_SimulatedProxy, ROLE_AutonomousProxy, ROLE_Authority})
 {
  UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
  if (!TestNotNull(TEXT("Removal world exists"), World)) return false;
  GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
  ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
  TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
  auto* Character = World->SpawnActor<ATerritoryGuardCharacter>();
  auto* Visual = World->SpawnActor<ANarrativeCharacterVisual>();
  if (!Character || !Visual) return false;
  Visual->SetOwner(Character);
  FindFProperty<FObjectPropertyBase>(Character->GetClass(), TEXT("CharVisual"))->SetObjectPropertyValue_InContainer(Character, Visual);
  Character->GetMesh()->SetSkeletalMeshAsset(Asset);
  auto* Body = NewObject<USkeletalMeshComponent>(Visual);
  Visual->AddInstanceComponent(Body);
  Body->RegisterComponent();
  Body->SetSkeletalMeshAsset(Asset);
  Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
  Body->SetLeaderPoseComponent(Character->GetMesh());
  const int32 BoneCount = Body->GetEditableBoneVisibilityStates().Num();
  TestTrue(TEXT("Fixture has real bone visibility"), BoneCount > 0);
  TestEqual(TEXT("Follower intentionally uses leader instead of own pose buffers"), Body->GetNumComponentSpaceTransforms(), 0);

  // A second follower of a different live actor must retain its authored leader.
  auto* Other = World->SpawnActor<ATerritoryGuardCharacter>();
  if (!TestNotNull(TEXT("Unrelated character exists"), Other)) return false;
  Other->GetMesh()->SetSkeletalMeshAsset(Asset);
  auto* Independent = NewObject<USkeletalMeshComponent>(Visual);
  Visual->AddInstanceComponent(Independent);
  Independent->RegisterComponent();
  Independent->SetSkeletalMeshAsset(Asset);
  Independent->SetLeaderPoseComponent(Other->GetMesh());

  Character->SetRole(Role);
  Visual->SetRole(Role == ROLE_Authority ? ROLE_Authority : ROLE_SimulatedProxy);
  // This is the actor-channel removal boundary. Native then tries ordinary
  // CharVisual->Destroy(), which is correctly rejected for a simulated visual.
  TestTrue(TEXT("Network can remove its character replica"), World->DestroyActor(Character, true));
  if (Role == ROLE_Authority)
  {
   TestTrue(TEXT("Native server still owns and completes visual destruction"), !IsValid(Visual) || Visual->IsActorBeingDestroyed());
   continue;
  }
  TestTrue(TEXT("Client adapter preserves Native's separate replicated visual lifetime"), IsValid(Visual) && !Visual->IsActorBeingDestroyed());
  TestFalse(TEXT("Retired client leader cannot leave a stale weak pose link"), Body->LeaderPoseComponent.IsStale());
  TestFalse(TEXT("Retired leader is detached"), Body->LeaderPoseComponent.IsValid());
  TestEqual(TEXT("Detached follower has valid local pose buffers"), Body->GetNumComponentSpaceTransforms(), BoneCount);
  TestEqual(TEXT("Unrelated live leader remains unchanged"), Independent->LeaderPoseComponent.Get(), static_cast<USkinnedMeshComponent*>(Other->GetMesh()));
  if (Body->GetNumComponentSpaceTransforms() == BoneCount)
  {
   // Previously this exact Engine entrypoint produced the gameplay ensure.
   Body->RecalcRequiredBones(0);
  }
  Body->SetLeaderPoseComponent(Other->GetMesh());
  TestEqual(TEXT("Normal Native rebind remains usable after cleanup"), Body->LeaderPoseComponent.Get(), static_cast<USkinnedMeshComponent*>(Other->GetMesh()));
  TestEqual(TEXT("Rebound follower resumes ordinary leader pose storage"), Body->GetNumComponentSpaceTransforms(), 0);
 }
 return true;
}
#endif

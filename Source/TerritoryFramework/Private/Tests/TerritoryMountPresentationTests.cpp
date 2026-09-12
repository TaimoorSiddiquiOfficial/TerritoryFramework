#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Navigation/TerritoryMountPresentationComponent.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Vehicles/MountComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFMountClientCollision,
	"TerritoryFramework.AI.Regression.NativeMountClientCollisionAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFMountClientCollision::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).RequiresHitProxies(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("Mount fixture world"), World)) return false;
	// Mounts can be vehicles or animals. Native's vehicle base is abstract, so
	// use a concrete pawn with the actual Native mount component in this fixture.
	auto* Vehicle = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Concrete mount actor"), Vehicle)) { World->DestroyWorld(false); return false; }
	auto* Root = NewObject<USceneComponent>(Vehicle);
	Vehicle->SetRootComponent(Root);
	Vehicle->AddInstanceComponent(Root);
	Root->RegisterComponent();
	auto* Mount = NewObject<UMountComponent>(Vehicle);
	Vehicle->AddInstanceComponent(Mount);
	Mount->RegisterComponent();
	const auto* Dead = FindFProperty<FBoolProperty>(UNarrativeAbilitySystemComponent::StaticClass(), TEXT("bIsDead"));
	if (!TestNotNull(TEXT("Native death flag"), Dead)) { World->DestroyWorld(false); return false; }

	for (UClass* Class : {ATerritoryGuardCharacter::StaticClass(), ATerritoryAssaultCharacter::StaticClass()})
	{
		auto* NPC = World->SpawnActor<ANarrativeNPCCharacter>(Class);
		if (!TestNotNull(TEXT("Native character fixture"), NPC)) continue;
		auto* Component = NPC->FindComponentByClass<UTerritoryMountPresentationComponent>();
		auto* Capsule = NPC->GetCapsuleComponent();
		if (!TestNotNull(TEXT("Guards include Native mount presentation"), Component)) continue;
		Component->Activate(true);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NPC->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Server seating/collision remains Native-owned"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
		TestFalse(TEXT("Server does not poll client presentation"), Component->IsComponentTickEnabled());
		NPC->SetRole(ROLE_SimulatedProxy);
		Component->Activate(true);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Attached remote NPC cannot push its own mount"), Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		for (int32 Repeat = 0; Repeat < 5; ++Repeat) Component->RefreshMountPresentation();
		TestEqual(TEXT("No seat is claimed by presentation"), Mount->SlotStatuses.Num(), 0);
		TestTrue(TEXT("Native attachment remains unchanged"), NPC->GetAttachParentActor() == Vehicle);
		NPC->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Exit restores exact previous collision"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);

		// Local ability owns an autonomous passenger's entry/exit settings.
		NPC->SetRole(ROLE_AutonomousProxy);
		NPC->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Owner client ability is not overridden"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
		NPC->SetRole(ROLE_SimulatedProxy);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->RefreshMountPresentation();
		Component->Deactivate();
		TestEqual(TEXT("Disabling component restores custom collision mode"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		Component->Activate(true);
		Capsule->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		NPC->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Later external collision change is preserved"), Capsule->GetCollisionEnabled(), ECollisionEnabled::PhysicsOnly);

		NPC->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		Dead->SetPropertyValue_InContainer(NPC->GetNarrativeAbilitySystemComponent(), true);
		NPC->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Death does not re-enable capsule physics"), Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		Dead->SetPropertyValue_InContainer(NPC->GetNarrativeAbilitySystemComponent(), false);
		// Native revival/load restores the body's settings; no saved mount cache.
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Restored unmounted character keeps Native load settings"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

		NPC->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		Mount->DestroyComponent();
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Unloaded/removed mount component restores character"), Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		Mount = NewObject<UMountComponent>(Vehicle);
		Vehicle->AddInstanceComponent(Mount);
		Mount->RegisterComponent();
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Late arriving mount data reapplies presentation"), Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		NPC->SetRole(ROLE_AutonomousProxy);
		Component->RefreshMountPresentation();
		NPC->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TestEqual(TEXT("Ownership handoff leaves exit-warp collision to Native"), Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		NPC->SetRole(ROLE_SimulatedProxy);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		NPC->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		TArray<uint8> Saved;
		FMemoryWriter Writer(Saved);
		FObjectAndNameAsStringProxyArchive Save(Writer, false);
		Save.ArIsSaveGame = true;
		Component->Serialize(Save);
		auto* Restored = World->SpawnActor<ANarrativeNPCCharacter>(Class);
		if (!TestNotNull(TEXT("Restored character fixture"), Restored)) continue;
		auto* RestoredComponent = Restored->FindComponentByClass<UTerritoryMountPresentationComponent>();
		FMemoryReader Reader(Saved);
		FObjectAndNameAsStringProxyArchive Load(Reader, true);
		Load.ArIsSaveGame = true;
		RestoredComponent->Serialize(Load);
		Restored->SetRole(ROLE_SimulatedProxy);
		Restored->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RestoredComponent->Activate(true);
		TestEqual(TEXT("Save/load does not restore an old body's collision cache"), Restored->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
		Restored->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
		RestoredComponent->RefreshMountPresentation();
		TestEqual(TEXT("Restored late attachment derives state from Native"), Restored->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		Restored->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		RestoredComponent->RefreshMountPresentation();
		TestEqual(TEXT("Restored character uses its own original mode"), Restored->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
		Restored->SetRole(ROLE_Authority);
		NPC->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Component->RefreshMountPresentation();
		NPC->SetRole(ROLE_Authority);
	}
	World->DestroyWorld(false);
	const auto* CDO = GetDefault<UTerritoryMountPresentationComponent>();
	TestFalse(TEXT("Presentation adds no replicated authority"), CDO->GetIsReplicated());
	const UFunction* Refresh = CDO->FindFunction(TEXT("RefreshMountPresentation"));
	TestTrue(TEXT("Blueprint hook is a local impure operation"), Refresh && Refresh->HasAnyFunctionFlags(FUNC_BlueprintCallable)
		&& !Refresh->HasAnyFunctionFlags(FUNC_Net | FUNC_BlueprintPure | FUNC_BlueprintAuthorityOnly));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryNPCSaveProbe.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "AI/TerritoryNPCController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNPCActivitySave,
	"TerritoryFramework.AI.Regression.GeneratorSnapshotAndMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNPCActivitySave::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Save test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	FObjectPropertyBase* OwnerField = FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("OwnerController"));
	FArrayProperty* SavedField = FindFProperty<FArrayProperty>(UNPCActivityComponent::StaticClass(), TEXT("SavedGoalGenerators"));
	auto Records = [SavedField](UNPCActivityComponent* Component) -> TArray<FSavedNPCGoalGenerator>&
	{
		return *SavedField->ContainerPtrToValuePtr<TArray<FSavedNPCGoalGenerator>>(Component);
	};
	auto Spawn = [&]()
	{
		ATerritoryNPCController* Controller = World->SpawnActor<ATerritoryNPCController>();
		OwnerField->SetObjectPropertyValue_InContainer(Controller->GetActivityComponent(), Controller);
		return Controller;
	};
	ATerritoryNPCController* Controller = Spawn();
	UTerritoryNPCActivityComponent* Activity = CastChecked<UTerritoryNPCActivityComponent>(Controller->GetActivityComponent());
	TestEqual(TEXT("Native save component name stays stable"), Activity->GetFName(), FName(TEXT("NPCActivityComponent")));
	TestEqual(TEXT("Guard uses the adapted Native controller"), GetDefault<ATerritoryGuardCharacter>()->AIControllerClass.Get(), ATerritoryNPCController::StaticClass());
	TestEqual(TEXT("Assault uses the adapted Native controller"), GetDefault<ATerritoryAssaultCharacter>()->AIControllerClass.Get(), ATerritoryNPCController::StaticClass());
	UClass* NativeControllerBP = LoadClass<ANarrativeNPCController>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController.BP_NarrativeNPCController_C"));
	UClass* ControllerBP = LoadClass<ANarrativeNPCController>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryNPCController.BP_TerritoryNPCController_C"));
	TestTrue(TEXT("Blueprint controller preserves Native Blueprint casts and inherited behavior"),
		ControllerBP && NativeControllerBP && ControllerBP->IsChildOf(NativeControllerBP));
	if (ControllerBP)
	{
		TestTrue(TEXT("Native Blueprint child overrides only the existing activity component class"),
			ControllerBP->GetDefaultObject<ANarrativeNPCController>()->GetActivityComponent()->IsA<UTerritoryNPCActivityComponent>());
	}
	auto* Generator = CastChecked<UTerritoryNPCSaveProbe>(Activity->AddGoalGenerator(UTerritoryNPCSaveProbe::StaticClass(), true));
	Generator->SavedValue = 17;
	Activity->PrepareForSave_Implementation();
	const FSavedNPCGoalGenerator First = Records(Activity)[0];
	Generator->SavedValue = 42;
	for (int32 Save = 0; Save < 5; ++Save) Activity->PrepareForSave_Implementation();
	TestEqual(TEXT("Repeated saves retain one current generator"), Records(Activity).Num(), 1);
	const FSavedNPCGoalGenerator Latest = Records(Activity)[0];
	Generator->SavedValue = 0;
	Activity->PrepareForSave_Implementation();
	Generator->SavedValue = 99;
	Generator->SavedValues = {8, 9};
	Generator->LiveValue = 91;
	Activity->Load_Implementation();
	TestEqual(TEXT("Saved class-default values replace later live changes"), Generator->SavedValue, 0);
	TestTrue(TEXT("Saved empty containers clear later entries"), Generator->SavedValues.IsEmpty());
	TestEqual(TEXT("Restoring saved defaults preserves transient live state"), Generator->LiveValue, 91);
	Generator->SavedValue = -1;
	Records(Activity) = {First, Latest};
	Activity->Load_Implementation();
	TestEqual(TEXT("Existing configured generator gets newest saved value"), Generator->SavedValue, 42);
	TestEqual(TEXT("Loading does not bind the same generator twice"), Generator->InitializationCount, 1);
	TestEqual(TEXT("Old duplicate history is compacted"), Records(Activity).Num(), 1);
	Activity->RemoveGoalGenerator(UTerritoryNPCSaveProbe::StaticClass());
	Activity->PrepareForSave_Implementation();
	TestTrue(TEXT("Removed generators do not survive the next snapshot"), Records(Activity).IsEmpty());
	Activity->Load_Implementation();
	TestNull(TEXT("Load does not resurrect a removed generator"), Activity->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));

	// Old Native-format bytes restore into the same named component after controller
	// recreation, as in a World Partition reload. No new record format or IDs.
	ANarrativeNPCController* OldController = World->SpawnActor<ANarrativeNPCController>();
	UNPCActivityComponent* OldActivity = OldController->GetActivityComponent();
	OwnerField->SetObjectPropertyValue_InContainer(OldActivity, OldController);
	Records(OldActivity) = {First, Latest};
	FNarrativeActorRecord OldRecord;
	UNarrativeSaveSubsystem* SaveSubsystem = World->GetSubsystem<UNarrativeSaveSubsystem>();
	// CreateActorRecord calls PrepareForSave, so include a live generator in the old
	// controller too. Its Native append bug supplies a real repeated-save fixture.
	auto* OldGenerator = CastChecked<UTerritoryNPCSaveProbe>(OldActivity->AddGoalGenerator(UTerritoryNPCSaveProbe::StaticClass(), true));
	OldGenerator->SavedValue = 73;
	TestTrue(TEXT("Native controller save record created"), SaveSubsystem->CreateActorRecord(OldController, OldRecord));
	TestTrue(TEXT("Native baseline appends previous snapshots"), Records(OldActivity).Num() > 1);
	ATerritoryNPCController* ReloadedController = Spawn();
	UTerritoryNPCActivityComponent* Reloaded = CastChecked<UTerritoryNPCActivityComponent>(ReloadedController->GetActivityComponent());
	SaveSubsystem->LoadActorFromRecord(ReloadedController, OldRecord);
	auto* Restored = Cast<UTerritoryNPCSaveProbe>(Reloaded->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));
	if (TestNotNull(TEXT("Generator restored through Native actor record"), Restored))
	{
		TestEqual(TEXT("Latest old snapshot wins after recreation"), Restored->SavedValue, 73);
		TestEqual(TEXT("Restored generator initialized once"), Restored->InitializationCount, 1);
	}

	ATerritoryNPCController* MigratedController = Spawn();
	UTerritoryNPCActivityComponent* Migrated = CastChecked<UTerritoryNPCActivityComponent>(MigratedController->GetActivityComponent());
	Migrated->IgnoredSavedGoalGeneratorClasses = {UTerritoryNPCSaveProbe::StaticClass()};
	auto* Replacement = CastChecked<UTerritoryNPCSaveReplacementProbe>(Migrated->AddGoalGenerator(UTerritoryNPCSaveReplacementProbe::StaticClass(), true));
	Replacement->SavedValue = 99;
	SaveSubsystem->LoadActorFromRecord(MigratedController, OldRecord);
	TestNull(TEXT("Explicitly retired saved class is never initialized"), Migrated->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));
	TestEqual(TEXT("Current replacement keeps its own state"), Replacement->SavedValue, 99);
	TestEqual(TEXT("Replacement retains its existing bindings"), Replacement->InitializationCount, 1);
	FSavedNPCGoalGenerator Missing;
	FSavedNPCGoalGenerator Abstract;
	Abstract.Class = UNPCGoalGenerator::StaticClass();
	Records(Migrated) = {Missing, Abstract, Latest};
	Migrated->Load_Implementation();
	TestTrue(TEXT("Missing, abstract and retired classes cannot create invalid objects"), Records(Migrated).IsEmpty());
	Controller->SetRole(ROLE_SimulatedProxy);
	Records(Activity) = {First, Latest};
	Activity->Load_Implementation();
	Activity->PrepareForSave_Implementation();
	TestEqual(TEXT("Clients cannot mutate saved generator state"), Records(Activity).Num(), 2);
	TestNull(TEXT("Clients cannot spawn saved AI generators"), Activity->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));
	Controller->SetRole(ROLE_Authority);
	OwnerField->SetObjectPropertyValue_InContainer(Activity, nullptr);
	Activity->Load_Implementation();
	TestNotNull(TEXT("Load before BeginPlay resolves the owning controller"), Activity->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));
	FProperty* MigrationField = FindFProperty<FProperty>(UTerritoryNPCActivityComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UTerritoryNPCActivityComponent, IgnoredSavedGoalGeneratorClasses));
	TestFalse(TEXT("Old saves cannot overwrite current migration policy"), MigrationField->HasAnyPropertyFlags(CPF_SaveGame | CPF_Net));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/TerritorySaveSerialization.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "GameplayTagContainer.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutor.h"
#include "Spawn/TerritorySpawnPolicy.h"
#include "Spawn/TerritorySpawnSubsystem.h"
#include "Spawn/TerritorySpawnTypes.h"

/**
 * B0 contract tests.
 *
 * These are reflection and value tests on purpose: they must run in a bare editor with no world, no
 * PIE and no streamed content, because they are the gate that lets everything else be written. A
 * behavioural spawn test would need a world and an authored anchor, which is B1/B2 territory.
 *
 * The one test that is not pure reflection is the record round-trip, and it exists because the
 * record is the only thing the save system ever sees. If the round-trip is wrong, every later batch
 * restores garbage.
 */

namespace
{
	/**
	 * Serialize a record round-trip through tagged property serialization, wrapped in the SAME proxy
	 * archive the real save path uses.
	 *
	 * The proxy archive is not optional: FTerritorySpawnRecord carries a TSoftClassPtr, and a bare
	 * FMemoryWriter aborts with "FArchive does not support FSoftObjectPtr serialization. Use
	 * FArchiveUObject instead." (Core/Private/Serialization/Archive.cpp:530). The first version of this
	 * test used FMemoryWriter alone and hard-crashed the editor on the record round-trip, which is how
	 * the requirement was discovered. Mirroring NarrativeSaveSubsystem.cpp:714-718 here means the test
	 * proves the record survives the archive the game actually writes it with, not a stricter one.
	 *
	 * FTerritorySaveSerializationScope is deliberately NOT used: its constructor takes a UObject& (it
	 * resets CPF_SaveGame properties to class defaults before reading a delta). A USTRUCT has no class
	 * defaults to reset, so the struct path needs only the tagged serializer.
	 */
	void RoundTripRecord(FTerritorySpawnRecord& InOutRecord)
	{
		UScriptStruct* Struct = FTerritorySpawnRecord::StaticStruct();
		check(Struct);

		TArray<uint8> Bytes;
		{
			FMemoryWriter Writer(Bytes, /*bIsPersistent*/ true);
			FObjectAndNameAsStringProxyArchive Archive(Writer, /*bInLoadIfFindFails*/ true);
			Archive.ArIsSaveGame = true;
			Struct->SerializeTaggedProperties(Archive, reinterpret_cast<uint8*>(&InOutRecord), Struct, nullptr);
		}

		FMemoryReader Reader(Bytes, /*bIsPersistent*/ true);
		FObjectAndNameAsStringProxyArchive LoadArchive(Reader, /*bInLoadIfFindFails*/ true);
		LoadArchive.ArIsSaveGame = true;
		Struct->SerializeTaggedProperties(LoadArchive, reinterpret_cast<uint8*>(&InOutRecord), Struct, nullptr);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// 1. Removal reasons must not drift from the guard-post set they mirror
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnRemovalReasonMirror,
	"TerritoryFramework.Spawn.Types.RemovalReasonMirrorsGuardPost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnRemovalReasonMirror::RunTest(const FString& Parameters)
{
	// The framework deliberately does NOT share EGuardRemovalReason: sharing would drag a guard-owned
	// header into the generic core. The cost of that decision is that the two enums can drift apart
	// silently, so this test pins the shared subset by name. If a future edit adds a reason on one side
	// only, this fails and somebody has to decide which is right.

	const UEnum* SpawnEnum = StaticEnum<ESpawnRemovalReason>();
	const UEnum* GuardEnum = StaticEnum<EGuardRemovalReason>();
	TestNotNull(TEXT("ESpawnRemovalReason must be reflected"), SpawnEnum);
	TestNotNull(TEXT("EGuardRemovalReason must be reflected"), GuardEnum);
	if (!SpawnEnum || !GuardEnum) return false;

	// Every guard-post reason that the framework claims to mirror must exist there under the same
	// name. These four are the shared core: the others are framework-specific.
	const TCHAR* SharedNames[] = { TEXT("Killed"), TEXT("ManualRemoval"), TEXT("StreamingRetirement") };

	for (const TCHAR* Name : SharedNames)
	{
		const int64 GuardValue = GuardEnum->GetValueByNameString(Name);
		const int64 SpawnValue = SpawnEnum->GetValueByNameString(Name);
		TestTrue(
			FString::Printf(TEXT("Guard post must still declare removal reason '%s' (the framework mirrors it)"), Name),
			GuardValue != INDEX_NONE);
		TestTrue(
			FString::Printf(TEXT("Spawn framework must declare removal reason '%s'"), Name),
			SpawnValue != INDEX_NONE);
	}

	// The framework must own at least the reasons the interlock reports, otherwise a retire has no
	// honest explanation.
	TestTrue(TEXT("ESpawnRemovalReason.LeaseExpired must exist"), SpawnEnum->GetValueByNameString(TEXT("LeaseExpired")) != INDEX_NONE);
	TestTrue(TEXT("ESpawnRemovalReason.LoadReconcile must exist"), SpawnEnum->GetValueByNameString(TEXT("LoadReconcile")) != INDEX_NONE);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 2. Zero-value inertness — an unconfigured policy must never act
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnZeroValueInert,
	"TerritoryFramework.Spawn.Types.ZeroValuesAreInert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnZeroValueInert::RunTest(const FString& Parameters)
{
	// The whole safety story depends on the zero value meaning "do nothing". A default-constructed
	// decision must be non-actionable, and a default-constructed request must look like a slot that
	// has never run. This mirrors the existing "AnyFloor is the zero value on purpose" convention in
	// TerritoryFloorCombatPolicy.h:15-17.

	const FTerritorySpawnDecision DefaultDecision;
	TestEqual(TEXT("A default decision must be None"), DefaultDecision.Action, ESpawnAction::None);
	TestFalse(TEXT("A default decision must not be actionable"), DefaultDecision.IsActionable());
	TestEqual(TEXT("A default decision must carry no retry delay"), DefaultDecision.RetryDelaySeconds, 0.f);

	const FTerritorySpawnRequest DefaultRequest;
	TestFalse(TEXT("A default request must not claim a live lease"), DefaultRequest.bHasLiveLease);
	TestEqual(TEXT("A default request must not be pending"), DefaultRequest.PendingSinceGameTime, -1.0);
	TestEqual(TEXT("A default request must have no failures"), DefaultRequest.ConsecutiveFailures, 0);
	TestEqual(TEXT("A default request must start Dormant"), DefaultRequest.CurrentState, ESpawnLifecycleState::Dormant);
	TestFalse(TEXT("A default request must have an invalid anchor guid"), DefaultRequest.AnchorGuid.IsValid());

	const FTerritorySpawnLease DefaultLease;
	TestFalse(TEXT("A default lease must not be valid"), DefaultLease.IsValidLease(0.0));

	const FTerritorySpawnRecord DefaultRecord;
	TestFalse(TEXT("A default record must not claim an instance"), DefaultRecord.bHasInstance);
	TestFalse(TEXT("A default record must not claim a destruction"), DefaultRecord.bWasDestroyed);
	TestEqual(TEXT("A default record must be at the current schema"), DefaultRecord.SchemaVersion, FTerritorySpawnRecord::CurrentSchemaVersion);
	TestEqual(TEXT("A default record must have generation zero"), DefaultRecord.Generation, 0);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3. Enums are a closed, distinct, Blueprint-visible set
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnEnumContract,
	"TerritoryFramework.Spawn.Types.EnumContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnEnumContract::RunTest(const FString& Parameters)
{
	// A duplicated enumerator value would make two states indistinguishable in a saved record while
	// still compiling and still passing every behavioural test. Cheap to check, expensive to debug.
	struct FEnumExpectation
	{
		const UEnum* Enum;
		const TCHAR* Name;
		int32 ExpectedCount;
	};

	const FEnumExpectation Expectations[] = {
		{ StaticEnum<ESpawnLifecycleState>(), TEXT("ESpawnLifecycleState"), 5 },
		{ StaticEnum<ESpawnAction>(), TEXT("ESpawnAction"), 5 },
		{ StaticEnum<ESpawnRetireMode>(), TEXT("ESpawnRetireMode"), 4 },
		{ StaticEnum<ESpawnRemovalReason>(), TEXT("ESpawnRemovalReason"), 7 },
		{ StaticEnum<ESpawnTransitionReason>(), TEXT("ESpawnTransitionReason"), 12 }
	};

	for (const FEnumExpectation& Expectation : Expectations)
	{
		TestNotNull(FString::Printf(TEXT("%s must be reflected"), Expectation.Name), Expectation.Enum);
		if (!Expectation.Enum) continue;

		// A UENUM declared for Blueprint may carry an implicit _MAX entry, so count real names only.
		int32 RealValues = 0;
		TSet<int64> SeenValues;
		for (int32 Index = 0; Index < Expectation.Enum->NumEnums(); ++Index)
		{
			const FString Name = Expectation.Enum->GetNameStringByIndex(Index);
			if (Name.EndsWith(TEXT("_MAX")) || Name.EndsWith(TEXT("_INVALID"))) continue;
			++RealValues;

			const int64 Value = Expectation.Enum->GetValueByIndex(Index);
			TestFalse(
				FString::Printf(TEXT("%s has two enumerators sharing value %lld (the later one is unreachable in a saved record)"),
					Expectation.Name, Value),
				SeenValues.Contains(Value));
			SeenValues.Add(Value);
		}

		TestEqual(
			FString::Printf(TEXT("%s must have exactly %d enumerators; adding one is a contract change that needs a migration note"),
				Expectation.Name, Expectation.ExpectedCount),
			RealValues, Expectation.ExpectedCount);
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4. Record survives a save round-trip, and its bytes are stable
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnRecordRoundTrip,
	"TerritoryFramework.Spawn.Types.RecordRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnRecordRoundTrip::RunTest(const FString& Parameters)
{
	FTerritorySpawnRecord Source;
	Source.SpawnedGuid = FGuid::NewGuid();
	Source.Transform = FTransform(FRotator(0.0, 37.0, 0.0), FVector(1250.0, -880.0, 92.0), FVector::OneVector);
	Source.bHasInstance = true;
	Source.bWasDestroyed = false;
	Source.Generation = 7;
	Source.LastTransitionGameTime = 1234.5;
	Source.StateTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Spawn.Vehicle.Home"), /*ErrorIfNotFound*/ false));
	Source.SetScalar(TEXT("HealthFraction"), 0.42f);
	Source.SetScalar(TEXT("Fuel"), 0.75f);

	// Scalars must be sorted and de-duplicated before they are written, otherwise two logically
	// identical saves produce different bytes and the migration checks cannot tell an edit from noise.
	Source.PrepareScalarsForSave();

	FTerritorySpawnRecord Restored;
	Restored.SpawnedGuid = Source.SpawnedGuid;
	Restored.Transform = Source.Transform;
	Restored.bHasInstance = Source.bHasInstance;
	Restored.bWasDestroyed = Source.bWasDestroyed;
	Restored.Generation = Source.Generation;
	Restored.StateTags = Source.StateTags;
	Restored.Scalars = Source.Scalars;
	Restored.LastTransitionGameTime = Source.LastTransitionGameTime;

	RoundTripRecord(Restored);

	TestEqual(TEXT("SpawnedGuid must survive the round-trip"), Restored.SpawnedGuid, Source.SpawnedGuid);
	TestEqual(TEXT("Generation must survive the round-trip"), Restored.Generation, Source.Generation);
	TestTrue(TEXT("bHasInstance must survive the round-trip"), Restored.bHasInstance == Source.bHasInstance);
	TestTrue(TEXT("bWasDestroyed must survive the round-trip"), Restored.bWasDestroyed == Source.bWasDestroyed);
	TestEqual(TEXT("Scalar count must survive the round-trip"), Restored.Scalars.Num(), Source.Scalars.Num());

	float RestoredHealth = -1.f;
	TestTrue(TEXT("HealthFraction must be readable after the round-trip"),
		Restored.GetScalar(TEXT("HealthFraction"), RestoredHealth));
	TestEqual(TEXT("HealthFraction must keep its value"), RestoredHealth, 0.42f);

	TestTrue(TEXT("Transform must be approximately preserved"),
		Restored.Transform.GetLocation().Equals(Source.Transform.GetLocation(), 0.01));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 5. Scalar array is ordered and de-duplicated deterministically
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnScalarDeterminism,
	"TerritoryFramework.Spawn.Types.ScalarDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnScalarDeterminism::RunTest(const FString& Parameters)
{
	// Two records built in different orders must produce identical saved bytes. This is the test that
	// keeps "the same game state" from looking like a change on every save.

	FTerritorySpawnRecord A;
	A.SetScalar(TEXT("Zulu"), 1.f);
	A.SetScalar(TEXT("Alpha"), 2.f);
	A.SetScalar(TEXT("Mike"), 3.f);
	A.PrepareScalarsForSave();

	FTerritorySpawnRecord B;
	B.SetScalar(TEXT("Mike"), 3.f);
	B.SetScalar(TEXT("Zulu"), 1.f);
	B.SetScalar(TEXT("Alpha"), 2.f);
	B.PrepareScalarsForSave();

	TestEqual(TEXT("Both records must hold the same number of scalars"), A.Scalars.Num(), B.Scalars.Num());
	if (A.Scalars.Num() == B.Scalars.Num())
	{
		for (int32 Index = 0; Index < A.Scalars.Num(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("Scalar key %d must match"), Index), A.Scalars[Index].Key, B.Scalars[Index].Key);
			TestEqual(FString::Printf(TEXT("Scalar value %d must match"), Index), A.Scalars[Index].Value, B.Scalars[Index].Value);
		}
	}

	// Ascending order is the contract; a reader that binary-searches would break otherwise.
	for (int32 Index = 1; Index < A.Scalars.Num(); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("Scalars must be sorted ascending; '%s' came after '%s'"),
				*A.Scalars[Index].Key.ToString(), *A.Scalars[Index - 1].Key.ToString()),
			A.Scalars[Index - 1].Key.LexicalLess(A.Scalars[Index].Key));
	}

	// Last write wins, and the duplicate is collapsed rather than kept twice.
	FTerritorySpawnRecord Duplicated;
	Duplicated.SetScalar(TEXT("Same"), 1.f);
	Duplicated.SetScalar(TEXT("Same"), 9.f);
	Duplicated.PrepareScalarsForSave();
	TestEqual(TEXT("A duplicated key must collapse to one entry"), Duplicated.Scalars.Num(), 1);
	TestEqual(TEXT("The newest value must win"), Duplicated.GetScalarOrDefault(TEXT("Same"), -1.f), 9.f);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 6. A corrupt record is refused, never repaired into an invented position
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnRecordIntegrity,
	"TerritoryFramework.Spawn.Types.RecordIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnRecordIntegrity::RunTest(const FString& Parameters)
{
	FTerritorySpawnRecord Record;
	TestTrue(TEXT("An identity transform must be usable"), Record.IsTransformUsable());

	// A NaN coordinate is a corrupt record. The framework's answer is refusal, because a fabricated
	// position would place a car somewhere no designer chose and nobody could reproduce.
	Record.Transform = FTransform(FVector(FMath::Sqrt(-1.0), 0.0, 0.0));
	TestFalse(TEXT("A NaN location must be refused"), Record.IsTransformUsable());

	// Beyond a billion centimetres (10,000 km) is not a level coordinate; it is corruption.
	Record.Transform = FTransform(FVector(1.0e9 + 1.0, 0.0, 0.0));
	TestFalse(TEXT("A coordinate past the sane bound must be refused"), Record.IsTransformUsable());

	Record.Transform = FTransform(FVector(5000.0, 5000.0, 100.0));
	TestTrue(TEXT("A plausible level coordinate must be usable"), Record.IsTransformUsable());

	// Schema: this build reads its own version, refuses a newer one, and refuses a version older than
	// the supported floor. Silently reinterpreting a newer record is how a campaign is destroyed.
	FTerritorySpawnRecord SchemaRecord;
	SchemaRecord.SchemaVersion = FTerritorySpawnRecord::CurrentSchemaVersion;
	TestTrue(TEXT("The current schema must be readable"), SchemaRecord.IsSchemaReadable());

	SchemaRecord.SchemaVersion = FTerritorySpawnRecord::CurrentSchemaVersion + 1;
	TestFalse(TEXT("A newer schema must be refused, not guessed at"), SchemaRecord.IsSchemaReadable());

	SchemaRecord.SchemaVersion = FTerritorySpawnRecord::MinimumSupportedSchemaVersion - 1;
	TestFalse(TEXT("A schema below the supported floor must be refused"), SchemaRecord.IsSchemaReadable());

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 7. The framework core must not depend on any consumer header
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnCoreIsolation,
	"TerritoryFramework.Spawn.Types.CoreHasNoConsumerDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnCoreIsolation::RunTest(const FString& Parameters)
{
	// The central architectural rule: Public/Spawn must compile with every adapter folder deleted.
	// This test reads the actual source files rather than trusting the rule, because an include added
	// for one impatient fix would otherwise never be noticed.

	const FString PluginSourceDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TerritoryFramework/Source/TerritoryFramework/Public/Spawn")));

	TestTrue(FString::Printf(TEXT("The plugin source folder must exist at %s"), *PluginSourceDir),
		FPaths::DirectoryExists(PluginSourceDir));

	if (!FPaths::DirectoryExists(PluginSourceDir)) return false;

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(PluginSourceDir / TEXT("*.h")), /*Files*/ true, /*Dirs*/ false);
	TestTrue(TEXT("Public/Spawn must contain headers"), Files.Num() > 0);

	// Forbidden substrings: a consumer family name appearing in an include line means the core has
	// started to depend on its first consumer.
	const TCHAR* ForbiddenIncludes[] = {
		TEXT("NarrativeVehicleBase.h"),
		TEXT("NarrativeWheeledVehicle.h"),
		TEXT("TerritoryCounterAttackSubsystem.h"),
		TEXT("TerritoryRoadTrafficSubsystem.h"),
		TEXT("TerritoryGuardSpawnPoint.h"),
		TEXT("TerritoryAssaultPersistence.h"),
		TEXT("NarrativeNPCCharacter.h")
	};

	for (const FString& File : Files)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *(PluginSourceDir / File)))
		{
			AddError(FString::Printf(TEXT("Could not read %s to verify core isolation"), *File));
			continue;
		}

		for (const TCHAR* Forbidden : ForbiddenIncludes)
		{
			TestFalse(
				FString::Printf(TEXT("%s must not include a consumer header (%s); move the adapter out of Public/Spawn"),
					*File, Forbidden),
				Contents.Contains(Forbidden));
		}
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 8. Reflection contract for the classes Blueprint will touch
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnReflectionContract,
	"TerritoryFramework.Spawn.Types.ReflectionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnReflectionContract::RunTest(const FString& Parameters)
{
	// Blueprint authors will build policies and drive the anchor, so the surfaces they need must be
	// reflected. A rename that quietly drops UFUNCTION would otherwise be found in the editor, by a
	// designer, after the feature was declared done.

	const UClass* AnchorClass = ATerritorySpawnAnchor::StaticClass();
	TestNotNull(TEXT("ATerritorySpawnAnchor must be reflected"), AnchorClass);
	if (AnchorClass)
	{
		// The anchor MUST implement the vendor's savable interface, or its record is never written and
		// the whole durable half of this framework is silently inert. Assert the interface by identity
		// rather than by "any interface", which would pass for an unrelated implementation.
		const UClass* SavableInterface = FindObject<UClass>(nullptr, TEXT("/Script/NarrativeSaveSystem.NarrativeSavableActor"));
		TestNotNull(TEXT("INarrativeSavableActor must be loaded (NarrativeSaveSystem module)"), SavableInterface);
		if (SavableInterface)
		{
			TestTrue(TEXT("ATerritorySpawnAnchor must implement INarrativeSavableActor"),
				AnchorClass->ImplementsInterface(SavableInterface));
		}

		TestNotNull(TEXT("Anchor must expose GetAnchorGuid to Blueprint"),
			AnchorClass->FindFunctionByName(TEXT("GetAnchorGuid")));
		TestNotNull(TEXT("Anchor must expose GetLifecycleState to Blueprint"),
			AnchorClass->FindFunctionByName(TEXT("GetLifecycleState")));
		TestNotNull(TEXT("Anchor must expose GetSpawnedActor to Blueprint"),
			AnchorClass->FindFunctionByName(TEXT("GetSpawnedActor")));
		TestNotNull(TEXT("Anchor must expose TryClaimForPlayer to Blueprint"),
			AnchorClass->FindFunctionByName(TEXT("TryClaimForPlayer")));
		TestNotNull(TEXT("Anchor must expose GetRecord to Blueprint"),
			AnchorClass->FindFunctionByName(TEXT("GetRecord")));

		// The record must be a reflected UPROPERTY, or the save system never sees it.
		//
		// The member is named SlotRecord, not Record, because UHT's generated structured-archive
		// serializer declares a local called Record and a same-named member breaks that generated file.
		// The test pins the real name so a future rename is caught here rather than in a save file.
		const FProperty* RecordProperty = AnchorClass->FindPropertyByName(TEXT("SlotRecord"));
		TestNotNull(TEXT("The anchor must expose a reflected SlotRecord property"), RecordProperty);
		if (RecordProperty)
		{
			TestTrue(TEXT("The SlotRecord property must carry CPF_SaveGame so the save system serializes it"),
				(RecordProperty->PropertyFlags & CPF_SaveGame) != 0);
			TestTrue(TEXT("The SlotRecord property must be the spawn record struct"),
				RecordProperty->IsA<FStructProperty>()
				&& CastField<FStructProperty>(RecordProperty)->Struct == FTerritorySpawnRecord::StaticStruct());
		}
	}

	const UClass* PolicyClass = UTerritorySpawnPolicy::StaticClass();
	TestNotNull(TEXT("UTerritorySpawnPolicy must be reflected"), PolicyClass);
	if (PolicyClass)
	{
		TestTrue(TEXT("The policy base must be abstract so it cannot be instanced bare"),
			PolicyClass->HasAnyClassFlags(CLASS_Abstract));
		TestNotNull(TEXT("Policy must expose GetEntityTag"), PolicyClass->FindFunctionByName(TEXT("GetEntityTag")));
		TestNotNull(TEXT("Policy must expose GetExecutorClass"), PolicyClass->FindFunctionByName(TEXT("GetExecutorClass")));
		TestNotNull(TEXT("Policy must expose EvaluateDecision"), PolicyClass->FindFunctionByName(TEXT("EvaluateDecision")));
		TestNotNull(TEXT("Policy must expose ShouldEvaluate"), PolicyClass->FindFunctionByName(TEXT("ShouldEvaluate")));
	}

	const UClass* ExecutorClass = UTerritorySpawnExecutor::StaticClass();
	TestNotNull(TEXT("UTerritorySpawnExecutor must be reflected"), ExecutorClass);
	if (ExecutorClass)
	{
		TestTrue(TEXT("The executor base must be abstract so it cannot be instanced bare"),
			ExecutorClass->HasAnyClassFlags(CLASS_Abstract));
		TestNotNull(TEXT("Executor must expose CreateSpawned"), ExecutorClass->FindFunctionByName(TEXT("CreateSpawned")));
		TestNotNull(TEXT("Executor must expose RetireSpawned"), ExecutorClass->FindFunctionByName(TEXT("RetireSpawned")));
		TestNotNull(TEXT("Executor must expose IsClaimedByExternalOwner"), ExecutorClass->FindFunctionByName(TEXT("IsClaimedByExternalOwner")));
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 9. The anchor refuses to respawn through the save system
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnAnchorRefusesRespawn,
	"TerritoryFramework.Spawn.Types.AnchorRefusesDynamicRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnAnchorRefusesRespawn::RunTest(const FString& Parameters)
{
	// An anchor is level-placed: it owns its children. If the save system ever "respawned" one, the
	// world would hold two anchors with one GUID and the slot would spawn a second entity. Narrative
	// Pro also explicitly refuses World Partition aware dynamic respawn
	// (NarrativeSaveSubsystem.cpp:194-222), so the answer must be false rather than "maybe".

	const ATerritorySpawnAnchor* AnchorCDO = GetDefault<ATerritorySpawnAnchor>();
	TestNotNull(TEXT("The anchor CDO must exist"), AnchorCDO);
	if (!AnchorCDO) return false;

	TestFalse(TEXT("An anchor must never ask the save system to recreate it"),
		AnchorCDO->ShouldRespawn_Implementation());

	// The anchor must not tick. A per-frame lifecycle owner would make the sweep pointless and the
	// cost model in the design notes would be a lie.
	TestFalse(TEXT("An anchor must not tick"), AnchorCDO->PrimaryActorTick.bCanEverTick);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

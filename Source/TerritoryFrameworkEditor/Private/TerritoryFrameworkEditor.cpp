#include "TerritoryFrameworkEditor.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Navigation/TerritoryRoadTrafficActors.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "TerritoryDefinitionEditorLibrary.h"
#include "Vehicles/Mass/MassVehicleSpawner.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawnerTypes.h"
#include "MassEntityConfigAsset.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

namespace
{
	/**
	 * Narrative's AMassVehicleSpawner constructor creates its generator with no
	 * explicit Outer. Blueprint instances can consequently retain an archetype
	 * reference to a private object owned by the Blueprint CDO, and Unreal then
	 * refuses to save the containing map. Recreate only the placed actor's
	 * generator as a clean instance owned by that actor; Narrative source remains
	 * untouched and the configured generator class/proportion are preserved.
	 */
	bool RepairMassVehicleSpawnGeneratorOwnership(AMassVehicleSpawner* Spawner)
	{
		if (!IsValid(Spawner)) return false;

		FArrayProperty* SpawnGeneratorsProperty = FindFProperty<FArrayProperty>(
			Spawner->GetClass(), TEXT("SpawnDataGenerators"));
		if (!SpawnGeneratorsProperty) return false;

		FScriptArrayHelper SpawnGenerators(SpawnGeneratorsProperty,
			SpawnGeneratorsProperty->ContainerPtrToValuePtr<void>(Spawner));
		bool bRepaired = false;
		for (int32 Index = 0; Index < SpawnGenerators.Num(); ++Index)
		{
			FMassSpawnDataGenerator* Generator =
				reinterpret_cast<FMassSpawnDataGenerator*>(
					SpawnGenerators.GetRawPtr(Index));
			if (!Generator) continue;

			UClass* GeneratorClass = Generator->GeneratorClass.Get();
			if (!GeneratorClass && Generator->GeneratorInstance)
			{
				GeneratorClass = Generator->GeneratorInstance->GetClass();
			}
			if (!GeneratorClass || !GeneratorClass->IsChildOf(
				UMassEntitySpawnDataGeneratorBase::StaticClass()))
			{
				continue;
			}

			const UObject* CurrentArchetype = Generator->GeneratorInstance
				? Generator->GeneratorInstance->GetArchetype() : nullptr;
			const bool bHasPrivateBlueprintArchetype = CurrentArchetype
				&& CurrentArchetype->GetOuter()
				&& CurrentArchetype->GetOuter()->HasAnyFlags(RF_ClassDefaultObject)
				&& CurrentArchetype->GetOuter()->GetClass()->ClassGeneratedBy;
			if (Generator->GeneratorInstance
				&& Generator->GeneratorInstance->GetOuter() == Spawner
				&& !bHasPrivateBlueprintArchetype)
			{
				continue;
			}

			Generator->GeneratorInstance =
				NewObject<UMassEntitySpawnDataGeneratorBase>(Spawner,
					GeneratorClass, NAME_None, RF_Transactional);
			bRepaired = Generator->GeneratorInstance != nullptr || bRepaired;
		}
		return bRepaired;
	}

	class FTerritoryDefinitionDetails final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FTerritoryDefinitionDetails>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			TArray<TWeakObjectPtr<UObject>> Objects;
			DetailBuilder.GetObjectsBeingCustomized(Objects);
			bool bContainsNonPlace = false;
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (Object.IsValid() && !Object->IsA<UTerritoryPlaceDefinition>())
				{
					bContainsNonPlace = true;
					break;
				}
			}
			if (!bContainsNonPlace) return;

			// These legacy serialized fields remain loadable for bounded migration, but
			// City/District authors cannot edit them. Runtime and validation enforce the
			// same Place-only boundary.
			const FName PlaceOnlyProperties[] = {
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialOwningFaction),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialState),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, MaxConcurrentAttackers),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PeriodicIncome),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardUpkeepPerCycle),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardRecruitmentCost),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultStealthProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, bStoryCaptureFromBounds),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CapturePoint),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultGuardDefinition),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FactionGuardDefinitions),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialGuardCount),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PostCaptureGarrisonPolicy),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardBehavior),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardPosts),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefenderDiedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, AllDefendersDefeatedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackApproaches),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardQuality),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FortificationStrength),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, NearbyAlliedSupport)
			};
			for (const FName PropertyName : PlaceOnlyProperties)
			{
				DetailBuilder.HideProperty(PropertyName, UTerritoryDefinition::StaticClass());
			}
		}
	};
}

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkEditorModule"

void FTerritoryFrameworkEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
		TEXT("PropertyEditor"));
	for (const FName ClassName : { FName(TEXT("TerritoryDefinition")),
		FName(TEXT("TerritoryPlaceDefinition")), FName(TEXT("TerritoryDistrictDefinition")),
		FName(TEXT("TerritoryCityDefinition")) })
	{
		PropertyEditor.RegisterCustomClassLayout(ClassName,
			FOnGetDetailCustomizationInstance::CreateStatic(
				&FTerritoryDefinitionDetails::MakeInstance));
	}
	PropertyEditor.NotifyCustomizationModuleChanged();
	EnsureVehicleRoadsConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.EnsureVehicleRoads"),
		TEXT("Create or update ZoneGraph roads and Blueprint Territory Road Guides for every enabled Narrative Vehicle approach in the loaded editor level."),
		FConsoleCommandDelegate::CreateRaw(
			this, &FTerritoryFrameworkEditorModule::EnsureVehicleRoadsForLoadedLevel),
		ECVF_Default);
	NormalizeClaimedDiplomacyConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.NormalizeClaimedDiplomacy"),
		TEXT("Migrate owner-relative Claimed state diplomacy rules in the loaded editor level to Neutral / No Treaty."),
		FConsoleCommandDelegate::CreateRaw(
			this, &FTerritoryFrameworkEditorModule::NormalizeClaimedDiplomacyForLoadedLevel),
		ECVF_Default);
	MigrateFactionSignatureVehiclesConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Territory.Editor.MigrateFactionSignatureVehicles"),
		TEXT("Copy the first authored vehicle-approach Blueprint into each blank faction Signature Vehicle in loaded Territory profiles. Save All after review."),
		FConsoleCommandDelegate::CreateRaw(this,
			&FTerritoryFrameworkEditorModule::MigrateFactionSignatureVehiclesForLoadedLevel),
		ECVF_Default);
	// UEditorValidator subclasses are auto-registered by the DataValidation system
	// when the module loads — no manual registration needed
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module loaded (auto-validators registered)"));
}

void FTerritoryFrameworkEditorModule::ShutdownModule()
{
	if (EnsureVehicleRoadsConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			EnsureVehicleRoadsConsoleCommand, false);
		EnsureVehicleRoadsConsoleCommand = nullptr;
	}
	if (NormalizeClaimedDiplomacyConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			NormalizeClaimedDiplomacyConsoleCommand, false);
		NormalizeClaimedDiplomacyConsoleCommand = nullptr;
	}
	if (MigrateFactionSignatureVehiclesConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(
			MigrateFactionSignatureVehiclesConsoleCommand, false);
		MigrateFactionSignatureVehiclesConsoleCommand = nullptr;
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor"));
		for (const FName ClassName : { FName(TEXT("TerritoryDefinition")),
			FName(TEXT("TerritoryPlaceDefinition")), FName(TEXT("TerritoryDistrictDefinition")),
			FName(TEXT("TerritoryCityDefinition")) })
		{
			PropertyEditor.UnregisterCustomClassLayout(ClassName);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module unloaded"));
}

void FTerritoryFrameworkEditorModule::MigrateFactionSignatureVehiclesForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryVehicleMigration] Load a non-PIE editor level first."));
		return;
	}

	int32 UpdatedForces = 0;
	TSet<UTerritoryCounterAttackProfile*> VisitedProfiles;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		UTerritoryCounterAttackProfile* Profile = IsValid(Territory)
			? Territory->GetCounterAttackProfile() : nullptr;
		if (!IsValid(Profile) || VisitedProfiles.Contains(Profile)) continue;
		VisitedProfiles.Add(Profile);

		TSoftClassPtr<ANarrativeVehicleBase> ApproachFallback;
		for (const FTerritoryAssaultApproach& Approach :
			Territory->GetCounterAttackApproaches())
		{
			if (Approach.bEnabled
				&& Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle
				&& !Approach.VehicleClass.IsNull())
			{
				ApproachFallback = Approach.VehicleClass;
				break;
			}
		}
		if (ApproachFallback.IsNull()) continue;

		for (FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
		{
			if (!Force.Faction.IsValid() || !Force.SignatureVehicleClass.IsNull()) continue;
			Profile->Modify();
			Force.SignatureVehicleClass = ApproachFallback;
			Profile->MarkPackageDirty();
			++UpdatedForces;
			UE_LOG(LogTemp, Display,
				TEXT("[TerritoryVehicleMigration] %s now uses %s as the signature car in %s."),
				*Force.Faction.ToString(), *ApproachFallback.ToString(),
				*Profile->GetPathName());
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryVehicleMigration] Updated %d faction force(s) in %d profile(s). Review the cars, then Save All."),
		UpdatedForces, VisitedProfiles.Num());
}

void FTerritoryFrameworkEditorModule::NormalizeClaimedDiplomacyForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryDiplomacyMigration] Load a non-PIE editor level first."));
		return;
	}

	int32 UpdatedEvents = 0;
	TSet<UTerritoryDefinition*> VisitedDefinitions;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!IsValid(Territory)) continue;

		// Runtime State Configs are deliberately transient clones. Migrate the
		// authoritative Definition template so the correction survives reload,
		// then rebuild the actor clone below.
		UTerritoryDefinition* Definition = Territory->GetTerritoryDefinition();
		if (!IsValid(Definition) || VisitedDefinitions.Contains(Definition)) continue;
		VisitedDefinitions.Add(Definition);

		FTerritoryStateConfig* ClaimedConfig =
			Definition->StateConfigs.Find(ETerritoryState::Claimed);
		if (!ClaimedConfig) continue;

		// Older Place assets sometimes retained the Contested=War rule but lost
		// the matching Claimed=Neutral cleanup event. Repair that pair only when
		// the author has clearly opted into state-driven diplomacy.
		UTerritorySetDiplomacyEvent* ExistingClaimedDiplomacy = nullptr;
		for (const TObjectPtr<UNarrativeEvent>& Event : ClaimedConfig->EntryEvents)
		{
			if (UTerritorySetDiplomacyEvent* Typed =
				Cast<UTerritorySetDiplomacyEvent>(Event))
			{
				ExistingClaimedDiplomacy = Typed;
				break;
			}
		}
		const UTerritorySetDiplomacyEvent* ContestedWarTemplate = nullptr;
		if (const FTerritoryStateConfig* ContestedConfig =
			Definition->StateConfigs.Find(ETerritoryState::Contested))
		{
			for (const TObjectPtr<UNarrativeEvent>& Event : ContestedConfig->EntryEvents)
			{
				const UTerritorySetDiplomacyEvent* Typed =
					Cast<UTerritorySetDiplomacyEvent>(Event);
				if (Typed && Typed->NewState == EDiplomacyState::War)
				{
					ContestedWarTemplate = Typed;
					break;
				}
			}
		}
		if (!ExistingClaimedDiplomacy && ContestedWarTemplate)
		{
			Definition->Modify();
			UTerritorySetDiplomacyEvent* PeaceEvent =
				NewObject<UTerritorySetDiplomacyEvent>(Definition, NAME_None,
					RF_Transactional);
			PeaceEvent->FactionASource =
				ETerritoryDiplomacyFactionSource::CurrentOwningFaction;
			PeaceEvent->FactionBSource =
				ETerritoryDiplomacyFactionSource::TransitionOpposingFaction;
			PeaceEvent->FactionA = ContestedWarTemplate->FactionA;
			PeaceEvent->FactionB = ContestedWarTemplate->FactionB;
			PeaceEvent->NewState = EDiplomacyState::None;
			PeaceEvent->bApplyWhenStateStartsActive = true;
			PeaceEvent->bRequireContainingTerritoryOwner = true;
			PeaceEvent->bPreserveOtherActiveTerritoryWars = true;
			ClaimedConfig->EntryEvents.Add(PeaceEvent);
			Definition->MarkPackageDirty();
			++UpdatedEvents;
		}

		for (const TObjectPtr<UNarrativeEvent>& EntryEvent :
			ClaimedConfig->EntryEvents)
		{
			UTerritorySetDiplomacyEvent* DiplomacyEvent =
				Cast<UTerritorySetDiplomacyEvent>(EntryEvent);
			if (!DiplomacyEvent
				|| DiplomacyEvent->FactionASource !=
					ETerritoryDiplomacyFactionSource::CurrentOwningFaction
				|| DiplomacyEvent->FactionBSource !=
					ETerritoryDiplomacyFactionSource::TransitionOpposingFaction)
			{
				continue;
			}

			const bool bNeedsMigration =
				DiplomacyEvent->NewState != EDiplomacyState::None
				|| !DiplomacyEvent->bApplyWhenStateStartsActive
				|| !DiplomacyEvent->bRequireContainingTerritoryOwner
				|| !DiplomacyEvent->bPreserveOtherActiveTerritoryWars;
			if (bNeedsMigration)
			{
				Definition->Modify();
				DiplomacyEvent->Modify();
				DiplomacyEvent->NewState = EDiplomacyState::None;
				DiplomacyEvent->bApplyWhenStateStartsActive = true;
				DiplomacyEvent->bRequireContainingTerritoryOwner = true;
				DiplomacyEvent->bPreserveOtherActiveTerritoryWars = true;
				Definition->MarkPackageDirty();
				++UpdatedEvents;
			}
		}
	}

	if (UpdatedEvents > 0)
	{
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && VisitedDefinitions.Contains((*It)->GetTerritoryDefinition()))
			{
				(*It)->ApplyTerritoryDefinition();
			}
		}
	}
	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryDiplomacyMigration] Normalized %d owner-relative Claimed diplomacy event template(s) in %s. Save All to persist the changed Territory Definitions."),
		UpdatedEvents, *World->GetName());
}

void FTerritoryFrameworkEditorModule::EnsureVehicleRoadsForLoadedLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TerritoryRoadSetup] Load a non-PIE editor level first."));
		return;
	}

	int32 Attempted = 0;
	int32 Succeeded = 0;
	for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!IsValid(Territory)) continue;
		for (const FTerritoryAssaultApproach& Approach :
			Territory->GetCounterAttackApproaches())
		{
			if (!Approach.bEnabled || Approach.EntryType !=
				ETerritoryAssaultEntryType::NarrativeVehicle)
			{
				continue;
			}
			++Attempted;
			FText Failure;
			if (UTerritoryDefinitionEditorLibrary::EnsureStraightVehicleApproachRoad(
				Territory, Approach.ApproachID, Failure))
			{
				++Succeeded;
			}
			else
			{
				UE_LOG(LogTemp, Error,
					TEXT("[TerritoryRoadSetup] %s / %s failed: %s"),
					*Territory->GetTerritoryTag().ToString(),
					*Approach.ApproachID.ToString(), *Failure.ToString());
			}
		}
	}

	FBox CombinedRoadBounds(ForceInit);
	TArray<ATerritoryRoadGuide*> RoadGuides;
	for (TActorIterator<ATerritoryRoadGuide> It(World); It; ++It)
	{
		if (!IsValid(*It)) continue;
		RoadGuides.Add(*It);
		CombinedRoadBounds += It->GetComponentsBoundingBox(true);
	}
	if (CombinedRoadBounds.IsValid)
	{
		CombinedRoadBounds = CombinedRoadBounds.ExpandBy(
			FVector(2500.f, 2500.f, 1500.f));
	}

	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	TArray<AQuestRoadControls*> TrafficControllers;
	for (TActorIterator<AQuestRoadControls> It(World); It; ++It)
	{
		if (IsValid(*It)) TrafficControllers.Add(*It);
	}
	if (TrafficControllers.IsEmpty() && Settings)
	{
		UClass* ControlsClass =
			Settings->RoadTrafficControlsBlueprintClass.LoadSynchronous();
		if (ControlsClass && ControlsClass->IsChildOf(
			ATerritoryRoadTrafficControls::StaticClass()))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = World->PersistentLevel;
			SpawnParams.ObjectFlags = RF_Transactional;
			if (AQuestRoadControls* Controls = World->SpawnActor<AQuestRoadControls>(
				ControlsClass, FTransform::Identity, SpawnParams))
			{
				Controls->SetActorLabel(TEXT("Territory Mission Traffic Controls"));
				TrafficControllers.Add(Controls);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadSetup] Create and configure BP_TerritoryRoadTrafficControls as a Blueprint child of TerritoryRoadTrafficControls."));
		}
	}

	TArray<AMassVehicleSpawner*> TrafficSpawners;
	for (TActorIterator<AMassVehicleSpawner> It(World); It; ++It)
	{
		if (IsValid(*It)) TrafficSpawners.Add(*It);
	}
	if (TrafficSpawners.IsEmpty() && Settings)
	{
		UClass* SpawnerClass =
			Settings->RoadTrafficSpawnerBlueprintClass.LoadSynchronous();
		if (SpawnerClass && SpawnerClass->IsChildOf(
			AMassVehicleSpawner::StaticClass()))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = World->PersistentLevel;
			SpawnParams.ObjectFlags = RF_Transactional;
			if (AMassVehicleSpawner* Spawner = World->SpawnActor<AMassVehicleSpawner>(
				SpawnerClass, FTransform::Identity, SpawnParams))
			{
				Spawner->SetActorLabel(TEXT("Territory Mission Traffic Spawner"));
				TrafficSpawners.Add(Spawner);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TerritoryRoadSetup] Create and configure BP_TerritoryRoadTrafficSpawner as a Blueprint child of Narrative MassVehicleSpawner."));
		}
	}
	if (TrafficSpawners.Num() == 1 && Settings)
	{
		AMassVehicleSpawner* Spawner = TrafficSpawners[0];
		Spawner->Modify();
		if (RepairMassVehicleSpawnGeneratorOwnership(Spawner))
		{
			UE_LOG(LogTemp, Display,
				TEXT("[TerritoryRoadSetup] Re-owned Narrative's Mass spawn generator to the placed traffic spawner so the level can save safely."));
		}
		if (FArrayProperty* EntityTypesProperty = FindFProperty<FArrayProperty>(
			Spawner->GetClass(), TEXT("EntityTypes")))
		{
			FScriptArrayHelper EntityTypes(EntityTypesProperty,
				EntityTypesProperty->ContainerPtrToValuePtr<void>(Spawner));
			EntityTypes.EmptyValues();
			const int32 NewIndex = EntityTypes.AddValue();
			if (FMassSpawnedEntityType* Type = reinterpret_cast<FMassSpawnedEntityType*>(
				EntityTypes.GetRawPtr(NewIndex)))
			{
				Type->EntityConfig = Settings->DefaultRoadTrafficEntityConfig;
				Type->Proportion = 1.f;
			}
		}
		if (FIntProperty* CountProperty = FindFProperty<FIntProperty>(
			Spawner->GetClass(), TEXT("Count")))
		{
			CountProperty->SetPropertyValue_InContainer(Spawner, 0);
		}
		if (FBoolProperty* AutoSpawnProperty = FindFProperty<FBoolProperty>(
			Spawner->GetClass(), TEXT("bAutoSpawnOnBeginPlay")))
		{
			AutoSpawnProperty->SetPropertyValue_InContainer(Spawner, false);
		}
		Spawner->MarkPackageDirty();
	}

	if (TrafficControllers.Num() == 1)
	{
		if (ATerritoryRoadTrafficControls* TerritoryControls =
			Cast<ATerritoryRoadTrafficControls>(TrafficControllers[0]);
			TerritoryControls && CombinedRoadBounds.IsValid)
		{
			TerritoryControls->Modify();
			TerritoryControls->SetMissionTrafficWorldBounds(CombinedRoadBounds);
		}
		for (ATerritoryRoadGuide* Guide : RoadGuides)
		{
			Guide->Modify();
			Guide->NarrativeTrafficControls = TrafficControllers[0];
			Guide->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TerritoryRoadSetup] Found %d QuestRoadControls actors. Narrative traffic linking requires exactly one shared world controller; road splines were still created."),
			TrafficControllers.Num());
	}

	World->MarkPackageDirty();
	UE_LOG(LogTemp, Display,
		TEXT("[TerritoryRoadSetup] Completed %d/%d Narrative vehicle roads in %s with %d shared traffic controller(s) and %d Mass traffic spawner(s). Save the level and build ZoneGraph."),
		Succeeded, Attempted, *World->GetName(), TrafficControllers.Num(),
		TrafficSpawners.Num());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkEditorModule, TerritoryFrameworkEditor)

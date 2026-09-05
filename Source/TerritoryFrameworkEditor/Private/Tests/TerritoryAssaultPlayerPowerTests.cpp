#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeSavableComponent.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultPlayerPower,
	"TerritoryFramework.CounterAttack.Regression.PlayerPowerSurvivesVehiclePossession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultPlayerPower::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Player power world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(261, 262, 263, 264);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->CounterAttackProfile->bNotifyDefendingFactionOnly = true;
	Definition->ApplyToTerritory(Territory);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag PowerTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.Power.Tier.1"));
	const FGameplayTag VehiclePowerTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.Power.Tier.2"));
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(265, 266, 267, 268);
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.DefendingFaction = Heroes;
	Record.State = ETerritoryAssaultState::Active;
	FTerritoryFactionAssaultConfig Force;
	Force.bScaleLevelToRelevantPlayerPower = true;
	Force.EnemyLevelOffset = 2;
	FTerritoryPlayerPowerTier Tier;
	Tier.PlayerPowerTag = PowerTag;
	Tier.PlayerPowerLevel = 17;
	FTerritoryPlayerPowerTier VehicleTier;
	VehicleTier.PlayerPowerTag = VehiclePowerTag;
	VehicleTier.PlayerPowerLevel = 99;
	Force.PlayerPowerTiers = {Tier, VehicleTier};
	ANarrativePlayerState* State = World->SpawnActor<ANarrativePlayerState>();
	State->AddFaction(Heroes);
	ANarrativePlayerCharacter* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	Character->SetRole(ROLE_Authority);
	Character->SetPlayerState(State);
	ANarrativePlayerController* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	Controller->SetPlayerState(State);
	Controller->SetOwnedCharacter(Character);
	World->AddController(Controller);
	Controller->SetPawn(Character);
	UNarrativeAbilitySystemComponent* PlayerASC = CastChecked<UNarrativeAbilitySystemComponent>(State->GetAbilitySystemComponent());
	PlayerASC->InitAbilityActorInfo(State, Character);
	// Bind the real Native PlayerState attribute set as normal possession does;
	// this isolated fixture has no PlayerDefinition/BeginPlay spawn sequence.
	PlayerASC->AddAttributeSetSubobject(State->GetAttributeSetBase());
	FindFProperty<FObjectProperty>(ANarrativeCharacter::StaticClass(), TEXT("AttributeSetBase"))
		->SetObjectPropertyValue_InContainer(Character, State->GetAttributeSetBase());
	PlayerASC->AddLooseGameplayTag(PowerTag);
	TestEqual(TEXT("On-foot scaling reads the player's Native perk tier"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), 19);
	UClass* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	ANarrativeVehicleBase* Vehicle = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass);
	if (!TestNotNull(TEXT("Actual Narrative vehicle exists"), Vehicle))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Controller->SetPawn(Vehicle);
	UNarrativeAbilitySystemComponent* VehicleASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Vehicle);
	if (TestNotNull(TEXT("Native vehicle exposes its distinct ability system"), VehicleASC))
		VehicleASC->AddLooseGameplayTag(VehiclePowerTag);
	TestEqual(TEXT("Driving reads the player's tier rather than the car's tier"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), 19);
	Force.PlayerPowerTiers[0].PlayerPowerLevel = MAX_int32;
	TestEqual(TEXT("Large valid power plus offset clamps to the maximum without overflow"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), Force.MaximumScaledEnemyLevel);
	Controller->SetPawn(Character);
	TestEqual(TEXT("On-foot extreme power also clamps without overflow"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), Force.MaximumScaledEnemyLevel);
	Force.EnemyLevelOffset = MIN_int32;
	TestEqual(TEXT("Large negative offsets clamp to the minimum"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), Force.MinimumScaledEnemyLevel);
	Force.EnemyLevelOffset = 2;
	Force.PlayerPowerTiers.Reset();
	PlayerASC->RemoveLooseGameplayTag(PowerTag);
	PlayerASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetXPAttribute(), 900.f);
	const int32 NativeLevel = Character->GetCharacterLevel();
	TestTrue(TEXT("The player has a real Native XP-derived level"), NativeLevel > 1);
	PlayerASC->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetXPAttribute());
	INarrativeSavableComponent::Execute_PrepareForSave(PlayerASC);
	PlayerASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetXPAttribute(), 0.f);
	INarrativeSavableComponent::Execute_Load(PlayerASC);
	Controller->SetPawn(Vehicle);
	TestEqual(TEXT("Native saved XP still controls scaling while driving"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), FMath::Clamp(NativeLevel + 2, 1, 100));
	Character->SetRole(ROLE_SimulatedProxy);
	Controller->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("Replicated actor roles resolve the same player power"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), FMath::Clamp(NativeLevel + 2, 1, 100));
	Vehicle->SetActorLocation(FVector(1000000.f));
	TestEqual(TEXT("Range remains tied to the currently possessed vehicle"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), INDEX_NONE);
	Vehicle->SetActorLocation(Territory->GetActorLocation());
	Controller->SetPawn(nullptr);
	TestEqual(TEXT("An absent physical player does not contribute to local scaling"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), INDEX_NONE);
	Force.bScaleLevelToRelevantPlayerPower = false;
	Controller->SetPawn(Vehicle);
	TestEqual(TEXT("Disabled scaling remains disabled"), Counter->ResolveScaledEnemyLevel(Record, Territory, Force), INDEX_NONE);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/TerritoryDistractionAbility.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryPropertyTags.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/PlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPropertyBenefitRuntimeReconciliation,
	"TerritoryFramework.PropertyBenefits.Regression.RuntimeGrantReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPropertyBenefitRuntimeReconciliation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Benefit world exists"), World)) return false;
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	APlayerController* Controller = World->SpawnActor<APlayerController>(Spawn);
	ATerritoryGuardCharacter* Character = World->SpawnActor<ATerritoryGuardCharacter>(Spawn);
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>(Spawn);
	Controller->SetPawn(Character);
	UTerritoryPlayerManagementComponent* Management =
		NewObject<UTerritoryPlayerManagementComponent>(Controller);
	Controller->AddInstanceComponent(Management);
	UNarrativeAbilitySystemComponent* ASC = CastChecked<UNarrativeAbilitySystemComponent>(
		Character->GetAbilitySystemComponent());
	ASC->InitAbilityActorInfo(Character, Character);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Character)->AddFaction(Heroes);
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(41, 42, 43, 44);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	FTerritoryPropertyGameplayBenefit& Benefit = Definition->GameplayBenefits.AddDefaulted_GetRef();
	Benefit.BenefitTag = TerritoryPropertyTags::WeaponUpgradesBenefit;
	Benefit.GrantedAbilities.Add(UTerritoryDistractionAbility::StaticClass());
	Benefit.GrantedGameplayEffects.Add(UGameplayEffect::StaticClass());
	Definition->ApplyToTerritory(Property);
	FTerritoryOwnershipData State = Property->GetOwnershipData();
	State.OwningFaction = Heroes;
	State.State = ETerritoryState::Claimed;
	State.ControlProgress = 1.f;
	Property->CommitOwnershipData(State);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Property);

	// A transient empty effect exercises the real GAS lifetime without project assets.
	TGuardValue<EGameplayEffectDurationType> DurationGuard(
		GetMutableDefault<UGameplayEffect>()->DurationPolicy, EGameplayEffectDurationType::Infinite);
	Management->RefreshOwnedPropertyBenefits();
	TestEqual(TEXT("One Narrative ability is granted"), ASC->GetActivatableAbilities().Num(), 1);
	TestTrue(TEXT("Semantic benefit is present on Narrative GAS"), ASC->HasMatchingGameplayTag(Benefit.BenefitTag));
	TestEqual(TEXT("Persistent modifier and dynamic tag effects exist"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
	ASC->ClearAllAbilities();
	ASC->RemoveActiveEffects(FGameplayEffectQuery());
	Management->RefreshOwnedPropertyBenefits();
	TestEqual(TEXT("External ability removal is repaired"), ASC->GetActivatableAbilities().Num(), 1);
	TestTrue(TEXT("External dynamic tag removal is repaired"), ASC->HasMatchingGameplayTag(Benefit.BenefitTag));
	TestEqual(TEXT("External effect removal is repaired"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
	Management->RefreshOwnedPropertyBenefits();
	TestEqual(TEXT("Repeated refresh does not duplicate effects"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);

	// Native effect callbacks may synchronously ask the component to refresh again.
	ASC->RemoveActiveEffects(FGameplayEffectQuery());
	bool bReentered = false;
	const FDelegateHandle Callback = ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent*, const FGameplayEffectSpec&, FActiveGameplayEffectHandle)
		{
			if (!bReentered)
			{
				bReentered = true;
				Management->RefreshOwnedPropertyBenefits();
			}
		});
	Management->RefreshOwnedPropertyBenefits();
	ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(Callback);
	TestTrue(TEXT("Actual GAS application callback ran"), bReentered);
	TestEqual(TEXT("Reentrant refresh grants one set only"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);

	State.Availability = ETerritoryAvailability::Locked;
	Property->CommitOwnershipData(State);
	Management->RefreshOwnedPropertyBenefits();
	TestEqual(TEXT("Story lock revokes owned abilities"), ASC->GetActivatableAbilities().Num(), 0);
	TestEqual(TEXT("Story lock revokes owned effects"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 0);
	State.Availability = ETerritoryAvailability::Unlocked;
	Property->CommitOwnershipData(State);
	// A preexisting Narrative grant belongs to its source and must survive Territory revocation.
	const FGameplayAbilitySpecHandle External = Character->AddAbility(UTerritoryDistractionAbility::StaticClass(), Character);
	Management->RefreshOwnedPropertyBenefits();
	State.Availability = ETerritoryAvailability::Locked;
	Property->CommitOwnershipData(State);
	Management->RefreshOwnedPropertyBenefits();
	TestNotNull(TEXT("Revocation preserves another source's ability"), ASC->FindAbilitySpecFromHandle(External));

	GetMutableDefault<UGameplayEffect>()->DurationPolicy = EGameplayEffectDurationType::Instant;
	State.Availability = ETerritoryAvailability::Unlocked;
	Property->CommitOwnershipData(State);
	int32 InstantApplications = 0;
	const FDelegateHandle InstantCallback = ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (Spec.Def && Spec.Def->GetClass() == UGameplayEffect::StaticClass()) ++InstantApplications;
		});
	Management->RefreshOwnedPropertyBenefits();
	Management->RefreshOwnedPropertyBenefits();
	ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(InstantCallback);
	TestEqual(TEXT("Invalid Instant benefit never applies, including repeated refresh"), InstantApplications, 0);

	// The abstract Narrative save contract needs a project-authored player GUID at
	// spawn. These unspawned fixtures exercise its PlayerState/vehicle APIs.
	ANarrativePlayerController* Driver = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Driver->SetRole(ROLE_Authority);
	ANarrativePlayerCharacter* Player = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	Player->SetRole(ROLE_Authority);
	ANarrativePlayerState* PlayerState = World->SpawnActor<ANarrativePlayerState>(Spawn);
	APawn* Vehicle = World->SpawnActor<APawn>(Spawn);
	Player->SetPlayerState(PlayerState);
	Driver->PlayerState = PlayerState;
	PlayerState->AddFaction(Heroes);
	Driver->SetOwnedCharacter(Player);
	Driver->SetPawn(Vehicle);
	UNarrativeAbilitySystemComponent* PlayerASC = CastChecked<UNarrativeAbilitySystemComponent>(
		PlayerState->GetAbilitySystemComponent());
	PlayerASC->InitAbilityActorInfo(PlayerState, Player);
	UTerritoryPlayerManagementComponent* DriverManagement =
		NewObject<UTerritoryPlayerManagementComponent>(Driver);
	Driver->AddInstanceComponent(DriverManagement);
	Benefit.GrantedAbilities.Reset();
	Benefit.GrantedGameplayEffects.Reset();
	DriverManagement->RefreshOwnedPropertyBenefits();
	TestTrue(TEXT("Driving retains benefits on Narrative's player ASC"),
		PlayerASC->HasMatchingGameplayTag(Benefit.BenefitTag));
	PlayerASC->RemoveActiveEffects(FGameplayEffectQuery());
	DriverManagement->Load_Implementation();
	DriverManagement->RefreshOwnedPropertyBenefits();
	TestTrue(TEXT("Restored player ASC receives benefits from current owned Property state"),
		PlayerASC->HasMatchingGameplayTag(Benefit.BenefitTag));
	PlayerASC->RemoveActiveEffects(FGameplayEffectQuery());
	Driver->SetRole(ROLE_SimulatedProxy);
	DriverManagement->RefreshOwnedPropertyBenefits();
	TestFalse(TEXT("Client management cannot locally grant benefits"),
		PlayerASC->HasMatchingGameplayTag(Benefit.BenefitTag));
	Driver->SetRole(ROLE_Authority);
	World->DestroyWorld(false);
	return true;
}

#endif

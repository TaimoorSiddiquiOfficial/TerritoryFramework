#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/TerritoryDistractionAbility.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryStealthTags.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/TerritoryDistractionComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Items/InventoryComponent.h"
#include "Items/EquippableItem.h"
#include "Items/NarrativeItem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionLifecycle,
	"TerritoryFramework.Stealth.Regression.DistractionCancellationAndInventoryRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionLifecycle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Distraction test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Character = World->SpawnActor<ATerritoryGuardCharacter>();
	UNarrativeAbilitySystemComponent* ASC = Character->GetNarrativeAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	const FItemAddResult Added = Inventory->TryAddItemFromClass(UNarrativeItem::StaticClass(), 1);
	if (!TestEqual(TEXT("Native item was added"), Added.Stacks.Num(), 1))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UNarrativeItem* Item = Added.Stacks[0];
	// Exercise the documented unequipped-consumable option with a native item.
	TGuardValue<bool> EquippedOption(GetMutableDefault<UTerritoryDistractionAbility>()->bRequireEquippedNarrativeItemSource, false);
	FGameplayAbilitySpecHandle Handle = Character->AddAbility(UTerritoryDistractionAbility::StaticClass(), Item);
	UTerritoryDistractionAbility* Ability = CastChecked<UTerritoryDistractionAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	// Native class construction uses its C++ defaults for the per-actor instance;
	// configure both the CDO eligibility check and this transient test instance.
	Ability->bRequireEquippedNarrativeItemSource = false;
	int32 ThrownEvents = 0;
	TWeakObjectPtr<ATerritoryDistractionProjectile> LastProjectile;
	const FDelegateHandle ThrownListener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionThrownEvent).AddLambda(
		[&](const FGameplayEventData* Event)
		{
			++ThrownEvents;
			LastProjectile = Cast<ATerritoryDistractionProjectile>(const_cast<AActor*>(Event->Target.Get()));
		});
	TestTrue(TEXT("A real source item in the avatar inventory is ready"), Ability->IsThrowableSourceItemReady(Item));

	// CommitAbility calls external GAS listeners before returning true.
	int32 CommitCallbacks = 0;
	const FDelegateHandle CancelCommit = ASC->AbilityCommittedCallbacks.AddLambda(
		[&](UGameplayAbility*) { ++CommitCallbacks; ASC->CancelAbilityHandle(Handle); });
	ASC->TryActivateAbility(Handle);
	TestEqual(TEXT("Cancellation probe reached a real GAS commit"), CommitCallbacks, 1);
	TestEqual(TEXT("Cancellation during commit produces no throw event"), ThrownEvents, 0);
	TestEqual(TEXT("Cancellation during commit consumes no item"), Item->GetQuantity(), 1);
	TestFalse(TEXT("Cancelled ability has ended"), Ability->IsActive());
	ASC->AbilityCommittedCallbacks.Remove(CancelCommit);

	// World spawn callbacks can cancel while SpawnActorDeferred is on the stack.
	TWeakObjectPtr<AActor> RejectedProjectile;
	FDelegateHandle SpawnListener = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[&](AActor* Actor)
		{
			if (Actor->IsA<ATerritoryDistractionProjectile>())
			{
				RejectedProjectile = Actor;
				ASC->CancelAbilityHandle(Handle);
			}
		}));
	ASC->TryActivateAbility(Handle);
	World->RemoveOnActorSpawnedHandler(SpawnListener);
	TestTrue(TEXT("Spawn cancellation probe saw the deferred projectile"), RejectedProjectile.IsExplicitlyNull() == false);
	TestTrue(TEXT("Cancelled deferred projectile is removed"), !RejectedProjectile.IsValid() || RejectedProjectile->IsActorBeingDestroyed());
	TestEqual(TEXT("Cancelled spawn consumes no item"), Item->GetQuantity(), 1);
	TestEqual(TEXT("Cancelled spawn publishes no success"), ThrownEvents, 0);

	SpawnListener = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[](AActor* Actor) { if (Actor->IsA<ATerritoryDistractionProjectile>()) Actor->Destroy(); }));
	ASC->TryActivateAbility(Handle);
	World->RemoveOnActorSpawnedHandler(SpawnListener);
	TestEqual(TEXT("A destroyed spawn consumes no item"), Item->GetQuantity(), 1);
	TestEqual(TEXT("A destroyed spawn publishes no success"), ThrownEvents, 0);

	// Native Load rebuilds Items, but old live objects can retain OwningInventory.
	Inventory->PrepareForSave_Implementation();
	Inventory->Load_Implementation();
	UNarrativeItem* RestoredItem = Inventory->GetItems()[0];
	TestTrue(TEXT("Native restore creates a new source object"), RestoredItem != Item);
	TestEqual(TEXT("Native restore preserves the unspent unit"), RestoredItem->GetQuantity(), 1);
	TestFalse(TEXT("The old source is rejected after load"), Ability->IsThrowableSourceItemReady(Item));
	TestFalse(TEXT("A stale ability spec cannot consume the restored inventory"), ASC->TryActivateAbility(Handle));
	ASC->ClearAbility(Handle);
	Handle = Character->AddAbility(UTerritoryDistractionAbility::StaticClass(), RestoredItem);
	Ability = CastChecked<UTerritoryDistractionAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	Ability->bRequireEquippedNarrativeItemSource = false;

	ATerritoryGuardCharacter* OtherCharacter = World->SpawnActor<ATerritoryGuardCharacter>();
	UNarrativeAbilitySystemComponent* OtherASC = OtherCharacter->GetNarrativeAbilitySystemComponent();
	OtherASC->InitAbilityActorInfo(OtherCharacter, OtherCharacter);
	FGameplayAbilitySpecHandle ForeignHandle = OtherCharacter->AddAbility(UTerritoryDistractionAbility::StaticClass(), RestoredItem);
	CastChecked<UTerritoryDistractionAbility>(OtherASC->FindAbilitySpecFromHandle(ForeignHandle)->GetPrimaryInstance())->bRequireEquippedNarrativeItemSource = false;
	TestFalse(TEXT("Another avatar cannot throw this character's item"), OtherASC->TryActivateAbility(ForeignHandle));
	OtherASC->ClearAbility(ForeignHandle);

	Character->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client cannot execute this server-only throw locally"), ASC->TryActivateAbility(Handle, false));
	TestEqual(TEXT("A rejected client attempt consumes nothing"), RestoredItem->GetQuantity(), 1);
	Character->SetRole(ROLE_Authority);

	bool bReentered = false;
	const FDelegateHandle ReenterCommit = ASC->AbilityCommittedCallbacks.AddLambda(
		[&](UGameplayAbility*)
		{
			if (!bReentered)
			{
				bReentered = true;
				ASC->CancelAbilityHandle(Handle);
				ASC->TryActivateAbility(Handle);
			}
		});
	TestTrue(TEXT("Restored source can activate through Native GAS"), ASC->TryActivateAbility(Handle));
	ASC->AbilityCommittedCallbacks.Remove(ReenterCommit);
	TestTrue(TEXT("Commit listener exercised cancel and reactivation"), bReentered);
	TestEqual(TEXT("Successful throw publishes exactly one event"), ThrownEvents, 1);
	TestEqual(TEXT("Native inventory consumed the last unit"), Inventory->GetItems().Num(), 0);
	TestTrue(TEXT("Projectile survives its completed ability"), LastProjectile.IsValid() && !LastProjectile->IsActorBeingDestroyed());
	TestFalse(TEXT("Successful instant throw ends its ability"), Ability->IsActive());
	TestFalse(TEXT("Depleted source cannot throw again"), ASC->TryActivateAbility(Handle));
	Inventory->PrepareForSave_Implementation();
	Inventory->Load_Implementation();
	TestEqual(TEXT("Saving after the throw does not restore the consumed unit"), Inventory->GetItems().Num(), 0);

	if (LastProjectile.IsValid())
	{
		int32 ImpactEvents = 0;
		bool bNestedImpact = true;
		const FDelegateHandle ImpactListener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionImpactEvent).AddLambda(
			[&](const FGameplayEventData*)
			{
				++ImpactEvents;
				// Bound the probe so a regression fails without recursing indefinitely.
				if (ImpactEvents == 1) bNestedImpact = LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f));
			});
		TestTrue(TEXT("First impact publishes through Native GAS"), LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f)));
		TestFalse(TEXT("An impact listener cannot republish the same impact"), bNestedImpact);
		TestEqual(TEXT("Impact event is delivered once"), ImpactEvents, 1);
		TestFalse(TEXT("Later collision cannot repeat impact"), LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f)));
		ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionImpactEvent).Remove(ImpactListener);
		LastProjectile->Destroy();
	}
	ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionThrownEvent).Remove(ThrownListener);
	ASC->ClearAllAbilities();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionEquippedAsset,
	"TerritoryFramework.Stealth.Regression.DistractionEquippedBlueprintLastUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionEquippedAsset::RunTest(const FString& Parameters)
{
	UClass* RockClass = LoadClass<UEquippableItem>(nullptr,
		TEXT("/TerritoryFramework/Stealth/Equippable_Throwable_Rock.Equippable_Throwable_Rock_C"));
	if (!TestNotNull(TEXT("Shipped Narrative equippable Blueprint loads"), RockClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Equipped throw world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Character = World->SpawnActor<ATerritoryGuardCharacter>();
	// This fixture does not run a full NPC spawn. Supply Native's definition read
	// model without starting asynchronous appearance/AI loading in a unit world.
	FObjectPropertyBase* Definition = FindFProperty<FObjectPropertyBase>(ANarrativeNPCCharacter::StaticClass(), TEXT("NPCDefinition"));
	if (!TestNotNull(TEXT("Native definition field exists"), Definition))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Definition->SetObjectPropertyValue_InContainer(Character, NewObject<UNPCDefinition>(Character));
	UNarrativeAbilitySystemComponent* ASC = Character->GetNarrativeAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Territory.Property.Benefit.WeaponUpgrades")));
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	const FItemAddResult Added = Inventory->TryAddItemFromClass(RockClass, 1);
	if (!TestEqual(TEXT("Shipped rock is added"), Added.Stacks.Num(), 1))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UEquippableItem* Rock = CastChecked<UEquippableItem>(Added.Stacks[0]);
	TestTrue(TEXT("Native auto-equips the shipped rock"), Rock->IsEquipped());
	FGameplayAbilitySpecHandle Handle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.SourceObject == Rock && Spec.Ability->IsA<UTerritoryDistractionAbility>()) Handle = Spec.Handle;
	}
	if (!TestTrue(TEXT("Native equipment granted the authored ability with item source"), Handle.IsValid()))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	int32 ThrownEvents = 0;
	TWeakObjectPtr<ATerritoryDistractionProjectile> Projectile;
	const FDelegateHandle Listener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionThrownEvent).AddLambda(
		[&](const FGameplayEventData* Event)
		{
			++ThrownEvents;
			Projectile = Cast<ATerritoryDistractionProjectile>(const_cast<AActor*>(Event->Target.Get()));
		});
	TestTrue(TEXT("Authored throw activates"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Last equipped unit produces one successful throw"), ThrownEvents, 1);
	TestEqual(TEXT("Last unit is removed through Native inventory"), Inventory->GetItems().Num(), 0);
	TestFalse(TEXT("Native removal unequips the rock"), Rock->IsEquipped());
	TestNull(TEXT("Native equipment revokes its ability spec"), ASC->FindAbilitySpecFromHandle(Handle));
	TestTrue(TEXT("Paid projectile survives Native ability revocation"), Projectile.IsValid() && !Projectile->IsActorBeingDestroyed());
	if (Projectile.IsValid()) Projectile->Destroy();
	ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionThrownEvent).Remove(Listener);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

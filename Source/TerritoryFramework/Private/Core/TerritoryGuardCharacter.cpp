#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/EquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Items/WeaponItem.h"
#include "NarrativeArsenal.h"
#include "Tales/TriggerSet.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"

namespace
{
	struct FPendingTerritoryGuardSpawn
	{
		UNPCDefinition* Definition = nullptr;
		FGameplayTag ExactFaction;
		FGuid TerritoryGuid;
		FGuid SaveGuid;
		FTransform SpawnTransform;
		FName SpawnPointName;
		ATerritoryVolume* OwningTerritory = nullptr;
		ATerritoryGuardSpawnPoint* OwningSpawnPoint = nullptr;
		bool bApplied = false;
	};

	FPendingTerritoryGuardSpawn* GPendingTerritoryGuardSpawn = nullptr;

	void ApplyNarrativeCollisionOverrides(ATerritoryGuardCharacter& Character)
	{
		if (USkeletalMeshComponent* Mesh = Character.GetMesh())
		{
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeProjectile, ECR_Block);
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeCover, ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeTraversable, ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeClimbable, ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Ignore);
		}
		if (UCapsuleComponent* Capsule = Character.GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeWeapon, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeProjectile, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeCover, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeTraversable, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeClimbable, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Ignore);
		}
	}
}

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PatrolGoalClass = UTerritoryPatrolGoal::StaticClass();
	AIControllerClass = ANarrativeNPCController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	GetMesh()->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
}

bool ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
	const UNPCDefinition* Definition, int32 RequiredInstances, FText& OutFailureReason)
{
	OutFailureReason = FText();
	if (!Definition)
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "MissingGuardDefinition",
			"A Narrative NPC definition is required for Territory guards.");
		return false;
	}
	if (Definition->CharacterID.IsNone())
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "MissingGuardCharacterID",
			"Narrative guard definition must have a stable CharacterID.");
		return false;
	}
	if (Definition->NPCID.IsNone())
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "MissingGuardNPCID",
			"Narrative guard definition must have a stable NPCID while Narrative uses it for NPC lookup and duplicate admission.");
		return false;
	}

	UClass* CandidateClass = Definition->NPCClassPath.LoadSynchronous();
	if (!CandidateClass || !CandidateClass->IsChildOf(StaticClass()))
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "WrongGuardPawnClass",
			"Guard NPC class must derive from ATerritoryGuardCharacter.");
		return false;
	}
	const ATerritoryGuardCharacter* CDO =
		CandidateClass->GetDefaultObject<ATerritoryGuardCharacter>();
	if (!CDO || !CDO->AIControllerClass
		|| !CDO->AIControllerClass->IsChildOf(ANarrativeNPCController::StaticClass()))
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "MissingGuardNarrativeController",
			"Guard NPC class must use an ANarrativeNPCController-derived AI Controller Class.");
		return false;
	}
	if (CDO->AutoPossessAI != EAutoPossessAI::Spawned
		&& CDO->AutoPossessAI != EAutoPossessAI::PlacedInWorldOrSpawned)
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "GuardSpawnedAutoPossessRequired",
			"Guard NPC class Auto Possess AI must include Spawned actors.");
		return false;
	}
	if (RequiredInstances > 1 && !Definition->bAllowMultipleInstances)
	{
		OutFailureReason = NSLOCTEXT("TerritoryGuardCharacter", "MultipleGuardInstancesRequired",
			"Narrative guard definition must allow multiple instances when more than one guard can be deployed.");
		return false;
	}
	return true;
}

ATerritoryGuardCharacter* ATerritoryGuardCharacter::SpawnThroughNarrative(
	UNarrativeCharacterSubsystem* CharacterSubsystem, UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction, const FGuid& TerritoryGuid,
	const FGuid& SaveGuid, const FTransform& InSpawnTransform, FName SpawnPointName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
	ATerritoryVolume* InOwningTerritory, ATerritoryGuardSpawnPoint* InOwningSpawnPoint)
{
	if (!IsInGameThread() || !CharacterSubsystem || !Definition || !ExactFaction.IsValid()
		|| !TerritoryGuid.IsValid() || !SaveGuid.IsValid() || !InOwningTerritory
		|| !InOwningSpawnPoint || GPendingTerritoryGuardSpawn)
	{
		return nullptr;
	}

	FNPCSpawnParams SpawnParams;
	SpawnParams.bOverride_DefaultFactions = true;
	SpawnParams.DefaultFactions.AddTag(ExactFaction);
	if (OptionalActivityOverride)
	{
		SpawnParams.bOverride_ActivityConfiguration = true;
		SpawnParams.ActivityConfiguration = FSoftObjectPath(OptionalActivityOverride);
	}
	if (!OptionalTriggerOverrides.IsEmpty())
	{
		SpawnParams.bOverride_TriggerSets = true;
		SpawnParams.TriggerSets = OptionalTriggerOverrides;
	}

	FPendingTerritoryGuardSpawn Context;
	Context.Definition = Definition;
	Context.ExactFaction = ExactFaction;
	Context.TerritoryGuid = TerritoryGuid;
	Context.SaveGuid = SaveGuid;
	Context.SpawnTransform = InSpawnTransform;
	Context.SpawnPointName = SpawnPointName;
	Context.OwningTerritory = InOwningTerritory;
	Context.OwningSpawnPoint = InOwningSpawnPoint;
	TGuardValue<FPendingTerritoryGuardSpawn*> PendingGuard(
		GPendingTerritoryGuardSpawn, &Context);

	ANarrativeNPCCharacter* Spawned = CharacterSubsystem->SpawnNPC(
		Definition, InSpawnTransform, SpawnParams);
	ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(Spawned);
	if (!Context.bApplied || !Guard)
	{
		if (Spawned)
		{
			TerritoryNarrativeDeathSupport::PrepareForRemoval(*Spawned);
			CharacterSubsystem->DestroyNPC(Spawned);
		}
		return nullptr;
	}
	return Guard;
}

void ATerritoryGuardCharacter::SetNPCDefinition(UNPCDefinition* Definition)
{
	if (GPendingTerritoryGuardSpawn
		&& GPendingTerritoryGuardSpawn->Definition == Definition
		&& !GPendingTerritoryGuardSpawn->bApplied)
	{
		SpawnInfo.SpawnAssignedSaveGUID = GPendingTerritoryGuardSpawn->SaveGuid;
		SpawnInfo.SpawnTransform = GPendingTerritoryGuardSpawn->SpawnTransform;
		SpawnInfo.SpawnName = GPendingTerritoryGuardSpawn->SpawnPointName;
		SpawnInfo.OwningSpawnerGUID = GPendingTerritoryGuardSpawn->TerritoryGuid;
		TerritoryHomeTransform = GPendingTerritoryGuardSpawn->SpawnTransform;
		OwningTerritory = GPendingTerritoryGuardSpawn->OwningTerritory;
		OwningTerritorySpawnPoint = GPendingTerritoryGuardSpawn->OwningSpawnPoint;
		GPendingTerritoryGuardSpawn->bApplied = true;
	}
	Super::SetNPCDefinition(Definition);
}

ETeamAttitude::Type ATerritoryGuardCharacter::GetTeamAttitudeTowards(
	const AActor& Other) const
{
	if (CanEngageTerritoryTarget(&Other))
	{
		return ETeamAttitude::Hostile;
	}

	// Preserve a genuine friendly result for same/allied actors, but deliberately
	// downgrade stale Narrative hostility and the pawn Hostiles override to Neutral.
	const ETeamAttitude::Type NarrativeAttitude = Super::GetTeamAttitudeTowards(Other);
	return NarrativeAttitude == ETeamAttitude::Friendly
		? ETeamAttitude::Friendly : ETeamAttitude::Neutral;
}

bool ATerritoryGuardCharacter::CanEngageTerritoryTarget(const AActor* Target) const
{
	if (!IsValid(Target) || Target == this || !IsValid(OwningTerritory)
		|| OwningTerritory->GetTerritoryState() != ETerritoryState::Contested)
	{
		return false;
	}

	const INarrativeTeamAgentInterface* TargetTeam =
		Cast<INarrativeTeamAgentInterface>(Target);
	const UWorld* World = GetWorld();
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	return TargetTeam && Diplomacy
		&& Diplomacy->AreAnyFactionsAtWar(GetFactions(), TargetTeam->GetFactions());
}

bool ATerritoryGuardCharacter::ShouldRespawn_Implementation() const
{
	return false;
}

float ATerritoryGuardCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// P1-N14: Only update LastDamagingInstigator when actual damage is applied (> 0).
	// When damage is fully absorbed (e.g. shield, invulnerability), we don't want
	// to attribute a "last damager" that had zero effect.
	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		if (EventInstigator && EventInstigator->GetPawn())
		{
			LastDamagingInstigator = EventInstigator->GetPawn();
		}
		else if (DamageCauser)
		{
			APawn* InstigatorPawn = DamageCauser->GetInstigator();
			LastDamagingInstigator = InstigatorPawn ? InstigatorPawn : DamageCauser;
		}
	}
	return ActualDamage;
}

void ATerritoryGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyNarrativeCollisionOverrides(*this);

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			InitializeTerritoryPatrolGoal();
		}));

		// NPC definitions populate inventory after BeginPlay; retry briefly until the
		// first equipped weapon is available, then make it the mainhand weapon.
		GetWorldTimerManager().SetTimer(
			DefaultWeaponWieldTimer,
			this,
			&ATerritoryGuardCharacter::TryWieldDefaultWeapon,
			0.25f,
			true,
			0.25f);
	}
}

void ATerritoryGuardCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
	Super::EndPlay(EndPlayReason);
}

void ATerritoryGuardCharacter::HandleDeath_Implementation(
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC,
	const bool bIsDead)
{
	const bool bResolvedIsDead = TerritoryNarrativeDeathSupport::ResolveDeathState(
		KilledActorASC, bIsDead);
	if (bResolvedIsDead)
	{
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*this);
	}
	Super::HandleDeath_Implementation(KilledActor, KilledActorASC, bResolvedIsDead);
	if (bResolvedIsDead)
	{
		TerritoryNarrativeDeathSupport::FinalizePhysicalDeath(*this);
	}
}

void ATerritoryGuardCharacter::ReconcileNarrativeDeathState(
	UNarrativeAbilitySystemComponent* KilledActorASC, const bool bReportedIsDead)
{
	if (TerritoryNarrativeDeathSupport::ResolveDeathState(
		KilledActorASC, bReportedIsDead))
	{
		TerritoryNarrativeDeathSupport::FinalizePhysicalDeath(*this);
	}
}

void ATerritoryGuardCharacter::SetRagdoll(const bool bWantsRagdoll)
{
	if (!HasAuthority())
	{
		return;
	}
	Super::SetRagdoll(bWantsRagdoll);
}

void ATerritoryGuardCharacter::OnCharacterVisualInitialized()
{
	// Narrative grants a new NPC's definition loadout inside this call. Mark the
	// admission complete only afterwards, then wait for the specific weapon visual.
	Super::OnCharacterVisualInitialized();
	bNarrativeInitializationCompleted = true;
	DefaultWeaponPostInitializationAttempts = 0;
	TryWieldDefaultWeapon();
}

void ATerritoryGuardCharacter::TryWieldDefaultWeapon()
{
	++DefaultWeaponWieldAttempts;
	ANarrativeCharacterVisual* CharacterVisual = GetCharacterVisual();
	if (!GetNPCDefinition() || !CharacterVisual || !bNarrativeInitializationCompleted)
	{
		constexpr int32 MaxNarrativeInitializationAttempts = 240;
		if (DefaultWeaponWieldAttempts >= MaxNarrativeInitializationAttempts)
		{
			GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
			UE_LOG(LogTerritory, Warning,
				TEXT("GuardCharacter %s did not finish Narrative visual/loadout initialization before the wield timeout"),
				*GetName());
		}
		return;
	}
	++DefaultWeaponPostInitializationAttempts;

	if (GetWeapon(true))
	{
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		return;
	}

	UEquipmentComponent* Equipment = GetEquipmentComponent();
	if (!Equipment)
	{
		if (DefaultWeaponPostInitializationAttempts >= 40)
		{
			GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
			UE_LOG(LogTerritory, Warning,
				TEXT("GuardCharacter %s has no Narrative equipment component"), *GetName());
		}
		return;
	}

	const FGameplayTag MainhandWieldSlot = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Equipment.WieldSlot.Mainhand")), false);
	const TArray<FGameplayTag> WeaponSlots =
	{
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.HipLeft")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.HipRight")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.Back")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.BackA")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.BackB")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.Hip")), false)
	};

	bool bHasEquippedWeapon = false;
	for (const FGameplayTag& EquipSlot : WeaponSlots)
	{
		UWeaponItem* Weapon = Equipment->GetEquippedWeaponAtSlot(EquipSlot);
		if (!Weapon)
		{
			Weapon = Cast<UWeaponItem>(Equipment->GetEquippedItemAtSlot(EquipSlot));
		}
		if (!Weapon)
		{
			continue;
		}
		bHasEquippedWeapon = true;
		// SetWieldState immediately asks the Narrative character visual to attach the
		// weapon. Wait until that asynchronously created visual actually contains the
		// slot, otherwise Narrative emits a warning and never applies the overlay.
		if (!GetWeaponVisual(EquipSlot))
		{
			continue;
		}

		FWeaponWieldState DesiredWieldState;
		DesiredWieldState.EquipSlots.AddTag(EquipSlot);
		DesiredWieldState.WieldSlots.AddTag(MainhandWieldSlot);
		SetWieldState(DesiredWieldState);
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		return;
	}

	// An unarmed Narrative definition is a supported combatant. Give its asynchronous
	// inventory a short admission window, then stop polling without presenting a valid
	// data choice as a runtime failure.
	if (!bHasEquippedWeapon && DefaultWeaponPostInitializationAttempts >= 12)
	{
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		UE_LOG(LogTerritory, Verbose,
			TEXT("GuardCharacter %s has no equipped weapon; keeping the valid unarmed Narrative loadout"),
			*GetName());
		return;
	}
	if (bHasEquippedWeapon && DefaultWeaponPostInitializationAttempts >= 120)
	{
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		UE_LOG(LogTerritory, Warning,
			TEXT("GuardCharacter %s has an equipped weapon but Narrative did not create its weapon visual before the wield timeout"),
			*GetName());
		return;
	}

	// Definition inventory is populated asynchronously. Report the actual transient
	// condition; the GameplayTags above may all be valid even when no item exists yet.
	if (DefaultWeaponWieldAttempts == 1)
	{
		UE_LOG(LogTerritory, Verbose, TEXT("GuardCharacter %s has no equipped weapon in a supported slot yet; retrying while the Narrative definition loads"),
			*GetName());
	}
}

void ATerritoryGuardCharacter::ApplyActivityConfig_Implementation(UNPCActivityConfiguration* NPCActivityConfig)
{
	Super::ApplyActivityConfig_Implementation(NPCActivityConfig);
	InitializeTerritoryPatrolGoal();
	TryWieldDefaultWeapon();
}

void ATerritoryGuardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryGuardCharacter, TerritoryHomeTransform);
	DOREPLIFETIME(ATerritoryGuardCharacter, OwningTerritory);
	DOREPLIFETIME(ATerritoryGuardCharacter, OwningTerritorySpawnPoint);
}

void ATerritoryGuardCharacter::ConfigureTerritorySpawn(
	UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction,
	const FGuid& TerritoryGuid,
	const FGuid& SaveGuid,
	const FTransform& InSpawnTransform,
	FName SpawnPointName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides)
{
	ATerritoryVolume* ResolvedTerritory = nullptr;
	ATerritoryGuardSpawnPoint* ResolvedSpawnPoint = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			ResolvedTerritory = Registry->GetTerritoryByGUID(TerritoryGuid);
		}
	}
	if (ResolvedTerritory)
	{
		for (ATerritoryGuardSpawnPoint* Candidate : ResolvedTerritory->GetGuardSpawnPoints())
		{
			if (Candidate && Candidate->GetFName() == SpawnPointName)
			{
				ResolvedSpawnPoint = Candidate;
				break;
			}
		}
	}
	if (!ConfigureTerritorySpawnWithContext(Definition, ExactFaction, TerritoryGuid,
		SaveGuid, InSpawnTransform, SpawnPointName, OptionalActivityOverride,
		OptionalTriggerOverrides, ResolvedTerritory, ResolvedSpawnPoint))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("ConfigureTerritorySpawn legacy migration failed for %s; use ConfigureTerritorySpawnWithContext with typed ownership"),
			*GetName());
	}
}

bool ATerritoryGuardCharacter::ConfigureTerritorySpawnWithContext(
	UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction,
	const FGuid& TerritoryGuid,
	const FGuid& SaveGuid,
	const FTransform& InSpawnTransform,
	FName SpawnPointName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
	ATerritoryVolume* InOwningTerritory,
	ATerritoryGuardSpawnPoint* InOwningSpawnPoint)
{
	if (!Definition || !ExactFaction.IsValid() || !TerritoryGuid.IsValid()
		|| !SaveGuid.IsValid() || !InOwningTerritory || !InOwningSpawnPoint
		|| InOwningTerritory->GetTerritoryGUID() != TerritoryGuid
		|| !InOwningTerritory->GetGuardSpawnPoints().Contains(InOwningSpawnPoint))
	{
		return false;
	}

	// Fill ALL SpawnInfo fields that Narrative activities need.
	// BPA_ReturnToSpawn reads SpawnTransform — without it, SetupBlackboard fails.

	SpawnInfo.SpawnAssignedSaveGUID = SaveGuid;
	SpawnInfo.SpawnTransform = InSpawnTransform;
	SpawnInfo.SpawnName = SpawnPointName;

	// TerritoryVolume is not a Narrative NPCSpawnComponent.
	// Mark OwningSpawnerGUID as the territory's GUID for identification,
	// but OwningSpawn is left null since there's no UNPCSpawnComponent.
	SpawnInfo.OwningSpawnerGUID = TerritoryGuid;

	// Store territory context for BP-accessible ReturnToTerritory activity
	TerritoryHomeTransform = InSpawnTransform;
	OwningTerritory = InOwningTerritory;
	OwningTerritorySpawnPoint = InOwningSpawnPoint;

	// Override factions to exactly the territory owner
	if (ExactFaction.IsValid())
	{
		SpawnInfo.SpawnParams.bOverride_DefaultFactions = true;
		SpawnInfo.SpawnParams.DefaultFactions.Reset();
		SpawnInfo.SpawnParams.DefaultFactions.AddTag(ExactFaction);
	}

	// Optional activity configuration override
	if (OptionalActivityOverride)
	{
		SpawnInfo.SpawnParams.bOverride_ActivityConfiguration = true;
		SpawnInfo.SpawnParams.ActivityConfiguration = FSoftObjectPath(OptionalActivityOverride);
	}

	// Optional trigger set overrides
	if (!OptionalTriggerOverrides.IsEmpty())
	{
		SpawnInfo.SpawnParams.bOverride_TriggerSets = true;
		SpawnInfo.SpawnParams.TriggerSets = OptionalTriggerOverrides;
	}

	// Context is complete before Narrative applies activities and TriggerSets.
	SetNPCDefinition(Definition);
	return true;
}

FGuid ATerritoryGuardCharacter::GetActorGUID_Implementation() const
{
	if (SpawnInfo.SpawnAssignedSaveGUID.IsValid())
	{
		return SpawnInfo.SpawnAssignedSaveGUID;
	}

	// WARNING: This fallback generates a runtime GUID that differs each session.
	// Currently safe because ShouldRespawn_Implementation returns false — guards
	// are never saved. If a subclass overrides ShouldRespawn to return true, the
	// GUID will not match between save and load sessions, silently breaking save/load.
	// Override GetActorGUID in the subclass to provide a stable GUID (e.g. from
	// SpawnAssignedSaveGUID which is set in ConfigureTerritorySpawn).
	if (!CachedFallbackGUID.IsValid())
	{
		const_cast<ATerritoryGuardCharacter*>(this)->CachedFallbackGUID = FGuid::NewGuid();
	}
	return CachedFallbackGUID;
}

void ATerritoryGuardCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

TArray<FTerritoryPatrolNode> ATerritoryGuardCharacter::GetTerritoryPatrolRoute() const
{
	if (IsValid(OwningTerritorySpawnPoint))
	{
		return OwningTerritorySpawnPoint->GetPatrolRoute();
	}
	return TArray<FTerritoryPatrolNode>();
}

bool ATerritoryGuardCharacter::HasTerritoryPatrolRoute() const
{
	if (!IsValid(OwningTerritorySpawnPoint))
	{
		return false;
	}
	return OwningTerritorySpawnPoint->HasPatrolRoute();
}

int32 ATerritoryGuardCharacter::GetPatrolNodeCount() const
{
	if (IsValid(OwningTerritorySpawnPoint))
	{
		return OwningTerritorySpawnPoint->GetPatrolRoute().Num();
	}
	return 0;
}

bool ATerritoryGuardCharacter::GetSafePatrolNode(int32 Index, FTerritoryPatrolNode& OutNode) const
{
	if (!IsValid(OwningTerritorySpawnPoint))
	{
		return false;
	}
	const TArray<FTerritoryPatrolNode>& Route = OwningTerritorySpawnPoint->GetPatrolRoute();
	if (!Route.IsValidIndex(Index))
	{
		return false;
	}
	OutNode = Route[Index];
	return true;
}

FTransform ATerritoryGuardCharacter::GetSpawnTransform() const
{
	return TerritoryHomeTransform;
}

ATerritoryVolume* ATerritoryGuardCharacter::GetOwningTerritory() const
{
	return OwningTerritory;
}

FGameplayTag ATerritoryGuardCharacter::GetGuardFaction() const
{
	TArray<FGameplayTag> Factions;
	GetFactions().GetGameplayTagArray(Factions);
	if (!Factions.IsEmpty())
	{
		return Factions[0];
	}
	return FGameplayTag();
}

bool ATerritoryGuardCharacter::IsSpawnPointGuard() const
{
	return IsValid(OwningTerritorySpawnPoint);
}

bool ATerritoryGuardCharacter::InitializeTerritoryPatrolGoal()
{
	if (!HasAuthority() || !HasTerritoryPatrolRoute() || !PatrolGoalClass)
	{
		return false;
	}

	UNPCActivityComponent* ActivityComponent = GetActivityComponent();
	if (!ActivityComponent)
	{
		return false;
	}

	UObject* GoalKey = OwningTerritorySpawnPoint ? static_cast<UObject*>(OwningTerritorySpawnPoint) : this;
	bool bFoundExisting = false;
	if (UTerritoryPatrolGoal* ExistingGoal = Cast<UTerritoryPatrolGoal>(
		ActivityComponent->GetGoalByKey(PatrolGoalClass, GoalKey, bFoundExisting)))
	{
		ExistingGoal->TerritoryPatrol = GetTerritoryPatrolRoute();
		TerritoryPatrolGoal = ExistingGoal;
		return true;
	}

	UTerritoryPatrolGoal* NewGoal = NewObject<UTerritoryPatrolGoal>(ActivityComponent, PatrolGoalClass);
	if (!NewGoal)
	{
		return false;
	}

	NewGoal->GoalKey = GoalKey;
	NewGoal->TerritoryPatrol = GetTerritoryPatrolRoute();
	NewGoal->bSaveGoal = false;
	NewGoal->bRemoveOnSucceeded = false;
	TerritoryPatrolGoal = Cast<UTerritoryPatrolGoal>(ActivityComponent->AddGoal(NewGoal, true));
	return IsValid(TerritoryPatrolGoal);
}

#include "Combat/TerritoryAssaultCharacter.h"

#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "Character/NarrativeCharacterVisual.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "NarrativeArsenal.h"
#include "Tales/TriggerSet.h"

namespace
{
	struct FPendingTerritoryAssaultSpawn
	{
		UNPCDefinition* Definition = nullptr;
		FGameplayTag ExactFaction;
		FGuid TerritoryGuid;
		FGuid SpawnGuid;
		FTransform SpawnTransform;
		FName SpawnName;
		FGuid AssaultID;
		FGameplayTag TargetTerritory;
		bool bApplied = false;
	};

	// Narrative's SpawnNPC call is synchronous and server/game-thread only. Keeping
	// this context scoped avoids a second spawning authority while allowing the
	// complete Territory SpawnInfo to exist before Narrative applies the definition.
	FPendingTerritoryAssaultSpawn* GPendingTerritoryAssaultSpawn = nullptr;

	void ApplyNarrativeCollisionOverrides(ATerritoryAssaultCharacter& Character)
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

ATerritoryAssaultCharacter::ATerritoryAssaultCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTerritoryDiplomacyInteractable>(
		TEXT("NPCInteractable")))
{
	AssaultParticipant = CreateDefaultSubobject<UTerritoryAssaultParticipantComponent>(TEXT("TerritoryAssaultParticipant"));
	DiplomacyDialogue = CreateDefaultSubobject<UTerritoryDiplomacyDialogueComponent>(
		TEXT("TerritoryDiplomacyDialogue"));

	// Counterattack pawns are always created dynamically. Narrative activities and
	// attack tokens live on ANarrativeNPCController, so the native class must be a
	// complete usable default rather than relying on every project to create a BP child.
	AIControllerClass = ANarrativeNPCController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Narrative customizes individual channel responses in its base constructor,
	// which leaves the serialized profile name as the non-existent profile "Custom".
	// Restore valid named profiles for component initialization; BeginPlay reapplies
	// the exact Narrative channel overrides after registration without a bad lookup.
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	GetMesh()->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
}

bool ATerritoryAssaultCharacter::ValidateNarrativeSpawnClass(
	const UClass* CandidateClass, FText& OutFailureReason)
{
	OutFailureReason = FText();
	if (!CandidateClass || !CandidateClass->IsChildOf(StaticClass()))
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "WrongAssaultPawnClass",
			"NPC class must derive from ATerritoryAssaultCharacter.");
		return false;
	}

	const ATerritoryAssaultCharacter* CDO =
		CandidateClass->GetDefaultObject<ATerritoryAssaultCharacter>();
	if (!CDO || !CDO->AIControllerClass
		|| !CDO->AIControllerClass->IsChildOf(ANarrativeNPCController::StaticClass()))
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "MissingNarrativeController",
			"NPC class must use an ANarrativeNPCController-derived AI Controller Class.");
		return false;
	}

	if (CDO->AutoPossessAI != EAutoPossessAI::Spawned
		&& CDO->AutoPossessAI != EAutoPossessAI::PlacedInWorldOrSpawned)
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "SpawnedAutoPossessRequired",
			"NPC class Auto Possess AI must include Spawned actors.");
		return false;
	}

	return true;
}

bool ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
	const UNPCDefinition* Definition, int32 PlannedForce, FText& OutFailureReason)
{
	OutFailureReason = FText();
	if (!Definition)
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "MissingAssaultDefinition",
			"A Narrative NPC definition is required for physical assaults.");
		return false;
	}
	if (Definition->CharacterID.IsNone())
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "MissingAssaultCharacterID",
			"Narrative assault definition must have a stable CharacterID.");
		return false;
	}
	if (Definition->NPCID.IsNone())
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "MissingAssaultNPCID",
			"Narrative assault definition must have a stable NPCID while Narrative uses it for NPC lookup and duplicate admission.");
		return false;
	}

	UClass* CandidateClass = Definition->NPCClassPath.LoadSynchronous();
	if (!ValidateNarrativeSpawnClass(CandidateClass, OutFailureReason))
	{
		return false;
	}
	if (PlannedForce > 1 && !Definition->bAllowMultipleInstances)
	{
		OutFailureReason = NSLOCTEXT("TerritoryAssaultCharacter", "MultipleAssaultInstancesRequired",
			"Narrative NPC definition must allow multiple instances when Planned Force is greater than one.");
		return false;
	}
	return true;
}

ATerritoryAssaultCharacter* ATerritoryAssaultCharacter::SpawnThroughNarrative(
	UNarrativeCharacterSubsystem* CharacterSubsystem, UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction, const FGuid& TerritoryGuid,
	const FGuid& SpawnGuid, const FTransform& SpawnTransform, FName SpawnName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
	const FGuid& AssaultID, const FGameplayTag& TargetTerritory,
	int32 OverrideNarrativeLevel)
{
	if (!IsInGameThread() || !CharacterSubsystem || !Definition || !ExactFaction.IsValid()
		|| !TerritoryGuid.IsValid() || !SpawnGuid.IsValid() || !AssaultID.IsValid()
		|| !TargetTerritory.IsValid() || GPendingTerritoryAssaultSpawn)
	{
		return nullptr;
	}

	FNPCSpawnParams SpawnParams;
	if (OverrideNarrativeLevel > 0)
	{
		SpawnParams.bOverride_LevelRange = true;
		SpawnParams.MinLevel = OverrideNarrativeLevel;
		SpawnParams.MaxLevel = OverrideNarrativeLevel;
	}
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

	FPendingTerritoryAssaultSpawn Context;
	Context.Definition = Definition;
	Context.ExactFaction = ExactFaction;
	Context.TerritoryGuid = TerritoryGuid;
	Context.SpawnGuid = SpawnGuid;
	Context.SpawnTransform = SpawnTransform;
	Context.SpawnName = SpawnName;
	Context.AssaultID = AssaultID;
	Context.TargetTerritory = TargetTerritory;
	TGuardValue<FPendingTerritoryAssaultSpawn*> PendingGuard(
		GPendingTerritoryAssaultSpawn, &Context);

	ANarrativeNPCCharacter* Spawned = CharacterSubsystem->SpawnNPC(
		Definition, SpawnTransform, SpawnParams);
	ATerritoryAssaultCharacter* AssaultCharacter =
		Cast<ATerritoryAssaultCharacter>(Spawned);
	if (!Context.bApplied || !AssaultCharacter)
	{
		if (Spawned)
		{
			TerritoryNarrativeDeathSupport::PrepareForRemoval(*Spawned);
			CharacterSubsystem->DestroyNPC(Spawned);
		}
		return nullptr;
	}
	return AssaultCharacter;
}

bool ATerritoryAssaultCharacter::EnsureNarrativeControllerReady()
{
	if (!GetNPCController())
	{
		SpawnDefaultController();
	}
	return GetNPCController() && GetActivityComponent();
}

bool ATerritoryAssaultCharacter::IsNarrativeSpawnReady() const
{
	if (!GetNPCDefinition() || !GetCharacterDefinition()
		|| !GetNPCController() || !GetActivityComponent())
	{
		return false;
	}
	ANarrativeCharacterVisual* Visual = GetCharacterVisual();
	if (!Visual || Visual->HasLoadHandles())
	{
		return false;
	}
	return true;
}

bool ATerritoryAssaultCharacter::HasValidDeathRagdollSetup() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	return MeshComponent && MeshComponent->GetSkeletalMeshAsset()
		&& MeshComponent->GetPhysicsAsset();
}

void ATerritoryAssaultCharacter::SetNPCDefinition(UNPCDefinition* Definition)
{
	if (GPendingTerritoryAssaultSpawn
		&& GPendingTerritoryAssaultSpawn->Definition == Definition
		&& !GPendingTerritoryAssaultSpawn->bApplied)
	{
		SpawnInfo.OwningSpawnerGUID = GPendingTerritoryAssaultSpawn->TerritoryGuid;
		SpawnInfo.SpawnAssignedSaveGUID = GPendingTerritoryAssaultSpawn->SpawnGuid;
		SpawnInfo.SpawnTransform = GPendingTerritoryAssaultSpawn->SpawnTransform;
		SpawnInfo.SpawnName = GPendingTerritoryAssaultSpawn->SpawnName;
		if (AssaultParticipant)
		{
			AssaultParticipant->Configure(
				GPendingTerritoryAssaultSpawn->AssaultID,
				GPendingTerritoryAssaultSpawn->TerritoryGuid,
				GPendingTerritoryAssaultSpawn->TargetTerritory,
				GPendingTerritoryAssaultSpawn->ExactFaction);
		}
		GPendingTerritoryAssaultSpawn->bApplied = true;
	}
	Super::SetNPCDefinition(Definition);
}

ETeamAttitude::Type ATerritoryAssaultCharacter::GetTeamAttitudeTowards(
	const AActor& Other) const
{
	if (CanEngageAssaultTarget(&Other))
	{
		return ETeamAttitude::Hostile;
	}
	const ETeamAttitude::Type NarrativeAttitude = Super::GetTeamAttitudeTowards(Other);
	return NarrativeAttitude == ETeamAttitude::Friendly
		? ETeamAttitude::Friendly : ETeamAttitude::Neutral;
}

bool ATerritoryAssaultCharacter::CanEngageAssaultTarget(const AActor* Target) const
{
	if (!IsValid(Target) || Target == this || !AssaultParticipant
		|| !AssaultParticipant->IsConfigured())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UTerritoryCounterAttackSubsystem* Counterattacks = World
		? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	const INarrativeTeamAgentInterface* TargetTeam =
		Cast<INarrativeTeamAgentInterface>(Target);
	return Counterattacks && Diplomacy && TargetTeam
		&& Counterattacks->IsAssaultActive(AssaultParticipant->GetAssaultID())
		&& Diplomacy->AreAnyFactionsAtWar(GetFactions(), TargetTeam->GetFactions());
}

void ATerritoryAssaultCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyNarrativeCollisionOverrides(*this);
}

void ATerritoryAssaultCharacter::HandleDeath_Implementation(
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	// Narrative remains the death/ragdoll authority. The explicit override makes
	// the assault contract non-optional and gives invalid runtime appearances an
	// actionable diagnostic instead of silently leaving a standing corpse.
	const bool bResolvedIsDead = TerritoryNarrativeDeathSupport::ResolveDeathState(
		KilledActorASC, bIsDead);
	if (bResolvedIsDead)
	{
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*this);
	}
	Super::HandleDeath_Implementation(KilledActor, KilledActorASC, bResolvedIsDead);
	if (!bResolvedIsDead)
	{
		return;
	}
	TerritoryNarrativeDeathSupport::FinalizePhysicalDeath(*this);
	if (!HasValidDeathRagdollSetup())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack participant %s died without a valid Narrative ragdoll mesh/physics asset"),
			*GetNameSafe(this));
	}
}

void ATerritoryAssaultCharacter::ReconcileNarrativeDeathState(
	UNarrativeAbilitySystemComponent* KilledActorASC, const bool bReportedIsDead)
{
	if (TerritoryNarrativeDeathSupport::ResolveDeathState(
		KilledActorASC, bReportedIsDead))
	{
		TerritoryNarrativeDeathSupport::FinalizePhysicalDeath(*this);
	}
}

void ATerritoryAssaultCharacter::SetRagdoll(const bool bWantsRagdoll)
{
	if (!HasAuthority())
	{
		return;
	}
	Super::SetRagdoll(bWantsRagdoll);
}

FGuid ATerritoryAssaultCharacter::GetActorGUID_Implementation() const
{
	return SpawnInfo.SpawnAssignedSaveGUID;
}

void ATerritoryAssaultCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

bool ATerritoryAssaultCharacter::ShouldRespawn_Implementation() const
{
	// The durable assault record reconstructs surviving finite force after load.
	// Saving individual live pawn pointers/records would create a second authority.
	return false;
}

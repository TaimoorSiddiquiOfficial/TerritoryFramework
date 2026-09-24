#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryWorldState.h"
#include "AIController.h"
#include "ArsenalSettings.h"
#include "Perception/AIPerceptionComponent.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCInteractable.h"
#include "Character/CharacterMapMarker.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapons/WeaponVisual.h"
#include "NarrativeArsenal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool IsPlayerFactionFallbackCandidate(const AActor* Actor)
	{
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->IsPlayerControlled();
		}
		return Cast<APlayerController>(Actor) != nullptr;
	}

	FGameplayTagContainer GetConfiguredPlayerFactionFallback(const AActor* Actor)
	{
		FGameplayTagContainer Result;
		if (!IsPlayerFactionFallbackCandidate(Actor)) return Result;

		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && Settings->DefaultPlayerFaction.IsValid())
		{
			Result.AddTag(Settings->DefaultPlayerFaction);
		}
		return Result;
	}
}

UTerritoryRegistrySubsystem* UTerritoryBlueprintLibrary::GetTerritoryRegistry(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
}

UTerritoryControlSubsystem* UTerritoryBlueprintLibrary::GetTerritoryControl(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
}

UTerritoryEconomySubsystem* UTerritoryBlueprintLibrary::GetTerritoryEconomy(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
}

UTerritoryCombatDirector* UTerritoryBlueprintLibrary::GetTerritoryCombatDirector(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryCombatDirector>() : nullptr;
}

UTerritoryDiplomacySubsystem* UTerritoryBlueprintLibrary::GetTerritoryDiplomacy(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
}

ATerritoryVolume* UTerritoryBlueprintLibrary::GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryAtLocation(WorldLocation) : nullptr;
}

ATerritoryVolume* UTerritoryBlueprintLibrary::GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryByTag(TerritoryTag) : nullptr;
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetAllTerritories(const UObject* WorldContextObject)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetAllTerritories() : TArray<ATerritoryVolume*>();
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetTerritoriesByFaction(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoriesOwnedByFaction(FactionTag) : TArray<ATerritoryVolume*>();
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetChildTerritories(const UObject* WorldContextObject, const FGameplayTag& ParentTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetChildTerritories(ParentTag) : TArray<ATerritoryVolume*>();
}

FGameplayTagContainer UTerritoryBlueprintLibrary::GetFactionCommandCapabilities(
	const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	FGameplayTagContainer Result;
	if (!FactionTag.IsValid())
	{
		return Result;
	}

	for (const ATerritoryVolume* Territory : GetAllTerritories(WorldContextObject))
	{
		if (IsValid(Territory) && Territory->GetOwningFaction() == FactionTag)
		{
			Result.AppendTags(Territory->GetActiveCommandCapabilities());
		}
	}
	return Result;
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetFactionCommandCapabilitySources(
	const UObject* WorldContextObject, const FGameplayTag& FactionTag,
	const FGameplayTag& Capability)
{
	TArray<ATerritoryVolume*> Result;
	if (!FactionTag.IsValid() || !Capability.IsValid())
	{
		return Result;
	}

	for (ATerritoryVolume* Territory : GetAllTerritories(WorldContextObject))
	{
		if (IsValid(Territory) && Territory->GetOwningFaction() == FactionTag
			&& Territory->GetActiveCommandCapabilities().HasTagExact(Capability))
		{
			Result.Add(Territory);
		}
	}
	Result.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
	{
		return A.GetTerritoryDisplayName().ToString() < B.GetTerritoryDisplayName().ToString();
	});
	return Result;
}

bool UTerritoryBlueprintLibrary::IsCommandCapabilityUsed(
	const UObject* WorldContextObject, const FGameplayTag& Capability)
{
	if (!Capability.IsValid())
	{
		return false;
	}
	for (const ATerritoryVolume* Territory : GetAllTerritories(WorldContextObject))
	{
		if (IsValid(Territory) && Territory->IsCommandCapabilityConfigured(Capability))
		{
			return true;
		}
	}
	return false;
}

bool UTerritoryBlueprintLibrary::CanFactionUseCommandCapability(
	const UObject* WorldContextObject, const FGameplayTag& FactionTag,
	const FGameplayTag& Capability, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	if (!WorldContextObject || !FactionTag.IsValid() || !Capability.IsValid())
	{
		OutFailureReason = NSLOCTEXT("TerritoryCommand", "InvalidCapabilityContext",
			"The faction command network is unavailable.");
		return false;
	}

	// Capability gates are opt-in. Community projects authored before the command
	// network keep their existing controls until at least one Territory configures
	// the relevant capability in a State Config.
	if (!IsCommandCapabilityUsed(WorldContextObject, Capability))
	{
		return true;
	}
	if (GetFactionCommandCapabilities(WorldContextObject, FactionTag).HasTagExact(Capability))
	{
		return true;
	}

	OutFailureReason = FText::Format(
		NSLOCTEXT("TerritoryCommand", "CapabilityNotHeld",
			"Capture and hold a Territory whose active State Config grants {0}. Losing that Territory removes this control."),
		GetFriendlyTagDisplayName(Capability));
	return false;
}

int32 UTerritoryBlueprintLibrary::GetTerritoryCount(const UObject* WorldContextObject)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryCount() : 0;
}

int32 UTerritoryBlueprintLibrary::GetFactionTerritoryCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryCountForFaction(FactionTag) : 0;
}

bool UTerritoryBlueprintLibrary::IsTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation)
{
	return GetTerritoryAtLocation(WorldContextObject, WorldLocation) != nullptr;
}

bool UTerritoryBlueprintLibrary::GetTerritoryFloorGuards(const ATerritoryVolume* Territory,
	int32 FloorIndex, FTerritoryFloorSnapshot& OutFloor)
{
	if (!Territory) return false;
	// The snapshot is the replicated read model, so this resolves identically on a client.
	const FTerritoryGarrisonSnapshot Snapshot = Territory->GetGarrisonSnapshot();
	const FTerritoryFloorSnapshot* Found = Snapshot.Floors.FindByPredicate(
		[FloorIndex](const FTerritoryFloorSnapshot& Entry)
		{
			return Entry.FloorIndex == FloorIndex;
		});
	if (!Found) return false;
	OutFloor = *Found;
	return true;
}

bool UTerritoryBlueprintLibrary::IsTerritoryFloorCleared(const FTerritoryFloorSnapshot& Floor)
{
	return Floor.IsCleared();
}

int32 UTerritoryBlueprintLibrary::GetFactionGold(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetTreasury(FactionTag) : 0;
}

int32 UTerritoryBlueprintLibrary::GetFactionIncome(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetIncome(FactionTag) : 0;
}

TArray<FGameplayTag> UTerritoryBlueprintLibrary::GetAllFactions(const UObject* WorldContextObject)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetAllFactionsWithTreasury() : TArray<FGameplayTag>();
}

ETerritoryState UTerritoryBlueprintLibrary::GetTerritoryState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	return Territory ? Territory->GetTerritoryState() : ETerritoryState::Unclaimed;
}

float UTerritoryBlueprintLibrary::GetCaptureProgress(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	return Territory ? Territory->GetControlProgress() : 0.f;
}

void UTerritoryBlueprintLibrary::ForceCaptureTerritory(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag, const FGameplayTag& FactionTag)
{
	UTerritoryControlSubsystem* Control = GetTerritoryControl(WorldContextObject);
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	if (Control && Territory)
	{
		Control->ForceCapture(Territory, FactionTag);
	}
}

EDiplomacyState UTerritoryBlueprintLibrary::GetTreatyState(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->GetDiplomacyState(FactionA, FactionB) : EDiplomacyState::None;
}

bool UTerritoryBlueprintLibrary::IsAllied(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->IsAllied(FactionA, FactionB) : false;
}

bool UTerritoryBlueprintLibrary::IsAtWar(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->IsAtWar(FactionA, FactionB) : false;
}

bool UTerritoryBlueprintLibrary::IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	return FactionA == FactionB && FactionA.IsValid();
}

FGameplayTag UTerritoryBlueprintLibrary::GetNarrativeAttackDamageSetByCallerTag()
{
	return FNarrativeGameplayTags::Get().SetByCaller_AttackDamage;
}

FText UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return FText::GetEmpty();
	}

	// Narrative Pro owns project-wide player-facing Gameplay Tag names through
	// Project Settings > Narrative Pro > GAS > Tag Friendly Display Names.
	// Territory respects that authoring first so the same faction, capability,
	// equipment, and input tag is named consistently in every Narrative screen.
	if (const UArsenalSettings* NarrativeSettings = GetDefault<UArsenalSettings>())
	{
		if (const FText* NarrativeDisplayName =
			NarrativeSettings->TagFriendlyDisplayNames.Find(Tag))
		{
			if (!NarrativeDisplayName->IsEmpty())
			{
				return *NarrativeDisplayName;
			}
		}
	}

	FString FriendlyName = Tag.ToString();
	int32 LastSeparator = INDEX_NONE;
	if (FriendlyName.FindLastChar(TEXT('.'), LastSeparator))
	{
		FriendlyName = FriendlyName.Mid(LastSeparator + 1);
	}
	FriendlyName.ReplaceInline(TEXT("_"), TEXT(" "));

	for (int32 Index = 1; Index < FriendlyName.Len(); ++Index)
	{
		if (FChar::IsUpper(FriendlyName[Index]) && FChar::IsLower(FriendlyName[Index - 1]))
		{
			FriendlyName.InsertAt(Index, TEXT(' '));
			++Index;
		}
	}

	return FText::FromString(FriendlyName);
}

bool UTerritoryBlueprintLibrary::CanScoreTerritoryCombatGoal(
	const ANarrativeNPCController* OwnerController, const UNPCGoalItem* Goal)
{
	if (!IsValid(OwnerController) || !OwnerController->HasAuthority()
		|| OwnerController->IsActorBeingDestroyed() || !IsValid(Goal)
		|| Goal->OwnerController != OwnerController) return false;

	// A controller driving a Narrative vehicle is not currently controlling an
	// on-foot combatant. Do not use GetOwnedNPC(), which also returns the driver.
	const ANarrativeNPCCharacter* NPC = OwnerController->GetControlledNPC();
	const AActor* Target = Cast<AActor>(Goal->GetGoalKey());
	if (!IsValid(NPC) || !NPC->HasAuthority() || !NPC->IsAlive()
		|| NPC->IsActorBeingDestroyed() || !IsValid(Target) || Target == NPC
		|| Target->IsActorBeingDestroyed() || Target->GetWorld() != NPC->GetWorld()) return false;
	if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Target);
		Character && !Character->IsAlive()) return false;

	if (const ATerritoryAssaultCharacter* Assault = Cast<ATerritoryAssaultCharacter>(NPC))
	{
		const UTerritoryAssaultParticipantComponent* Participant = Assault->AssaultParticipant;
		return Participant && !Participant->HasRetired()
			&& IsValid(Participant->GetTargetTerritory())
			&& Assault->CanEngageAssaultTarget(Target);
	}
	if (const ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(NPC))
	{
		return Guard->CanEngageTerritoryTarget(Target);
	}
	return true;
}

bool UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(
	const UObject* GoalGenerator, const AAIController* OwnerController)
{
	if (!IsValid(GoalGenerator) || !IsValid(OwnerController)
		|| OwnerController->IsActorBeingDestroyed())
	{
		return false;
	}

	const APawn* ControlledPawn = OwnerController->GetPawn();
	const UAIPerceptionComponent* Perception =
		OwnerController->GetAIPerceptionComponent();
	const UNPCActivityComponent* ActivityComponent =
		Cast<UNPCActivityComponent>(GoalGenerator->GetOuter());
	return IsValid(ControlledPawn)
		&& IsValid(Perception)
		&& Perception->GetOwner() == OwnerController
		&& IsValid(ActivityComponent)
		&& ActivityComponent->IsActive()
		// Narrative owns activities on ANarrativeNPCController, including while
		// that controller temporarily possesses a vehicle.
		&& ActivityComponent->GetOwner() == OwnerController;
}

bool UTerritoryBlueprintLibrary::RefreshParentPerceivedActorsSafely(
	UObject* GoalGenerator, AAIController* OwnerController)
{
	if (!CanSafelyRefreshPerceivedActors(GoalGenerator, OwnerController))
	{
		return false;
	}

	static const FName RefreshFunctionName(TEXT("RefreshPerceivedActors"));
	UFunction* OverrideFunction = GoalGenerator->FindFunction(RefreshFunctionName);
	UFunction* ParentFunction = OverrideFunction
		? OverrideFunction->GetSuperFunction() : nullptr;
	if (!ParentFunction || ParentFunction == OverrideFunction
		|| ParentFunction->ParmsSize != 0)
	{
		return false;
	}

	// Invoke the exact inherited Blueprint bytecode rather than ProcessEvent by name,
	// which would dispatch back into this project-owned override and recurse.
	GoalGenerator->ProcessEvent(ParentFunction, nullptr);
	return true;
}

bool UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(
	ANarrativeNPCCharacter* NPC)
{
	if (!IsValid(NPC) || NPC->HasAuthority() || NPC->IsActorBeingDestroyed()
		|| !NPC->GetWorld() || NPC->GetWorld()->bIsTearingDown) return false;
	const UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent();
	if (!IsValid(ASC)) return false;
	const bool bDead = ASC->IsDead();

	// The simulated proxy has no AI controller. Do not run the Native Blueprint's
	// RemoveAllGoals path or its C++ SetRagdoll call, which would send an unowned RPC.
	// These are local read-model updates; the ASC and ragdoll property still replicate.
	if (UCharacterMovementComponent* Movement = NPC->GetCharacterMovement(); bDead && Movement)
		Movement->StopMovementImmediately();
	if (UCapsuleComponent* Capsule = NPC->GetCapsuleComponent())
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bDead ? ECR_Ignore : ECR_Block);
	if (USkeletalMeshComponent* Mesh = NPC->GetMesh())
		Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction,
			bDead ? ECR_Block : ECR_Ignore);
	if (UCharacterMapMarker* Marker = NPC->GetMarkerComponent())
	{
		if (bDead) Marker->RemoveMarker();
		else Marker->RegisterMarker();
	}
	if (UNPCInteractable* Interactable = NPC->FindComponentByClass<UNPCInteractable>())
	{
		const UNPCInteractable* Defaults = Cast<UNPCInteractable>(Interactable->GetArchetype());
		Interactable->SetInteractableActionText(bDead
			? NSLOCTEXT("NPCCharacter", "LootInteractText", "Loot")
			: (Defaults ? Defaults->GetInteractableActionText(nullptr, nullptr)
				: NSLOCTEXT("NPCInteractable", "TalkInteractableActionText", "Talk")));
	}
	TArray<AActor*> ChildActors;
	NPC->GetAllChildActors(ChildActors);
	for (AActor* Child : ChildActors)
		if (IsValid(Child)) Child->SetActorHiddenInGame(bDead);
	// Native's multiplayer NPC Blueprint removes the equipped weapon visual on death.
	// Do not drop inventory, create a pickup or simulate a second weapon on the client.
	if (AWeaponVisual* Weapon = NPC->GetEquippedWeaponVisual(); bDead && IsValid(Weapon))
		Weapon->Destroy();
	return true;
}

// ─── Narrative Pro Faction Bridge ───

FGameplayTagContainer UTerritoryBlueprintLibrary::GetActorFactions(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTagContainer();
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Actor))
	{
		return TeamAgent->GetFactions();
	}
	return GetConfiguredPlayerFactionFallback(Actor);
}

bool UTerritoryBlueprintLibrary::IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag)
{
	if (!Actor || !FactionTag.IsValid()) return false;
	return GetActorFactions(WorldContextObject, Actor).HasTag(FactionTag);
}

bool UTerritoryBlueprintLibrary::IsNarrativeFactionTag(const FGameplayTag& Tag)
{
	const FGameplayTag Root = FNarrativeGameplayTags::Get().Narrative_Factions;
	return Tag.IsValid() && Tag != Root && Tag.MatchesTag(Root);
}

bool UTerritoryBlueprintLibrary::IsPoliticalFactionTag(const FGameplayTag& Tag)
{
	const FNarrativeGameplayTags& Tags = FNarrativeGameplayTags::Get();
	return IsNarrativeFactionTag(Tag) && Tag != Tags.Narrative_Factions_HostileAll
		&& Tag != Tags.Narrative_Factions_HostileOthers && Tag != Tags.Narrative_Factions_FriendlyAll;
}

FGameplayTag UTerritoryBlueprintLibrary::GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTag();
	const FGameplayTagContainer Factions = GetActorFactions(WorldContextObject, Actor);
	for (const FGameplayTag& Faction : Factions)
	{
		if (IsPoliticalFactionTag(Faction)) return Faction;
	}
	return FGameplayTag();
}

bool UTerritoryBlueprintLibrary::AreActorsAllied(AActor* A, AActor* B)
{
	if (!A || !B) return false;
	const FGameplayTagContainer FactionsA = GetActorFactions(A, A);
	const FGameplayTagContainer FactionsB = GetActorFactions(B, B);
	return !FactionsA.IsEmpty() && FactionsA.HasAny(FactionsB);
}

// ─── City / District Queries ───

TArray<ATerritoryCity*> UTerritoryBlueprintLibrary::GetAllCities(const UObject* WorldContextObject)
{
	TArray<ATerritoryCity*> Result;
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	if (!Registry) return Result;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Vol : All)
	{
		if (ATerritoryCity* City = Cast<ATerritoryCity>(Vol))
		{
			Result.Add(City);
		}
	}
	return Result;
}

TArray<ATerritoryDistrict*> UTerritoryBlueprintLibrary::GetAllDistricts(const UObject* WorldContextObject)
{
	TArray<ATerritoryDistrict*> Result;
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	if (!Registry) return Result;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Vol : All)
	{
		if (ATerritoryDistrict* D = Cast<ATerritoryDistrict>(Vol))
		{
			Result.Add(D);
		}
	}
	return Result;
}

ATerritoryCity* UTerritoryBlueprintLibrary::GetCityForDistrict(const UObject* WorldContextObject, ATerritoryDistrict* District)
{
	if (!District) return nullptr;
	return District->GetOwningCity();
}

bool UTerritoryBlueprintLibrary::DoesFactionControlCity(const UObject* WorldContextObject, ATerritoryCity* City, const FGameplayTag& FactionTag)
{
	if (!City || !FactionTag.IsValid()) return false;
	return City->AllDistrictsOwnedBy(FactionTag);
}

int32 UTerritoryBlueprintLibrary::GetFactionCityCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	if (!FactionTag.IsValid()) return 0;
	int32 Count = 0;
	for (ATerritoryCity* City : GetAllCities(WorldContextObject))
	{
		if (City && City->AllDistrictsOwnedBy(FactionTag))
		{
			++Count;
		}
	}
	return Count;
}

int32 UTerritoryBlueprintLibrary::GetFactionDistrictCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	if (!FactionTag.IsValid()) return 0;
	if (const ATerritoryWorldState* WorldState =
		ATerritoryWorldState::FindTerritoryWorldState(WorldContextObject))
	{
		const bool bHasStrategicDistrictRows =
			WorldState->GetAllCaptureSummaries().ContainsByPredicate(
				[](const FReplicatedCaptureSummary& Summary)
				{
					return Summary.HierarchyLevel
						== ETerritoryHierarchyLevel::District;
				});
		if (bHasStrategicDistrictRows)
		{
			return WorldState->GetClaimedDistrictCountForFaction(FactionTag);
		}
	}

	// Small maps may intentionally omit TerritoryWorldState. In that case the
	// loaded aggregate District remains the authority and must be fully secured.
	int32 Count = 0;
	for (ATerritoryDistrict* D : GetAllDistricts(WorldContextObject))
	{
		if (D && D->IsAvailableForGameplay()
			&& D->GetTerritoryState() == ETerritoryState::Claimed
			&& D->GetOwningFaction() == FactionTag
			&& D->AllPropertiesOwnedBy(FactionTag))
		{
			++Count;
		}
	}
	return Count;
}

TArray<ATerritoryDistrict*> UTerritoryBlueprintLibrary::GetCapitalDistricts(const UObject* WorldContextObject)
{
	TArray<ATerritoryDistrict*> Result;
	for (ATerritoryDistrict* D : GetAllDistricts(WorldContextObject))
	{
		if (D && D->IsCapitalDistrict())
		{
			Result.Add(D);
		}
	}
	return Result;
}

// ─── Debug Helpers ───

void UTerritoryBlueprintLibrary::PrintTerritoryDebug(const UObject* WorldContextObject, ATerritoryVolume* Territory, float Duration)
{
	if (!Territory) return;

	const FString DebugStr = Territory->GetDebugString();
	UE_LOG(LogTerritory, Log, TEXT("%s"), *DebugStr);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Orange, DebugStr);
	}
}

void UTerritoryBlueprintLibrary::PrintAllTerritoryDebug(const UObject* WorldContextObject, float Duration)
{
	TArray<ATerritoryVolume*> All = GetAllTerritories(WorldContextObject);
	for (ATerritoryVolume* Vol : All)
	{
		if (Vol)
		{
			PrintTerritoryDebug(WorldContextObject, Vol, Duration);
		}
	}
}

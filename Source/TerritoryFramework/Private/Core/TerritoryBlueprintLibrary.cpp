#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "AI/Activities/NPCActivityComponent.h"
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

FText UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return FText::GetEmpty();
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
		&& ActivityComponent->GetOwner() == ControlledPawn;
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

// ─── Narrative Pro Faction Bridge ───

FGameplayTagContainer UTerritoryBlueprintLibrary::GetActorFactions(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTagContainer();
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Actor))
	{
		FGameplayTagContainer Factions = TeamAgent->GetFactions();
		if (!Factions.IsEmpty()) return Factions;
	}
	return GetConfiguredPlayerFactionFallback(Actor);
}

bool UTerritoryBlueprintLibrary::IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag)
{
	if (!Actor || !FactionTag.IsValid()) return false;
	return GetActorFactions(WorldContextObject, Actor).HasTag(FactionTag);
}

FGameplayTag UTerritoryBlueprintLibrary::GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTag();
	const FGameplayTagContainer Factions = GetActorFactions(WorldContextObject, Actor);
	if (!Factions.IsEmpty())
	{
		return Factions.GetByIndex(0);
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
	int32 Count = 0;
	for (ATerritoryDistrict* D : GetAllDistricts(WorldContextObject))
	{
		if (D && D->IsOwnedByFaction(FactionTag))
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

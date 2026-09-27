#include "AI/TerritoryInvestigationGoal.h"
#include "Engine/World.h"

#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "AI/NarrativeNPCController.h"

UTerritoryInvestigationGoal::UTerritoryInvestigationGoal(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Narrative attack goals normally score 3. Investigation deliberately sits
	// below combat and above the Territory patrol goal (1).
	DefaultScore = 2.5f;
	bRemoveOnSucceeded = true;
	bSaveGoal = false;
	GoalLifetime = 12.f;
	OwnedTags.AddTag(TerritoryStealthTags::InvestigationActive);
}

void UTerritoryInvestigationGoal::Refresh(const FGameplayTag& InTerritoryTag,
	ETerritoryStealthEvidence InEvidence, const FVector& InLocation,
	const FVector& InEstimatedDirection, AActor* InSuspectedSource,
	bool bInIdentityConfirmed, float InLifetime, float InAcceptanceRadius)
{
	TerritoryTag = InTerritoryTag;
	Evidence = InEvidence;
	InvestigationLocation = InLocation;
	EstimatedSourceDirection = InEstimatedDirection.GetSafeNormal();
	SuspectedSource = InSuspectedSource;
	bIdentityConfirmed = bInIdentityConfirmed;
	GoalLifetime = FMath::Max(0.5f, InLifetime);
	AcceptanceRadius = FMath::Max(10.f, InAcceptanceRadius);
	if (UWorld* World = GetWorld())
	{
		CreationTime = World->GetTimeSeconds();
	}
}

FString UTerritoryInvestigationGoal::GetDebugString_Implementation() const
{
	const UEnum* EvidenceEnum = StaticEnum<ETerritoryStealthEvidence>();
	const FString EvidenceName = EvidenceEnum
		? EvidenceEnum->GetDisplayNameTextByValue(static_cast<int64>(Evidence)).ToString()
		: TEXT("Unknown");
	return FString::Printf(TEXT("Territory investigate %s at %s in %s"),
		*EvidenceName, *InvestigationLocation.ToCompactString(), *TerritoryTag.ToString());
}

bool UTerritoryInvestigationGoal::ShouldCleanup_Implementation() const
{
	if (bRequireDamageReport)
	{
		const ATerritoryGuardCharacter* Guard = OwnerController
			? Cast<ATerritoryGuardCharacter>(OwnerController->GetControlledNPC()) : nullptr;
		if (!Guard || !Guard->HasAuthority() || !SuspectedSource.IsValid()
			|| !IsValid(Guard->OwningTerritorySpawnPoint)
			|| !Guard->OwningTerritorySpawnPoint->HasRecentGuardDamageFrom(SuspectedSource.Get())
			|| !Guard->CanInvestigateRecentGuardDamage(SuspectedSource.Get())) return true;
	}
	return !TerritoryTag.IsValid() || GoalLifetime >= 0.f
		&& GetGoalAgeSeconds() >= GoalLifetime;
}

float UTerritoryInvestigationGoal::GetGoalScore_Implementation() const
{
	return bRequireDamageReport && ShouldCleanup_Implementation()
		? -1.f : Super::GetGoalScore_Implementation();
}

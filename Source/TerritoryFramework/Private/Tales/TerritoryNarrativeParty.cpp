#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

namespace
{
	UTalesComponent* GetPartyMember(ANarrativeParty* Party, ANarrativePlayerState* State)
	{
		if (!Party->HasAuthority() || !IsValid(State) || !State->HasAuthority() || State->GetWorld() != Party->GetWorld()) return nullptr;
		ANarrativePlayerController* Controller = Cast<ANarrativePlayerController>(State->GetOwningController());
		return IsValid(Controller) && Controller->HasAuthority() && Controller->PlayerState == State
			&& Controller->GetWorld() == Party->GetWorld() ? Controller->GetTalesComponent() : nullptr;
	}
}

ATerritoryNarrativeParty::ATerritoryNarrativeParty(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTerritoryNarrativePartyComponent>(TEXT("PartyTalesComponent")))
{
}

void ATerritoryNarrativeParty::AddPartyMember(ANarrativePlayerState* PS)
{
	// The component commits Native membership and its actor read model together.
	// Super's actor wrapper fills the caches even when the component rejects a join.
	if (UTalesComponent* Member = GetPartyMember(this, PS))
	{
		if (IsValid(PartyTalesComponent)) PartyTalesComponent->AddPartyMember(Member);
	}
}

void ATerritoryNarrativeParty::RemovePartyMember(ANarrativePlayerState* PS)
{
	if (UTalesComponent* Member = GetPartyMember(this, PS))
	{
		if (IsValid(PartyTalesComponent)) PartyTalesComponent->RemovePartyMember(Member);
	}
}

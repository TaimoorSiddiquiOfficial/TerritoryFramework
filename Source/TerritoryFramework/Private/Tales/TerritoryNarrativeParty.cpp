#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"

ATerritoryNarrativeParty::ATerritoryNarrativeParty(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTerritoryNarrativePartyComponent>(TEXT("PartyTalesComponent")))
{
}

void ATerritoryNarrativeParty::AddPartyMember(ANarrativePlayerState* PS)
{
	if (HasAuthority()) Super::AddPartyMember(PS);
}

void ATerritoryNarrativeParty::RemovePartyMember(ANarrativePlayerState* PS)
{
	if (HasAuthority()) Super::RemovePartyMember(PS);
}

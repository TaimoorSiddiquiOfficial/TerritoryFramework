#include "Core/TerritoryDisguiseProfile.h"

#include "Core/TerritoryStealthTags.h"

UTerritoryDisguiseProfile::UTerritoryDisguiseProfile()
{
	ActivatedEventTag = TerritoryStealthTags::DisguiseActivatedEvent;
	RemovedEventTag = TerritoryStealthTags::DisguiseRemovedEvent;
	CompromisedEventTag = TerritoryStealthTags::DisguiseCompromisedEvent;
	RestoredEventTag = TerritoryStealthTags::DisguiseRestoredEvent;
	IdentityCheckPassedEventTag = TerritoryStealthTags::DisguiseCheckPassedEvent;
	IdentityCheckFailedEventTag = TerritoryStealthTags::DisguiseCheckFailedEvent;
}

FPrimaryAssetId UTerritoryDisguiseProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryDisguiseProfile"), GetFName());
}

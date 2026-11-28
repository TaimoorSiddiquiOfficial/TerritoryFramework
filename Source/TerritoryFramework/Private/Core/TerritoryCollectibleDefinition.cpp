#include "Core/TerritoryCollectibleDefinition.h"

#include "Core/TerritoryCollectibleTags.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TerritoryCollectibleDefinition"

int32 UTerritoryCollectibleSet::LoadDefinitions(TArray<UTerritoryCollectibleDefinition*>& OutDefinitions) const
{
	const int32 StartCount = OutDefinitions.Num();
	for (const TSoftObjectPtr<UTerritoryCollectibleDefinition>& Entry : Collectibles)
	{
		if (UTerritoryCollectibleDefinition* Definition = Entry.LoadSynchronous())
		{
			OutDefinitions.Add(Definition);
		}
	}
	return OutDefinitions.Num() - StartCount;
}

int32 UTerritoryCollectibleSet::GetCompletionCount() const
{
	// The denominator has to be true even before the player opens the Dossier, so this
	// resolves the set. A count that silently drops unresolved entries would make
	// "found 3 of 5" become "found 3 of 3" while content is still streaming in.
	TArray<UTerritoryCollectibleDefinition*> Definitions;
	LoadDefinitions(Definitions);

	int32 Count = 0;
	for (const UTerritoryCollectibleDefinition* Definition : Definitions)
	{
		if (Definition && Definition->bCountsTowardCompletion)
		{
			++Count;
		}
	}
	return Count;
}

#if WITH_EDITOR
EDataValidationResult UTerritoryCollectibleDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bValid = SuperResult != EDataValidationResult::Invalid;

	const FGameplayTag CollectibleRoot = TerritoryCollectibleTags::Collectible.GetTag();
	if (!CollectibleTag.IsValid())
	{
		Context.AddError(LOCTEXT("MissingCollectibleTag",
			"Collectible needs a CollectibleTag; the campaign records findings by tag."));
		bValid = false;
	}
	else if (CollectibleRoot.IsValid() && !CollectibleTag.MatchesTag(CollectibleRoot))
	{
		Context.AddError(FText::Format(LOCTEXT("WrongCollectibleFamily",
			"CollectibleTag '{0}' must belong to the Territory.Collectible.* family."),
			FText::FromString(CollectibleTag.ToString())));
		bValid = false;
	}

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("MissingCollectibleName",
			"No DisplayName: the Dossier grid will show the raw tag instead of a name."));
	}

	if (CategoryTag.IsValid())
	{
		const FGameplayTag CategoryRoot = TerritoryCollectibleTags::Category.GetTag();
		if (CategoryRoot.IsValid() && !CategoryTag.MatchesTag(CategoryRoot))
		{
			Context.AddWarning(FText::Format(LOCTEXT("ForeignCategoryTag",
				"CategoryTag '{0}' is outside Territory.CollectibleCategory.*; grouping will look arbitrary."),
				FText::FromString(CategoryTag.ToString())));
		}
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

EDataValidationResult UTerritoryCollectibleSet::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bValid = SuperResult != EDataValidationResult::Invalid;

	if (Collectibles.Num() == 0)
	{
		Context.AddWarning(LOCTEXT("EmptyCollectibleSet",
			"The set has no collectibles, so the Dossier can only ever report a total of zero."));
	}

	int32 NullEntries = 0;
	TMap<FGameplayTag, FString> TagsSeen;
	for (const TSoftObjectPtr<UTerritoryCollectibleDefinition>& Entry : Collectibles)
	{
		const UTerritoryCollectibleDefinition* Definition = Entry.Get();
		if (!Definition)
		{
			++NullEntries;
			continue;
		}

		const FGameplayTag Tag = Definition->CollectibleTag;
		if (!Tag.IsValid())
		{
			// The definition asset reports this itself; do not double-report it here.
			continue;
		}

		if (const FString* Existing = TagsSeen.Find(Tag))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateCollectibleTag",
				"Two entries share the tag '{0}' ('{1}' and '{2}'). Findings are keyed by tag, so one of them could never be found."),
				FText::FromString(Tag.ToString()),
				FText::FromString(*Existing),
				FText::FromString(Definition->GetName())));
			bValid = false;
		}
		else
		{
			TagsSeen.Add(Tag, Definition->GetName());
		}
	}

	if (NullEntries > 0)
	{
		Context.AddWarning(FText::Format(LOCTEXT("UnresolvedCollectibleEntries",
			"{0} entry(ies) in this set do not resolve to a definition and are ignored."),
			FText::AsNumber(NullEntries)));
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

#undef LOCTEXT_NAMESPACE

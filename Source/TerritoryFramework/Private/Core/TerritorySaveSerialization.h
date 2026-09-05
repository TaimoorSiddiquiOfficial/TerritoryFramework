#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

/** Adapts Narrative's tagged SaveGame archive for reload into existing plugin objects. */
class FTerritorySaveSerializationScope final
{
public:
	FTerritorySaveSerializationScope(UObject& Object, FArchive& InArchive)
		: Archive(InArchive), bSaveGame(InArchive.IsSaveGame()), bPreviousNoDelta(InArchive.ArNoDelta)
	{
		if (!bSaveGame) return;
		if (Archive.IsLoading())
		{
			// Match UObject::SerializeScriptProperties' delta baseline. Legacy saves
			// may omit zero/empty/default fields; replace live values before reading
			// those deltas, including nested structs and Blueprint SaveGame fields.
			UObject* Defaults = Archive.GetArchetypeFromLoader(&Object);
			if (!Defaults) Defaults = Object.GetArchetype();
			if (Defaults && Defaults != &Object)
			{
				for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
				{
					FProperty* Property = *It;
					if (Property->HasAnyPropertyFlags(CPF_SaveGame)
						&& Property->ShouldSerializeValue(Archive)
						&& Defaults->IsA(Property->GetOwnerClass()))
					{
						Property->CopyCompleteValue_InContainer(&Object, Defaults);
					}
				}
			}
		}
		// New records store complete values in the same compatible tagged format.
		Archive.ArNoDelta = true;
	}

	~FTerritorySaveSerializationScope()
	{
		if (bSaveGame) Archive.ArNoDelta = bPreviousNoDelta;
	}

	FTerritorySaveSerializationScope(const FTerritorySaveSerializationScope&) = delete;
	FTerritorySaveSerializationScope& operator=(const FTerritorySaveSerializationScope&) = delete;

private:
	FArchive& Archive;
	const bool bSaveGame;
	const bool bPreviousNoDelta;
};

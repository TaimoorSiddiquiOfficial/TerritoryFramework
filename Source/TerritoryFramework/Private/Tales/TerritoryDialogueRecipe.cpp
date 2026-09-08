#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/TerritorySituationCondition.h"
#include "Misc/DataValidation.h"

bool UTerritoryDialogueRecipe::ValidateRecipe(FText& OutError) const
{
	const auto Fail = [&](const TCHAR* Message)
	{
		OutError = FText::FromString(Message);
		return false;
	};
	OutError = FText::GetEmpty();
	if (RootID.IsNone() || Nodes.IsEmpty()) return Fail(TEXT("Choose a root and at least one dialogue node."));
	TMap<FName, const FTerritoryDialogueRecipeNode*> ByID;
	TSet<FName> SpeakerIDs;
	for (const UNPCDefinition* Speaker : Speakers)
	{
		if (!Speaker || Speaker->NPCID.IsNone() || SpeakerIDs.Contains(Speaker->NPCID))
			return Fail(TEXT("Speakers require distinct Narrative NPC IDs."));
		SpeakerIDs.Add(Speaker->NPCID);
	}
	for (const auto& Node : Nodes)
	{
		if (Node.ID.IsNone() || ByID.Contains(Node.ID)) return Fail(TEXT("Dialogue node IDs must be nonempty and unique."));
		if (!FMath::IsFinite(Node.Position.X) || !FMath::IsFinite(Node.Position.Y)) return Fail(TEXT("Node positions must be finite."));
#if WITH_EDITOR
		// Native editor graph coordinates are int32. Validate before asset creation
		// so a finite scripted double cannot overflow the builder's conversion.
		if (Node.Position.X < MIN_int32 || Node.Position.X > MAX_int32
			|| Node.Position.Y < MIN_int32 || Node.Position.Y > MAX_int32)
			return Fail(TEXT("Node positions must fit the Narrative editor graph's signed 32-bit coordinate range."));
#endif
		if (!Node.bPlayer && !Node.Text.IsEmpty() && !SpeakerIDs.Contains(Node.SpeakerID))
			return Fail(TEXT("Each spoken NPC line must identify a configured speaker."));
		ByID.Add(Node.ID, &Node);
		for (const UNarrativeCondition* Condition : Node.Conditions)
		{
			if (!Condition) return Fail(TEXT("Remove empty condition entries; Native ignores them at runtime."));
			if (const auto* Situation = Cast<UTerritorySituationCondition>(Condition))
			{
				if (!Situation->Profile || !Situation->Profile->Territory.IsValid()
					|| !FMath::IsFinite(Situation->Value) || Situation->Value < 0.f)
					return Fail(TEXT("Situation conditions require a valid profile and finite nonnegative threshold."));
			}
		}
		for (const UNarrativeEvent* Event : Node.Events)
		{
			if (!Event) return Fail(TEXT("Remove empty event entries."));
		}
	}
	if (!ByID.Contains(RootID) || ByID[RootID]->bPlayer) return Fail(TEXT("The root must identify an NPC node."));
	for (const auto& Node : Nodes)
	{
		TSet<FName> SeenReplies;
		for (FName Reply : Node.Replies)
		{
			if (Reply == RootID) return Fail(TEXT("The Native root is an entry point and cannot receive replies."));
			if (!ByID.Contains(Reply) || SeenReplies.Contains(Reply)) return Fail(TEXT("Replies must reference distinct existing node IDs."));
			if (Node.bPlayer && ByID[Reply]->bPlayer) return Fail(TEXT("Native player replies must lead to an NPC line."));
			SeenReplies.Add(Reply);
		}
	}
	TSet<FName> Visited;
	TArray<FName> Pending = {RootID};
	while (!Pending.IsEmpty())
	{
		const FName Next = Pending.Pop(EAllowShrinking::No);
		if (Visited.Contains(Next)) continue;
		Visited.Add(Next);
		Pending.Append(ByID[Next]->Replies);
	}
	if (Visited.Num() != Nodes.Num()) return Fail(TEXT("Every authored node must be reachable from the root."));
	// Native constructs a complete NPC chunk before a player makes another choice.
	// Cycles must cross a player choice; an NPC-only cycle has no finite chunk.
	TSet<FName> Complete, Active;
	TFunction<bool(FName)> CheckChunk = [&](FName ID)
	{
		if (ByID[ID]->bPlayer || Complete.Contains(ID)) return true;
		if (Active.Contains(ID)) return false;
		Active.Add(ID);
		for (FName Reply : ByID[ID]->Replies) if (!CheckChunk(Reply)) return false;
		Active.Remove(ID);
		Complete.Add(ID);
		return true;
	};
	for (const auto& Node : Nodes) if (!CheckChunk(Node.ID))
		return Fail(TEXT("Dialogue cycles must cross a player choice; NPC-only cycles cannot form a finite Native dialogue chunk."));
	return true;
}

#if WITH_EDITOR
EDataValidationResult UTerritoryDialogueRecipe::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	FText Error;
	if (!ValidateRecipe(Error)) Context.AddError(Error);
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

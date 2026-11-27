#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/TerritorySituationCondition.h"
#include "Tales/TerritoryAmbientDialogue.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * Narrative 2.5 derives FSpeakerInfo::GetSpeakerID() from UNPCDefinition::CharacterID;
	 * 2.4.2 read NPCID, which 2.5 still carries but marks deprecated (NPCDefinition.h:26-28).
	 * Recipe validation, Blueprint authoring and the compiled-defaults verification all have
	 * to agree on one identity, otherwise a definition authored the 2.5 way (CharacterID only)
	 * is rejected here and a legacy one never matches the speaker Narrative compiles.
	 */
	FName TerritorySpeakerIdentity(const UNPCDefinition& Definition)
	{
		return Definition.CharacterID.IsNone() ? Definition.NPCID : Definition.CharacterID;
	}
}

UTerritoryDialogueRecipe::UTerritoryDialogueRecipe()
{
	ParentDialogueClass = UDialogue::StaticClass();
}

bool UTerritoryDialogueRecipe::UsesAmbientRoles() const
{
	return ParentDialogueClass && ParentDialogueClass->IsChildOf(UTerritoryAmbientDialogue::StaticClass());
}

bool UTerritoryDialogueRecipe::ValidateRecipe(FText& OutError) const
{
	const auto Fail = [&](const TCHAR* Message)
	{
		OutError = FText::FromString(Message);
		return false;
	};
	OutError = FText::GetEmpty();
	if (!ParentDialogueClass || ParentDialogueClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		return Fail(TEXT("Choose a concrete, current Narrative dialogue parent class."));
	if (RootID.IsNone() || Nodes.IsEmpty()) return Fail(TEXT("Choose a root and at least one dialogue node."));
	const bool bAmbient = UsesAmbientRoles();
	TMap<FName, const FTerritoryDialogueRecipeNode*> ByID;
	TSet<FName> SpeakerIDs;
	TSet<FName> SpokenRoles;
	if (bAmbient)
	{
		if (Nodes.Num() > 1024) return Fail(TEXT("Ambient meetings support at most 1024 dialogue nodes."));
		if (!Speakers.IsEmpty()) return Fail(TEXT("Ambient meetings bind two recruited actors by role; remove NPC Definition speakers."));
		const FName PlayerID = ParentDialogueClass.GetDefaultObject()->PlayerSpeakerInfo.GetSpeakerID();
		if (FirstRoleID.IsNone() || SecondRoleID.IsNone() || FirstRoleID == SecondRoleID
			|| FirstRoleID == PlayerID || SecondRoleID == PlayerID)
			return Fail(TEXT("Ambient meetings require two distinct, nonempty role IDs that do not identify the player."));
		SpeakerIDs.Add(FirstRoleID);
		SpeakerIDs.Add(SecondRoleID);
	}
	for (const UNPCDefinition* Speaker : Speakers)
	{
		const FName SpeakerIdentity = Speaker ? TerritorySpeakerIdentity(*Speaker) : NAME_None;
		if (!Speaker || SpeakerIdentity.IsNone() || SpeakerIDs.Contains(SpeakerIdentity))
			return Fail(TEXT("Speakers require distinct Narrative NPC IDs."));
		// The generated Blueprint belongs to a different package. Native compilation
		// clears private external references, so an inline/transient definition cannot bind a speaker.
		if (!Speaker->IsAsset() || Speaker->GetOutermost() == GetTransientPackage()
			|| Speaker->GetOutermost()->HasAnyFlags(RF_Transient))
			return Fail(TEXT("Speakers must reference public Narrative NPC Definition assets, not private or transient objects."));
		SpeakerIDs.Add(SpeakerIdentity);
	}
	for (const auto& Node : Nodes)
	{
		if (bAmbient && Node.bPlayer) return Fail(TEXT("Ambient meeting dialogues cannot contain player replies."));
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
		if (bAmbient && !Node.Text.IsEmpty()) SpokenRoles.Add(Node.SpeakerID);
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
	if (bAmbient && SpokenRoles.Num() != 2)
		return Fail(TEXT("Both ambient meeting roles must have a spoken line."));
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
bool UTerritoryDialogueRecipe::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty)) return false;
	if (!InProperty) return true;
	const FName Name = InProperty->GetFName();
	// Preserve stale ordinary speakers when switching parent, but let the author remove them.
	if (Name == GET_MEMBER_NAME_CHECKED(UTerritoryDialogueRecipe, Speakers)) return !UsesAmbientRoles() || !Speakers.IsEmpty();
	if (Name == GET_MEMBER_NAME_CHECKED(UTerritoryDialogueRecipe, FirstRoleID)
		|| Name == GET_MEMBER_NAME_CHECKED(UTerritoryDialogueRecipe, SecondRoleID)) return UsesAmbientRoles();
	return true;
}

EDataValidationResult UTerritoryDialogueRecipe::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	FText Error;
	if (!ValidateRecipe(Error)) Context.AddError(Error);
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

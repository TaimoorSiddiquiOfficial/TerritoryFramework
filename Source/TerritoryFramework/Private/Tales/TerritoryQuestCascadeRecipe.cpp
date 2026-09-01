#include "Tales/TerritoryQuestCascadeRecipe.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "TerritoryQuestCascadeRecipe"

namespace TerritoryQuestCascadeRecipe
{
	FText StateTypeText(const ETerritoryQuestCascadeStateType Type)
	{
		switch (Type)
		{
		case ETerritoryQuestCascadeStateType::Objective:
			return LOCTEXT("ObjectiveType", "Objective");
		case ETerritoryQuestCascadeStateType::Success:
			return LOCTEXT("SuccessType", "Success");
		case ETerritoryQuestCascadeStateType::Failure:
			return LOCTEXT("FailureType", "Failure");
		default:
			return LOCTEXT("UnknownType", "Unknown");
		}
	}
}

FTerritoryQuestCascadeValidation
UTerritoryQuestCascadeRecipe::ValidateRecipe() const
{
	FTerritoryQuestCascadeValidation Result;
	auto Error = [&Result](const FText& Message)
	{
		Result.Errors.Add(Message);
	};
	auto Warning = [&Result](const FText& Message)
	{
		Result.Warnings.Add(Message);
	};

	if (QuestName.IsEmpty())
	{
		Warning(LOCTEXT("MissingQuestName",
			"Quest Name is empty; the generated Narrative journal entry will have no friendly title."));
	}
	if (States.IsEmpty())
	{
		Error(LOCTEXT("NoStates",
			"Add at least one Objective state and one Success state."));
		Result.bValid = false;
		return Result;
	}
	if (StartStateID.IsNone())
	{
		Error(LOCTEXT("NoStartState",
			"Start State ID is empty. Choose the Objective state where the quest begins."));
	}

	TMap<FName, const FTerritoryQuestCascadeState*> StatesByID;
	TSet<FName> AllNodeIDs;
	int32 SuccessStateCount = 0;
	for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
	{
		const FTerritoryQuestCascadeState& State = States[StateIndex];
		if (State.StateID.IsNone())
		{
			Error(FText::Format(LOCTEXT("StateMissingID",
				"State row {0} has no State ID."), FText::AsNumber(StateIndex)));
			continue;
		}
		if (StatesByID.Contains(State.StateID))
		{
			Error(FText::Format(LOCTEXT("DuplicateStateID",
				"State ID '{0}' is used more than once."),
				FText::FromName(State.StateID)));
		}
		else
		{
			StatesByID.Add(State.StateID, &State);
		}
		if (AllNodeIDs.Contains(State.StateID))
		{
			Error(FText::Format(LOCTEXT("DuplicateNodeIDState",
				"Narrative node ID '{0}' must be unique across both states and branches."),
				FText::FromName(State.StateID)));
		}
		AllNodeIDs.Add(State.StateID);

		if (State.Type == ETerritoryQuestCascadeStateType::Success)
		{
			++SuccessStateCount;
		}
		if (State.Type != ETerritoryQuestCascadeStateType::Objective
			&& !State.Branches.IsEmpty())
		{
			Error(FText::Format(LOCTEXT("TerminalHasBranches",
				"Terminal state '{0}' cannot have outgoing branches."),
				FText::FromName(State.StateID)));
		}
		if (State.Type == ETerritoryQuestCascadeStateType::Objective
			&& State.Branches.IsEmpty())
		{
			Warning(FText::Format(LOCTEXT("ObjectiveNoBranches",
				"Objective state '{0}' has no route forward, so the quest will stop there."),
				FText::FromName(State.StateID)));
		}

		for (int32 BranchIndex = 0;
			BranchIndex < State.Branches.Num(); ++BranchIndex)
		{
			const FTerritoryQuestCascadeBranch& Branch =
				State.Branches[BranchIndex];
			if (Branch.BranchID.IsNone())
			{
				Error(FText::Format(LOCTEXT("BranchMissingID",
					"Branch row {0} on state '{1}' has no Branch ID."),
					FText::AsNumber(BranchIndex), FText::FromName(State.StateID)));
			}
			else if (AllNodeIDs.Contains(Branch.BranchID))
			{
				Error(FText::Format(LOCTEXT("DuplicateNodeIDBranch",
					"Narrative node ID '{0}' must be unique across both states and branches."),
					FText::FromName(Branch.BranchID)));
			}
			AllNodeIDs.Add(Branch.BranchID);

			if (Branch.DestinationStateID.IsNone())
			{
				Error(FText::Format(LOCTEXT("BranchNoDestination",
					"Branch '{0}' has no Destination State ID."),
					FText::FromName(Branch.BranchID)));
			}
			if (Branch.Tasks.IsEmpty())
			{
				Error(FText::Format(LOCTEXT("BranchNoTasks",
					"Branch '{0}' needs at least one Narrative Task. Empty Narrative branches complete immediately and are unsafe."),
					FText::FromName(Branch.BranchID)));
			}
			for (int32 TaskIndex = 0; TaskIndex < Branch.Tasks.Num(); ++TaskIndex)
			{
				const UNarrativeTask* Task = Branch.Tasks[TaskIndex];
				if (!IsValid(Task))
				{
					Error(FText::Format(LOCTEXT("NullTask",
						"Task row {0} on branch '{1}' is empty."),
						FText::AsNumber(TaskIndex), FText::FromName(Branch.BranchID)));
				}
				else if (Task->RequiredQuantity < 1)
				{
					Error(FText::Format(LOCTEXT("InvalidTaskQuantity",
						"Task row {0} on branch '{1}' has Required Quantity below 1."),
						FText::AsNumber(TaskIndex), FText::FromName(Branch.BranchID)));
				}
			}
		}
	}

	const FTerritoryQuestCascadeState* const* StartState =
		StatesByID.Find(StartStateID);
	if (!StartState)
	{
		Error(FText::Format(LOCTEXT("StartStateMissing",
			"Start State ID '{0}' does not match a state row."),
			FText::FromName(StartStateID)));
	}
	else if ((*StartState)->Type !=
		ETerritoryQuestCascadeStateType::Objective)
	{
		Error(LOCTEXT("StartStateTerminal",
			"Start State ID must name an Objective state, not a Success or Failure ending."));
	}
	if (SuccessStateCount == 0)
	{
		Error(LOCTEXT("NoSuccessState",
			"Add at least one Success state so the generated Narrative Quest can complete."));
	}

	for (const FTerritoryQuestCascadeState& State : States)
	{
		for (const FTerritoryQuestCascadeBranch& Branch : State.Branches)
		{
			if (!Branch.DestinationStateID.IsNone()
				&& !StatesByID.Contains(Branch.DestinationStateID))
			{
				Error(FText::Format(LOCTEXT("DestinationMissing",
					"Branch '{0}' points to missing state '{1}'."),
					FText::FromName(Branch.BranchID),
					FText::FromName(Branch.DestinationStateID)));
			}
		}
	}

	if (StartState)
	{
		TSet<FName> Reachable;
		TArray<FName> Pending = { StartStateID };
		while (!Pending.IsEmpty())
		{
			const FName CurrentID = Pending.Pop(EAllowShrinking::No);
			if (Reachable.Contains(CurrentID)) continue;
			Reachable.Add(CurrentID);
			if (const FTerritoryQuestCascadeState* const* Current =
				StatesByID.Find(CurrentID))
			{
				for (const FTerritoryQuestCascadeBranch& Branch :
					(*Current)->Branches)
				{
					if (StatesByID.Contains(Branch.DestinationStateID))
					{
						Pending.Add(Branch.DestinationStateID);
					}
				}
			}
		}
		for (const FTerritoryQuestCascadeState& State : States)
		{
			if (!State.StateID.IsNone() && !Reachable.Contains(State.StateID))
			{
				Warning(FText::Format(LOCTEXT("UnreachableState",
					"State '{0}' cannot be reached from Start State '{1}'."),
					FText::FromName(State.StateID), FText::FromName(StartStateID)));
			}
		}
	}

	Result.bValid = Result.Errors.IsEmpty();
	return Result;
}

FString UTerritoryQuestCascadeRecipe::BuildPlainTextPreview() const
{
	FString Preview = FString::Printf(
		TEXT("NARRATIVE QUEST CASCADE (AUTHORING RECIPE)\n%s\n%s\nStart: %s\n\n"),
		*QuestName.ToString(), *QuestDescription.ToString(),
		*StartStateID.ToString());
	for (const FTerritoryQuestCascadeState& State : States)
	{
		Preview += FString::Printf(TEXT("STATE %s [%s]\n%s\n"),
			*State.StateID.ToString(),
			*TerritoryQuestCascadeRecipe::StateTypeText(State.Type).ToString(),
			*State.Description.ToString());
		for (const FTerritoryQuestCascadeBranch& Branch : State.Branches)
		{
			Preview += FString::Printf(TEXT("  -> %s -> %s (%d task%s; ALL required)%s\n"),
				*Branch.BranchID.ToString(),
				*Branch.DestinationStateID.ToString(), Branch.Tasks.Num(),
				Branch.Tasks.Num() == 1 ? TEXT("") : TEXT("s"),
				Branch.bHidden ? TEXT(" [hidden]") : TEXT(""));
			for (const UNarrativeTask* Task : Branch.Tasks)
			{
				Preview += FString::Printf(TEXT("     - %s: %s\n"),
					*GetNameSafe(Task), Task
						? *Task->GetTaskDescription().ToString()
						: TEXT("EMPTY TASK"));
			}
		}
		Preview += TEXT("\n");
	}

	const FTerritoryQuestCascadeValidation Validation = ValidateRecipe();
	Preview += Validation.bValid
		? TEXT("VALID: Ready to generate a Narrative Quest.\n")
		: TEXT("INVALID: Fix the errors below before generation.\n");
	for (const FText& Error : Validation.Errors)
	{
		Preview += TEXT("Error: ") + Error.ToString() + TEXT("\n");
	}
	for (const FText& Warning : Validation.Warnings)
	{
		Preview += TEXT("Warning: ") + Warning.ToString() + TEXT("\n");
	}
	return Preview;
}

#if WITH_EDITOR
EDataValidationResult UTerritoryQuestCascadeRecipe::IsDataValid(
	FDataValidationContext& Context) const
{
	const FTerritoryQuestCascadeValidation Validation = ValidateRecipe();
	for (const FText& Error : Validation.Errors)
	{
		Context.AddError(Error);
	}
	for (const FText& Warning : Validation.Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Validation.bValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif

#undef LOCTEXT_NAMESPACE

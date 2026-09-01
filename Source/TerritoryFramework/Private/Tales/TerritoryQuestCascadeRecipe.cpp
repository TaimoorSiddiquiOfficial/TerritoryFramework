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

	void ValidateConditions(
		const TArray<TObjectPtr<UNarrativeCondition>>& Conditions,
		const FString& OwnerLabel,
		const TFunctionRef<void(const FText&)>& Error)
	{
		for (int32 Index = 0; Index < Conditions.Num(); ++Index)
		{
			if (!IsValid(Conditions[Index]))
			{
				Error(FText::Format(LOCTEXT("EmptyCondition",
					"Condition row {0} on {1} is empty."),
					FText::AsNumber(Index), FText::FromString(OwnerLabel)));
			}
		}
	}

	void ValidateEvents(
		const TArray<TObjectPtr<UNarrativeEvent>>& Events,
		const FString& OwnerLabel,
		const TFunctionRef<void(const FText&)>& Error)
	{
		for (int32 EventIndex = 0; EventIndex < Events.Num(); ++EventIndex)
		{
			const UNarrativeEvent* Event = Events[EventIndex];
			if (!IsValid(Event))
			{
				Error(FText::Format(LOCTEXT("EmptyEvent",
					"Event row {0} on {1} is empty."),
					FText::AsNumber(EventIndex), FText::FromString(OwnerLabel)));
				continue;
			}
			for (int32 ConditionIndex = 0;
				ConditionIndex < Event->Conditions.Num(); ++ConditionIndex)
			{
				if (!IsValid(Event->Conditions[ConditionIndex]))
				{
					Error(FText::Format(LOCTEXT("EmptyEventCondition",
						"Condition row {0} on event '{1}' on {2} is empty."),
						FText::AsNumber(ConditionIndex),
						FText::FromString(GetNameSafe(Event)),
						FText::FromString(OwnerLabel)));
				}
			}
		}
	}

	FString ConditionLabel(const UNarrativeCondition* Condition)
	{
		if (!Condition) return TEXT("EMPTY CONDITION");
		const FString Display = const_cast<UNarrativeCondition*>(Condition)
			->GetGraphDisplayText();
		return Display.IsEmpty() ? GetNameSafe(Condition) : Display;
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
	if (!QuestDialogue && bResumeDialogueAfterLoad)
	{
		Warning(LOCTEXT("ResumeWithoutDialogue",
			"Resume Dialogue After Load is enabled, but no Quest Dialogue is assigned. Narrative has no linked conversation to resume."));
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
		if (State.Type != ETerritoryQuestCascadeStateType::Objective
			&& !State.Conditions.IsEmpty())
		{
			Warning(FText::Format(LOCTEXT("TerminalHasConditions",
				"Terminal state '{0}' has departure conditions, but an ending has no outgoing route to gate. Move them to the branch that enters this ending."),
				FText::FromName(State.StateID)));
		}
		if (State.Type == ETerritoryQuestCascadeStateType::Objective
			&& State.Branches.IsEmpty())
		{
			Warning(FText::Format(LOCTEXT("ObjectiveNoBranches",
				"Objective state '{0}' has no route forward, so the quest will stop there."),
				FText::FromName(State.StateID)));
		}
		TerritoryQuestCascadeRecipe::ValidateConditions(State.Conditions,
			FString::Printf(TEXT("state '%s'"), *State.StateID.ToString()), Error);
		TerritoryQuestCascadeRecipe::ValidateEvents(State.Events,
			FString::Printf(TEXT("state '%s'"), *State.StateID.ToString()), Error);

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
			const bool bHasAnyGateCondition =
				!State.Conditions.IsEmpty() || !Branch.Conditions.IsEmpty();
			if (Branch.Tasks.IsEmpty() && !bHasAnyGateCondition)
			{
				Error(FText::Format(LOCTEXT("BranchNoTasks",
					"Branch '{0}' needs at least one Narrative Task or Condition. A completely empty Narrative branch completes immediately and is unsafe."),
					FText::FromName(Branch.BranchID)));
			}
			TerritoryQuestCascadeRecipe::ValidateConditions(Branch.Conditions,
				FString::Printf(TEXT("branch '%s'"), *Branch.BranchID.ToString()), Error);
			TerritoryQuestCascadeRecipe::ValidateEvents(Branch.Events,
				FString::Printf(TEXT("branch '%s'"), *Branch.BranchID.ToString()), Error);
			bool bAllAuthoredTasksOptional = !Branch.Tasks.IsEmpty();
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
				if (Task)
				{
					bAllAuthoredTasksOptional &= Task->bOptional;
					if (Task->MarkerSettings.bAddNavigationMarker
						&& !Task->MarkerSettings.MarkerClass)
					{
						Warning(FText::Format(LOCTEXT("MarkerWithoutClass",
							"Task row {0} on branch '{1}' enables a navigation marker but has no Marker Class."),
							FText::AsNumber(TaskIndex),
							FText::FromName(Branch.BranchID)));
					}
				}
			}
			if (bAllAuthoredTasksOptional && !bHasAnyGateCondition)
			{
				Error(FText::Format(LOCTEXT("OnlyOptionalTasks",
					"Branch '{0}' contains only Optional tasks and no conditions, so Narrative will complete it immediately. Add one required task or a condition."),
					FText::FromName(Branch.BranchID)));
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

FTerritoryQuestCascadeLogicSummary
UTerritoryQuestCascadeRecipe::BuildMissionLogicSummary() const
{
	FTerritoryQuestCascadeLogicSummary Summary;
	const FTerritoryQuestCascadeValidation Validation = ValidateRecipe();
	Summary.bValid = Validation.bValid;
	Summary.Errors = Validation.Errors;
	Summary.Warnings = Validation.Warnings;

	for (const FTerritoryQuestCascadeState& State : States)
	{
		switch (State.Type)
		{
		case ETerritoryQuestCascadeStateType::Objective:
			++Summary.ObjectiveStates;
			break;
		case ETerritoryQuestCascadeStateType::Success:
			++Summary.SuccessEndings;
			break;
		case ETerritoryQuestCascadeStateType::Failure:
			++Summary.FailureEndings;
			break;
		default:
			break;
		}
		Summary.Conditions += State.Conditions.Num();
		Summary.Events += State.Events.Num();
		for (const FTerritoryQuestCascadeBranch& Branch : State.Branches)
		{
			++Summary.Routes;
			Summary.PlayerTasks += Branch.Tasks.Num();
			Summary.Conditions += Branch.Conditions.Num();
			Summary.Events += Branch.Events.Num();
			for (const UNarrativeEvent* Event : Branch.Events)
			{
				Summary.Conditions += Event ? Event->Conditions.Num() : 0;
			}
			for (const UNarrativeTask* Task : Branch.Tasks)
			{
				if (!Task) continue;
				Summary.OptionalTasks += Task->bOptional ? 1 : 0;
				Summary.HiddenTasks += Task->bHidden ? 1 : 0;
				Summary.NavigationMarkerTasks +=
					Task->MarkerSettings.bAddNavigationMarker ? 1 : 0;
			}
			Summary.FlowLines.Add(FString::Printf(
				TEXT("%s -> %s -> %s | %d task(s), %d route condition(s)%s"),
				*State.StateID.ToString(), *Branch.BranchID.ToString(),
				*Branch.DestinationStateID.ToString(), Branch.Tasks.Num(),
				State.Conditions.Num() + Branch.Conditions.Num(),
				Branch.bHidden ? TEXT(" | hidden route") : TEXT("")));
		}
		for (const UNarrativeEvent* Event : State.Events)
		{
			Summary.Conditions += Event ? Event->Conditions.Num() : 0;
		}
	}

	Summary.Headline = FString::Printf(
		TEXT("%d objective state(s) • %d route(s) • %d player task(s) • %d condition(s) • %d event(s) • %d success / %d failure ending(s)"),
		Summary.ObjectiveStates, Summary.Routes, Summary.PlayerTasks,
		Summary.Conditions, Summary.Events, Summary.SuccessEndings,
		Summary.FailureEndings);
	return Summary;
}

FString UTerritoryQuestCascadeRecipe::BuildPlainTextPreview() const
{
	const FTerritoryQuestCascadeLogicSummary Summary = BuildMissionLogicSummary();
	FString Preview = FString::Printf(
		TEXT("NARRATIVE QUEST CASCADE (AUTHORING RECIPE)\n%s\n%s\nStart: %s\nTracked: %s\nQuest Dialogue: %s\nResume Dialogue After Load: %s\n%s\n\n"),
		*QuestName.ToString(), *QuestDescription.ToString(),
		*StartStateID.ToString(), bTracked ? TEXT("Yes") : TEXT("No"),
		QuestDialogue ? *GetNameSafe(QuestDialogue.Get()) : TEXT("None"),
		bResumeDialogueAfterLoad ? TEXT("Yes") : TEXT("No"),
		*Summary.Headline);
	for (const FTerritoryQuestCascadeState& State : States)
	{
		Preview += FString::Printf(TEXT("STATE %s [%s] | %d shared condition(s) | %d event(s)\n%s\n"),
			*State.StateID.ToString(),
			*TerritoryQuestCascadeRecipe::StateTypeText(State.Type).ToString(),
			State.Conditions.Num(), State.Events.Num(),
			*State.Description.ToString());
		for (const UNarrativeCondition* Condition : State.Conditions)
		{
			Preview += FString::Printf(TEXT("     REQUIRE EVERY ROUTE: %s\n"),
				*TerritoryQuestCascadeRecipe::ConditionLabel(Condition));
		}
		for (const FTerritoryQuestCascadeBranch& Branch : State.Branches)
		{
			Preview += FString::Printf(TEXT("  -> %s -> %s (%d task%s; ALL non-optional tasks required; %d route condition%s)%s\n"),
				*Branch.BranchID.ToString(),
				*Branch.DestinationStateID.ToString(), Branch.Tasks.Num(),
				Branch.Tasks.Num() == 1 ? TEXT("") : TEXT("s"),
				Branch.Conditions.Num(), Branch.Conditions.Num() == 1 ? TEXT("") : TEXT("s"),
				Branch.bHidden ? TEXT(" [hidden]") : TEXT(""));
			for (const UNarrativeCondition* Condition : Branch.Conditions)
			{
				Preview += FString::Printf(TEXT("     REQUIRE THIS ROUTE: %s\n"),
					*TerritoryQuestCascadeRecipe::ConditionLabel(Condition));
			}
			for (const UNarrativeTask* Task : Branch.Tasks)
			{
				Preview += FString::Printf(TEXT("     - %s: %s | quantity %d%s%s%s\n"),
					*GetNameSafe(Task), Task
						? *Task->GetTaskDescription().ToString()
						: TEXT("EMPTY TASK"), Task ? Task->RequiredQuantity : 0,
					Task && Task->bOptional ? TEXT(" | optional") : TEXT(""),
					Task && Task->bHidden ? TEXT(" | hidden") : TEXT(""),
					Task && Task->MarkerSettings.bAddNavigationMarker
						? TEXT(" | navigation marker") : TEXT(""));
			}
			Preview += FString::Printf(TEXT("     EVENTS: %d\n"), Branch.Events.Num());
		}
		Preview += TEXT("\n");
	}

	Preview += Summary.bValid
		? TEXT("VALID: Ready to generate a Narrative Quest.\n")
		: TEXT("INVALID: Fix the errors below before generation.\n");
	for (const FText& Error : Summary.Errors)
	{
		Preview += TEXT("Error: ") + Error.ToString() + TEXT("\n");
	}
	for (const FText& Warning : Summary.Warnings)
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

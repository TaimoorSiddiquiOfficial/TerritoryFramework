#include "Tales/TerritoryQuestCascadeRecipe.h"

#include "Misc/DataValidation.h"
#include "Tales/Quest.h"
#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/QuestSM.h"
#include "Tales/TerritoryNarrativeCheckpointEvent.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "UObject/UnrealType.h"

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

	FString EventLabel(const UNarrativeEvent* Event)
	{
		if (!Event) return TEXT("EMPTY EVENT");
		const FString Display = const_cast<UNarrativeEvent*>(Event)
			->GetGraphDisplayText();
		return Display.IsEmpty() ? GetNameSafe(Event) : Display;
	}

	FString TaskLabel(const UNarrativeTask* Task)
	{
		if (!Task) return TEXT("EMPTY TASK");
		const FString Display = Task->GetTaskDescription().ToString();
		return Display.IsEmpty() ? GetNameSafe(Task) : Display;
	}

	FString RuntimeStateTypeText(const EStateNodeType Type)
	{
		switch (Type)
		{
		case EStateNodeType::Regular: return TEXT("OBJECTIVE");
		case EStateNodeType::Success: return TEXT("SUCCESS ENDING");
		case EStateNodeType::Failure: return TEXT("FAILURE ENDING");
		default: return TEXT("UNKNOWN");
		}
	}

	const UQuest* ResolveRuntimeQuestTemplate(
		const TSoftClassPtr<UQuest>& QuestReference, FString& OutFailure)
	{
		OutFailure.Reset();
		if (QuestReference.IsNull())
		{
			OutFailure = TEXT("No Narrative Quest Graph is selected.");
			return nullptr;
		}
		UClass* QuestClass = QuestReference.LoadSynchronous();
		if (!QuestClass || !QuestClass->IsChildOf(UQuest::StaticClass()))
		{
			OutFailure = FString::Printf(
				TEXT("The selected asset '%s' is not a loadable Narrative Quest class."),
				*QuestReference.ToSoftObjectPath().ToString());
			return nullptr;
		}
		if (const UQuestBlueprintGeneratedClass* GeneratedClass =
			Cast<UQuestBlueprintGeneratedClass>(QuestClass))
		{
			if (const UQuest* Template = GeneratedClass->GetQuestTemplate())
			{
				return Template;
			}
			OutFailure = FString::Printf(
				TEXT("Narrative Quest '%s' has no compiled Quest Template. Compile and save the Quest, then press Refresh Runtime Quest Summary."),
				*QuestClass->GetName());
			return nullptr;
		}
		const UQuest* Defaults = Cast<UQuest>(QuestClass->GetDefaultObject());
		if (!Defaults)
		{
			OutFailure = FString::Printf(
				TEXT("Narrative Quest class '%s' has no readable defaults."),
				*QuestClass->GetName());
		}
		return Defaults;
	}

	void AppendConfiguredProperties(const UObject* Object, FString& Report,
		const FString& Indent, TSet<const UObject*>& Visited)
	{
		if (!IsValid(Object) || Visited.Contains(Object)) return;
		Visited.Add(Object);
		static const TSet<FName> StructuralProperties = {
			TEXT("Conditions"), TEXT("Events"), TEXT("QuestTasks"),
			TEXT("Branches"), TEXT("DestinationState"), TEXT("Description"),
			TEXT("ID"), TEXT("NodePos")
		};
		for (TFieldIterator<FProperty> It(Object->GetClass(),
			EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Edit)
				|| Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				|| StructuralProperties.Contains(Property->GetFName()))
			{
				continue;
			}
			FString Value;
			Property->ExportText_InContainer(0, Value, Object, Object,
				const_cast<UObject*>(Object), PPF_None);
			const FString DisplayName = FName::NameToDisplayString(
				Property->GetName(), Property->IsA<FBoolProperty>());
			Report += FString::Printf(TEXT("%s%s: %s\n"), *Indent,
				*DisplayName, *Value);

			const FObjectPropertyBase* ObjectProperty =
				CastField<FObjectPropertyBase>(Property);
			if (ObjectProperty
				&& Property->HasAnyPropertyFlags(CPF_InstancedReference))
			{
				const UObject* Nested = ObjectProperty->GetObjectPropertyValue_InContainer(
					Object);
				if (Nested)
				{
					Report += FString::Printf(TEXT("%s  %s CONFIGURATION\n"),
						*Indent, *GetNameSafe(Nested->GetClass()));
					AppendConfiguredProperties(Nested, Report, Indent + TEXT("    "),
						Visited);
				}
			}
		}
	}

	FString CheckpointModeText(const ETerritoryQuestCheckpointMode Mode)
	{
		switch (Mode)
		{
		case ETerritoryQuestCheckpointMode::Disabled:
			return TEXT("Disabled");
		case ETerritoryQuestCheckpointMode::ObjectiveStates:
			return TEXT("Every Objective State");
		case ETerritoryQuestCheckpointMode::EveryState:
			return TEXT("Every State Including Endings");
		default:
			return TEXT("Unknown");
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
	if (QuestDescription.IsEmpty())
	{
		Warning(LOCTEXT("MissingQuestDescription",
			"Quest Description is empty; the generated Narrative journal entry will have no mission summary."));
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
	Summary.Source = GetPathName();
	Summary.InspectedQuestName = QuestName;
	Summary.InspectedQuestDescription = QuestDescription;
	Summary.InspectedStartState = StartStateID;
	Summary.bInspectedQuestTracked = bTracked;
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
		const bool bStateHasManualCheckpoint = State.Events.ContainsByPredicate(
			[](const TObjectPtr<UNarrativeEvent>& Event)
			{
				return Event
					&& Event->IsA<UTerritoryNarrativeCheckpointEvent>();
			});
		const bool bAutoCheckpointState = CheckpointMode ==
			ETerritoryQuestCheckpointMode::EveryState
			|| (CheckpointMode == ETerritoryQuestCheckpointMode::ObjectiveStates
				&& State.Type == ETerritoryQuestCascadeStateType::Objective);
		Summary.AutomaticCheckpoints +=
			bAutoCheckpointState && !bStateHasManualCheckpoint ? 1 : 0;
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
		TEXT("%d objective state(s) • %d route(s) • %d player task(s) • %d condition(s) • %d event(s) • %d auto checkpoint(s) • %d success / %d failure ending(s)"),
		Summary.ObjectiveStates, Summary.Routes, Summary.PlayerTasks,
		Summary.Conditions, Summary.Events, Summary.AutomaticCheckpoints,
		Summary.SuccessEndings,
		Summary.FailureEndings);
	return Summary;
}

FString UTerritoryQuestCascadeRecipe::BuildPlainTextPreview() const
{
	const FTerritoryQuestCascadeLogicSummary Summary = BuildMissionLogicSummary();
	FString Preview = FString::Printf(
		TEXT("NARRATIVE QUEST CASCADE (AUTHORING RECIPE)\n%s\n%s\nStart: %s\nTracked: %s\nQuest Dialogue: %s\nResume Dialogue After Load: %s\nAuto Checkpoint: %s\n%s\n\n"),
		*QuestName.ToString(), *QuestDescription.ToString(),
		*StartStateID.ToString(), bTracked ? TEXT("Yes") : TEXT("No"),
		QuestDialogue ? *GetNameSafe(QuestDialogue.Get()) : TEXT("None"),
		bResumeDialogueAfterLoad ? TEXT("Yes") : TEXT("No"),
		*TerritoryQuestCascadeRecipe::CheckpointModeText(CheckpointMode),
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

FTerritoryQuestCascadeLogicSummary
UTerritoryQuestCascadeRecipe::BuildSelectedRuntimeQuestSummary() const
{
	FTerritoryQuestCascadeLogicSummary Summary;
	Summary.Source = NarrativeQuestGraph.ToSoftObjectPath().ToString();
	FString Failure;
	const UQuest* Quest = TerritoryQuestCascadeRecipe::ResolveRuntimeQuestTemplate(
		NarrativeQuestGraph, Failure);
	if (!Quest)
	{
		Summary.Errors.Add(FText::FromString(Failure));
		Summary.Headline = TEXT("Runtime Quest graph is unavailable");
		return Summary;
	}

	Summary.InspectedQuestName = Quest->GetQuestName();
	Summary.InspectedQuestDescription = Quest->GetQuestDescription();
	Summary.bInspectedQuestTracked = Quest->IsTracked();
	const UQuestState* StartState = Quest->GetQuestStartState();
	Summary.InspectedStartState = StartState ? StartState->GetID() : NAME_None;
	if (!StartState)
	{
		Summary.Errors.Add(LOCTEXT("RuntimeQuestNoStart",
			"The selected compiled Narrative Quest has no Start State."));
	}
	if (Quest->GetStates().IsEmpty())
	{
		Summary.Errors.Add(LOCTEXT("RuntimeQuestNoStates",
			"The selected compiled Narrative Quest has no states."));
	}
	if (Quest->GetBranches().IsEmpty())
	{
		Summary.Errors.Add(LOCTEXT("RuntimeQuestNoBranches",
			"The selected compiled Narrative Quest has no branches. Narrative cannot initialize a Quest without at least one route."));
	}

	TSet<FName> NodeIDs;
	TSet<const UQuestBranch*> ReachedBranches;
	TSet<const UQuestState*> ReachableStates;
	TArray<const UQuestState*> PendingStates;
	if (StartState) PendingStates.Add(StartState);
	while (!PendingStates.IsEmpty())
	{
		const UQuestState* State = PendingStates.Pop(EAllowShrinking::No);
		if (!State || ReachableStates.Contains(State)) continue;
		ReachableStates.Add(State);
		for (const UQuestBranch* Branch : State->Branches)
		{
			if (Branch && Branch->DestinationState)
			{
				PendingStates.Add(Branch->DestinationState);
			}
		}
	}

	for (const UQuestState* State : Quest->GetStates())
	{
		if (!State)
		{
			Summary.Errors.Add(LOCTEXT("RuntimeEmptyState",
				"The compiled Narrative Quest contains an empty State reference."));
			continue;
		}
		if (State->GetID().IsNone())
		{
			Summary.Errors.Add(LOCTEXT("RuntimeStateNoID",
				"A compiled Narrative Quest State has no stable ID."));
		}
		else if (NodeIDs.Contains(State->GetID()))
		{
			Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeDuplicateStateID",
				"Compiled Narrative node ID '{0}' is duplicated."),
				FText::FromName(State->GetID())));
		}
		NodeIDs.Add(State->GetID());
		if (!ReachableStates.Contains(State))
		{
			Summary.Warnings.Add(FText::Format(LOCTEXT("RuntimeUnreachableState",
				"Compiled State '{0}' is unreachable from Start State '{1}'."),
				FText::FromName(State->GetID()),
				FText::FromName(Summary.InspectedStartState)));
		}

		switch (State->StateNodeType)
		{
		case EStateNodeType::Regular: ++Summary.ObjectiveStates; break;
		case EStateNodeType::Success: ++Summary.SuccessEndings; break;
		case EStateNodeType::Failure: ++Summary.FailureEndings; break;
		default: break;
		}
		Summary.Conditions += State->Conditions.Num();
		Summary.Events += State->Events.Num();
		for (const UNarrativeEvent* Event : State->Events)
		{
			if (!Event)
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeEmptyStateEvent",
					"Compiled State '{0}' contains an empty Event row."),
					FText::FromName(State->GetID())));
				continue;
			}
			Summary.Conditions += Event->Conditions.Num();
			Summary.AutomaticCheckpoints +=
				Event->IsA<UTerritoryNarrativeCheckpointEvent>() ? 1 : 0;
		}

		for (const UQuestBranch* Branch : State->Branches)
		{
			if (!Branch)
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeEmptyBranch",
					"Compiled State '{0}' contains an empty Branch reference."),
					FText::FromName(State->GetID())));
				continue;
			}
			ReachedBranches.Add(Branch);
			++Summary.Routes;
			if (Branch->GetID().IsNone())
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeBranchNoID",
					"A route leaving State '{0}' has no stable Branch ID."),
					FText::FromName(State->GetID())));
			}
			else if (NodeIDs.Contains(Branch->GetID()))
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeDuplicateBranchID",
					"Compiled Narrative node ID '{0}' is duplicated."),
					FText::FromName(Branch->GetID())));
			}
			NodeIDs.Add(Branch->GetID());
			if (!Branch->DestinationState)
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeMissingDestination",
					"Compiled Branch '{0}' has no destination State."),
					FText::FromName(Branch->GetID())));
			}
			if (Branch->QuestTasks.IsEmpty())
			{
				Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeEmptyTaskRoute",
					"Compiled Branch '{0}' has no Quest Task. Narrative has no task completion callback that can take this route."),
					FText::FromName(Branch->GetID())));
			}

			Summary.Conditions += Branch->Conditions.Num();
			Summary.Events += Branch->Events.Num();
			for (const UNarrativeEvent* Event : Branch->Events)
			{
				if (!Event)
				{
					Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeEmptyBranchEvent",
						"Compiled Branch '{0}' contains an empty Event row."),
						FText::FromName(Branch->GetID())));
					continue;
				}
				Summary.Conditions += Event->Conditions.Num();
				Summary.AutomaticCheckpoints +=
					Event->IsA<UTerritoryNarrativeCheckpointEvent>() ? 1 : 0;
			}

			bool bHasConditionGate = false;
			int32 LocalInternalTasks = 0;
			int32 GateConditionCount = 0;
			for (const UNarrativeTask* Task : Branch->QuestTasks)
			{
				if (!Task)
				{
					Summary.Errors.Add(FText::Format(LOCTEXT("RuntimeEmptyTask",
						"Compiled Branch '{0}' contains an empty Quest Task row."),
						FText::FromName(Branch->GetID())));
					continue;
				}
				if (const UTerritoryNarrativeConditionTask* Gate =
					Cast<UTerritoryNarrativeConditionTask>(Task))
				{
					bHasConditionGate = true;
					++LocalInternalTasks;
					GateConditionCount += Gate->Conditions.Num();
					++Summary.InternalTasks;
				}
				else
				{
					++Summary.PlayerTasks;
				}
				Summary.OptionalTasks += Task->bOptional ? 1 : 0;
				Summary.HiddenTasks += Task->bHidden ? 1 : 0;
				Summary.NavigationMarkerTasks +=
					Task->MarkerSettings.bAddNavigationMarker ? 1 : 0;
			}
			const int32 VisibleNodeConditions =
				State->Conditions.Num() + Branch->Conditions.Num();
			if (VisibleNodeConditions > 0 && !bHasConditionGate)
			{
				Summary.Warnings.Add(FText::Format(LOCTEXT("RuntimeConditionsNotGated",
					"Branch '{0}' displays {1} Quest-node condition(s), but has no condition-gate Task. This Narrative Pro version does not evaluate Quest-node conditions."),
					FText::FromName(Branch->GetID()),
					FText::AsNumber(VisibleNodeConditions)));
			}
			if (VisibleNodeConditions == 0)
			{
				Summary.Conditions += GateConditionCount;
			}
			Summary.FlowLines.Add(FString::Printf(
				TEXT("%s -> %s -> %s | %d player task(s), %d internal gate(s), %d node condition(s), %d event(s)%s"),
				*State->GetID().ToString(), *Branch->GetID().ToString(),
				Branch->DestinationState
					? *Branch->DestinationState->GetID().ToString() : TEXT("MISSING"),
				Branch->QuestTasks.Num() - LocalInternalTasks,
				LocalInternalTasks, VisibleNodeConditions,
				Branch->Events.Num(), Branch->bHidden ? TEXT(" | hidden route") : TEXT("")));
		}
	}

	for (const UQuestBranch* Branch : Quest->GetBranches())
	{
		if (Branch && !ReachedBranches.Contains(Branch))
		{
			Summary.Warnings.Add(FText::Format(LOCTEXT("RuntimeOrphanBranch",
				"Compiled Branch '{0}' is not owned by any State and can never activate."),
				FText::FromName(Branch->GetID())));
		}
	}
	if (Summary.SuccessEndings == 0)
	{
		Summary.Errors.Add(LOCTEXT("RuntimeNoSuccessEnding",
			"The selected compiled Narrative Quest has no Success ending."));
	}
	if (Summary.InspectedQuestName.IsEmpty())
	{
		Summary.Warnings.Add(LOCTEXT("RuntimeNoQuestName",
			"The selected compiled Narrative Quest has no player-facing Quest Name."));
	}
	if (Summary.InspectedQuestDescription.IsEmpty())
	{
		Summary.Warnings.Add(LOCTEXT("RuntimeNoQuestDescription",
			"The selected compiled Narrative Quest has no player-facing Quest Description."));
	}

	Summary.bValid = Summary.Errors.IsEmpty();
	Summary.Headline = FString::Printf(
		TEXT("%d objective state(s) • %d route(s) • %d player task(s) + %d internal gate(s) • %d condition(s) • %d event(s) • %d success / %d failure ending(s)"),
		Summary.ObjectiveStates, Summary.Routes, Summary.PlayerTasks,
		Summary.InternalTasks, Summary.Conditions, Summary.Events,
		Summary.SuccessEndings, Summary.FailureEndings);
	return Summary;
}

FString UTerritoryQuestCascadeRecipe::BuildSelectedRuntimeQuestReport() const
{
	const FTerritoryQuestCascadeLogicSummary Summary =
		BuildSelectedRuntimeQuestSummary();
	FString Failure;
	const UQuest* Quest = TerritoryQuestCascadeRecipe::ResolveRuntimeQuestTemplate(
		NarrativeQuestGraph, Failure);
	if (!Quest)
	{
		return FString::Printf(TEXT("COMPILED NARRATIVE QUEST RUNTIME SUMMARY\nERROR: %s\n"),
			*Failure);
	}

	FString Report = FString::Printf(
		TEXT("COMPILED NARRATIVE QUEST RUNTIME SUMMARY\nSource: %s\nQuest Name: %s\nQuest Description: %s\nStart State: %s\nTracked: %s\n%s\n\nQUEST SETTINGS\n"),
		*Summary.Source, *Summary.InspectedQuestName.ToString(),
		*Summary.InspectedQuestDescription.ToString(),
		*Summary.InspectedStartState.ToString(),
		Summary.bInspectedQuestTracked ? TEXT("Yes") : TEXT("No"),
		*Summary.Headline);
	TSet<const UObject*> Visited;
	TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
		Quest, Report, TEXT("  "), Visited);

	for (const UQuestState* State : Quest->GetStates())
	{
		if (!State)
		{
			Report += TEXT("\nSTATE: EMPTY\n");
			continue;
		}
		Report += FString::Printf(TEXT("\nSTATE %s [%s]%s\n  Description: %s\n"),
			*State->GetID().ToString(),
			*TerritoryQuestCascadeRecipe::RuntimeStateTypeText(State->StateNodeType),
			State == Quest->GetQuestStartState() ? TEXT(" [START]") : TEXT(""),
			*State->Description.ToString());
		for (const UNarrativeCondition* Condition : State->Conditions)
		{
			Report += TEXT("  STATE CONDITION: ")
				+ TerritoryQuestCascadeRecipe::ConditionLabel(Condition) + TEXT("\n");
			TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
				Condition, Report, TEXT("    "), Visited);
		}
		for (const UNarrativeEvent* Event : State->Events)
		{
			Report += TEXT("  STATE EVENT: ")
				+ TerritoryQuestCascadeRecipe::EventLabel(Event) + TEXT("\n");
			TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
				Event, Report, TEXT("    "), Visited);
			if (Event)
			{
				for (const UNarrativeCondition* Condition : Event->Conditions)
				{
					Report += TEXT("    EVENT CONDITION: ")
						+ TerritoryQuestCascadeRecipe::ConditionLabel(Condition)
						+ TEXT("\n");
					TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
						Condition, Report, TEXT("      "), Visited);
				}
			}
		}

		for (const UQuestBranch* Branch : State->Branches)
		{
			if (!Branch)
			{
				Report += TEXT("  ROUTE: EMPTY\n");
				continue;
			}
			Report += FString::Printf(
				TEXT("  ROUTE %s -> %s%s\n    Description: %s\n"),
				*Branch->GetID().ToString(), Branch->DestinationState
					? *Branch->DestinationState->GetID().ToString() : TEXT("MISSING"),
				Branch->bHidden ? TEXT(" [HIDDEN]") : TEXT(""),
				*Branch->Description.ToString());
			for (const UNarrativeCondition* Condition : Branch->Conditions)
			{
				Report += TEXT("    ROUTE CONDITION: ")
					+ TerritoryQuestCascadeRecipe::ConditionLabel(Condition)
					+ TEXT("\n");
				TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
					Condition, Report, TEXT("      "), Visited);
			}
			for (const UNarrativeTask* Task : Branch->QuestTasks)
			{
				Report += FString::Printf(
					TEXT("    TASK: %s [%s] | quantity %d%s%s%s\n"),
					*TerritoryQuestCascadeRecipe::TaskLabel(Task),
					Task ? *GetNameSafe(Task->GetClass()) : TEXT("EMPTY"),
					Task ? Task->RequiredQuantity : 0,
					Task && Task->bOptional ? TEXT(" | OPTIONAL") : TEXT(" | REQUIRED"),
					Task && Task->bHidden ? TEXT(" | HIDDEN") : TEXT(""),
					Task && Task->MarkerSettings.bAddNavigationMarker
						? TEXT(" | NAVIGATION MARKER") : TEXT(""));
				TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
					Task, Report, TEXT("      "), Visited);
				if (const UTerritoryNarrativeConditionTask* Gate =
					Cast<UTerritoryNarrativeConditionTask>(Task))
				{
					for (const UNarrativeCondition* Condition : Gate->Conditions)
					{
						Report += TEXT("      GATE CONDITION: ")
							+ TerritoryQuestCascadeRecipe::ConditionLabel(Condition)
							+ TEXT("\n");
						TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
							Condition, Report, TEXT("        "), Visited);
					}
				}
			}
			for (const UNarrativeEvent* Event : Branch->Events)
			{
				Report += TEXT("    ROUTE EVENT: ")
					+ TerritoryQuestCascadeRecipe::EventLabel(Event) + TEXT("\n");
				TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
					Event, Report, TEXT("      "), Visited);
				if (Event)
				{
					for (const UNarrativeCondition* Condition : Event->Conditions)
					{
						Report += TEXT("      EVENT CONDITION: ")
							+ TerritoryQuestCascadeRecipe::ConditionLabel(Condition)
							+ TEXT("\n");
						TerritoryQuestCascadeRecipe::AppendConfiguredProperties(
							Condition, Report, TEXT("        "), Visited);
					}
				}
			}
		}
	}

	Report += TEXT("\nVALIDATION\n");
	Report += Summary.bValid
		? TEXT("  Runtime graph is structurally ready.\n")
		: TEXT("  Runtime graph has blocking setup errors.\n");
	for (const FText& Error : Summary.Errors)
	{
		Report += TEXT("  ERROR: ") + Error.ToString() + TEXT("\n");
	}
	for (const FText& Warning : Summary.Warnings)
	{
		Report += TEXT("  WARNING: ") + Warning.ToString() + TEXT("\n");
	}
	return Report;
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

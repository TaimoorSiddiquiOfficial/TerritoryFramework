#include "Tales/TerritoryQuestCascadeEditorLibrary.h"

#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "QuestBlueprint.h"
#include "ScopedTransaction.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"
#include "Tales/TerritoryNarrativeCheckpointEvent.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "TerritoryQuestCascadeEditorLibrary"

namespace TerritoryQuestCascadeEditor
{
	constexpr int32 StateHorizontalSpacing = 900;
	constexpr int32 StateVerticalSpacing = 360;
	constexpr int32 BranchVerticalSpacing = 180;

	UClass* LoadNarrativeEditorClass(const TCHAR* ClassName)
	{
		FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("NarrativeQuestEditor"));
		return FindObject<UClass>(nullptr, *FString::Printf(
			TEXT("/Script/NarrativeQuestEditor.%s"), ClassName));
	}

	bool SetNodeObject(UEdGraphNode* Node, const FName PropertyName,
		UObject* Value, FTerritoryQuestCascadeBuildReport& Report)
	{
		FObjectPropertyBase* Property = Node
			? FindFProperty<FObjectPropertyBase>(Node->GetClass(), PropertyName)
			: nullptr;
		if (!Property)
		{
			Report.Errors.Add(FText::Format(LOCTEXT("NarrativeNodePropertyMissing",
				"Narrative graph node class '{0}' does not expose expected property '{1}'. The installed Narrative Pro editor API may have changed."),
				FText::FromString(GetNameSafe(Node ? Node->GetClass() : nullptr)),
				FText::FromName(PropertyName)));
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Node, Value);
		return true;
	}

	template<typename TObjectType>
	TObjectType* GetNodeObject(UEdGraphNode* Node, const FName PropertyName)
	{
		FObjectPropertyBase* Property = Node
			? FindFProperty<FObjectPropertyBase>(Node->GetClass(), PropertyName)
			: nullptr;
		return Property
			? Cast<TObjectType>(Property->GetObjectPropertyValue_InContainer(Node))
			: nullptr;
	}

	UEdGraphPin* FindPin(const UEdGraphNode* Node,
		const EEdGraphPinDirection Direction)
	{
		if (!Node) return nullptr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction) return Pin;
		}
		return nullptr;
	}

	UEdGraphNode* AddGraphNode(UEdGraph* Graph, UClass* NodeClass,
		const FVector2D& Position)
	{
		if (!Graph || !NodeClass
			|| !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
		{
			return nullptr;
		}
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeClass,
			NAME_None, RF_Transactional);
		Graph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->SnapToGrid(16);
		return Node;
	}

	UEdGraphNode* AddPreparedBranchNode(UEdGraph* Graph, UClass* NodeClass,
		const FVector2D& Position, UQuestBranch* Branch,
		FTerritoryQuestCascadeBuildReport& Report)
	{
		if (!Graph || !NodeClass || !Branch
			|| !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
		{
			return nullptr;
		}
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeClass,
			NAME_None, RF_Transactional);
		Graph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		if (!SetNodeObject(Node, TEXT("Branch"), Branch, Report)
			|| !SetNodeObject(Node, TEXT("QuestNode"), Branch, Report))
		{
			Graph->RemoveNode(Node);
			return nullptr;
		}
		// Narrative sees the prepared branch during PostPlacedNewNode and adopts
		// it instead of allocating an unwanted empty branch.
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->SnapToGrid(16);
		return Node;
	}

	int32 CopyEvents(const TArray<TObjectPtr<UNarrativeEvent>>& Templates,
		UNarrativeNodeBase* Destination)
	{
		if (!Destination) return 0;
		Destination->Events.Reset();
		int32 Copied = 0;
		for (const UNarrativeEvent* EventTemplate : Templates)
		{
			if (EventTemplate)
			{
				Destination->Events.Add(DuplicateObject<UNarrativeEvent>(
					EventTemplate, Destination));
				++Copied;
			}
		}
		return Copied;
	}

	bool AreConditionsEquivalent(const UNarrativeCondition* A,
		const UNarrativeCondition* B)
	{
		if (!A || !B || A->GetClass() != B->GetClass()) return false;
		for (TFieldIterator<FProperty> It(A->GetClass(),
			EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Transient)
				&& !Property->Identical_InContainer(A, B, PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	UTerritoryNarrativeConditionTask* FindOrCreateConditionGate(
		UQuestBranch* Branch, FTerritoryQuestCascadeBuildReport& Report)
	{
		if (!Branch) return nullptr;
		for (UNarrativeTask* Task : Branch->QuestTasks)
		{
			if (UTerritoryNarrativeConditionTask* Gate =
				Cast<UTerritoryNarrativeConditionTask>(Task))
			{
				return Gate;
			}
		}

		UTerritoryNarrativeConditionTask* Gate =
			NewObject<UTerritoryNarrativeConditionTask>(
				Branch, NAME_None, RF_Transactional);
		Gate->RequirementDescription = Branch->Description.IsEmpty()
			? LOCTEXT("DefaultMigratedGateDescription", "Meet the route requirements")
			: Branch->Description;
		Branch->QuestTasks.Insert(Gate, 0);
		++Report.CreatedTasks;
		++Report.CreatedConditionGates;
		return Gate;
	}

	void AddUniqueGateCondition(const UNarrativeCondition* Template,
		UTerritoryNarrativeConditionTask* Gate,
		FTerritoryQuestCascadeBuildReport& Report)
	{
		if (!Template || !Gate) return;
		if (Gate->Conditions.ContainsByPredicate(
			[Template](const UNarrativeCondition* Existing)
			{
				return AreConditionsEquivalent(Template, Existing);
			}))
		{
			return;
		}
		Gate->Conditions.Add(DuplicateObject<UNarrativeCondition>(
			Template, Gate));
		++Report.CopiedConditions;
	}

	bool CopyMatchingProperty(const UObject* Source, UObject* Destination,
		const FName PropertyName, FTerritoryQuestCascadeBuildReport& Report)
	{
		const FProperty* SourceProperty = Source
			? FindFProperty<FProperty>(Source->GetClass(), PropertyName) : nullptr;
		FProperty* DestinationProperty = Destination
			? FindFProperty<FProperty>(Destination->GetClass(), PropertyName) : nullptr;
		if (!SourceProperty || !DestinationProperty
			|| !SourceProperty->SameType(DestinationProperty))
		{
			Report.Errors.Add(FText::Format(LOCTEXT("QuestPropertyMismatch",
				"Could not copy Narrative Quest setting '{0}'. The installed Narrative Pro API may have changed."),
				FText::FromName(PropertyName)));
			return false;
		}
		void* DestinationValue =
			DestinationProperty->ContainerPtrToValuePtr<void>(Destination);
		const void* SourceValue =
			SourceProperty->ContainerPtrToValuePtr<void>(Source);
		DestinationProperty->CopyCompleteValue(DestinationValue, SourceValue);
		return true;
	}

	bool ApplyQuestDefaults(const UTerritoryQuestCascadeRecipe* Recipe,
		UQuest* Destination, FTerritoryQuestCascadeBuildReport& Report)
	{
		if (!Recipe || !Destination) return false;
		Destination->Modify();
		Destination->SetQuestName(Recipe->QuestName);
		Destination->SetQuestDescription(Recipe->QuestDescription);
		const FName SettingsToCopy[] = {
			TEXT("bTracked"), TEXT("QuestDialogue"),
			TEXT("QuestDialoguePlayParams"), TEXT("bResumeDialogueAfterLoad")
		};
		for (const FName Setting : SettingsToCopy)
		{
			if (!CopyMatchingProperty(Recipe, Destination, Setting, Report))
			{
				return false;
			}
		}
		return true;
	}

	FString MakeCompactAssetStem(const FString& Source)
	{
		FString Result;
		bool bCapitalizeNext = true;
		for (const TCHAR Character : Source)
		{
			if (FChar::IsAlnum(Character))
			{
				Result.AppendChar(bCapitalizeNext
					? FChar::ToUpper(Character) : Character);
				bCapitalizeNext = false;
			}
			else
			{
				bCapitalizeNext = true;
			}
		}
		return ObjectTools::SanitizeObjectName(Result);
	}

	bool IsGenericRecipeName(const FString& Name)
	{
		return Name.StartsWith(TEXT("NewTerritoryQuestCascadeRecipe"),
			ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("TerritoryQuestCascadeRecipe"),
				ESearchCase::IgnoreCase)
			|| Name.StartsWith(TEXT("DA_QC_NewMission"),
				ESearchCase::IgnoreCase)
			|| Name.StartsWith(TEXT("NewMission"), ESearchCase::IgnoreCase);
	}

	bool ShouldGenerateCheckpoint(
		const UTerritoryQuestCascadeRecipe* Recipe,
		const FTerritoryQuestCascadeState& State)
	{
		if (!Recipe
			|| Recipe->CheckpointMode == ETerritoryQuestCheckpointMode::Disabled)
		{
			return false;
		}
		if (Recipe->CheckpointMode ==
			ETerritoryQuestCheckpointMode::ObjectiveStates
			&& State.Type != ETerritoryQuestCascadeStateType::Objective)
		{
			return false;
		}
		return !State.Events.ContainsByPredicate(
			[](const TObjectPtr<UNarrativeEvent>& Event)
			{
				return Event
					&& Event->IsA<UTerritoryNarrativeCheckpointEvent>();
			});
	}

	void ComputeStatePositions(const UTerritoryQuestCascadeRecipe* Recipe,
		TMap<FName, FVector2D>& OutPositions)
	{
		TMap<FName, const FTerritoryQuestCascadeState*> StatesByID;
		for (const FTerritoryQuestCascadeState& State : Recipe->States)
		{
			StatesByID.Add(State.StateID, &State);
		}

		TMap<FName, int32> DepthByState;
		TArray<FName> Pending = { Recipe->StartStateID };
		DepthByState.Add(Recipe->StartStateID, 0);
		while (!Pending.IsEmpty())
		{
			const FName CurrentID = Pending[0];
			Pending.RemoveAt(0, 1, EAllowShrinking::No);
			const int32 CurrentDepth = DepthByState.FindRef(CurrentID);
			const FTerritoryQuestCascadeState* const* Current =
				StatesByID.Find(CurrentID);
			if (!Current) continue;
			for (const FTerritoryQuestCascadeBranch& Branch :
				(*Current)->Branches)
			{
				if (!DepthByState.Contains(Branch.DestinationStateID))
				{
					DepthByState.Add(Branch.DestinationStateID,
						CurrentDepth + 1);
					Pending.Add(Branch.DestinationStateID);
				}
			}
		}

		TMap<int32, int32> RowsAtDepth;
		for (int32 Index = 0; Index < Recipe->States.Num(); ++Index)
		{
			const FTerritoryQuestCascadeState& State = Recipe->States[Index];
			const int32 Depth = DepthByState.Contains(State.StateID)
				? DepthByState.FindRef(State.StateID) : Index + 1;
			const int32 Row = RowsAtDepth.FindOrAdd(Depth)++;
			OutPositions.Add(State.StateID, FVector2D(
				Depth * StateHorizontalSpacing,
				Row * StateVerticalSpacing));
		}
	}

	UEdGraph* EnsureQuestGraph(UQuestBlueprint* QuestBlueprint,
		FTerritoryQuestCascadeBuildReport& Report)
	{
		if (QuestBlueprint->QuestGraph) return QuestBlueprint->QuestGraph;

		UClass* GraphClass = LoadNarrativeEditorClass(TEXT("QuestGraph"));
		UClass* SchemaClass = LoadNarrativeEditorClass(TEXT("QuestGraphSchema"));
		if (!GraphClass || !SchemaClass)
		{
			Report.Errors.Add(LOCTEXT("NarrativeGraphClassesMissing",
				"Narrative Quest Editor graph classes are unavailable. Enable the Narrative Pro editor module."));
			return nullptr;
		}

		QuestBlueprint->QuestGraph = FBlueprintEditorUtils::CreateNewGraph(
			QuestBlueprint, TEXT("Quest Graph"), GraphClass, SchemaClass);
		if (!QuestBlueprint->QuestGraph)
		{
			Report.Errors.Add(LOCTEXT("QuestGraphCreateFailed",
				"Could not create Narrative's Quest Graph."));
			return nullptr;
		}
		FBlueprintEditorUtils::AddUbergraphPage(QuestBlueprint,
			QuestBlueprint->QuestGraph);
		const UEdGraphSchema* Schema = QuestBlueprint->QuestGraph->GetSchema();
		if (!Schema)
		{
			Report.Errors.Add(LOCTEXT("QuestSchemaMissing",
				"The new Narrative Quest Graph has no schema."));
			return nullptr;
		}
		Schema->CreateDefaultNodesForGraph(*QuestBlueprint->QuestGraph);
		return QuestBlueprint->QuestGraph;
	}

	UQuestBlueprint* CreateQuestAsset(const FString& ContentPath,
		const FString& DesiredName,
		FTerritoryQuestCascadeBuildReport& Report)
	{
		if (!FPackageName::IsValidLongPackageName(ContentPath))
		{
			Report.Errors.Add(FText::Format(LOCTEXT("InvalidContentPath",
				"'{0}' is not a valid Unreal content path. Use a path such as /Game/Quests."),
				FText::FromString(ContentPath)));
			return nullptr;
		}
		FString CleanName = ObjectTools::SanitizeObjectName(DesiredName);
		if (CleanName.IsEmpty()) CleanName = TEXT("NQ_TerritoryStory");

		UClass* FactoryClass = LoadNarrativeEditorClass(TEXT("QuestAssetFactory"));
		if (!FactoryClass || !FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			Report.Errors.Add(LOCTEXT("QuestFactoryMissing",
				"Narrative Quest Asset factory is unavailable."));
			return nullptr;
		}

		IAssetTools& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
			.Get();
		FString UniquePackageName;
		FString UniqueAssetName;
		AssetTools.CreateUniqueAssetName(
			ContentPath / CleanName, TEXT(""),
			UniquePackageName, UniqueAssetName);
		const FString UniquePath =
			FPackageName::GetLongPackagePath(UniquePackageName);
		UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		UQuestBlueprint* Quest = Cast<UQuestBlueprint>(AssetTools.CreateAsset(
			UniqueAssetName, UniquePath, UQuestBlueprint::StaticClass(), Factory));
		if (!Quest)
		{
			Report.Errors.Add(LOCTEXT("QuestAssetCreateFailed",
				"Narrative Quest asset creation failed."));
			return nullptr;
		}
		Report.QuestAsset = Quest;
		Report.QuestPackageName = Quest->GetOutermost()->GetName();
		return Quest;
	}

	bool HasAuthoredNodes(const UQuestBlueprint* QuestBlueprint)
	{
		const UQuest* Quest = QuestBlueprint ? QuestBlueprint->QuestTemplate : nullptr;
		if (!Quest) return true;
		return !Quest->GetBranches().IsEmpty()
			|| Quest->GetStates().Num() > 1;
	}
}

FString UTerritoryQuestCascadeEditorLibrary::GetSuggestedQuestAssetName(
	const UTerritoryQuestCascadeRecipe* Recipe)
{
	if (!Recipe) return TEXT("NQ_TerritoryStory");

	FString RecipeName = Recipe->GetName();
	RecipeName.RemoveFromStart(TEXT("DA_QC_"));
	RecipeName.RemoveFromStart(TEXT("DA_"));
	RecipeName.RemoveFromStart(TEXT("QC_"));
	FString Stem = RecipeName;
	if (TerritoryQuestCascadeEditor::IsGenericRecipeName(Recipe->GetName())
		|| TerritoryQuestCascadeEditor::IsGenericRecipeName(RecipeName))
	{
		Stem = TerritoryQuestCascadeEditor::MakeCompactAssetStem(
			Recipe->QuestName.ToString());
	}
	Stem = ObjectTools::SanitizeObjectName(Stem);
	if (Stem.IsEmpty()) Stem = TEXT("TerritoryStory");
	return Stem.StartsWith(TEXT("NQ_")) ? Stem : TEXT("NQ_") + Stem;
}

FTerritoryQuestCascadeBuildReport
UTerritoryQuestCascadeEditorLibrary::CreateQuestBesideRecipe(
	UTerritoryQuestCascadeRecipe* Recipe)
{
	if (!Recipe)
	{
		FTerritoryQuestCascadeBuildReport Report;
		Report.Errors.Add(LOCTEXT("RecipeRequired",
			"Select a Territory Narrative Quest Cascade Recipe."));
		return Report;
	}
	const FString RecipePackage = Recipe->GetOutermost()->GetName();
	const FString Folder = FPackageName::GetLongPackagePath(RecipePackage);
	return CreateQuestFromRecipe(Recipe, Folder,
		GetSuggestedQuestAssetName(Recipe));
}

FTerritoryQuestCascadeBuildReport
UTerritoryQuestCascadeEditorLibrary::CreateQuestFromRecipe(
	UTerritoryQuestCascadeRecipe* Recipe,
	const FString& DestinationContentPath,
	const FString& DesiredAssetName)
{
	FTerritoryQuestCascadeBuildReport Report;
	if (!Recipe)
	{
		Report.Errors.Add(LOCTEXT("RecipeRequiredCreate",
			"Select a Territory Narrative Quest Cascade Recipe."));
		return Report;
	}
	const FTerritoryQuestCascadeValidation Validation = Recipe->ValidateRecipe();
	Report.Errors = Validation.Errors;
	Report.Warnings = Validation.Warnings;
	if (!Validation.bValid) return Report;

	UQuestBlueprint* Quest = TerritoryQuestCascadeEditor::CreateQuestAsset(
		DestinationContentPath, DesiredAssetName, Report);
	if (!Quest) return Report;

	FTerritoryQuestCascadeBuildReport Build =
		BuildEmptyQuestFromRecipe(Quest, Recipe);
	Build.QuestAsset = Quest;
	Build.QuestPackageName = Quest->GetOutermost()->GetName();
	return Build;
}

FTerritoryQuestCascadeBuildReport
UTerritoryQuestCascadeEditorLibrary::BuildEmptyQuestFromRecipe(
	UQuestBlueprint* EmptyQuest,
	UTerritoryQuestCascadeRecipe* Recipe)
{
	using namespace TerritoryQuestCascadeEditor;
	FTerritoryQuestCascadeBuildReport Report;
	Report.QuestAsset = EmptyQuest;
	Report.QuestPackageName = EmptyQuest
		? EmptyQuest->GetOutermost()->GetName() : FString();
	if (!EmptyQuest || !Recipe)
	{
		Report.Errors.Add(LOCTEXT("QuestAndRecipeRequired",
			"Select both an empty Narrative Quest and a cascade recipe."));
		return Report;
	}
	const FTerritoryQuestCascadeValidation Validation = Recipe->ValidateRecipe();
	Report.Errors = Validation.Errors;
	Report.Warnings = Validation.Warnings;
	if (!Validation.bValid) return Report;
	if (!EmptyQuest->QuestTemplate)
	{
		Report.Errors.Add(LOCTEXT("QuestTemplateMissing",
			"The selected asset has no Narrative Quest Template."));
		return Report;
	}
	if (HasAuthoredNodes(EmptyQuest))
	{
		Report.Errors.Add(LOCTEXT("QuestNotEmpty",
			"The selected Narrative Quest already has authored nodes. Cascade generation never overwrites existing quest work; create a new quest instead."));
		return Report;
	}

	const FScopedTransaction Transaction(LOCTEXT("BuildQuestTransaction",
		"Build Narrative Quest From Territory Cascade Recipe"));
	EmptyQuest->Modify();
	EmptyQuest->QuestTemplate->Modify();
	UEdGraph* Graph = EnsureQuestGraph(EmptyQuest, Report);
	if (!Graph) return Report;
	Graph->Modify();
	const UEdGraphSchema* Schema = Graph->GetSchema();

	UEdGraphNode* RootNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetClass()->GetName() == TEXT("QuestGraphNode_Root"))
		{
			RootNode = Node;
			break;
		}
	}
	if (!RootNode)
	{
		Report.Errors.Add(LOCTEXT("QuestRootMissing",
			"Narrative did not create the required Quest root node."));
		return Report;
	}

	UClass* StateNodeClass = LoadNarrativeEditorClass(TEXT("QuestGraphNode_State"));
	UClass* SuccessNodeClass = LoadNarrativeEditorClass(TEXT("QuestGraphNode_Success"));
	UClass* FailureNodeClass = LoadNarrativeEditorClass(TEXT("QuestGraphNode_Failure"));
	UClass* BranchNodeClass = LoadNarrativeEditorClass(TEXT("QuestGraphNode_Action"));
	if (!StateNodeClass || !SuccessNodeClass || !FailureNodeClass
		|| !BranchNodeClass)
	{
		Report.Errors.Add(LOCTEXT("NarrativeNodeClassesMissing",
			"Narrative Quest graph node classes are unavailable."));
		return Report;
	}

	TMap<FName, FVector2D> StatePositions;
	ComputeStatePositions(Recipe, StatePositions);
	TMap<FName, UEdGraphNode*> StateNodes;
	TMap<FName, UQuestState*> RuntimeStates;

	for (const FTerritoryQuestCascadeState& StateSpec : Recipe->States)
	{
		UEdGraphNode* GraphNode = nullptr;
		if (StateSpec.StateID == Recipe->StartStateID)
		{
			GraphNode = RootNode;
			const FVector2D Position = StatePositions.FindRef(StateSpec.StateID);
			GraphNode->NodePosX = FMath::RoundToInt(Position.X);
			GraphNode->NodePosY = FMath::RoundToInt(Position.Y);
		}
		else
		{
			UClass* DesiredClass = StateNodeClass;
			if (StateSpec.Type == ETerritoryQuestCascadeStateType::Success)
			{
				DesiredClass = SuccessNodeClass;
			}
			else if (StateSpec.Type == ETerritoryQuestCascadeStateType::Failure)
			{
				DesiredClass = FailureNodeClass;
			}
			GraphNode = AddGraphNode(Graph, DesiredClass,
				StatePositions.FindRef(StateSpec.StateID));
		}
		UQuestState* RuntimeState = GetNodeObject<UQuestState>(
			GraphNode, TEXT("State"));
		if (!GraphNode || !RuntimeState)
		{
			Report.Errors.Add(FText::Format(LOCTEXT("StateNodeCreateFailed",
				"Could not create Narrative state '{0}'."),
				FText::FromName(StateSpec.StateID)));
			return Report;
		}
		RuntimeState->Modify();
		RuntimeState->SetID(StateSpec.StateID);
		RuntimeState->Description = StateSpec.Description;
		Report.CopiedEvents += CopyEvents(StateSpec.Events, RuntimeState);
		if (ShouldGenerateCheckpoint(Recipe, StateSpec))
		{
			UTerritoryNarrativeCheckpointEvent* Checkpoint =
				NewObject<UTerritoryNarrativeCheckpointEvent>(
					RuntimeState, NAME_None, RF_Transactional);
			Checkpoint->SaveNameOverride = Recipe->CheckpointSaveNameOverride;
			Checkpoint->FallbackCampaignIndex =
				Recipe->CheckpointFallbackCampaignIndex;
			RuntimeState->Events.Add(Checkpoint);
			++Report.CreatedCheckpointEvents;
		}
		// Narrative Pro exposes Conditions on its common node base, but its Quest
		// state machine never evaluates state/branch Conditions. Recipe conditions
		// are therefore materialized only into the functional hidden gate Task below.
		RuntimeState->Conditions.Reset();
		StateNodes.Add(StateSpec.StateID, GraphNode);
		RuntimeStates.Add(StateSpec.StateID, RuntimeState);
		++Report.CreatedStates;
	}

	EmptyQuest->QuestTemplate->SetQuestStartState(
		RuntimeStates.FindRef(Recipe->StartStateID));
	if (!ApplyQuestDefaults(Recipe, EmptyQuest->QuestTemplate, Report))
	{
		return Report;
	}
	UQuest* QuestClassDefaults = EmptyQuest->GeneratedClass
		? Cast<UQuest>(EmptyQuest->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!QuestClassDefaults
		|| !ApplyQuestDefaults(Recipe, QuestClassDefaults, Report))
	{
		Report.Errors.Add(LOCTEXT("QuestDefaultsMissing",
			"The generated Narrative Quest class defaults are unavailable. Quest Name and Description could not be copied."));
		return Report;
	}

	for (const FTerritoryQuestCascadeState& StateSpec : Recipe->States)
	{
		UEdGraphNode* SourceNode = StateNodes.FindRef(StateSpec.StateID);
		for (int32 BranchIndex = 0;
			BranchIndex < StateSpec.Branches.Num(); ++BranchIndex)
		{
			const FTerritoryQuestCascadeBranch& BranchSpec =
				StateSpec.Branches[BranchIndex];
			UEdGraphNode* DestinationNode =
				StateNodes.FindRef(BranchSpec.DestinationStateID);
			UQuestBranch* RuntimeBranch = NewObject<UQuestBranch>(
				EmptyQuest->QuestTemplate, NAME_None, RF_Transactional);
			RuntimeBranch->SetID(BranchSpec.BranchID);
			RuntimeBranch->Description = BranchSpec.Description;
			RuntimeBranch->bHidden = BranchSpec.bHidden;
			Report.CopiedEvents += CopyEvents(BranchSpec.Events, RuntimeBranch);
			RuntimeBranch->Conditions.Reset();

			if (!StateSpec.Conditions.IsEmpty()
				|| !BranchSpec.Conditions.IsEmpty())
			{
				UTerritoryNarrativeConditionTask* Gate =
					NewObject<UTerritoryNarrativeConditionTask>(
						RuntimeBranch, NAME_None, RF_Transactional);
				Gate->RequirementDescription = BranchSpec.Description.IsEmpty()
					? FText::FromString(TEXT("Meet the route requirements"))
					: BranchSpec.Description;
				for (const UNarrativeCondition* Template : StateSpec.Conditions)
				{
					AddUniqueGateCondition(Template, Gate, Report);
				}
				for (const UNarrativeCondition* Template : BranchSpec.Conditions)
				{
					AddUniqueGateCondition(Template, Gate, Report);
				}
				RuntimeBranch->QuestTasks.Add(Gate);
				++Report.CreatedTasks;
				++Report.CreatedConditionGates;
			}
			for (const UNarrativeTask* TaskTemplate : BranchSpec.Tasks)
			{
				if (TaskTemplate)
				{
					RuntimeBranch->QuestTasks.Add(
						DuplicateObject<UNarrativeTask>(
							TaskTemplate, RuntimeBranch));
					++Report.CreatedTasks;
				}
			}

			const FVector2D SourcePosition =
				StatePositions.FindRef(StateSpec.StateID);
			const FVector2D DestinationPosition =
				StatePositions.FindRef(BranchSpec.DestinationStateID);
			const FVector2D BranchPosition(
				(SourcePosition.X + DestinationPosition.X) * 0.5,
				(SourcePosition.Y + DestinationPosition.Y) * 0.5
					+ BranchIndex * BranchVerticalSpacing);
			UEdGraphNode* BranchNode = AddPreparedBranchNode(
				Graph, BranchNodeClass, BranchPosition, RuntimeBranch, Report);
			if (!BranchNode)
			{
				Report.Errors.Add(FText::Format(LOCTEXT("BranchNodeCreateFailed",
					"Could not create Narrative branch '{0}'."),
					FText::FromName(BranchSpec.BranchID)));
				return Report;
			}
			UEdGraphPin* SourceOutput = FindPin(SourceNode, EGPD_Output);
			UEdGraphPin* BranchInput = FindPin(BranchNode, EGPD_Input);
			UEdGraphPin* BranchOutput = FindPin(BranchNode, EGPD_Output);
			UEdGraphPin* DestinationInput = FindPin(DestinationNode, EGPD_Input);
			if (!SourceOutput || !BranchInput || !BranchOutput || !DestinationInput
				|| !Schema->TryCreateConnection(SourceOutput, BranchInput)
				|| !Schema->TryCreateConnection(BranchOutput, DestinationInput))
			{
				Report.Errors.Add(FText::Format(LOCTEXT("BranchWireFailed",
					"Could not wire Narrative branch '{0}' from '{1}' to '{2}'."),
					FText::FromName(BranchSpec.BranchID),
					FText::FromName(StateSpec.StateID),
					FText::FromName(BranchSpec.DestinationStateID)));
				return Report;
			}
			++Report.CreatedBranches;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(EmptyQuest);
	FKismetEditorUtilities::CompileBlueprint(EmptyQuest);
	// Narrative reads the journal title, description, tracking, and dialogue
	// settings from the generated class default object. Compilation may replace
	// that object, so apply the recipe to the final CDO as well as QuestTemplate.
	QuestClassDefaults = EmptyQuest->GeneratedClass
		? Cast<UQuest>(EmptyQuest->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!QuestClassDefaults
		|| !ApplyQuestDefaults(Recipe, QuestClassDefaults, Report))
	{
		Report.Errors.Add(LOCTEXT("CompiledQuestDefaultsMissing",
			"The compiled Narrative Quest defaults are unavailable. Quest Name and Description were not preserved."));
		return Report;
	}
	EmptyQuest->MarkPackageDirty();
	Graph->NotifyGraphChanged();
	Report.bSucceeded = EmptyQuest->Status != BS_Error;
	if (!Report.bSucceeded)
	{
		Report.Errors.Add(LOCTEXT("GeneratedQuestCompileFailed",
			"The generated Narrative Quest did not compile. Open it to inspect compiler messages."));
	}
	return Report;
}

FTerritoryQuestCascadeBuildReport
UTerritoryQuestCascadeEditorLibrary::MigrateQuestNodeConditionsToGateTasks(
	UQuestBlueprint* QuestBlueprint)
{
	using namespace TerritoryQuestCascadeEditor;
	FTerritoryQuestCascadeBuildReport Report;
	Report.QuestAsset = QuestBlueprint;
	Report.QuestPackageName = QuestBlueprint
		? QuestBlueprint->GetOutermost()->GetName() : FString();
	UQuest* Quest = QuestBlueprint ? QuestBlueprint->QuestTemplate : nullptr;
	if (!QuestBlueprint || !Quest)
	{
		Report.Errors.Add(LOCTEXT("QuestMigrationAssetRequired",
			"Select a compiled Narrative Quest Blueprint with a valid Quest Template."));
		return Report;
	}

	const FScopedTransaction Transaction(LOCTEXT("MigrateQuestConditionsTransaction",
		"Migrate Unsupported Narrative Quest Node Conditions"));
	QuestBlueprint->Modify();
	Quest->Modify();
	TSet<UQuestBranch*> ReachedBranches;

	const auto MigrateBranch = [&Report](UQuestBranch* Branch,
		const TArray<UNarrativeCondition*>& SharedStateConditions)
	{
		if (!Branch) return;
		Branch->Modify();
		const bool bNeedsGate = !SharedStateConditions.IsEmpty()
			|| !Branch->Conditions.IsEmpty();
		if (!bNeedsGate) return;

		UTerritoryNarrativeConditionTask* Gate =
			FindOrCreateConditionGate(Branch, Report);
		if (!Gate) return;
		Gate->Modify();
		for (const UNarrativeCondition* Condition : SharedStateConditions)
		{
			AddUniqueGateCondition(Condition, Gate, Report);
		}
		for (const UNarrativeCondition* Condition : Branch->Conditions)
		{
			AddUniqueGateCondition(Condition, Gate, Report);
		}
		Report.RemovedQuestNodeConditions += Branch->Conditions.Num();
		Branch->Conditions.Reset();
	};

	for (UQuestState* State : Quest->GetStates())
	{
		if (!State) continue;
		State->Modify();
		TArray<UNarrativeCondition*> SharedStateConditions;
		SharedStateConditions.Reserve(State->Conditions.Num());
		for (UNarrativeCondition* Condition : State->Conditions)
		{
			SharedStateConditions.Add(Condition);
		}
		if (!SharedStateConditions.IsEmpty() && State->Branches.IsEmpty())
		{
			Report.Warnings.Add(FText::Format(LOCTEXT("TerminalStateConditionRemoved",
				"Terminal Quest State '{0}' had {1} unsupported condition row(s). It has no outgoing route to gate, so those rows were removed."),
				FText::FromName(State->GetID()),
				FText::AsNumber(SharedStateConditions.Num())));
		}
		for (UQuestBranch* Branch : State->Branches)
		{
			ReachedBranches.Add(Branch);
			MigrateBranch(Branch, SharedStateConditions);
		}
		Report.RemovedQuestNodeConditions += State->Conditions.Num();
		State->Conditions.Reset();
	}

	// Preserve conditions from malformed/orphan branches too. The runtime summary
	// will still report the unreachable route, but no authored requirement is lost.
	for (UQuestBranch* Branch : Quest->GetBranches())
	{
		if (Branch && !ReachedBranches.Contains(Branch))
		{
			const TArray<UNarrativeCondition*> NoSharedConditions;
			MigrateBranch(Branch, NoSharedConditions);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(QuestBlueprint);
	FKismetEditorUtilities::CompileBlueprint(QuestBlueprint);
	QuestBlueprint->MarkPackageDirty();
	Report.bSucceeded = QuestBlueprint->Status != BS_Error
		&& Report.Errors.IsEmpty();
	if (!Report.bSucceeded && QuestBlueprint->Status == BS_Error)
	{
		Report.Errors.Add(LOCTEXT("MigratedQuestCompileFailed",
			"The Narrative Quest did not compile after migration. Open it and inspect the compiler messages before saving."));
	}
	return Report;
}

#undef LOCTEXT_NAMESPACE

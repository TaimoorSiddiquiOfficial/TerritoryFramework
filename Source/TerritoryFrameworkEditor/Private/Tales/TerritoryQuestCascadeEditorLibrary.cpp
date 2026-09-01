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
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"
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

	void CopyEvents(const TArray<TObjectPtr<UNarrativeEvent>>& Templates,
		UNarrativeNodeBase* Destination)
	{
		if (!Destination) return;
		Destination->Events.Reset();
		for (const UNarrativeEvent* EventTemplate : Templates)
		{
			if (EventTemplate)
			{
				Destination->Events.Add(DuplicateObject<UNarrativeEvent>(
					EventTemplate, Destination));
			}
		}
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
	FString RecipeName = Recipe->GetName();
	RecipeName.RemoveFromStart(TEXT("DA_QC_"));
	RecipeName.RemoveFromStart(TEXT("DA_"));
	return CreateQuestFromRecipe(Recipe, Folder,
		TEXT("NQ_") + RecipeName);
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
		CopyEvents(StateSpec.Events, RuntimeState);
		StateNodes.Add(StateSpec.StateID, GraphNode);
		RuntimeStates.Add(StateSpec.StateID, RuntimeState);
		++Report.CreatedStates;
	}

	EmptyQuest->QuestTemplate->SetQuestStartState(
		RuntimeStates.FindRef(Recipe->StartStateID));
	EmptyQuest->QuestTemplate->SetQuestName(Recipe->QuestName);
	EmptyQuest->QuestTemplate->SetQuestDescription(Recipe->QuestDescription);

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
			CopyEvents(BranchSpec.Events, RuntimeBranch);
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

#undef LOCTEXT_NAMESPACE

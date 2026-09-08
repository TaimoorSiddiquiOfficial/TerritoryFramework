#include "TerritoryDialogueEditorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DialogueAssetFactory.h"
#include "DialogueBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/DialogueBlueprintGeneratedClass.h"
#include "Tales/NarrativeDialogueSettings.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	UDialogueNode* RuntimeDialogueNode(UEdGraphNode* Node)
	{
		const FObjectPropertyBase* Property = Node ? FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("DialogueNode")) : nullptr;
		return Property ? Cast<UDialogueNode>(Property->GetObjectPropertyValue_InContainer(Node)) : nullptr;
	}
}

bool UTerritoryDialogueEditorLibrary::AddDialogueReply(UDialogueBlueprint* Dialogue,
	FName FromID, const FTerritoryDialogueRecipeNode& Reply, FText& OutError)
{
	OutError = FText::GetEmpty();
	UEdGraph* Graph = Dialogue ? Dialogue->DialogueGraph : nullptr;
	UDialogue* Template = Dialogue ? Dialogue->DialogueTemplate : nullptr;
	const UDialogue* Defaults = Dialogue && Dialogue->GeneratedClass
		? Cast<UDialogue>(Dialogue->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Graph || !Graph->GetSchema() || !Template || !Defaults || Reply.ID.IsNone()
		|| !Reply.Replies.IsEmpty() || Reply.Position.ContainsNaN()
		|| FMath::Abs(Reply.Position.X) > 1000000.0 || FMath::Abs(Reply.Position.Y) > 1000000.0
		|| (!Reply.bPlayer && !Defaults->Speakers.ContainsByPredicate([&](const FSpeakerInfo& Speaker)
			{ return Speaker.SpeakerID == Reply.SpeakerID; })))
	{
		OutError = FText::FromString(TEXT("Choose an existing dialogue, a new reply ID, a configured speaker and valid coordinates. Add outgoing links separately."));
		return false;
	}
	UEdGraphPin* From = nullptr;
	for (UEdGraphNode* Existing : Graph->Nodes)
	{
		UDialogueNode* Runtime = RuntimeDialogueNode(Existing);
		if (!Runtime) continue;
		if (Runtime->GetID() == Reply.ID)
		{
			OutError = FText::FromString(TEXT("This dialogue already contains that reply ID."));
			return false;
		}
		if (Runtime->GetID() == FromID)
			for (UEdGraphPin* Pin : Existing->Pins) if (Pin->Direction == EGPD_Output) From = Pin;
	}
	for (UDialogueNode* Existing : Template->GetNodes())
		if (Existing && Existing->GetID() == Reply.ID)
		{
			OutError = FText::FromString(TEXT("The runtime template already contains that reply ID."));
			return false;
		}
	UClass* NodeClass = LoadClass<UEdGraphNode>(nullptr, Reply.bPlayer
		? TEXT("/Script/NarrativeDialogueEditor.DialogueGraphNode_Player")
		: TEXT("/Script/NarrativeDialogueEditor.DialogueGraphNode_NPC"));
	FObjectPropertyBase* Binding = NodeClass ? FindFProperty<FObjectPropertyBase>(NodeClass, TEXT("DialogueNode")) : nullptr;
	if (!From || !Binding || Reply.Conditions.Contains(nullptr) || Reply.Events.Contains(nullptr))
	{
		OutError = FText::FromString(TEXT("The parent node, Native graph binding, conditions or events are invalid."));
		return false;
	}
	UEdGraphNode* GraphNode = NewObject<UEdGraphNode>(Graph, NodeClass, NAME_None, RF_Transactional);
	GraphNode->AllocateDefaultPins();
	UEdGraphPin* To = nullptr;
	for (UEdGraphPin* Pin : GraphNode->Pins) if (Pin->Direction == EGPD_Input) To = Pin;
	if (!To || Graph->GetSchema()->CanCreateConnection(From, To).Response != CONNECT_RESPONSE_MAKE)
	{
		OutError = FText::FromString(TEXT("Narrative does not allow this reply connection."));
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("TerritoryDialogue", "AddReply", "Add Territory Dialogue Reply"));
	Dialogue->Modify(); Graph->Modify(); Template->Modify(); From->GetOwningNode()->Modify();
	UDialogueNode* Runtime = Reply.bPlayer
		? static_cast<UDialogueNode*>(NewObject<UDialogueNode_Player>(Template, NAME_None, RF_Transactional))
		: static_cast<UDialogueNode*>(NewObject<UDialogueNode_NPC>(Template, NAME_None, RF_Transactional));
	Runtime->SetID(Reply.ID); Runtime->Line.Text = Reply.Text;
	for (UNarrativeCondition* Condition : Reply.Conditions) Runtime->Conditions.Add(DuplicateObject(Condition, Runtime));
	for (UNarrativeEvent* Event : Reply.Events) Runtime->Events.Add(DuplicateObject(Event, Runtime));
	if (auto* NPC = Cast<UDialogueNode_NPC>(Runtime))
	{
		NPC->SetSpeakerID(Reply.SpeakerID); Template->NPCReplies.Add(NPC);
	}
	else Template->PlayerReplies.Add(CastChecked<UDialogueNode_Player>(Runtime));
	Binding->SetObjectPropertyValue_InContainer(GraphNode, Runtime);
	GraphNode->CreateNewGuid();
	Graph->AddNode(GraphNode, false, false);
	Graph->GetSchema()->SetNodePosition(GraphNode, FVector2f(Reply.Position));
	if (!Graph->GetSchema()->TryCreateConnection(From, To))
	{
		Graph->RemoveNode(GraphNode);
		Template->NPCReplies.Remove(Cast<UDialogueNode_NPC>(Runtime));
		Template->PlayerReplies.Remove(Cast<UDialogueNode_Player>(Runtime));
		OutError = FText::FromString(TEXT("Narrative rejected the reply. The new node was removed."));
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Dialogue);
	return true;
}

bool UTerritoryDialogueEditorLibrary::ConnectDialogueReply(UDialogueBlueprint* Dialogue,
	FName FromID, FName ToID, FText& OutError)
{
	OutError = FText::GetEmpty();
	UEdGraph* Graph = Dialogue ? Dialogue->DialogueGraph : nullptr;
	if (!Graph || !Graph->GetSchema() || FromID == ToID)
	{
		OutError = FText::FromString(TEXT("Choose an existing Native dialogue and two different node IDs."));
		return false;
	}
	UEdGraphPin* From = nullptr;
	UEdGraphPin* To = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UDialogueNode* Runtime = RuntimeDialogueNode(Node);
		if (!Runtime) continue;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Runtime->GetID() == FromID && Pin->Direction == EGPD_Output) From = Pin;
			if (Runtime->GetID() == ToID && Pin->Direction == EGPD_Input) To = Pin;
		}
	}
	if (!From || !To || !Graph->GetSchema()->TryCreateConnection(From, To))
	{
		OutError = FText::FromString(TEXT("Narrative rejected the connection, or a node ID was not found."));
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Dialogue);
	return true;
}

bool UTerritoryDialogueEditorLibrary::SetDialogueNodePosition(UDialogueBlueprint* Dialogue,
	FName NodeID, FVector2D Position, FText& OutError)
{
	OutError = FText::GetEmpty();
	UEdGraph* Graph = Dialogue ? Dialogue->DialogueGraph : nullptr;
	if (Graph && Graph->GetSchema() && !Position.ContainsNaN()
		&& FMath::Abs(Position.X) <= 1000000.0 && FMath::Abs(Position.Y) <= 1000000.0)
	{
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UDialogueNode* Node = RuntimeDialogueNode(GraphNode);
			if (Node && Node->GetID() == NodeID)
			{
				const FScopedTransaction Transaction(NSLOCTEXT("TerritoryDialogue", "Move", "Move Territory Dialogue Node"));
				Dialogue->Modify(); Graph->Modify(); Node->Modify(); GraphNode->Modify();
				Graph->GetSchema()->SetNodePosition(GraphNode, FVector2f(Position));
				FBlueprintEditorUtils::MarkBlueprintAsModified(Dialogue);
				return true;
			}
		}
	}
	OutError = FText::FromString(TEXT("Choose an existing node ID and finite coordinates within one million units."));
	return false;
}

bool UTerritoryDialogueEditorLibrary::ArrangeDialogue(UDialogueBlueprint* Dialogue, FText& OutError)
{
	OutError = FText::GetEmpty();
	UEdGraph* Graph = Dialogue ? Dialogue->DialogueGraph : nullptr;
	UDialogue* Template = Dialogue ? Dialogue->DialogueTemplate : nullptr;
	if (!Graph || !Graph->GetSchema() || !Template || !Template->RootDialogue)
	{
		OutError = FText::FromString(TEXT("A Native dialogue graph and root are required."));
		return false;
	}
	const bool bVertical = GetDefault<UNarrativeDialogueSettings>()->bEnableVerticalWiring;
	const auto Priority = [bVertical](const FVector2D& Position) { return bVertical ? Position.X : Position.Y; };
	TArray<UDialogueNode*> Nodes;
	TMap<UDialogueNode*, UEdGraphNode*> GraphNodes;
	TMap<UDialogueNode*, FVector2D> Original;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (UDialogueNode* Node = RuntimeDialogueNode(GraphNode))
		{
			Nodes.Add(Node);
			GraphNodes.Add(Node, GraphNode);
			Original.Add(Node, Node->NodePos);
		}
	}
	if (Nodes.IsEmpty() || Nodes.Num() > 2000)
	{
		OutError = FText::FromString(TEXT("The dialogue must contain between 1 and 2000 nodes."));
		return false;
	}
	Nodes.StableSort([&](const UDialogueNode& A, const UDialogueNode& B)
	{
		return Priority(A.NodePos) < Priority(B.NodePos);
	});
	TMap<UDialogueNode*, int32> Depth;
	TArray<UDialogueNode*> Queue = {Template->RootDialogue};
	Depth.Add(Template->RootDialogue, 0);
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		UDialogueNode* Parent = Queue[Index];
		const auto Visit = [&](UDialogueNode* Child)
		{
			if (GraphNodes.Contains(Child) && !Depth.Contains(Child))
			{
				Depth.Add(Child, Depth[Parent] + 1);
				Queue.Add(Child);
			}
		};
		for (UDialogueNode* Child : Parent->NPCReplies) Visit(Child);
		for (UDialogueNode* Child : Parent->PlayerReplies) Visit(Child);
	}
	int32 LastRow = 0;
	for (const auto& Pair : Depth) LastRow = FMath::Max(LastRow, Pair.Value);
	TMap<int32, int32> Columns;
	TMap<UDialogueNode*, FVector2D> Positions;
	for (UDialogueNode* Node : Nodes)
	{
		const int32 Row = Depth.Contains(Node) ? Depth[Node] : LastRow + 1;
		const int32 Column = Columns.FindOrAdd(Row)++;
		Positions.Add(Node, bVertical ? FVector2D(Column * 640, Row * 960) : FVector2D(Row * 960, Column * 640));
	}
	const auto KeepsPriority = [&]()
	{
		for (UDialogueNode* Parent : Nodes)
		{
			const auto SameOrder = [&](const auto& Replies)
			{
				for (int32 A = 0; A < Replies.Num(); ++A)
					for (int32 B = A + 1; B < Replies.Num(); ++B)
					{
						UDialogueNode* Left = Replies[A]; UDialogueNode* Right = Replies[B];
						if (!Original.Contains(Left) || !Original.Contains(Right)) return false;
						if (FMath::Sign(Priority(Original[Left]) - Priority(Original[Right]))
							!= FMath::Sign(Priority(Positions[Left]) - Priority(Positions[Right]))) return false;
					}
				return true;
			};
			if (!SameOrder(Parent->NPCReplies) || !SameOrder(Parent->PlayerReplies)) return false;
		}
		return true;
	};
	if (!KeepsPriority())
	{
		// Shared replies can occupy different depths. Keep a global priority column
		// in that case; compressing each row would silently change branch selection.
		int32 Column = -1;
		double PreviousPriority = -TNumericLimits<double>::Max();
		for (UDialogueNode* Node : Nodes)
		{
			const double Current = Priority(Original[Node]);
			if (Current != PreviousPriority) ++Column;
			PreviousPriority = Current;
			if (bVertical) Positions[Node].X = Column * 640; else Positions[Node].Y = Column * 640;
		}
	}
	if (!KeepsPriority())
	{
		OutError = FText::FromString(TEXT("The graph contains a missing runtime reply. Repair its links before arranging it."));
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("TerritoryDialogue", "Arrange", "Arrange Territory Dialogue"));
	Dialogue->Modify(); Graph->Modify();
	for (UDialogueNode* Node : Nodes)
	{
		Node->Modify(); GraphNodes[Node]->Modify();
		Graph->GetSchema()->SetNodePosition(GraphNodes[Node], FVector2f(Positions[Node]));
	}
	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Dialogue);
	FKismetEditorUtilities::CompileBlueprint(Dialogue);
	if (Dialogue->Status == BS_Error)
	{
		OutError = FText::FromString(TEXT("The arranged dialogue has a compile error. It has not been saved."));
		return false;
	}
	return true;
}

UDialogueBlueprint* UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(
	UTerritoryDialogueRecipe* Recipe, const FString& NewAssetPath, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!Recipe || !Recipe->ValidateRecipe(OutError)) return nullptr;
	// Native Blueprint compilation may collect unreferenced transient objects.
	// The recipe must survive until the post-compile node-count check finishes.
	const TStrongObjectPtr<UTerritoryDialogueRecipe> KeepRecipe(Recipe);
	if (!NewAssetPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(NewAssetPath)
		|| FPackageName::DoesPackageExist(NewAssetPath) || FindPackage(nullptr, *NewAssetPath))
	{
		OutError = NSLOCTEXT("TerritoryDialogueBuilder", "NewProjectAssetRequired", "Choose a new /Game/ asset path. Existing dialogues are never overwritten.");
		return nullptr;
	}
	// Native graph classes are reflected but not exported C++ API. Instantiate
	// their existing classes and invoke public UEdGraph virtuals; do not copy them.
	UClass* GraphClass = LoadClass<UEdGraph>(nullptr, TEXT("/Script/NarrativeDialogueEditor.DialogueGraph"));
	UClass* SchemaClass = LoadClass<UEdGraphSchema>(nullptr, TEXT("/Script/NarrativeDialogueEditor.DialogueGraphSchema"));
	UClass* RootClass = LoadClass<UEdGraphNode>(nullptr, TEXT("/Script/NarrativeDialogueEditor.DialogueGraphNode_Root"));
	UClass* NPCClass = LoadClass<UEdGraphNode>(nullptr, TEXT("/Script/NarrativeDialogueEditor.DialogueGraphNode_NPC"));
	UClass* PlayerClass = LoadClass<UEdGraphNode>(nullptr, TEXT("/Script/NarrativeDialogueEditor.DialogueGraphNode_Player"));
	FObjectPropertyBase* NodeProperty = NPCClass ? FindFProperty<FObjectPropertyBase>(NPCClass, TEXT("DialogueNode")) : nullptr;
	if (!GraphClass || !SchemaClass || !RootClass || !NPCClass || !PlayerClass || !NodeProperty)
	{
		OutError = NSLOCTEXT("TerritoryDialogueBuilder", "NativeContractUnavailable", "The installed Narrative Dialogue editor graph contract is unavailable.");
		return nullptr;
	}
	UPackage* Package = CreatePackage(*NewAssetPath);
	UDialogueAssetFactory* Factory = NewObject<UDialogueAssetFactory>();
	UDialogueBlueprint* BP = Cast<UDialogueBlueprint>(Factory->FactoryCreateNew(UDialogueBlueprint::StaticClass(),
		Package, *FPackageName::GetLongPackageAssetName(NewAssetPath), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
	if (!BP || !BP->DialogueTemplate || !BP->GeneratedClass)
	{
		OutError = NSLOCTEXT("TerritoryDialogueBuilder", "NativeFactoryFailed", "Narrative could not create the Dialogue Blueprint.");
		return nullptr;
	}
	UDialogue* Template = BP->DialogueTemplate;
	BP->DialogueGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("DialogueGraph"), GraphClass, SchemaClass);
	UDialogue* CDO = CastChecked<UDialogue>(BP->GeneratedClass->GetDefaultObject());
	CDO->Speakers.Reset();
	for (UNPCDefinition* Definition : Recipe->Speakers)
	{
		FSpeakerInfo Speaker;
		Speaker.NPCDataAsset = Definition;
		Speaker.SpeakerID = Definition->NPCID;
		CDO->Speakers.Add(Speaker);
	}
	TMap<FName, UDialogueNode*> RuntimeNodes;
	TMap<FName, UEdGraphNode*> GraphNodes;
	Template->NPCReplies.Reset();
	Template->PlayerReplies.Reset();
	for (const auto& Row : Recipe->Nodes)
	{
		UDialogueNode* Node = Row.bPlayer
			? static_cast<UDialogueNode*>(NewObject<UDialogueNode_Player>(Template, Row.ID, RF_Transactional))
			: static_cast<UDialogueNode*>(NewObject<UDialogueNode_NPC>(Template, Row.ID, RF_Transactional));
		if (auto* NPC = Cast<UDialogueNode_NPC>(Node))
		{
			NPC->SetSpeakerID(Row.SpeakerID);
			Template->NPCReplies.Add(NPC);
			if (Row.ID == Recipe->RootID) Template->RootDialogue = NPC;
		}
		else Template->PlayerReplies.Add(CastChecked<UDialogueNode_Player>(Node));
		Node->SetID(Row.ID);
		Node->Line.Text = Row.Text;
		Node->NodePos = Row.Position;
		for (UNarrativeCondition* Condition : Row.Conditions) Node->Conditions.Add(DuplicateObject(Condition, Node));
		for (UNarrativeEvent* Event : Row.Events) Node->Events.Add(DuplicateObject(Event, Node));
		UClass* NodeClass = Row.bPlayer ? PlayerClass : Row.ID == Recipe->RootID ? RootClass : NPCClass;
		UEdGraphNode* GraphNode = NewObject<UEdGraphNode>(BP->DialogueGraph, NodeClass, NAME_None, RF_Transactional);
		NodeProperty->SetObjectPropertyValue_InContainer(GraphNode, Node);
		GraphNode->CreateNewGuid();
		GraphNode->NodePosX = FMath::RoundToInt(Row.Position.X);
		GraphNode->NodePosY = FMath::RoundToInt(Row.Position.Y);
		GraphNode->AllocateDefaultPins();
		BP->DialogueGraph->AddNode(GraphNode, false, false);
		RuntimeNodes.Add(Row.ID, Node);
		GraphNodes.Add(Row.ID, GraphNode);
	}
	const auto Pin = [](UEdGraphNode* Node, EEdGraphPinDirection Direction) -> UEdGraphPin*
	{
		for (UEdGraphPin* Candidate : Node->Pins) if (Candidate->Direction == Direction) return Candidate;
		return nullptr;
	};
	for (const auto& Row : Recipe->Nodes)
	{
		for (FName ReplyID : Row.Replies)
		{
			UDialogueNode* Reply = RuntimeNodes[ReplyID];
			if (auto* NPC = Cast<UDialogueNode_NPC>(Reply)) RuntimeNodes[Row.ID]->NPCReplies.Add(NPC);
			else RuntimeNodes[Row.ID]->PlayerReplies.Add(CastChecked<UDialogueNode_Player>(Reply));
			UEdGraphPin* From = Pin(GraphNodes[Row.ID], EGPD_Output);
			UEdGraphPin* To = Pin(GraphNodes[ReplyID], EGPD_Input);
			if (!From || !To)
			{
				OutError = NSLOCTEXT("TerritoryDialogueBuilder", "MissingNativePin", "A Native dialogue graph node has no expected input/output pin. The unsaved asset needs review.");
				return nullptr;
			}
			From->MakeLinkTo(To);
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	const auto* Generated = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass);
	const UDialogue* Compiled = Generated ? Generated->GetDialogueTemplate() : nullptr;
	if (BP->Status == BS_Error || !Compiled || !Compiled->RootDialogue
		|| Compiled->GetNodes().Num() != Recipe->Nodes.Num())
	{
		OutError = NSLOCTEXT("TerritoryDialogueBuilder", "CompilationFailed", "Native dialogue compilation or node preservation failed. The unsaved asset needs review.");
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(BP);
	BP->MarkPackageDirty();
	return BP;
}

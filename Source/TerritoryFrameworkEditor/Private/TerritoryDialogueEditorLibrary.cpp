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
#include "UObject/UnrealType.h"

UDialogueBlueprint* UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(
	UTerritoryDialogueRecipe* Recipe, const FString& NewAssetPath, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!Recipe || !Recipe->ValidateRecipe(OutError)) return nullptr;
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

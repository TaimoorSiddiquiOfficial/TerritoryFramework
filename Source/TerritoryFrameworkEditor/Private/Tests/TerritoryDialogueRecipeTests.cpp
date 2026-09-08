#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryDialogueEditorLibrary.h"
#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/TerritorySituationCondition.h"
#include "Tales/DialogueBlueprintGeneratedClass.h"
#include "Tales/TalesComponent.h"
#include "DialogueBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDialogueRecipeBuilder,
	"TerritoryFramework.Dialogue.Editor.NativeRecipeBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDialogueRecipeBuilder::RunTest(const FString& Parameters)
{
	auto* Recipe = NewObject<UTerritoryDialogueRecipe>();
	const TStrongObjectPtr<UTerritoryDialogueRecipe> KeepRecipe(Recipe);
	auto* Speaker = NewObject<UNPCDefinition>(Recipe);
	Speaker->NPCID = TEXT("RetakeTestOwner");
	Recipe->Speakers.Add(Speaker);
	Recipe->RootID = TEXT("Entry");
	FTerritoryDialogueRecipeNode Entry;
	Entry.ID = Recipe->RootID;
	Entry.Replies = {TEXT("Offer")};
	FTerritoryDialogueRecipeNode Offer;
	Offer.ID = TEXT("Offer");
	Offer.SpeakerID = Speaker->NPCID;
	Offer.Text = FText::FromString(TEXT("We can discuss the Place."));
	Offer.Position = FVector2D(500, 500);
	Offer.Replies = {TEXT("Leave")};
	FTerritoryDialogueRecipeNode Leave;
	Leave.ID = TEXT("Leave");
	Leave.bPlayer = true;
	Leave.Text = FText::FromString(TEXT("I will return."));
	Leave.Position = FVector2D(1000, 1000);
	Recipe->Nodes = {Entry, Offer, Leave};
	FText Error;
	TestTrue(TEXT("Valid Native recipe"), Recipe->ValidateRecipe(Error));
	for (double InvalidPosition : {static_cast<double>(MAX_int32) + 1., static_cast<double>(MIN_int32) - 1.})
	{
		Recipe->Nodes[1].Position.X = InvalidPosition;
		TestFalse(TEXT("Finite out-of-range graph coordinates fail before creation"), Recipe->ValidateRecipe(Error));
	}
	Recipe->Nodes[1].Position.X = MAX_int32;
	TestTrue(TEXT("Exact graph coordinate boundary remains valid"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[1].Position.X = Offer.Position.X;
	Recipe->Nodes[2].Replies = {TEXT("Missing")};
	TestFalse(TEXT("Missing references are rejected before asset creation"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[2].Replies.Reset();
	Recipe->Nodes[1].Replies.Add(TEXT("Entry"));
	TestFalse(TEXT("Replies cannot enter the Native root"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[1].Replies.Pop();
	Recipe->Nodes[1].Replies.Add(TEXT("Offer"));
	TestFalse(TEXT("NPC-only cycles cannot hang Native chunk generation"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[1].Replies.Pop();
	const FString Path = TEXT("/Game/Developers/TerritoryAutomation/DBP_Recipe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UDialogueBlueprint* BP = UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, Path, Error);
	if (!TestNotNull(*FString::Printf(TEXT("Native blueprint built: %s"), *Error.ToString()), BP)) return false;
	const auto* Generated = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass);
	const auto* Template = Generated ? Generated->GetDialogueTemplate() : nullptr;
	TestNotNull(TEXT("Native generated runtime template exists"), Template);
	if (Template)
	{
		TestEqual(TEXT("All runtime nodes survive compilation"), Template->GetNodes().Num(), 3);
		TestEqual(TEXT("Stable root ID survives compilation"), Template->RootDialogue->GetID(), Recipe->RootID);
		TestEqual(TEXT("NPC to player link survives compilation"), Template->RootDialogue->NPCReplies[0]->PlayerReplies.Num(), 1);
	}
	TestEqual(TEXT("Editable Native graph has all three nodes"), BP->DialogueGraph->Nodes.Num(), 3);
	int32 Links = 0;
	for (const UEdGraphNode* Node : BP->DialogueGraph->Nodes)
		for (const UEdGraphPin* Pin : Node->Pins) if (Pin->Direction == EGPD_Output) Links += Pin->LinkedTo.Num();
	TestEqual(TEXT("Visible editor wires match runtime links"), Links, 2);
	TestTrue(TEXT("A known node moves through the Native schema"),
		UTerritoryDialogueEditorLibrary::SetDialogueNodePosition(BP, Offer.ID, FVector2D(768, 512), Error));
	TestFalse(TEXT("Out-of-range node coordinates are rejected"),
		UTerritoryDialogueEditorLibrary::SetDialogueNodePosition(BP, Offer.ID, FVector2D(1.e20, 512), Error));
	TestTrue(TEXT("Existing Native graph can be arranged"), UTerritoryDialogueEditorLibrary::ArrangeDialogue(BP, Error));
	TestEqual(TEXT("Arranging keeps every runtime node"), BP->DialogueTemplate->GetNodes().Num(), 3);
	UDialogueNode_NPC* FirstReply = BP->DialogueTemplate->RootDialogue->GetFirstValidNPCReply(
		nullptr, nullptr, NewObject<UTalesComponent>());
	if (TestNotNull(TEXT("Arranged Native graph still selects a reply"), FirstReply))
		TestEqual(TEXT("Arranging keeps the root's first valid reply"), FirstReply->GetID(), Offer.ID);
	for (UEdGraphNode* Node : BP->DialogueGraph->Nodes)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("DialogueNode"));
		const UDialogueNode* Runtime = Property ? Cast<UDialogueNode>(Property->GetObjectPropertyValue_InContainer(Node)) : nullptr;
		if (TestNotNull(TEXT("Native graph runtime node binding"), Runtime))
			TestEqual(TEXT("Native schema persists layout in both graph and runtime"), Runtime->NodePos,
				FVector2D(Node->NodePosX, Node->NodePosY));
	}
	TestFalse(TEXT("A missing reply is rejected without inventing a node"),
		UTerritoryDialogueEditorLibrary::ConnectDialogueReply(BP, TEXT("Missing"), Offer.ID, Error));
	FTerritoryDialogueRecipeNode NewReply = Offer;
	NewReply.ID = TEXT("Reinforcements"); NewReply.Replies.Reset(); NewReply.Position = FVector2D(2000, 960);
	TestFalse(TEXT("Adding a reply requires an existing parent"),
		UTerritoryDialogueEditorLibrary::AddDialogueReply(BP, TEXT("Missing"), NewReply, Error));
	TestTrue(TEXT("Native reply enters both graph and runtime template"),
		UTerritoryDialogueEditorLibrary::AddDialogueReply(BP, Entry.ID, NewReply, Error));
	TestFalse(TEXT("Repeated authoring cannot duplicate a reply ID"),
		UTerritoryDialogueEditorLibrary::AddDialogueReply(BP, Entry.ID, NewReply, Error));
	TestTrue(TEXT("Added reply compiles and arranges"), UTerritoryDialogueEditorLibrary::ArrangeDialogue(BP, Error));
	const auto* UpdatedClass = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass);
	const auto* UpdatedTemplate = UpdatedClass ? UpdatedClass->GetDialogueTemplate() : nullptr;
	if (TestNotNull(TEXT("Updated compiled Native template"), UpdatedTemplate))
	{
		TestEqual(TEXT("New reply survives compilation"), UpdatedTemplate->GetNodes().Num(), 4);
		TestEqual(TEXT("New reply has a live root connection"), UpdatedTemplate->RootDialogue->NPCReplies.Num(), 2);
	}
	auto* Profile = NewObject<UTerritoryDiplomacyDialogueProfile>();
	Profile->WarDialogue = BP->GeneratedClass;
	auto* Validator = NewObject<UTerritoryDataValidator>();
	const TArray<FAssetData> Associated;
	FDataValidationContext ValidationContext(false, EDataValidationUsecase::Script, Associated);
	TestEqual(TEXT("Diplomacy accepts the compiled Native template without requiring a CDO graph"),
		Validator->ValidateLoadedAsset_Implementation(FAssetData(Profile), Profile, ValidationContext), EDataValidationResult::Valid);
	TestNull(TEXT("Builder cannot overwrite a live dialogue"), UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, Path, Error));
	TestNull(TEXT("Builder rejects the Narrative vendor mount"), UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, TEXT("/NarrativePro/ForbiddenRecipe"), Error));
	FAssetRegistryModule::AssetDeleted(BP);
	BP->GetOutermost()->SetDirtyFlag(false);
	BP->ClearFlags(RF_Public | RF_Standalone);
	BP->SetFlags(RF_Transient);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryDialogueEditorLibrary.h"
#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/TerritorySituationCondition.h"
#include "Tales/DialogueBlueprintGeneratedClass.h"
#include "DialogueBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDialogueRecipeBuilder,
	"TerritoryFramework.Dialogue.Editor.NativeRecipeBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDialogueRecipeBuilder::RunTest(const FString& Parameters)
{
	auto* Recipe = NewObject<UTerritoryDialogueRecipe>();
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
	TestNull(TEXT("Builder cannot overwrite a live dialogue"), UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, Path, Error));
	TestNull(TEXT("Builder rejects the Narrative vendor mount"), UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, TEXT("/NarrativePro/ForbiddenRecipe"), Error));
	FAssetRegistryModule::AssetDeleted(BP);
	BP->GetOutermost()->SetDirtyFlag(false);
	BP->ClearFlags(RF_Public | RF_Standalone);
	BP->SetFlags(RF_Transient);
	return true;
}
#endif

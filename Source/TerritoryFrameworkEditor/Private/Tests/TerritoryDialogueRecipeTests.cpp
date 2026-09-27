#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryDialogueEditorLibrary.h"
#include "Tales/TerritoryDialogueRecipe.h"
#include "Tales/TerritoryAmbientDialogue.h"
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
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDialogueRecipeBuilder,
	"TerritoryFramework.Dialogue.Editor.NativeRecipeBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDialogueRecipeBuilder::RunTest(const FString& Parameters)
{
	auto* Recipe = NewObject<UTerritoryDialogueRecipe>();
	const TStrongObjectPtr<UTerritoryDialogueRecipe> KeepRecipe(Recipe);
	TestEqual(TEXT("Existing recipes default to ordinary Native dialogue"), Recipe->ParentDialogueClass.Get(), UDialogue::StaticClass());
	TestFalse(TEXT("Ordinary recipes do not opt into meeting roles"), Recipe->UsesAmbientRoles());
	// Use a public top-level asset, matching real NPC Definition authoring. Blueprint
	// reinstancing deliberately clears private cross-package UObject references.
	const FString SpeakerName = TEXT("NPC_Recipe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString SpeakerPath = TEXT("/Game/Developers/TerritoryAutomation/") + SpeakerName;
	UPackage* SpeakerPackage = CreatePackage(*SpeakerPath);
	auto* Speaker = NewObject<UNPCDefinition>(SpeakerPackage, *SpeakerName, RF_Public);
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
	auto* PrivateSpeaker = NewObject<UNPCDefinition>(Recipe);
	PrivateSpeaker->NPCID = Speaker->NPCID;
	Recipe->Speakers[0] = PrivateSpeaker;
	TestFalse(TEXT("Private cross-package speakers fail before Native compilation can clear them"), Recipe->ValidateRecipe(Error));
	Recipe->Speakers[0] = Speaker;
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
	TestEqual(TEXT("Ordinary recipe preserves the Native parent"), BP->ParentClass.Get(), UDialogue::StaticClass());
	const UDialogue* Defaults = Cast<UDialogue>(BP->GeneratedClass->GetDefaultObject());
	if (TestNotNull(TEXT("Ordinary dialogue defaults exist"), Defaults) && TestEqual(TEXT("Ordinary speaker count"), Defaults->Speakers.Num(), 1))
		TestTrue(TEXT("Ordinary dialogue keeps its configured NPC Definition"), Defaults->Speakers[0].NPCDataAsset == Speaker);
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
	const UDialogue* UpdatedDefaults = UpdatedClass ? Cast<UDialogue>(UpdatedClass->GetDefaultObject()) : nullptr;
	if (TestNotNull(TEXT("Recompiled ordinary defaults"), UpdatedDefaults) && TestEqual(TEXT("Recompiled speaker count"), UpdatedDefaults->Speakers.Num(), 1))
		TestTrue(TEXT("NPC asset identity survives later editor recompilation"), UpdatedDefaults->Speakers[0].NPCDataAsset == Speaker);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDialogueAmbientRecipeBuilder,
	"TerritoryFramework.Dialogue.Editor.AmbientRecipeBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDialogueAmbientRecipeBuilder::RunTest(const FString& Parameters)
{
	auto* Recipe = NewObject<UTerritoryDialogueRecipe>();
	const TStrongObjectPtr<UTerritoryDialogueRecipe> KeepRecipe(Recipe);
	Recipe->ParentDialogueClass = UTerritoryAmbientDialogue::StaticClass();
	Recipe->FirstRoleID = TEXT("Watcher");
	Recipe->SecondRoleID = TEXT("Partner");
	Recipe->RootID = TEXT("Entry");
	FTerritoryDialogueRecipeNode Entry;
	Entry.ID = Recipe->RootID; Entry.Replies = {TEXT("Ask")};
	FTerritoryDialogueRecipeNode Ask;
	Ask.ID = TEXT("Ask"); Ask.SpeakerID = Recipe->FirstRoleID;
	Ask.Text = FText::FromString(TEXT("How is the road?"));
	Ask.Position = FVector2D(500, 500); Ask.Replies = {TEXT("Answer")};
	FTerritoryDialogueRecipeNode Answer;
	Answer.ID = TEXT("Answer"); Answer.SpeakerID = Recipe->SecondRoleID;
	Answer.Text = FText::FromString(TEXT("Quiet since morning."));
	Answer.Position = FVector2D(1000, 1000);
	Recipe->Nodes = {Entry, Ask, Answer};
	FText Error;
	TestTrue(TEXT("Ambient mode follows the selected parent"), Recipe->UsesAmbientRoles());
	TestTrue(TEXT("Two explicit meeting roles form a valid recipe"), Recipe->ValidateRecipe(Error));
	Recipe->FirstRoleID = NAME_None;
	TestFalse(TEXT("An empty role is rejected"), Recipe->ValidateRecipe(Error));
	Recipe->FirstRoleID = Recipe->SecondRoleID;
	TestFalse(TEXT("Duplicate role IDs are rejected"), Recipe->ValidateRecipe(Error));
	Recipe->FirstRoleID = GetDefault<UTerritoryAmbientDialogue>()->PlayerSpeakerInfo.GetSpeakerID();
	TestFalse(TEXT("A recruited actor cannot become the player speaker"), Recipe->ValidateRecipe(Error));
	Recipe->FirstRoleID = Ask.SpeakerID;
	Recipe->Nodes[1].SpeakerID = TEXT("UnknownRole");
	TestFalse(TEXT("Unbound role IDs are rejected"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[1].SpeakerID = Ask.SpeakerID;
	Recipe->Nodes[2].SpeakerID = Ask.SpeakerID;
	TestFalse(TEXT("Both roles must speak"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[2].SpeakerID = Answer.SpeakerID;
	Recipe->Speakers.Add(NewObject<UNPCDefinition>(Recipe));
	TestFalse(TEXT("NPC Definition speakers cannot be mixed with recruited roles"), Recipe->ValidateRecipe(Error));
	TestTrue(TEXT("Stale NPC definitions remain editable so a changed recipe can be repaired"),
		Recipe->CanEditChange(FindFProperty<FProperty>(Recipe->GetClass(), GET_MEMBER_NAME_CHECKED(UTerritoryDialogueRecipe, Speakers))));
	Recipe->Speakers.Reset();
	Recipe->Nodes[2].bPlayer = true;
	const FString RejectedPath = TEXT("/Game/Developers/TerritoryAutomation/DBP_InvalidAmbient_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestNull(TEXT("Player choices fail before asset creation"), UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, RejectedPath, Error));
	TestNull(TEXT("Rejected recipes leave no asset package"), FindPackage(nullptr, *RejectedPath));
	Recipe->Nodes[2].bPlayer = false;
	Recipe->Nodes[2].Replies = {Ask.ID};
	TestFalse(TEXT("Ambient NPC cycles are rejected"), Recipe->ValidateRecipe(Error));
	Recipe->Nodes[2].Replies.Reset();
	const FString Path = TEXT("/Game/Developers/TerritoryAutomation/DBP_Ambient_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UDialogueBlueprint* BP = UTerritoryDialogueEditorLibrary::CreateDialogueFromRecipe(Recipe, Path, Error);
	if (!TestNotNull(*FString::Printf(TEXT("Ambient Blueprint built: %s"), *Error.ToString()), BP)) return false;
	const TStrongObjectPtr<UDialogueBlueprint> KeepBP(BP);
	TestEqual(TEXT("Ambient recipe selects the specialized parent"), BP->ParentClass.Get(), UTerritoryAmbientDialogue::StaticClass());
	const auto CheckCompiledContract = [&]()
	{
		const auto* Generated = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass);
		const auto* Defaults = Generated ? Cast<UTerritoryAmbientDialogue>(Generated->GetDefaultObject()) : nullptr;
		const UDialogue* Compiled = Generated ? Generated->GetDialogueTemplate() : nullptr;
		if (TestNotNull(TEXT("Specialized defaults survive Native compilation"), Defaults))
		{
			TestEqual(TEXT("First custom role survives compilation"), Defaults->FirstRoleID, Recipe->FirstRoleID);
			TestEqual(TEXT("Second custom role survives compilation"), Defaults->SecondRoleID, Recipe->SecondRoleID);
			if (TestEqual(TEXT("Exactly two recruited speakers"), Defaults->Speakers.Num(), 2))
			{
				TestEqual(TEXT("First speaker uses its role ID"), Defaults->Speakers[0].GetSpeakerID(), Recipe->FirstRoleID);
				TestEqual(TEXT("Second speaker uses its role ID"), Defaults->Speakers[1].GetSpeakerID(), Recipe->SecondRoleID);
				for (const FSpeakerInfo& Speaker : Defaults->Speakers)
				{
					TestNull(TEXT("Role speaker has no NPC Definition identity override"), Speaker.NPCDataAsset.Get());
					TestFalse(TEXT("Role speaker is not a player"), Speaker.bIsPlayer);
				}
			}
		}
		if (TestNotNull(TEXT("Native compiled template exists"), Compiled))
		{
			TestEqual(TEXT("Native still owns a base graph template"), Compiled->GetClass(), UDialogue::StaticClass());
			TestTrue(TEXT("Template speakers cannot overwrite CDO roles during Native migration"), Compiled->Speakers.IsEmpty());
			TestTrue(TEXT("Ambient template has no player replies"), Compiled->PlayerReplies.IsEmpty());
			TestEqual(TEXT("All ambient nodes survive compilation"), Compiled->GetNodes().Num(), 3);
			for (const UDialogueNode_NPC* Node : Compiled->NPCReplies)
			{
				if (Node->GetID() == Ask.ID) TestEqual(TEXT("First spoken node binds the first role"), Node->GetSpeakerID(), Recipe->FirstRoleID);
				if (Node->GetID() == Answer.ID) TestEqual(TEXT("Second spoken node binds the second role"), Node->GetSpeakerID(), Recipe->SecondRoleID);
			}
		}
		FString Failure;
		const bool bValid = UTerritoryAmbientDialogue::ValidateDialogueClass(BP->GeneratedClass.Get(), Failure);
		TestTrue(*FString::Printf(TEXT("Generated class satisfies meeting runtime contract: %s"), *Failure),
			bValid);
	};
	CheckCompiledContract();
	TestTrue(TEXT("Ambient graph uses the existing Native layout and compiler"), UTerritoryDialogueEditorLibrary::ArrangeDialogue(BP, Error));
	CheckCompiledContract();
	FTerritoryDialogueRecipeNode PlayerReply = Answer;
	PlayerReply.ID = TEXT("PlayerChoice"); PlayerReply.bPlayer = true;
	TestFalse(TEXT("Editor reply helper cannot introduce a player into an ambient graph"),
		UTerritoryDialogueEditorLibrary::AddDialogueReply(BP, Answer.ID, PlayerReply, Error));
	TestEqual(TEXT("Rejected player reply does not mutate the graph"), BP->DialogueGraph->Nodes.Num(), 3);
	FAssetRegistryModule::AssetDeleted(BP);
	BP->GetOutermost()->SetDirtyFlag(false);
	BP->ClearFlags(RF_Public | RF_Standalone);
	BP->SetFlags(RF_Transient);
	return true;
}
#endif

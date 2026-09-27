#include "Assets/TerritoryAuthoringMenu.h"

#include "Assets/TerritoryAssetFactories.h"
#include "AssetToolsModule.h"
#include "Core/TerritoryFloorVolume.h"
#include "Social/TerritoryConversationMeetingPoint.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Editor.h"
#include "IAssetTools.h"
#include "IPlacementModeModule.h"
#include "Misc/MessageDialog.h"
#include "TerritoryDefinitionEditorLibrary.h"
#include "ToolMenus.h"
#include "UObject/UObjectHash.h"

namespace TerritoryAuthoringMenu
{
	static FDelegateHandle StartupHandle;
	static const FName OwnerName(TEXT("TerritoryFrameworkAuthoring"));
	static const FName PlacementCategory(TEXT("TerritoryFramework"));

	static bool CanAuthor() { return GEditor && !GEditor->PlayWorld; }

	void SynchronizeFloorsWithDialog(UTerritoryPlaceDefinition* Definition)
	{
		if (!CanAuthor()) return;
		const FTerritoryDefinitionSyncReport Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(
			GEditor->GetEditorWorldContext().World(), Definition, true);
		FString Message = FString::Printf(TEXT("%s\nUpdated %d Definition assets, %d floor assignments and %d actors."),
			Report.bSucceeded ? TEXT("Floor synchronization complete. Review and save changes.") : TEXT("Floor synchronization blocked; no floor changes were applied."),
			Report.UpdatedDefinitions, Report.UpdatedFloorAssignments, Report.UpdatedActors);
		for (const FString& Error : Report.Errors) Message += TEXT("\nError: ") + Error;
		for (const FString& Warning : Report.Warnings) Message += TEXT("\nReview: ") + Warning;
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
	}

	static void PopulateFactories(UToolMenu* Menu, UClass* FactoryBase)
	{
		// The same concrete factories drive Content Browser and this shortcut menu.
		// A new supported authoring type therefore cannot disappear from one list.
		TArray<UClass*> Classes;
		GetDerivedClasses(FactoryBase, Classes, true);
		Classes.RemoveAll([](const UClass* Class)
		{
			return Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
		});
		Classes.Sort([](const UClass& A, const UClass& B)
		{
			return A.GetDefaultObject<UFactory>()->GetDisplayName().ToString()
				< B.GetDefaultObject<UFactory>()->GetDisplayName().ToString();
		});
		FToolMenuSection& Section = Menu->AddSection(TEXT("TerritoryFactories"));
		for (UClass* Class : Classes)
		{
			const UFactory* Factory = Class->GetDefaultObject<UFactory>();
			Section.AddMenuEntry(Class->GetFName(), Factory->GetDisplayName(), Factory->GetToolTip(), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Class]()
				{
					UFactory* Instance = NewObject<UFactory>(GetTransientPackage(), Class);
					FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get()
						.CreateAssetWithDialog(Instance->GetSupportedClass(), Instance);
				}), FCanExecuteAction::CreateStatic(&CanAuthor)));
		}
	}

	static void PopulateTools(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection(TEXT("TerritorySetup"), FText::FromString(TEXT("Authoring")));
		Section.AddSubMenu(TEXT("CreateDefinition"), FText::FromString(TEXT("Create Definition or Profile")),
			FText::FromString(TEXT("Uses the same Territory factories as Content Browser Add/New.")),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu) { PopulateFactories(SubMenu, UTerritoryAssetFactoryBase::StaticClass()); }));
		Section.AddSubMenu(TEXT("CreateActorBlueprint"), FText::FromString(TEXT("Create Actor Blueprint")),
			FText::FromString(TEXT("Create a reusable Territory actor, including a Floor Volume.")),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu) { PopulateFactories(SubMenu, UTerritoryBlueprintFactoryBase::StaticClass()); }));
		Section.AddMenuEntry(TEXT("DetectAndSyncFloors"), FText::FromString(TEXT("Detect & Sync Loaded Floors")),
			FText::FromString(TEXT("Bind unassigned contained floor boxes and update canonical post-floor rows. Undoable; does not move actors or save packages.")),
			FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
			{
				SynchronizeFloorsWithDialog(nullptr);
			}), FCanExecuteAction::CreateStatic(&CanAuthor)));
		Section.AddMenuEntry(TEXT("ValidateTerritoryWorld"), FText::FromString(TEXT("Validate Loaded Territory World")),
			FText::FromString(TEXT("Read-only authoring checks for the currently loaded map and cells.")),
			FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
			{
				TArray<FString> Errors, Warnings;
				const bool bValid = UTerritoryDataValidator::ValidateWorld(GEditor->GetEditorWorldContext().World(), Errors, Warnings);
				FString Message = bValid && Warnings.IsEmpty() ? TEXT("Loaded Territory world validation passed.") : TEXT("Loaded Territory world needs attention.");
				for (const FString& Error : Errors) Message += TEXT("\nError: ") + Error;
				for (const FString& Warning : Warnings) Message += TEXT("\nWarning: ") + Warning;
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
			}), FCanExecuteAction::CreateStatic(&CanAuthor)));
	}

	static void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(OwnerName);
		UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))->FindOrAddSection(TEXT("TerritoryFramework"))
			.AddSubMenu(TEXT("TerritoryFramework"), FText::FromString(TEXT("Territory Framework")),
				FText::FromString(TEXT("Create Territory content, synchronize floor authoring and validate the loaded world.")),
				FNewToolMenuDelegate::CreateStatic(&PopulateTools));
	}

	void Register()
	{
		if (IsRunningCommandlet()) return;
		StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
		IPlacementModeModule& Placement = FModuleManager::LoadModuleChecked<IPlacementModeModule>(TEXT("PlacementMode"));
		Placement.RegisterPlacementCategory(FPlacementCategoryInfo(FText::FromString(TEXT("Territory Framework")), PlacementCategory, TEXT("TerritoryFramework"), 45));
		Placement.RegisterPlaceableItem(PlacementCategory, MakeShared<FPlaceableItem>(static_cast<UActorFactory*>(nullptr), FAssetData(ATerritoryFloorVolume::StaticClass())));
		Placement.RegisterPlaceableItem(PlacementCategory, MakeShared<FPlaceableItem>(static_cast<UActorFactory*>(nullptr), FAssetData(ATerritoryConversationMeetingPoint::StaticClass())));
	}

	void Unregister()
	{
		if (IsRunningCommandlet()) return;
		UToolMenus::UnRegisterStartupCallback(StartupHandle);
		UToolMenus::UnregisterOwner(OwnerName);
		if (IPlacementModeModule* Placement = FModuleManager::GetModulePtr<IPlacementModeModule>(TEXT("PlacementMode")))
			Placement->UnregisterPlacementCategory(PlacementCategory);
	}
}

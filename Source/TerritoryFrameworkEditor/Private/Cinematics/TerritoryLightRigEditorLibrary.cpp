#include "Cinematics/TerritoryLightRigEditorLibrary.h"
#include "Cinematics/TerritoryCinematicLightRig.h"
#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Components/ActorComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "UObject/UnrealType.h"

namespace
{
	class FRejectWorldReferences : public FArchiveUObject
	{
	public:
		bool bFound = false;
		FRejectWorldReferences() { ArIsObjectReferenceCollector = true; }
		virtual FArchive& operator<<(UObject*& Object) override
		{
			bFound |= Object && (Object->IsA<AActor>() || Object->IsA<UActorComponent>() || Object->IsA<UWorld>());
			return *this;
		}
	};
}

UEditorUtilityWidget* UTerritoryLightRigEditorLibrary::OpenControlPanel(UTerritoryCinematicLightRigProfile* Profile)
{
	if (!GEditor || GEditor->PlayWorld || !IsValid(Profile)) return nullptr;
	auto* Blueprint = Cast<UEditorUtilityWidgetBlueprint>(Profile->AuthoringPanel.LoadSynchronous());
	auto* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	return Blueprint && Subsystem ? Subsystem->SpawnAndRegisterTab(Blueprint) : nullptr;
}

bool UTerritoryLightRigEditorLibrary::CopyPanelLook(UTerritoryCinematicLightRigProfile* Profile, FString& Result)
{
	Result = TEXT("Stop Play/Simulate and select a valid light rig profile.");
	if (!GEditor || GEditor->PlayWorld || !IsValid(Profile) || !Profile->RigClass) return false;
	auto* PanelBP = Cast<UEditorUtilityWidgetBlueprint>(Profile->AuthoringPanel.LoadSynchronous());
	auto* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	auto* Panel = PanelBP && Subsystem ? Subsystem->FindUtilityWidgetFromBlueprint(PanelBP) : nullptr;
	auto* RigProperty = Panel ? FindFProperty<FObjectPropertyBase>(Panel->GetClass(), Profile->PreviewRigProperty) : nullptr;
	auto* Preview = RigProperty ? Cast<AActor>(RigProperty->GetObjectPropertyValue_InContainer(Panel)) : nullptr;
	auto* Blueprint = Cast<UBlueprint>(Profile->RigClass->ClassGeneratedBy);
	auto* Defaults = Profile->RigClass->GetDefaultObject<AActor>();
	Result = TEXT("Open the control panel and select a preview rig first. The runtime rig must be a project Blueprint.");
	if (!IsValid(Preview) || !Blueprint || !Defaults || Profile->PreviewPropertiesToCopy.IsEmpty()) return false;
	TArray<TPair<FProperty*, FProperty*>> Properties;
	for (const FName Name : Profile->PreviewPropertiesToCopy)
	{
		FProperty* From = FindFProperty<FProperty>(Preview->GetClass(), Name);
		FProperty* To = FindFProperty<FProperty>(Defaults->GetClass(), Name);
		Result = FString::Printf(TEXT("Cannot copy preset property '%s'. It must be an editable matching property without scene references."), *Name.ToString());
		if (!From || !To || !From->SameType(To) || !To->HasAnyPropertyFlags(CPF_Edit)
			|| To->HasAnyPropertyFlags(CPF_Transient | CPF_InstancedReference | CPF_ContainsInstancedReference)) return false;
		FRejectWorldReferences References;
		FStructuredArchiveFromArchive Archive(References);
		From->SerializeItem(Archive.GetSlot(), From->ContainerPtrToValuePtr<void>(Preview), nullptr);
		if (References.bFound) return false;
		Properties.Emplace(From, To);
	}
	const FScopedTransaction Transaction(NSLOCTEXT("TerritoryLightRig", "CopyLook", "Use light rig panel look"));
	Blueprint->Modify();
	Defaults->Modify();
	for (const auto& Pair : Properties)
		Pair.Value->CopyCompleteValue(Pair.Value->ContainerPtrToValuePtr<void>(Defaults), Pair.Key->ContainerPtrToValuePtr<void>(Preview));
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Result = TEXT("Panel look copied to the runtime rig. Review the rig Blueprint and Save All when ready.");
	return true;
}

#include "TerritoryFrameworkEditor.h"
#include "Core/TerritoryDefinition.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

namespace
{
	class FTerritoryDefinitionDetails final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FTerritoryDefinitionDetails>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			TArray<TWeakObjectPtr<UObject>> Objects;
			DetailBuilder.GetObjectsBeingCustomized(Objects);
			bool bContainsNonPlace = false;
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (Object.IsValid() && !Object->IsA<UTerritoryPlaceDefinition>())
				{
					bContainsNonPlace = true;
					break;
				}
			}
			if (!bContainsNonPlace) return;

			// These legacy serialized fields remain loadable for bounded migration, but
			// City/District authors cannot edit them. Runtime and validation enforce the
			// same Place-only boundary.
			const FName PlaceOnlyProperties[] = {
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialOwningFaction),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialState),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, MaxConcurrentAttackers),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PeriodicIncome),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardUpkeepPerCycle),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardRecruitmentCost),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultStealthProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, bStoryCaptureFromBounds),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CapturePoint),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefaultGuardDefinition),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FactionGuardDefinitions),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, InitialGuardCount),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, PostCaptureGarrisonPolicy),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardBehavior),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardPosts),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, DefenderDiedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, AllDefendersDefeatedEvents),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackProfile),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, CounterAttackApproaches),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, GuardQuality),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, FortificationStrength),
				GET_MEMBER_NAME_CHECKED(UTerritoryDefinition, NearbyAlliedSupport)
			};
			for (const FName PropertyName : PlaceOnlyProperties)
			{
				DetailBuilder.HideProperty(PropertyName, UTerritoryDefinition::StaticClass());
			}
		}
	};
}

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkEditorModule"

void FTerritoryFrameworkEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
		TEXT("PropertyEditor"));
	for (const FName ClassName : { FName(TEXT("TerritoryDefinition")),
		FName(TEXT("TerritoryPlaceDefinition")), FName(TEXT("TerritoryDistrictDefinition")),
		FName(TEXT("TerritoryCityDefinition")) })
	{
		PropertyEditor.RegisterCustomClassLayout(ClassName,
			FOnGetDetailCustomizationInstance::CreateStatic(
				&FTerritoryDefinitionDetails::MakeInstance));
	}
	PropertyEditor.NotifyCustomizationModuleChanged();
	// UEditorValidator subclasses are auto-registered by the DataValidation system
	// when the module loads — no manual registration needed
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module loaded (auto-validators registered)"));
}

void FTerritoryFrameworkEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor"));
		for (const FName ClassName : { FName(TEXT("TerritoryDefinition")),
			FName(TEXT("TerritoryPlaceDefinition")), FName(TEXT("TerritoryDistrictDefinition")),
			FName(TEXT("TerritoryCityDefinition")) })
		{
			PropertyEditor.UnregisterCustomClassLayout(ClassName);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkEditorModule, TerritoryFrameworkEditor)

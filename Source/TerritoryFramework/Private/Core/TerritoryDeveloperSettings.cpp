#include "Core/TerritoryDeveloperSettings.h"

UTerritoryDeveloperSettings::UTerritoryDeveloperSettings()
{
	DefaultTerritoryButtonStyle = TSoftClassPtr<UCommonButtonStyle>(FSoftObjectPath(
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritoryAction.ButtonStyle_TerritoryAction_C")));
	TerritoryTabButtonStyle = TSoftClassPtr<UCommonButtonStyle>(FSoftObjectPath(
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritoryTab.ButtonStyle_TerritoryTab_C")));
	TerritoryActionButtonStyle = DefaultTerritoryButtonStyle;

	DefaultNarrativeButtonClass = TSoftClassPtr<UNarrativeCommonButtonBase>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Widgets/Base/WBP_NarrativeButton_Text.WBP_NarrativeButton_Text_C")));
	DefaultTerritoryTextStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Style/MasterStyles/Text/Primary/TextStyle_Master_Primary.TextStyle_Master_Primary_C")));
	TerritoryTitleTextStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Style/MasterStyles/Text/Primary/TextStyle_Master_Primary_Title.TextStyle_Master_Primary_Title_C")));
	TerritoryHeadingTextStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Style/MasterStyles/Text/Primary/TextStyle_Master_Primary_H3.TextStyle_Master_Primary_H3_C")));
	TerritoryMutedTextStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Style/MasterStyles/Text/Disabled/TextStyle_Master_Disabled.TextStyle_Master_Disabled_C")));
	TerritoryInterfaceFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Fonts/RobotoCondensed.RobotoCondensed")));
}

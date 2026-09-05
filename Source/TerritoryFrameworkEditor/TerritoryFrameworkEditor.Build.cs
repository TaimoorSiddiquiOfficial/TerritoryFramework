using UnrealBuildTool;

public class TerritoryFrameworkEditor : ModuleRules
{
	public TerritoryFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"TerritoryFramework"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"PropertyEditor",
			"BlueprintGraph",
			"DataValidation",
			"GameplayAbilities",
			"GameplayTags",
			"NavigationSystem",
			"NarrativeArsenal",
			"NarrativeSaveSystem",
			"NarrativeDialogueEditor",
			"NarrativeQuestEditor",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"CinematicCamera",
			"ZoneGraph",
			"MassSpawner",
			"MassEntity"
		});
	}
}

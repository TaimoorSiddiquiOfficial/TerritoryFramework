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
			"NarrativeQuestEditor",
			"ZoneGraph",
			"MassSpawner",
			"MassEntity"
		});
	}
}

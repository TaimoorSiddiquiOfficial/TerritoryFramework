using UnrealBuildTool;

public class TerritoryFramework : ModuleRules
{
	public TerritoryFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NarrativeArsenal",
			"NarrativeSaveSystem",
			"GameplayAbilities",
			"GameplayTags",
			"AIModule",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}

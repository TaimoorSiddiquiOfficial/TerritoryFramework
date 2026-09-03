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
			"GameplayAbilities",
			"GameplayTags",
			"AIModule",
			"UMG",
			"CommonUI",
			"EnhancedInput",
			"NavigationSystem",
			"ChaosVehicles",
			"NarrativeArsenal",
			"NarrativeCommonUI",
			"NarrativeSaveSystem",
			"MassSpawner",
			"MassEntity"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CinematicCamera",
			"DeveloperSettings",
			"HairStrandsCore",
			"ZoneGraph"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});

		// Optional: Gameplay Debugger support
		if (Target.bBuildDeveloperTools)
		{
			PrivateDependencyModuleNames.Add("GameplayDebugger");
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
		}
	}
}

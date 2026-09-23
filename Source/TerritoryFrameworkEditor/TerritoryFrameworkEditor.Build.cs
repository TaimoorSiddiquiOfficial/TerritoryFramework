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
			"NetCore",
			"AssetTools",
			"AssetRegistry",
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"UMG",
			"PropertyEditor",
			"BlueprintGraph",
			"Blutility",
			"UMGEditor",
			"DataValidation",
			"GameplayAbilities",
			"GameplayTags",
			"NavigationSystem",
			"AIModule",
			"NarrativeArsenal",
			"NarrativeSaveSystem",
			"NarrativeDialogueEditor",
			"NarrativeQuestEditor",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"CinematicCamera",
			"HairStrandsCore",
			"RenderCore",
			"RHI",
			"ZoneGraph",
			"PhysicsCore",
			"MassSpawner",
			"MassEntity"
		});
	}
}

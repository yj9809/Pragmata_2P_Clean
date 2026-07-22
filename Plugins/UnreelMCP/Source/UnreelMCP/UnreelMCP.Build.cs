using UnrealBuildTool;

public class UnreelMCP : ModuleRules
{
	public UnreelMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"Json",
				"JsonUtilities",
				"HTTPServer",
				"UnrealEd",
				"EditorFramework",
				"InputCore",
				"EnhancedInput",
				"GameplayTags",
				"AssetTools",
				"AssetRegistry",
				"EditorScriptingUtilities",
				"KismetCompiler",
				"BlueprintGraph",
				"AIModule",
				"AIGraph",
				"BehaviorTreeEditor",
				"AnimGraph",
				"UMG",
				"UMGEditor",
				"ModelViewViewModel",
				"ModelViewViewModelBlueprint",
				"ModelViewViewModelEditor",
				"MaterialEditor",
				"StaticMeshEditor",
				"SkeletalMeshEditor",
				"Landscape",
				"Foliage",
				"Niagara",
				"NiagaraCore",
				"NiagaraEditor",
				"MetasoundEngine",
				"MetasoundFrontend",
				"MetasoundEditor",
				"StateTreeModule",
				"StateTreeEditorModule",
				"GameplayStateTreeModule"
			}
		);
	}
}

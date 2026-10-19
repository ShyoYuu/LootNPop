using UnrealBuildTool;

public class LootNPopEditor : ModuleRules
{
	public LootNPopEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"LootNPop",
			"GeometryScriptingEditor",
		]);

		PrivateDependencyModuleNames.AddRange([
			"AssetRegistry",
			"AssetTools",
			"Chaos",
			"Chooser",
			"DynamicMesh",
			"GeometryCore",
			"InteractiveToolsFramework",
			"Json",
			"MassCore",
			"MassEntity",
			"MassSpawner",
			"MeshConversion",
			"MeshDescription",
			"MeshModelingTools",
			"MeshPartitionModelingToolset",
			"ModelingComponents",
			"ModelingComponentsEditorOnly",
			"PhysicsCore",
			"PCG",
			"PoseSearch",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			"UnrealEd",
		]);

		PublicIncludePaths.AddRange(["LootNPopEditor"]);
	}
}

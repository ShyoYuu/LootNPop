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
			"InteractiveToolsFramework",
			"MeshDescription",
			"MeshModelingTools",
			"MeshPartitionModelingToolset",
			"ModelingComponents",
			"ModelingComponentsEditorOnly",
			"PhysicsCore",
			"PCG",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			"UnrealEd",
		]);

		PublicIncludePaths.AddRange(["LootNPopEditor"]);
	}
}

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
			"GameplayInsights",
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
			"RewindDebugger",
			"RewindDebuggerInterface",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			"TraceBasedDebuggersAnalysis",
			"UnrealEd",
		]);

		PublicIncludePaths.AddRange(["LootNPopEditor"]);

		// UE 5.8 공개 인터페이스에는 Trace 세션 관리자 접근자가 없다.
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("RewindDebugger"), "Private"));
	}
}

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
			"PropertyEditor",
			"Slate",
			"SlateCore",
		]);

		PublicIncludePaths.AddRange(["LootNPopEditor"]);
	}
}

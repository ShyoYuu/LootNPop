// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class LootNPop : ModuleRules
{
	public LootNPop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
            "Mover",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "MassCommon",
            "MassEntity",
            "MassRepresentation",
            "MassSpawner",
            "MassMovement",
            "MassActors",
            "MassNavigation",
            "MassAIBehavior",
            "MassLOD",
            "MassSignals",
            "MassGameplayDebug",
            "Niagara",
            "ModelViewViewModel",
			"PCG",
			"AnimGraphRuntime",
			"SmartObjectsModule",
			"DeveloperSettings"
			]);
		PrivateDependencyModuleNames.AddRange([]);

		PublicIncludePaths.AddRange([
			"LootNPop"
		]);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

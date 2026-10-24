// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class LootNPop : ModuleRules
{
	public LootNPop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		SetupIrisSupport(Target, true);

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
			"SlateCore",
			"CommonUI",
			"CommonInput",
            "Mover",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "MassCore",
            "MassCommon",
            "MassEntity",
            "MassRepresentation",
            "MassCrowd",
            "MassSpawner",
            "MassMovement",
            "MassActors",
            "MassNavigation",
            "MassAIBehavior",
            "MassLOD",
            "MassSignals",
            "MassSimulation",
            "MassGameplayDebug",
            "Niagara",
            "ModelViewViewModel",
			"PCG",
			"AnimGraphRuntime",
			"SmartObjectsModule",
			"DeveloperSettings",
            "GameplayCameras",
			"Chooser",
            "NetCore",
            "MassReplication",
            "MotionWarping",
            "PoseSearch",
            "AnimationWarpingRuntime",
            "BlendStack",
            "PhysicsControl",
            "LNPUI"
            ]);
		PrivateDependencyModuleNames.AddRange([
			"UAF", // Phase 5 스파이크 전용 입력 전달
			"UAFAnimGraph",
			"UAFWarping",
			"RigVM", // UAF 스파이크 공유 구조체 타입 등록
			"Chaos", // Gate 0 스파이크의 scene read lock 계측
			"GeometryCore" // Support Atlas 베이크 광선 교차(TMeshAABBTree3)
			]);

		PublicIncludePaths.AddRange([
			"LootNPop"
		]);

		// Phase 5 스파이크: 기본 Offset Root의 누적 상태를 같은 평가 프로그램에서 보정한다.
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAFWarping"), "Private"));

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

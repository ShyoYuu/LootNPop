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
			"UAFPoseSearch", // 스파이크 Steering은 실제 UAF 검색 결과의 DB 태그를 읽는다.
			"RigVM", // UAF 스파이크 공유 구조체 타입 등록
			"Chaos", // Gate 0 스파이크의 scene read lock 계측
			"GeometryCore" // Support Atlas 베이크 광선 교차(TMeshAABBTree3)
			]);

		PublicIncludePaths.AddRange([
			"LootNPop"
		]);

		// Phase 5 스파이크: 기본 Offset Root의 누적 상태를 같은 평가 프로그램에서 보정한다.
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAF"), "Internal")); // 완료된 루트를 모듈 피드백 컴포넌트에 기록
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAFAnimGraph"), "Internal")); // Bool의 기본 갱신 조건과 같은 상태를 읽어 누락만 보충
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAFWarping"), "Private"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAFPoseSearch"), "Private"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("UAFPoseSearch"), "Internal"));

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

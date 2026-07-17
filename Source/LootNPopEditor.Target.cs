// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LootNPopEditorTarget : TargetRules
{
	public LootNPopEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        //bUseIris = true;
		ExtraModuleNames.Add("LootNPop");

		// 풀 빌드(UBT/VS 솔루션 빌드)마다 clangd용 compile_commands.json을 자동 재생성한다.
		// Serena 1.6.0 이상은 UE 프로젝트에 유효한 컴파일 DB가 없으면 cpp 언어 서버 시작에 실패한다.
		// GenerateClangDatabase 모드는 PostBuildSteps를 실행하지 않으므로 재귀 호출 위험이 없다.
		// -NoMutex: 부모 빌드가 UBT 글로벌 mutex를 점유 중이므로, 중첩 호출은 mutex 검사를 건너뛴다.
		PostBuildSteps.Add(
			"\"$(EngineDir)/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe\" " +
			"-mode=GenerateClangDatabase -project=\"$(ProjectDir)/LootNPop.uproject\" " +
			"$(TargetName) $(TargetPlatform) $(TargetConfiguration) -OutputDir=\"$(ProjectDir)\" -NoMutex");
	}
}

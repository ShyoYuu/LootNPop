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

		// 코드가 실제로 변경돼 리싯이 갱신될 때만 clangd용 compile_commands.json을 재생성한다.
		// (UBT는 이 포스트빌드 액션의 선행 항목을 리싯 파일로 잡으므로, 변경 없는 빌드에선 실행되지 않는다.)
		// Serena 1.6.0 이상은 UE 프로젝트에 유효한 컴파일 DB가 없으면 cpp 언어 서버 시작에 실패한다.
		// GenerateClangDatabase 모드는 PostBuildSteps를 실행하지 않으므로 재귀 호출 위험이 없다.
		// -NoMutex: 부모 빌드가 UBT 글로벌 mutex를 점유 중이므로, 중첩 호출은 mutex 검사를 건너뛴다.
		//
		// ⚠️ DB 생성은 반드시 '지금 빌드 중이 아닌' 컨피그 폴더에 주차해야 한다. 설치본(런처)
		// 엔진에서는 UBT가 IntermediateEnvironment를 Default로 강제해(UEBuildTarget.cs:1894)
		// clang DB 전용 폴더 접미사 "GCD"가 무시된다. 그래서 빌드 중인 컨피그를 그대로 넘기면
		// GCD가 실제 빌드와 같은 Intermediate/.../UnrealEditor/<컨피그>/ 아래 *.rsp를 clang
		// 인자로 덮어써, 다음 빌드가 "rsp modified since makefile"로 메이크파일을 무효화하고
		// 전 모듈을 재컴파일하는 무한 루프에 빠진다.
		//
		// Debug는 설치본 엔진이 거부하므로("Targets cannot be built in the Debug configuration
		// with this engine distribution") 빈 주차 공간이 없다. 실제로 쓰는 두 컨피그를 서로
		// 엇갈리게 주차시킨다 — 같은 컨피그를 반복 빌드하는 동안은 무변경 빌드가 스킵되고,
		// 컨피그를 전환한 직후 첫 빌드만 풀 리빌드가 난다.
		string ClangDbConfig = Target.Configuration == UnrealTargetConfiguration.DebugGame ? "Development" : "DebugGame";
		PostBuildSteps.Add(
			"\"$(EngineDir)/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe\" " +
			"-mode=GenerateClangDatabase -project=\"$(ProjectDir)/LootNPop.uproject\" " +
			$"$(TargetName) $(TargetPlatform) {ClangDbConfig} -OutputDir=\"$(ProjectDir)\" -NoMutex");
	}
}

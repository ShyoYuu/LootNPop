// Copyright (c) 2026 LootNPop. All rights reserved.

using UnrealBuildTool;

/**
 * LootNPop 전용 커스텀 Slate/UMG 위젯 모듈.
 * 게임플레이 모듈(LootNPop)을 참조하지 않는다 — 위젯은 게임 타입을 모르고 값은 밖에서 주입받는다.
 */
public class LNPUI : ModuleRules
{
	public LNPUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG"
			]);

		PrivateDependencyModuleNames.AddRange([]);
	}
}

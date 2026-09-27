// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"

/**
 * 동굴 키트 greybox 모듈(`design/TerrainContract.md` §6, D-035).
 *
 * 에셋 규약(제작 도구와 무관하게 모든 키트 에셋이 따른다):
 * - 모듈마다 `SM_CaveKit_<Module>_Floor`(Support+Blocker+Static)와 `_Shell`(Blocker+Static) 두 메시
 * - 피벗은 바닥 기준점, 로컬 +Z = Up(월드 중심 방향), 로컬 +X = 통로가 지각 입구로 향하는 쪽
 * - Floor는 위(+Z)를 향하는 단면, Shell은 안팎 모두 막는 양면
 * - 연결점은 StaticMesh 소켓으로 표시한다. 공동의 문은 `Door_<n>`, 통로는 `Lower`(공동 쪽)·`Mouth`(지각 쪽)
 * - 충돌은 complex-as-simple
 *
 * 이 파일의 메시는 코드로 만든 greybox다. 프로덕션 에셋(Blender 등)은 같은 이름·피벗·소켓 규약을 지키면
 * 1:1로 교체되고, 자동화 `Bake.CaveKitContract`가 규약을 검사한다.
 */
namespace LNPCaveKit
{
	const TCHAR* const KitFolder = TEXT("/Game/Maps/CaveKit");

	const TCHAR* const RoomFloorName = TEXT("SM_CaveKit_RoomBox_Floor");
	const TCHAR* const RoomShellName = TEXT("SM_CaveKit_RoomBox_Shell");
	const TCHAR* const CorridorFloorName = TEXT("SM_CaveKit_RampCorridor_Floor");
	const TCHAR* const CorridorShellName = TEXT("SM_CaveKit_RampCorridor_Shell");

	const FName DoorSocket(TEXT("Door_0"));
	const FName LowerSocket(TEXT("Lower"));
	const FName MouthSocket(TEXT("Mouth"));

	/** 직육면체 공동: 바닥 한 변(cm)과 바닥 중심에서 천장까지 높이. */
	constexpr double RoomSize = 1600.0;
	constexpr double RoomHeight = 500.0;
	/** 문과 통로 단면. 적 캡슐(반지름 35, 반높이 88)이 여유 있게 지나간다. */
	constexpr double PassageWidth = 400.0;
	constexpr double PassageHeight = 350.0;
	/** 경사 통로: 수평 길이와 경사각. 수직 통로는 지원하지 않는다(D-035). */
	constexpr double RampLength = 2400.0;
	constexpr double RampAngleDeg = 20.0;
	/** 벽을 바닥 아래로 내려 바닥 곡률·접합부 틈을 막는 깊이. */
	constexpr double WallSink = 30.0;
	/** 바닥 구면 캡의 곡률 반지름. 기준 지각 반지름과 같게 둔다(§6 제작 가이드). */
	constexpr double FloorCurvatureRadius = 30000.0;
	/** 키트 검증의 최소 천장 높이(바닥에서 천장까지). 캡슐 높이 176cm + 여유. */
	constexpr double MinClearance = 250.0;

	/** 공동 바닥 구면 캡의 높이(피벗 로컬 +Z). 중심 0, 가장자리로 갈수록 Up 쪽으로 오른다. */
	double RoomFloorHeight(double X, double Y);

	/** 통로 경사의 tan. */
	double RampSlope();

	/** 공동 로컬에서 통로 피벗(`Lower`)의 transform. */
	FTransform GetRoomDoorTransform();

	/** 통로 로컬에서 지각 입구 바닥점(`Mouth`)의 transform. */
	FTransform GetCorridorMouthTransform();

	FLNPFixtureMesh BuildRoomFloor();
	FLNPFixtureMesh BuildRoomShell();
	FLNPFixtureMesh BuildCorridorFloor();
	FLNPFixtureMesh BuildCorridorShell();

	/** 통로 로컬에서 통로 내부(바닥 위·천장 아래·두 벽 사이·양 끝 사이)의 볼록 영역. PlaneDot < 0이 내부다. */
	TArray<FPlane> GetCorridorInteriorPlanes();

	/** 공동과 통로의 옥탄트 로컬 transform. */
	struct FPlacement
	{
		FTransform Room;
		FTransform Corridor;
	};

	/**
	 * 월드 중심에서 RoomDirection 쪽에 공동을 두고 Forward(접평면 성분만 쓴다) 쪽으로 통로를 낸다.
	 * 통로 `Mouth` 바닥점이 반지름 CrustRadius 구면에 정확히 오도록 공동 깊이를 정한다.
	 */
	FPlacement PlaceUnderSphere(const FVector& RoomDirection, const FVector& Forward, double CrustRadius);

	/** 키트 메시 네 개를 KitFolder에 만들거나 제자리 갱신한다. 하나라도 실패하면 false. */
	bool WriteKitAssets();
}

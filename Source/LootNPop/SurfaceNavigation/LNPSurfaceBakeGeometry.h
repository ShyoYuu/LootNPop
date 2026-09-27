// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 베이커 입력 삼각형 메시. 좌표는 옥탄트 로컬(source level) 공간이다.
 * 앞면 법선은 (B-A)×(C-A)이고, exact query hit 법선과 같은 쪽을 향한다(Chaos 면 법선 규약).
 */
struct LOOTNPOP_API FLNPBakeTriangleMesh
{
	TArray<FVector3d> Vertices;
	TArray<FIntVector3> Triangles;

	/**
	 * 삼각형마다 exact hit의 FaceIndex와 같은 번호. 엔진은 Chaos 내부 face 번호를 원본 mesh 삼각형 번호(external)로
	 * 바꿔 돌려주므로 추출 순서와 다르다. face→Layer 표(D-037)의 키다. 합성 입력은 비워 둔다.
	 */
	TArray<int32> ExternalFaceIndices;

	/** 정규화한 앞면 법선. 퇴화 삼각형이면 영벡터다. */
	FVector3d GetTriangleNormal(int32 TriangleIndex) const;
};

/**
 * Support 역할 source 하나의 삼각형. Name은 오류 보고용 component 경로다.
 * Key는 `<Actor FName>.<Component FName>`이다. slot Level Instance의 component는 outer 경로가 달라서
 * runtime이 같은 source를 찾을 수 있도록 LVI 패키지 경로를 넣지 않는다.
 */
struct LOOTNPOP_API FLNPBakeSupportSource
{
	FString Name;
	FString Key;
	FLNPBakeTriangleMesh Mesh;
};

/**
 * 베이커 핵심 계산 중 geometry 검증(D-034). Editor 추출기가 만든 삼각형만 입력으로 받는 순수 함수다.
 * source 옥탄트는 (+X,+Y,+Z)이고 이음매 평면은 x=0, y=0, z=0이다.
 */
namespace LNPSurfaceBake
{
	/** 정점이 이음매 평면에 "닿는다"고 보는 거리(cm). 옥탄트 밖으로 이만큼까지는 허용한다. */
	constexpr double SeamPlaneTolerance = 1.0;

	/** Mass 위치 복제의 성분별 int16 캡(cm, `.context/Guide_NetBandwidth.md` §2.4). */
	constexpr double ReplicatedPositionCap = 32767.0;

	/**
	 * 정점 좌표가 유한하고, 옥탄트 경계를 넘지 않으며(D-030), 좌표 성분 최대 절댓값이 int16 캡 이하인지 검사한다.
	 * slot 회전은 축을 ±축으로 보내므로 성분 최대 절댓값은 slot과 무관하다. 로컬 좌표 검사로 8 slot을 모두 덮는다.
	 */
	LOOTNPOP_API bool ValidateSupportSource(const FLNPBakeSupportSource& Source, FString& OutError);

	/** 정점이 이음매 평면 x=0(Axis 0), y=0(1), z=0(2) 각각에 닿는지 비트로 돌려준다. */
	LOOTNPOP_API uint8 GetTouchedSeamPlanes(const FLNPBakeTriangleMesh& Mesh);

	/**
	 * D-055: 세 이음매 평면 모두에 닿는 유일한 Support source를 지각으로 고른다.
	 * 해당 source가 0개나 2개 이상이면 베이크 오류다.
	 */
	LOOTNPOP_API bool IdentifyCrust(
		TConstArrayView<FLNPBakeSupportSource> Sources,
		int32& OutCrustIndex,
		FString& OutError);
}

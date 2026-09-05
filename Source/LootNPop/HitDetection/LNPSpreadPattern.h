// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 산탄 방사 패턴 — 육각 링 배치의 **단일 정의**.
 *
 * GAS 어빌리티(`ULNPAbility_RangedSpreadAttack`)와 순수 엔티티 공격
 * (`ULNPEntityAttackProcessor`)이 같은 공식을 봐야 한다. 같은 판정식을 두 곳에 적으면
 * 언젠가 갈라진다는 것이 이 프로젝트가 두 번 겪은 교훈이다
 * (→ TechDesign_HitDetection.md §7.5·§7.6).
 */
namespace LNPSpread
{
	/** 육각 링 N개일 때의 발사 수: 1 + 3N(N+1) → 0=1발, 1=7발, 2=19발, 3=37발. */
	inline int32 ComputeShotCount(const int32 HexRingCount)
	{
		const int32 Rings = FMath::Max(0, HexRingCount);
		return 1 + 3 * Rings * (Rings + 1);
	}

	/**
	 * `BaseDir`을 중심으로 한 육각 링 방사 방향을 만든다.
	 *
	 * 확산 축은 **BaseDir 자신의 직교 기저**에서 잡는다. 월드 오일러 각(Yaw/Pitch) 덧셈으로 만들면
	 * Yaw가 월드 Z축 둘레의 회전이라 실제 각변위가 cos(Pitch)에 비례해 줄고, 조준이 월드 Z와
	 * 나란해질수록 가로 폭이 0으로 붕괴한다(짐벌 수렴). 구면 중력에서는 서 있는 위치에 따라
	 * "캡슐 기준 수평"이 월드 Pitch 0°가 되기도 ±90°가 되기도 하므로 극/적도에서 증상이 뒤집힌다.
	 *
	 * @param RefUp      기준 상방 — 월드 Z가 아니라 **사수의 중력 Up**이다. 확산 폭은 기준축 선택과
	 *                   무관하지만, 육각형의 롤(회전)이 사수 자세를 따라가야 화면상 방향이 일정하다.
	 * @param RefForward RefUp과 BaseDir이 나란한 특이점에서 롤을 정하는 대체 축(사수 전방).
	 */
	inline void BuildHexRingDirections(const FVector& BaseDir, const FVector& RefUp, const FVector& RefForward,
		const int32 HexRingCount, const float HexStepDegrees, TArray<FVector>& OutDirections)
	{
		// Cube 좌표계 6방향 — 육각형 링 순회용 (x+y+z=0 불변)
		static const FIntVector CubeDirs[6] = {
			{1,-1,0}, {1,0,-1}, {0,1,-1}, {-1,1,0}, {-1,0,1}, {0,-1,1}
		};

		FVector RightAxis = FVector::CrossProduct(RefUp, BaseDir).GetSafeNormal();
		if (RightAxis.IsNearlyZero())
		{
			// 조준이 기준 상방과 나란한 특이점 — 기저가 정해지지 않는다. 접평면 위에 있는
			// 사수 전방으로 롤을 정한다(전방은 상방과 직교하므로 여기서 다시 0이 되지 않는다).
			RightAxis = FVector::CrossProduct(RefForward, BaseDir).GetSafeNormal();
		}
		const FVector UpAxis = FVector::CrossProduct(BaseDir, RightAxis).GetSafeNormal();

		const int32 Rings = FMath::Max(0, HexRingCount);

		OutDirections.Reset();
		OutDirections.Reserve(ComputeShotCount(Rings));
		OutDirections.Add(BaseDir); // 중앙

		for (int32 Ring = 1; Ring <= Rings; ++Ring)
		{
			// 링 순회 시작점: CubeDirs[4] * Ring = (-Ring, 0, Ring)
			FIntVector Hex(CubeDirs[4] * Ring);

			for (int32 Side = 0; Side < 6; ++Side)
			{
				for (int32 Step = 0; Step < Ring; ++Step)
				{
					// Cube(x,y,z) → flat-top 2D 오프셋
					// OffX = x + z/2,  OffY = z * (√3/2)
					const float OffX = Hex.X + Hex.Z * 0.5f;
					const float OffY = Hex.Z * 0.8660254f; // √3/2

					// BaseDir에 수직인 평면 위의 정육각 격자를 그대로 투영한다(tan = 평면상 거리).
					// HexStepDegrees는 상한이 없어 링 수와 곱하면 90°를 넘을 수 있고, 그러면 tan이
					// 부호를 뒤집어 뒤로 발사되므로 반각을 클램프한다.
					static constexpr float MaxHalfAngleDeg = 80.f;
					const float AngleX = FMath::Clamp(OffX * HexStepDegrees, -MaxHalfAngleDeg, MaxHalfAngleDeg);
					const float AngleY = FMath::Clamp(OffY * HexStepDegrees, -MaxHalfAngleDeg, MaxHalfAngleDeg);

					const FVector Offset = RightAxis * FMath::Tan(FMath::DegreesToRadians(AngleX))
						+ UpAxis * FMath::Tan(FMath::DegreesToRadians(AngleY));

					OutDirections.Add((BaseDir + Offset).GetSafeNormal());
					Hex += CubeDirs[Side];
				}
			}
		}
	}
}

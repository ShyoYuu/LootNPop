// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedSpreadAttack.h"

// 육각형 방사형 설정 (테스트용)
// 링 수: 2 → 중앙 1 + 1뎁스 6 + 2뎁스 12 = 19발 (1 + 3*N*(N+1))
static constexpr int32 MaxHexRings = 2;
static constexpr float HexStepDeg  = 7.5f; // 인접 셀 간 각도 간격

TArray<FVector> ULNPAbility_RangedSpreadAttack::GetFireDirections(const FVector& SpawnPos) const
{
	const TArray<FVector> Base = Super::GetFireDirections(SpawnPos);
	const FVector BaseDir = Base.IsEmpty() ? FVector::ForwardVector : Base[0];

	// Cube 좌표계 6방향 — 육각형 링 순회용 (x+y+z=0 불변)
	static const FIntVector CubeDirs[6] = {
		{1,-1,0}, {1,0,-1}, {0,1,-1}, {-1,1,0}, {-1,0,1}, {0,-1,1}
	};

	const int32 TotalCount = 1 + 3 * MaxHexRings * (MaxHexRings + 1);
	TArray<FVector> Directions;
	Directions.Reserve(TotalCount);

	const FRotator BaseRot = BaseDir.Rotation();
	Directions.Add(BaseDir); // 중앙

	for (int32 Ring = 1; Ring <= MaxHexRings; ++Ring)
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

				FRotator SpreadRot = BaseRot;
				SpreadRot.Yaw   += OffX * HexStepDeg;
				SpreadRot.Pitch -= OffY * HexStepDeg; // 위(+Pitch) 방향 보정

				Directions.Add(SpreadRot.Vector());
				Hex += CubeDirs[Side];
			}
		}
	}

	return Directions;
}

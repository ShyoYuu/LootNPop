// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedSpreadAttack.h"
#include "Character/LNPCharacterBase.h"

TArray<FVector> ULNPAbility_RangedSpreadAttack::GetFireDirections(const FVector& SpawnPos) const
{
	const TArray<FVector> Base = Super::GetFireDirections(SpawnPos);
	const FVector BaseDir = Base.IsEmpty() ? FVector::ForwardVector : Base[0];

	// Cube 좌표계 6방향 — 육각형 링 순회용 (x+y+z=0 불변)
	static const FIntVector CubeDirs[6] = {
		{1,-1,0}, {1,0,-1}, {0,1,-1}, {-1,1,0}, {-1,0,1}, {0,-1,1}
	};

	const ALNPCharacterBase* Character = GetOwningCharacter();

	// 확산 축은 BaseDir 자신의 직교 기저에서 잡는다. 월드 오일러 각(Yaw/Pitch) 덧셈으로 만들면
	// Yaw가 월드 Z축 둘레의 회전이라 실제 각변위가 cos(Pitch)에 비례해 줄고, 조준이 월드 Z와
	// 나란해질수록 가로 폭이 0으로 붕괴한다(짐벌 수렴). 구면 중력에서는 서 있는 위치에 따라
	// "캡슐 기준 수평"이 월드 Pitch 0°가 되기도 ±90°가 되기도 하므로 극/적도에서 증상이 뒤집힌다.
	//
	// 기준 상방은 월드 Z가 아니라 캐릭터의 중력 Up이다. 확산 폭은 기준축 선택과 무관하지만,
	// 육각형의 롤(회전)이 캐릭터 자세를 따라가야 화면상 방향이 일정하다.
	const FVector RefUp = (nullptr != Character) ? Character->GetUpDirection() : FVector::UpVector;

	FVector RightAxis = FVector::CrossProduct(RefUp, BaseDir).GetSafeNormal();
	if (RightAxis.IsNearlyZero())
	{
		// 조준이 기준 상방과 나란한 특이점 — 기저가 정해지지 않는다. 접평면 위에 있는
		// 캐릭터 전방으로 롤을 정한다(전방은 상방과 직교하므로 여기서 다시 0이 되지 않는다).
		const FVector RefForward = (nullptr != Character) ? Character->GetActorForwardVector() : FVector::ForwardVector;
		RightAxis = FVector::CrossProduct(RefForward, BaseDir).GetSafeNormal();
	}
	const FVector UpAxis = FVector::CrossProduct(BaseDir, RightAxis).GetSafeNormal();

	const int32 TotalCount = 1 + 3 * HexRingCount * (HexRingCount + 1);
	TArray<FVector> Directions;
	Directions.Reserve(TotalCount);

	Directions.Add(BaseDir); // 중앙

	for (int32 Ring = 1; Ring <= HexRingCount; ++Ring)
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

				Directions.Add((BaseDir + Offset).GetSafeNormal());
				Hex += CubeDirs[Side];
			}
		}
	}

	return Directions;
}

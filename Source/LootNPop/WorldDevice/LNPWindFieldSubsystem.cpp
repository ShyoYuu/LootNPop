// Copyright (c) 2026 LootNPop. All rights reserved.

#include "WorldDevice/LNPWindFieldSubsystem.h"
#include "WorldDevice/LNPWindField.h"

namespace
{
	/**
	 * 목표 고도 위로 이만큼까지는 필드 안으로 본다 (cm). 호버 중 한 스텝 오버슛으로 필드를 벗어나
	 * 하강 로직과 번갈아 튀지 않게 하는 여유다.
	 */
	constexpr float WindFieldCeilingMargin = 150.f;
}

void ULNPWindFieldSubsystem::RegisterWindField(ALNPWindField* Field)
{
	WindFields.AddUnique(Field);
}

void ULNPWindFieldSubsystem::UnregisterWindField(ALNPWindField* Field)
{
	WindFields.Remove(Field);
}

bool ULNPWindFieldSubsystem::FindWindLift(const FVector& Location, float& OutTargetCenterDistance) const
{
	const float LocationDistance = Location.Size();
	if (LocationDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector LocationDir = Location / LocationDistance;

	bool bFound = false;
	float BestRemaining = -UE_BIG_NUMBER;
	for (const TWeakObjectPtr<ALNPWindField>& WeakField : WindFields)
	{
		const ALNPWindField* Field = WeakField.Get();
		if (Field == nullptr || !Field->IsWindFieldEnabled())
		{
			continue;
		}

		const FVector Origin = Field->GetActorLocation();
		const float OriginDistance = Origin.Size();
		if (OriginDistance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector OriginDir = Origin / OriginDistance;

		// 구면 원판: 배치 지점과의 지표 거리(= 각도 × 배치 반지름)가 Radius 이하.
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(LocationDir, OriginDir), -1.f, 1.f));
		if (Angle * OriginDistance > Field->GetRadius())
		{
			continue;
		}

		// 고도: 내벽/외벽 부호는 필드의 Up이 행성 중심 쪽인지 바깥쪽인지로 정한다(하드코딩하지 않는다).
		const float TargetDistance = Field->GetTargetCenterDistance();
		const float UpSign = FVector::DotProduct(Field->GetActorUpVector(), OriginDir) >= 0.f ? 1.f : -1.f;
		const float Remaining = UpSign * (TargetDistance - LocationDistance);
		if (Remaining < -WindFieldCeilingMargin)
		{
			continue;
		}

		if (!bFound || Remaining > BestRemaining)
		{
			bFound = true;
			BestRemaining = Remaining;
			OutTargetCenterDistance = TargetDistance;
		}
	}
	return bFound;
}

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** 수집된 적 하나의 마지막 히스토리 조회. 수집 배열과 함께 매 processor 실행마다 새로 생성한다. */
struct FLNPProjectileHistoryCache
{
	bool Find(double InQueryTime, FVector& OutLocation) const
	{
		// ping·보간 대역이 다른 시각을 반올림하거나 허용 오차로 합치지 않는다.
		if (!bValid || QueryTime != InQueryTime)
			return false;
		OutLocation = Location;
		return true;
	}

	void Store(double InQueryTime, const FVector& InLocation)
	{
		QueryTime = InQueryTime;
		Location = InLocation;
		bValid = true;
	}

private:
	double QueryTime = 0.0;
	FVector Location = FVector::ZeroVector;
	bool bValid = false;
};

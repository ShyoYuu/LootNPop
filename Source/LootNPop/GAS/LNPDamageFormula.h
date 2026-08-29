// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Math/UnrealMathUtility.h"

namespace LNPDamage
{
	const float DEFENSE_CONSTANT = 100.f;

	inline float ApplyDefense(float RawDamage, float Defense)
	{
		float calcDamage = FMath::Max(0.f, RawDamage);
		float calcDefense = FMath::Max(0.f, Defense);

		return calcDamage * (DEFENSE_CONSTANT / (DEFENSE_CONSTANT + calcDefense));
	}
}

namespace LNPPoise
{
	const float RESISTANCE_CONSTANT = 100.f;

	/**
	 * 경직저항력을 적용해 실제로 누적될 경직력을 구한다. 방어력과 같은 감쇠식이다 —
	 * 저항이 유입량을 나누므로 경직 임계값은 전역 상수 하나로 둘 수 있다.
	 */
	inline float ApplyResistance(float RawPoise, float Resistance)
	{
		const float CalcPoise      = FMath::Max(0.f, RawPoise);
		const float CalcResistance = FMath::Max(0.f, Resistance);

		return CalcPoise * (RESISTANCE_CONSTANT / (RESISTANCE_CONSTANT + CalcResistance));
	}
}

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

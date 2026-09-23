// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Engine/EngineTypes.h"

/**
 * DefaultEngine.ini에 예약된 Terrain Contract trace channel.
 * 슬롯 번호를 코드 곳곳에 흩뿌리지 않도록 이 상수만 사용한다.
 */
namespace LNPCollisionChannels
{
	/** 에디터 Support bake와 support source 검증. */
	inline constexpr ECollisionChannel SurfaceSupport = ECC_GameTraceChannel1;

	/** 벽·천장·프랍·동적 지형을 포함하는 정밀 world query. */
	inline constexpr ECollisionChannel WorldExact = ECC_GameTraceChannel2;
}

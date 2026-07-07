// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassEntityManager.h"
#include "Engine/World.h"

namespace LNPMass
{
	/**
	 * 서버 전용 시뮬레이션 Processor의 클라이언트 실행 차단 가드.
	 *
	 * MassReplication(Phase 6~7) 이후 클라이언트 월드에도 서버와 동일한 아키타입의 엔티티가
	 * 존재하므로, AI·이동·HP 판정 등 authoritative 로직의 Execute 첫 줄에서 이 가드로
	 * 조기 반환해야 한다. 클라이언트 측 결과는 각 시스템의 복제 채널(bubble, Actor 복제,
	 * GAS Attribute)이 전달한다.
	 */
	inline bool IsClientWorld(const FMassEntityManager& EntityManager)
	{
		const UWorld* World = EntityManager.GetWorld();
		return World && World->GetNetMode() == NM_Client;
	}
}

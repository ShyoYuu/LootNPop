// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;

/**
 * 적 사격의 사선(LoS) 게이트(SurfaceSupportNavigation Phase03c §3.2, D-052).
 *
 * - 판정만 한다. 막혔을 때의 반응(재배치)은 이동 도메인마다 다르므로 호출자가 맡는다.
 * - Mass fragment를 받지 않는 순수 함수다. 순수 엔티티 공격 프로세서가 부르고, 지상 원거리형·ActorPromoted 엘리트도 같은 함수를 쓴다.
 * - 모든 스레드에서 호출할 수 있다(동기 exact query, D-025). query 분류는 EnemyLineOfSight다.
 */
namespace LNPEnemyLineOfSight
{
	/**
	 * 총구 → 조준점 사이가 트였는가. exact raycast 1회.
	 * Pawn은 LNPWorldExact에 응답하지 않으므로 타겟 자신은 사선을 막지 않는다. Unknown hit도 막힘이다(D-037).
	 */
	LOOTNPOP_API bool HasClearShot(const ULNPMassWorldCollisionSubsystem& Collision, const FVector& Muzzle, const FVector& AimPoint);
}

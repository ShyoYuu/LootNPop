// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ALNPCharacterBase;
class ULNPWeaponData;

/**
 * 발사 기하의 **단일 정의** — 총구 위치와 조준 방향.
 *
 * 실탄을 쏘는 어빌리티(`ULNPAbility_RangedAttack`)와 예상 궤도를 그리는 ADS 가이드
 * (`ULNPTrajectoryGuideComponent`)가 반드시 같은 값을 봐야 한다. 가이드가 자기 나름대로
 * 총구·조준선을 다시 계산하면 그 순간부터 "가이드가 가리키는 곳"과 "실제 착탄"이 갈린다
 * (→ TechDesign_HitDetection.md §7.6·§7.7).
 */
namespace LNPFireGeometry
{
	/** 무기 메시의 Muzzle 소켓 + WeaponDef->MuzzleOffset. 소켓이 없으면 ActorLocation 폴백. */
	FVector ResolveMuzzleLocation(const ALNPCharacterBase& Character, const ULNPWeaponData& WeaponDef);

	/**
	 * 총구에서 조준점으로 수렴시킨 발사 방향. 조준점이 없거나(nullptr) 검증에 걸리면 시선 방향 폴백.
	 *
	 * 입력을 캐릭터에서 꺼내지 않고 인자로 받는다 — 서버는 발동 RPC에 실려 온 **발사 순간의** 조준점·시선을,
	 * 로컬(가이드)은 방금 계산한 값을 넘긴다. 캐릭터에서 읽으면 서버는 과거 InputCmd를 보게 된다.
	 */
	FVector ResolveAimDirection(const FVector& SpawnPos, const FVector& ViewDirection, const FVector* AimTarget);
}

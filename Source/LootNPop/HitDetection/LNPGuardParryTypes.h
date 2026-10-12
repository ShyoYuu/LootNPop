// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPGuardParryTypes.generated.h"

/**
 * 패링 가능 엔티티(액터 상태)에 부착하는 Fragment.
 * 현재는 Player 엔티티에 사용; 향후 Enemy Actor 패링 지원 시 Enemy 엔티티에도 추가.
 *
 * bIsParrying / bIsGuarding 관리:
 *   - Player: ULNPInputHandlerComponent::OnGuardStarted/Released 에서 ASC 태그와 동시에 갱신
 *   - Enemy Actor: StateTree 또는 GA에서 갱신 (향후 지원)
 */
USTRUCT()
struct LOOTNPOP_API FLNPParryStateFragment : public FMassFragment
{
	GENERATED_BODY()

	bool  bIsParrying   = false;   // 패링 창 활성 여부 (TAG_State_ParryWindow 미러)
	bool  bIsGuarding   = false;   // 가드 중 여부 (TAG_State_Guarding 미러)
	float ParryAngleCos = 0.707f;  // cos(45°)
	float GuardAngleCos = 0.5f;    // cos(60°)

	/**
	 * 서버 전용: 패링 창의 RTT 역보정 절대 만료 시각(World->GetTimeSeconds() 기준).
	 * -1 = 서버 미기록(레거시/로컬 전용 갱신). Server_SetGuardState가 방어자 RTT/2만큼 앞당겨 계산한다.
	 * HitDetectionProcessor는 bIsParrying과 함께 이 값도 확인해 지연을 보정한다.
	 */
	double ParryWindowExpiryTime = -1.0;
};

/** 플레이어 피격 대상 판정 결과. */
enum class ELNPPlayerDefenseResult : uint8
{
	None,     // 닿지 않았다
	Parried,
	Guarded,
	Hit,
};

namespace LNPGuardParry
{
	/**
	 * 플레이어 대상의 **패링 → 가드 → 피격** 순서를 정하는 단일 정의 (TechDesign_HitDetection.md §7.5).
	 * 근접 PvP·Enemy→Player와 화염 원뿔(아군 사격)이 이 함수 하나를 공유한다 — 분기를 복제했던 시절
	 * PvP 쪽에만 패링 체크가 빠진 버그가 있었다.
	 *
	 * 기하 판정(칼날 거리, 원뿔 형상)은 호출자 몫이다. 여기서는 그 결과만 받는다.
	 * @param FacingDot     피격자 정면 · (피격자 → 공격자) 방향
	 * @param bInParryRange 패링 반경 안인가 — 패링할 수 없는 공격(화염 틱)은 false를 넘긴다
	 * @param bInHitRange   피격 반경 안인가
	 */
	inline ELNPPlayerDefenseResult Judge(const FLNPParryStateFragment& PS, const float FacingDot, const double Now,
		const bool bInParryRange, const bool bInHitRange)
	{
		if (bInParryRange && PS.bIsParrying && (PS.ParryWindowExpiryTime < 0.0 || Now <= PS.ParryWindowExpiryTime)
			&& FacingDot >= PS.ParryAngleCos)
			return ELNPPlayerDefenseResult::Parried;

		if (!bInHitRange)
			return ELNPPlayerDefenseResult::None;

		if (PS.bIsGuarding && FacingDot >= PS.GuardAngleCos)
			return ELNPPlayerDefenseResult::Guarded;

		return ELNPPlayerDefenseResult::Hit;
	}
}

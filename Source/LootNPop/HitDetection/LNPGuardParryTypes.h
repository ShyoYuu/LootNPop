// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPGuardParryTypes.generated.h"

class AActor;

/**
 * 방어 상태(가드·패링·회피)를 담는 Fragment. 이름은 패링 시절 그대로 둔다 — 판정 쿼리와 미러 경로가 이 이름을 쓴다.
 * 현재는 Player 엔티티에 사용; 향후 Enemy Actor 패링 지원 시 Enemy 엔티티에도 추가.
 *
 * bIsParrying / bIsGuarding 관리:
 *   - Player: ULNPInputHandlerComponent::OnGuardStarted/Released 에서 ASC 태그와 동시에 갱신
 *   - Enemy Actor: StateTree 또는 GA에서 갱신 (향후 지원)
 *
 * 회피 창은 ULNPCharacterMoverComponent::ExecuteDash가 만료 시각으로 기록한다 (.context/JustDodge/design/Architecture.md §2).
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

	/**
	 * 대시 무적 창의 절대 만료 시각(World->GetTimeSeconds() 기준). -1 = 없음.
	 * 권위(서버)와 오토노머스(소유 클라)만 기록한다 — 서버 값은 판정에, 소유 클라 값은 예측 연출 생략에만 쓰인다.
	 * 해제 경로가 없다: 시각 비교만 하므로 가드 bool 미러처럼 내리는 곳이 빠져 남을 일이 없다.
	 */
	double DodgeIFrameExpiryTime = -1.0;

	/**
	 * 저스트 회피 창의 절대 만료 시각. 기록 규칙은 DodgeIFrameExpiryTime과 같고, 창 길이는 무적 창 이하로 묶인다 —
	 * 그래서 저스트 창이 열려 있으면 무적 창도 열려 있다.
	 */
	double JustDodgeExpiryTime = -1.0;

	/**
	 * 이번 대시의 저스트 회피를 이미 썼는가 (대시 한 번에 한 번, J-011). 대시가 false로 되돌린다.
	 * ⚠️ 판정 프로세서(Worker Thread)는 읽기만 한다 — 세우는 것은 Game Thread의 성공 커맨드 몫이다.
	 */
	bool bJustDodgeConsumed = false;

	/**
	 * 서버 전용: 이 피격자의 Hit·Guarded 효과를 보류할 길이(초). 0이면 즉시 적용한다(호스트·적·유예 꺼짐).
	 * 원격 클라이언트 피격자만 0보다 크다 — 늦게 도착한 대시가 소급 회피할 틈이다 (.context/JustDodge/phases/Phase04_HitGraceBuffer.md).
	 * ULNPHitGraceSubsystem이 매 틱 Game Thread에서 쓴다. 판정 프로세서가 PlayerState를 읽지 않게 하는 미러다.
	 */
	float HitGraceSeconds = 0.f;
};

/** 플레이어 피격 대상 판정 결과. */
enum class ELNPPlayerDefenseResult : uint8
{
	None,     // 닿지 않았다
	Parried,
	JustDodged, // 저스트 회피 — 피해 없이 버프를 받는다. 무적 창도 열려 있다
	Dodged,   // 대시 무적으로 무시했다 — 피해·경직·큐 모두 없다
	Guarded,
	Hit,
};

namespace LNPGuardParry
{
	/**
	 * Owner(플레이어 폰)의 Mass 엔티티에서 방어 상태 Fragment를 찾는다. 엔티티가 아직 없으면 null.
	 * UMassAgentComponent만 쓰므로 서버의 원격 폰과 소유 클라이언트 양쪽에서 동작한다.
	 */
	LOOTNPOP_API FLNPParryStateFragment* FindFragment(const AActor* Owner);

	/** LNP.Debug.Dodge(서버 로그)가 켜져 있는가. 판정 프로세서(Worker Thread)에서 불러도 된다. */
	LOOTNPOP_API bool IsDodgeDebugEnabled();

	/** LNP.Debug.ShowDodge(화면 문구)가 켜져 있는가. */
	LOOTNPOP_API bool IsDodgeScreenEnabled();

	/**
	 * 서버 판정이 무적으로 피격을 무시했음을 알린다 — LNP.Debug.Dodge면 로그, LNP.Debug.ShowDodge면 화면 문구.
	 * 둘 다 꺼져 있으면 아무것도 하지 않는다. Worker Thread에서 불러도 된다(화면 문구는 Game Thread로 넘긴다).
	 * 문구가 뜨면 "무적이 없었다면 맞았을 공격"이다 — 이동만으로 빗나간 공격과 플레이 중에 구분하는 용도.
	 */
	LOOTNPOP_API void ReportDodgeIgnored(const TCHAR* Path, const AActor* Victim);

	/**
	 * 대시가 무적·저스트 창을 열었음을 알린다 — LNP.Debug.Dodge면 로그, LNP.Debug.ShowDodge면 로컬 폰의 화면에 무적 창 길이만큼 문구. Game Thread 전용.
	 * @param PressTime 서버가 원격 게스트의 누름을 추정한 시각(피격 유예). 0 미만이면 추정하지 않았다 — 로그에 press를 남기지 않는다.
	 */
	LOOTNPOP_API void ReportDodgeWindowOpened(const AActor* Owner, double ExpiryTime, float Duration, float JustDuration, double PressTime = -1.0);

	/**
	 * 보류된 피격을 늦게 도착한 대시가 소급 회피했음을 알린다 — LNP.Debug.Dodge면 로그, LNP.Debug.ShowDodge면 화면 문구. Game Thread 전용.
	 * @param Result      "Dodged" | "JustDodged"
	 * @param HeldSeconds 피격 판정에서 대시 실행까지 보류된 시간
	 */
	LOOTNPOP_API void ReportRetroDodge(const TCHAR* Result, const TCHAR* Path, const AActor* Victim, double HeldSeconds);

	/** 클라 Ghost가 무적 때문에 예측 임팩트를 생략했음을 알린다 — LNP.Debug.Dodge면 로그. 판정 프로세서(Worker Thread)에서 불러도 된다. */
	LOOTNPOP_API void ReportGhostImpactSkipped(const AActor* Victim);

	/**
	 * 저스트 회피 대상 공격(근접·투사체 직격)이 그대로 맞았음을 알린다 — LNP.Debug.Dodge면 로그만. Worker Thread에서 불러도 된다.
	 * "놓친 저스트 회피"를 세는 계측이다(.context/JustDodge/phases/Phase03_TuningLatency.md 단위 1).
	 * 이른 대시는 sinceJust(저스트 창이 닫힌 뒤 경과 ms)로, 늦은 대시는 이 줄 뒤의 서버 IFrame open과 짝지어 집계한다.
	 */
	LOOTNPOP_API void ReportDodgeHit(const TCHAR* Path, const AActor* Victim, const FLNPParryStateFragment& PS, double Now);

	/** 서버가 저스트 회피를 확정했음을 알린다 — LNP.Debug.Dodge면 로그, LNP.Debug.ShowDodge면 화면 문구. Game Thread 전용(성공 커맨드). */
	LOOTNPOP_API void ReportJustDodgeSuccess(const TCHAR* Attack, const AActor* Defender);

	/** 저스트 회피가 아직 가능한가 — 창이 열려 있고 이번 대시에서 쓰지 않았다. */
	inline bool IsJustDodgeOpen(const FLNPParryStateFragment& PS, const double Now)
	{
		return Now <= PS.JustDodgeExpiryTime && !PS.bJustDodgeConsumed;
	}

	/**
	 * Judge가 bInParryRange를 볼 상태인가. 패링 반경 계산이 비싼 경로(투사체)는 이 값이 false면 계산을 건너뛴다 —
	 * 조건을 경로마다 복제하면 Judge에 새 반경 소비자가 생길 때 한쪽이 빠진다.
	 */
	inline bool NeedsParryRange(const FLNPParryStateFragment& PS, const double Now)
	{
		return PS.bIsParrying || IsJustDodgeOpen(PS, Now);
	}

	/**
	 * 플레이어 대상의 **패링 → 저스트 회피 → 회피 무적 → 가드 → 피격** 순서를 정하는 단일 정의 (TechDesign_HitDetection.md §7.5).
	 * 근접 PvP(다른 팀 플레이어)·Enemy→Player와 화염 원뿔이 이 함수 하나를 공유한다 — 분기를 복제했던 시절
	 * PvP 쪽에만 패링 체크가 빠진 버그가 있었다.
	 *
	 * 기하 판정(칼날 거리, 원뿔 형상)은 호출자 몫이다. 여기서는 그 결과만 받는다.
	 * @param FacingDot     피격자 정면 · (피격자 → 공격자) 방향
	 * @param bInParryRange 패링 반경 안인가 — 저스트 회피도 이 반경을 쓴다. 패링·저스트 회피 대상이 아닌 공격(화염 틱·스플래시)은 false를 넘긴다
	 * @param bInHitRange   피격 반경 안인가
	 */
	inline ELNPPlayerDefenseResult Judge(const FLNPParryStateFragment& PS, const float FacingDot, const double Now,
		const bool bInParryRange, const bool bInHitRange)
	{
		if (bInParryRange && PS.bIsParrying && (PS.ParryWindowExpiryTime < 0.0 || Now <= PS.ParryWindowExpiryTime)
			&& FacingDot >= PS.ParryAngleCos)
			return ELNPPlayerDefenseResult::Parried;

		// 피격 반경 검사보다 앞이다 — 패링 반경 안까지 들어온 "아슬아슬하게 빗나간" 공격도 저스트 회피다. 각도는 보지 않는다(J-004).
		if (bInParryRange && IsJustDodgeOpen(PS, Now))
			return ELNPPlayerDefenseResult::JustDodged;

		if (!bInHitRange)
			return ELNPPlayerDefenseResult::None;

		// 회피는 전방향이라 각도를 보지 않는다. 가드보다 앞이라 가드 중 대시해도 경직이 쌓이지 않는다.
		if (Now <= PS.DodgeIFrameExpiryTime)
			return ELNPPlayerDefenseResult::Dodged;

		if (PS.bIsGuarding && FacingDot >= PS.GuardAngleCos)
			return ELNPPlayerDefenseResult::Guarded;

		return ELNPPlayerDefenseResult::Hit;
	}

	/**
	 * 피격 유예로 보류된 피격(Hit·Guarded)을 늦게 도착한 대시가 소급 회피하는가 — Judge의 저스트 회피·무적 단계를
	 * 서버 실행 시각이 아니라 **누름 추정 시각** 기준으로 다시 본다 (.context/JustDodge/phases/Phase04_HitGraceBuffer.md 단위 3).
	 *
	 * 반경은 다시 보지 않는다 — 보류된 것은 피격 반경 안이었고, 패링 반경은 피격 반경보다 크다.
	 * Guarded도 소급된다. Judge 순서(J-006)에서 회피가 가드보다 앞이기 때문이다.
	 * @param bJustEligible 저스트 회피 대상 공격인가(근접·직격, J-002)
	 * @return JustDodged / Dodged. 소급하지 않으면 None
	 */
	inline ELNPPlayerDefenseResult JudgeRetro(const double HitTime, const double PressTime, const float IFrameDuration,
		const float JustWindow, const bool bJustEligible, const bool bJustConsumed)
	{
		if (HitTime < PressTime || HitTime > PressTime + IFrameDuration)
			return ELNPPlayerDefenseResult::None;

		if (bJustEligible && !bJustConsumed && HitTime <= PressTime + JustWindow)
			return ELNPPlayerDefenseResult::JustDodged;

		return ELNPPlayerDefenseResult::Dodged;
	}
}

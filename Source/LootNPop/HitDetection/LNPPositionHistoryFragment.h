// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPPositionHistoryFragment.generated.h"

/** Lag Compensation용 위치 스냅샷 1개. */
USTRUCT()
struct FLNPPositionHistorySample
{
	GENERATED_BODY()

	double  Timestamp = 0.0;   // World->GetTimeSeconds() 기준
	FVector Location  = FVector::ZeroVector;
};

/**
 * 서버 전용 위치 히스토리 링버퍼. Enemy NPC·플레이어 캐릭터 아키타입에 부착된다.
 * ULNPPositionHistoryRecordProcessor가 50ms 간격으로 기록하며, 최대 200ms(5샘플) 되감기를 지원한다.
 * 클라이언트에서는 기록하지 않는다 (Lag Compensation은 서버 판정 전용).
 */
USTRUCT()
struct LOOTNPOP_API FLNPPositionHistoryFragment : public FMassFragment
{
	GENERATED_BODY()

	/**
	 * 링버퍼 길이. 덮는 시간 = (MaxSamples - 1) x RecordInterval(50ms) = 500ms.
	 *
	 * 예산은 되감기 두 항의 합이다: 핑 항 상한 `LNPHitDetection::MaxPingRewindSeconds`(0.2초)
	 * + 보간 지연 항 최댓값(복제 LOD Low의 갱신 주기 0.3초) = 0.5초.
	 * ⚠️ 모자라면 `GetInterpolatedLocation`이 **조용히** 최고령 샘플로 클램프한다 — 에러가 나지 않는다.
	 * 아래 .cpp의 static_assert가 두 상수와의 관계를 못 박는다.
	 */
	static constexpr int32 MaxSamples = 11;

	FLNPPositionHistorySample Samples[MaxSamples];
	int32  Count          = 0;   // 채워진 샘플 수 (포화 전에는 MaxSamples 미만)
	int32  NextWriteIdx   = 0;   // 다음에 덮어쓸 링버퍼 인덱스
	double LastRecordTime = 0.0;

	/**
	 * QueryTime 시점의 위치를 샘플 사이 선형 보간해 반환한다. 범위를 벗어나면 최고령/최신 샘플로 클램프한다.
	 *
	 * ⚠️ **호출 전에 `Count > 0`을 확인할 것.** 샘플이 하나도 없으면 돌려줄 위치가 없어 `ZeroVector`를
	 * 반환하는데, 구 내벽 월드에서 그것은 "원점"이 아니라 **행성 중심**이다. 되감기 델타
	 * (`과거 위치 - 현재 위치`)에 그대로 넣으면 판정 캡슐이 반지름만큼 아래로 튄다.
	 * 스폰 직후 첫 기록(50ms) 전의 대상이 정확히 이 창에 걸린다.
	 */
	FVector GetInterpolatedLocation(double QueryTime) const;
};

namespace LNPHitDetection
{
	/**
	 * 되감기 **핑 항**의 상한(초). 공격자 회선이라는 가변·통제 불가 값을 묶어,
	 * 남의 나쁜 회선 때문에 피격자가 엄폐물 뒤에서 죽는 일을 제한하는 공정성 한계선이다.
	 *
	 * ⚠️ **보간 지연 항에는 이 상한을 적용하지 않는다.** 그쪽은 서버 자신의 송신 주기라
	 * 고정값이고 적대적이지도 않다 — 서버가 자기가 늦게 보낸 것을 갚는 데 한계선을 걸 이유가 없다.
	 * 둘을 한 상한으로 묶었더니 Medium 이상에서 대역항(0.2초)만으로 포화해 **핑 보정이 통째로
	 * 잘려나갔다**(= 상한의 원래 목적과 정반대). 그래서 항을 분리한다.
	 */
	inline constexpr float MaxPingRewindSeconds = 0.2f;

	/**
	 * 공격자 화면에 그려진 **순수 엔티티**가 서버 현재보다 얼마나 더 과거인지(초).
	 * 되감기의 `RTT/2`에 더해지는 항이며, `RTT/2`와 달리 **핑과 무관하게 상존한다.**
	 *
	 * 유도: 클라이언트의 `ULNPMassSmoothingProcessor`는 마지막 두 수신 사이를 `BlendDuration`
	 * (= 실측 수신 간격)에 걸쳐 블렌드한다. 풀어 보면 `TimeSinceUpdate` 항이 상쇄되고
	 * 갱신 주기 하나만 남는다:
	 *
	 *     그려진 위치의 서버 시각 ~= T_send - (BlendDuration - TimeSinceUpdate)
	 *     서버 현재                = T_send + RTT/2 + TimeSinceUpdate
	 *     ----------------------------------------------------------------
	 *     표시 지연                = RTT/2 + BlendDuration
	 *
	 * 그래서 대상의 **복제 LOD 갱신 주기**가 곧 이 값이다. 뷰어(= 공격자) 기준 거리로 티어를 고른다.
	 *
	 * ⚠️ **보간을 타는 대상에만 더할 것.** 플레이어와 LootPod은 Mass 버블로 위치가 오지 않고
	 *    (`FLNPMassClientBubbleHandler::PushSmoothingTarget`의 폴백), 승격 Actor는 스무딩에서
	 *    제외된 뒤 Mover `ForwardPredict`로 현재까지 전방 예측된다. 둘 다 표시 지연이 `RTT/2`뿐이라
	 *    여기서 더하면 **과대 보정**이 된다.
	 *
	 * A/B 측정용으로 CVar `LNP.HitDetection.CompensateInterpolationLag` 0을 주면 항상 0을 돌려준다.
	 */
	LOOTNPOP_API float GetInterpolationLagSeconds(float DistSqFromViewer, bool bIgnoreCVar = false);

	/** 뷰어 거리(제곱)에 해당하는 복제 LOD 대역 이름 — 계측 로그·크로스헤어 표시용. */
	LOOTNPOP_API const TCHAR* GetReplicationLODName(float DistSqFromViewer);

	/**
	 * 계측 모드인가 (`LNP.HitDetection.MeasureInterpolationLag`). 근접·원거리 판정이 공용으로 쓴다 —
	 * 두 경로가 같은 스위치로 켜져야 한 판에서 둘을 함께 잴 수 있다.
	 * ⚠️ 켜면 대상 쌍마다 판정을 2회 더 하므로 측정할 때만 켠다.
	 */
	LOOTNPOP_API bool IsMeasuringInterpolationLag();
}

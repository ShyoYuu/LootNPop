// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/NetSerialization.h"
#include "LNPAttackInputTargetData.generated.h"

/*
 * 플레이어 기본 공격의 **발동 순간 입력 스냅샷**.
 *
 * 규약: 공격 판정·보정이 읽는 입력 중 **소유 클라이언트만 아는 값**(카메라 조준점, 락온, 자동 탐색 대상, 이동 입력)은
 * InputCmd가 아니라 **발동 요청에 싣는다.** 소유 클라이언트가 발동 직전에 채워 `TriggerAbilityFromGameplayEvent`로
 * 발동하면, 엔진이 `ServerTryActivateAbilityWithEventData` 한 번에 담아 보내고 서버 `ActivateAbility`의
 * `TriggerEventData`로 그대로 도착한다 (`ALNPPlayerCharacter::TryActivateAttack_Impl`).
 *
 * **왜 InputCmd가 아닌가:**
 *   · 시점 — 서버의 `GetLastInputCmd()`는 NetworkPrediction이 고정 틱에서 **마지막으로 소비한** 커맨드라 입력 버퍼 깊이만큼
 *     과거다. 발동 RPC는 도착 즉시 처리되므로 서버가 그 커맨드를 읽으면 누른 프레임보다 몇 프레임 전 값을 본다
 *     (2P 실측, 근접: 서버만 "이동 입력 중" 13/45 → 이관 후 0/58, 목적지 차이 최대 341cm → 0.81cm).
 *   · 대역폭 — InputCmd 필드는 60Hz × 전송당 6개 중복(`FixedTickInputSendCount`)으로 **조건이 맞는 동안 상시** 나간다
 *     (좌표 하나 약 2.5KB/s). 발동 요청은 공격 1회에 한 번이다.
 *
 * ⚠️ **필드는 반드시 UPROPERTY여야 하고, 커스텀 NetSerialize를 두지 않는다.** Iris의 TargetDataHandle 직렬화기는
 * 파생 타입마다 **리플렉션으로** 디스크립터를 만들고 NetSerialize는 무시한다
 * (`Generating descriptor for struct ... that has custom serialization` 경고). UPROPERTY 없는 필드 + NetSerialize로
 * 만들었더니 타입만 도착하고 내용은 전부 기본값이라 서버가 한 번도 보정을 걸지 않았다(2026-09-15 2P).
 * 양자화는 필드 타입으로 표현한다.
 */

/** 근접 공격 보정의 입력 스냅샷 (`ULNPAbility_MeleeAttack::ApplyMeleeAssist`가 소비). */
USTRUCT()
struct LOOTNPOP_API FLNPMeleeAssistTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 보정 대상의 월드 좌표. 대상이 없으면 0 — 구 내벽 월드에서 원점(행성 중심)은 대상이 될 수 없다. 1cm 양자화. */
	UPROPERTY()
	FVector_NetQuantize TargetLocation = FVector::ZeroVector;

	/** 대상이 락온 지목인가(아니면 자동 탐색). 락온 중에는 카메라가 이미 대상을 추적하므로 회전 보정을 건너뛴다. */
	UPROPERTY()
	bool bLockOn = false;

	/** 발동 순간 이동 입력이 들어오고 있었는가. 이동 입력이 우선이라 위치 보정을 건너뛰고 회전 보정만 남긴다. */
	UPROPERTY()
	bool bHasMoveInput = false;

	virtual UScriptStruct* GetScriptStruct() const override { return FLNPMeleeAssistTargetData::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FLNPMeleeAssistTargetData"); }
};

/** 원거리 발사 방향의 입력 스냅샷 (`ULNPAbility_RangedAttack` → `LNPFireGeometry::ResolveAimDirection`이 소비). */
USTRUCT()
struct LOOTNPOP_API FLNPFireAimTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/**
	 * 크로스헤어 조준점(월드 좌표). 트레이스 결과가 없으면 0.
	 *
	 * **왜 필요한가:** 발사 방향의 원본은 "총구에서 크로스헤어 지점으로"인데, 그 지점은 카메라 트레이스 결과라
	 * **소유 클라이언트만 알 수 있다.** 없으면 서버는 시선 방향으로만 쏘고, 그 광선은 카메라 광선과 **평행**하므로
	 * 총구-카메라 간격만큼 거리와 무관하게 일정하게 빗나간다.
	 *
	 * 1cm 양자화로 충분한 근거:
	 *   · 필요한 범위 = 행성 반경(최대 30,000. 월드 반지름을 int16에 담는 제약상 상한 32,767)
	 *     + 조준 트레이스 거리(50,000) → 최악이 원점에서 약 80,000이다. NetQuantize의 성분 범위(2^20) 안이다.
	 *   · 소비처는 총구에서 이 점으로 향하는 **방향**뿐이다. 1cm 오차의 각오차는 50m에서 0.011°, 1m에서 0.57°다.
	 */
	UPROPERTY()
	FVector_NetQuantize AimTargetLocation = FVector::ZeroVector;

	/**
	 * 발동 순간의 시선 방향(단위 벡터). 조준점이 없거나 검증에 걸릴 때의 폴백이자, **조준점 검증의 기준 축**이다.
	 * 조준점만 최신이고 축을 서버의 과거 커맨드(GetBaseAimRotation)에서 가져오면, 빠르게 조준을 옮기는 순간
	 * 둘이 어긋나 검증에 걸린다 — 그래서 같은 순간의 값을 함께 싣는다. 성분당 16비트(각오차 약 0.003°).
	 */
	UPROPERTY()
	FVector_NetQuantizeNormal ViewDirection = FVector::ZeroVector;

	virtual UScriptStruct* GetScriptStruct() const override { return FLNPFireAimTargetData::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FLNPFireAimTargetData"); }
};

namespace LNPAttackInput
{
	/** 발동 이벤트에서 T 타입 스냅샷을 꺼낸다. 없거나 다른 타입이면 nullptr (NPC·다른 경로로 발동). */
	template <typename T>
	const T* Find(const FGameplayEventData* TriggerEventData)
	{
		if (!TriggerEventData || !TriggerEventData->TargetData.IsValid(0))
			return nullptr;
		const FGameplayAbilityTargetData* Data = TriggerEventData->TargetData.Get(0);
		return (Data && Data->GetScriptStruct() == T::StaticStruct()) ? static_cast<const T*>(Data) : nullptr;
	}
}

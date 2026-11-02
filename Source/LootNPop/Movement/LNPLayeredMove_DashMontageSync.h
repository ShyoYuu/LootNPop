// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "LNPLayeredMove_DashMontageSync.generated.h"

/**
 * 대시 몽타주를 원격 사본에 실어 나르는 동기화 전용 무브. 수명이 대시 길이(DurationMs)로만 정해진다는 점만 부모와 다르다.
 *
 * ⚠️ 부모 FLayeredMove_AnimRootMotion은 재시뮬레이션이 아닌 스텝마다 "메시에서 몽타주가 재생 중인가"를 보고,
 * 아니면 그 자리에서 무브를 끝낸다. ForwardPredict 시뮬레이티드 프록시는 대시를 서버 상태 수신 뒤 재시뮬레이션으로만
 * 지나가서 그때까지 몽타주가 없다. 재시뮬레이션 직후의 첫 예측 스텝이 무브를 끝내 버려 FinalizeFrame에서 몽타주를
 * 재생할 단서가 사라졌다(게스트 화면의 호스트 대시 몽타주 누락, 2026-11-02 2인 PIE).
 *
 * 대시 이동은 같은 틱에 먼저 큐잉된 FLayeredMove_LinearVelocity가 소유한다 — 우선순위·시작 시각이 같으면 먼저 들어간
 * 무브가 Override를 가져가므로 이 무브는 속도에 기여하지 않는다. 그래서 수명을 늘려도 이동은 바뀌지 않는다.
 * 프록시의 재생은 ULNPCharacterMoverComponent::UpdateSyncedMontageState가 이 타입을 보고 한다.
 */
USTRUCT(BlueprintType)
struct FLNPLayeredMove_DashMontageSync : public FLayeredMove_AnimRootMotion
{
	GENERATED_BODY()

public:
	virtual ~FLNPLayeredMove_DashMontageSync() override {}

	// --- FLayeredMoveBase 인터페이스 ---
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp,
		UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// -----------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPLayeredMove_DashMontageSync> : public TStructOpsTypeTraitsBase2<FLNPLayeredMove_DashMontageSync>
{
	enum { WithCopy = true };
};

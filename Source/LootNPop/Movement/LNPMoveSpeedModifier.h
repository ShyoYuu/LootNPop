// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPMoveSpeedModifier.generated.h"

/**
 * MoveSpeed 어트리뷰트(버프 합산 후 최종값)를 이동 속도에 반영하는 상시 Modifier.
 *
 * **MaxSpeed의 단일 소유자다.** Sprint/Guard/ADS Modifier는 Acceleration만 만지고,
 * 기준 속도는 매 틱 여기서 CDO 기준으로 다시 계산한다:
 *   BaseSpeed = AIDesiredSpeed(InputCmd, >0) 또는 CDO.MaxSpeed
 *             | CDO.SprintSpeed (IsSprinting) | CDO.GuardWalkSpeed (IsGuarding) | CDO.ADSWalkSpeed (IsADS)
 *   MaxSpeed  = BaseSpeed × MoveSpeed 어트리뷰트
 *
 * **왜 OnPreMovement(매 틱)인가:** 원래 Sprint/Guard가 라이브 설정에 MaxSpeed를 쓰고 종료 시
 * CDO 원본으로 되돌렸다. 버프를 적용 시점에 한 번만 써 두면 첫 질주가 끝나는 순간 CDO 값으로
 * 복원되며 영구히 사라진다. 매 틱 CDO 기준으로 다시 계산하면 배율이 누적되지 않고(곱셈 폭주 없음)
 * Modifier 실행 순서와도 무관해진다. 그래서 MaxSpeed 대입은 이쪽으로 일원화했다.
 *
 * 배율은 Mover의 예측 상태가 아니라 ASC 어트리뷰트에서 직접 읽는다. 버프가 적용·만료되는
 * 순간에는 서버/클라 적용 틱이 어긋나 짧은 보정이 생길 수 있다 (30초 버프당 2회).
 */
USTRUCT(BlueprintType)
struct FLNPMoveSpeedModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPMoveSpeedModifier();
	virtual ~FLNPMoveSpeedModifier() override {}

	// --- FMovementModifierBase 인터페이스 ---
	virtual void OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep) override;

	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPMoveSpeedModifier> : public TStructOpsTypeTraitsBase2<FLNPMoveSpeedModifier>
{
	enum { WithCopy = true };
};

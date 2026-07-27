// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPMoveSpeedModifier.generated.h"

/**
 * MoveSpeed 어트리뷰트(버프 합산 후 최종값)를 이동 속도에 반영하는 상시 Modifier.
 *
 * **왜 OnPreMovement(매 틱)인가:** Sprint/Guard Modifier는 라이브 설정에 값을 쓰고
 * 종료 시 CDO 원본으로 되돌린다(FLNPSprintModifier::OnEnd 참고). 버프를 적용 시점에
 * 한 번만 써 두면 첫 질주가 끝나는 순간 CDO 값으로 복원되며 영구히 사라진다.
 * 그래서 매 틱 CDO를 기준으로 MaxSpeed를 **다시 계산**한다 —
 *  - CDO에서 새로 계산하므로 배율이 누적되지 않는다 (곱셈 폭주 없음)
 *  - Sprint/Guard의 실행 순서와 무관하고, 그들이 복원해도 다음 틱에 다시 덮어쓴다
 *  - 따라서 Sprint/Guard 코드는 수정할 필요가 없다
 *
 * MaxSpeed만 담당한다. Acceleration은 Sprint/Guard가 계속 소유한다.
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

// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "LNPLayeredMove_LinearVelocity.generated.h"

/**
 * FinishVelocitySettings가 네트워크를 타는 FLayeredMove_LinearVelocity. 동작은 부모와 똑같다.
 *
 * ⚠️ **엔진 우회용이다.** UE 5.8의 `FLayeredMoveFinishVelocitySettings::NetSerialize`(LayeredMove.cpp)는
 * 송신 측이 항상 "설정 없음" 비트만 쓰므로 Mode·ClampVelocity·SetVelocity가 전송되지 않는다.
 * 수신 측 무브는 기본값 MaintainLastRootMotionVelocity로 남고, 롤백 시 이 값이 로컬 무브를 덮어쓴다.
 * 그래서 보정 후 재시뮬레이션이나 ForwardPredict 프록시에서는 Clamp·Set 대신 마지막 속도가 그대로 유지된다.
 *
 * 부모 직렬화 뒤에 설정을 한 번 더 직접 싣는다. Maintain이 아닌 FinishVelocityMode를 쓰는 무브는 이 타입으로 큐잉한다.
 * 엔진이 고쳐지면 이 타입을 지우고 FLayeredMove_LinearVelocity로 되돌리면 된다.
 */
USTRUCT(BlueprintType)
struct FLNPLayeredMove_LinearVelocity : public FLayeredMove_LinearVelocity
{
	GENERATED_BODY()

public:
	virtual ~FLNPLayeredMove_LinearVelocity() override {}

	// --- FLayeredMoveBase 인터페이스 ---
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// -----------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPLayeredMove_LinearVelocity> : public TStructOpsTypeTraitsBase2<FLNPLayeredMove_LinearVelocity>
{
	enum { WithCopy = true };
};

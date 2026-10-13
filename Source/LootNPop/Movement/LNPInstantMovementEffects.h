// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "LNPInstantMovementEffects.generated.h"

/*
 * 위치·회전을 컴포넌트가 아니라 서브스텝 시작 상태에서 쓰는 엔진 인스턴트 이펙트 파생형. 속도·모드는 부모와 똑같다.
 *
 * ⚠️ **엔진 우회용이다.** UE 5.8의 `FJumpImpulseEffect`·`FApplyVelocityEffect`(BasicInstantMovementEffects.cpp)는
 * 출력 위치·회전을 `UpdatedComponent`에서 읽는다. Async 모드는 시뮬레이션 중 컴포넌트를 옮기지 않아 컴포넌트는
 * 화면 프레임마다(FinalizeFrame) 한 번만 따라온다. 한 화면 프레임에 고정 틱이 두 번 돌면 두 번째 틱에서 한 틱 늦고,
 * 재시뮬 중엔 롤백 지점에 멈춰 있다. 이펙트가 그 위치를 쓰면 직전 틱 이동분이 사라진다. 스텝 수는 머신마다 달라
 * 서버·게스트 위치가 한 틱 이동분 갈리고 보정이 났다(2026-10-13 대시 점프 2인 PIE, 약 32cm).
 *
 * 엔진이 고쳐지면 이 타입들을 지우고 부모 타입으로 되돌리면 된다.
 */

/** 점프 임펄스. ULNPCharacterMoverComponent::Jump()가 큐잉한다. */
USTRUCT(BlueprintType)
struct FLNPJumpImpulseEffect : public FJumpImpulseEffect
{
	GENERATED_BODY()

public:
	virtual ~FLNPJumpImpulseEffect() override {}

	// --- FInstantMovementEffect 인터페이스 ---
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPJumpImpulseEffect> : public TStructOpsTypeTraitsBase2<FLNPJumpImpulseEffect>
{
	enum { WithCopy = true };
};

/** 속도 적용(넉백). ULNPCharacterMoverComponent::ApplyKnockback()이 큐잉한다. */
USTRUCT(BlueprintType)
struct FLNPApplyVelocityEffect : public FApplyVelocityEffect
{
	GENERATED_BODY()

public:
	virtual ~FLNPApplyVelocityEffect() override {}

	// --- FInstantMovementEffect 인터페이스 ---
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPApplyVelocityEffect> : public TStructOpsTypeTraitsBase2<FLNPApplyVelocityEffect>
{
	enum { WithCopy = true };
};

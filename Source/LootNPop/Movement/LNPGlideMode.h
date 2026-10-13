// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"
#include "LNPGlideMode.generated.h"

/**
 * 행글라이더 활공 모드. 천천히 일정 속도로 내려가면서 질주 수준 속도로 수평 이동한다.
 *
 * 낙하 모드를 상속해 SimulationTick(충돌 슬라이드·착지 판정)을 그대로 쓴다 — 착지하면 엔진이 지상 모드로
 * 전환하므로 행글라이더가 따로 접는 코드 없이 접힌다. 낙하 태그(Mover_IsFalling)도 물려받아 ABP는 낙하 포즈를,
 * CanDash·CanSprint(접지 요구)는 차단을 그대로 유지한다. 바꾸는 것은 GenerateMove의 수평·수직 속도뿐이다.
 *
 * 전개·접기는 ULNPCharacterMoverComponent가 공중 점프 입력으로 이 모드를 큐잉/해제한다.
 * 모드 이름이 SyncState에 실리므로 활공 상태의 예측·복제·롤백에 추가 상태가 없다.
 */
UCLASS()
class LOOTNPOP_API ULNPGlideMode : public UAsyncFallingMode
{
	GENERATED_BODY()

public:
	ULNPGlideMode(const FObjectInitializer& ObjectInitializer);

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** 수평 최대 속력 (cm/s). CommonSettings->MaxSpeed를 쓰지 않으므로 MoveSpeed 버프·가드 속도의 영향을 받지 않는다. */
	UPROPERTY(Category = "LNP|Glide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float GlideSpeed = 1200.0f;

	UPROPERTY(Category = "LNP|Glide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GlideAcceleration = 3000.0f;

	/** 입력이 없을 때 수평 감속 */
	UPROPERTY(Category = "LNP|Glide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GlideDeceleration = 1000.0f;

	/** 목표 하강 속력 (cm/s). 수직 속도는 이 값으로 수렴한다. */
	UPROPERTY(Category = "LNP|Glide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float GlideDescentSpeed = 300.0f;

	/**
	 * 목표보다 빠르게 떨어지는 중에 펼쳤을 때 하강을 늦추는 감속 (cm/s²).
	 * 목표보다 느리게 떨어지거나 올라가는 중이면 이 값이 아니라 중력으로 목표까지 내려간다 — 그래서 런처로 솟구치다
	 * 펼쳐도 평소처럼 오르다 꺾이고, 접었다 펼치기를 반복해도 고도를 벌 수 없다.
	 */
	UPROPERTY(Category = "LNP|Glide", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GlideCatchDeceleration = 3000.0f;
};

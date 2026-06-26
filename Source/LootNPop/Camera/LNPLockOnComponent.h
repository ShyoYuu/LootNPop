#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPLockOnComponent.generated.h"

class ALNPEnemyCharacter;
class ULNPPawnGravityComponent;
class ULNPControlRotationComponent;

/**
 * 카메라 락온을 관리하는 컴포넌트. ALNPPlayerCharacter에 붙인다.
 * 토글 방식: 비활성 → 화면 중앙에서 가장 가까운 적에 락온 / 활성 → 해제.
 * 보정 델타를 계산하여 ControlRotationComponent에 적립한다.
 * ControlRotationComponent가 이 컴포넌트 이후에 Tick하여 최종 SetControlRotation을 수행한다.
 */
UCLASS(ClassGroup=(LNP), meta=(BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPLockOnComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 락온 토글. 활성 상태면 해제, 비활성이면 최적 타겟으로 락온. */
	void ToggleLockOn();

	bool IsLockOnActive() const { return LockOnTarget.IsValid(); }
	ALNPEnemyCharacter* GetLockOnTarget() const { return LockOnTarget.Get(); }

	/** 최대 락온 탐색 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float MaxLockOnRange = 1000.f;

	/** 화면 절반 너비 기준 비율 (0~1). 이 비율 이내이면 좌우 보정 없음. 예) 0.1 = 화면 너비의 10% */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadzoneRatioX = 0.2f;

	/** 화면 절반 높이 기준 비율 (0~1). 이 비율 이내이면 상하 보정 없음. 좌우보다 크게 설정 권장. */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadzoneRatioY = 0.7f;

	/** 데드존 초과 시 최대 보정 각속도 (도/초) */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float MaxCorrectionDegPerSec = 240.f;

	/** 이 거리를 초과하면 자동 해제 */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float AutoBreakRange = 10000.f;

	/** 락온 중 카메라 전방과 타겟 방향의 최대 허용 각도 (도). 이 각도를 초과하는 시선 이탈은 강제로 되돌린다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float MaxDeviationDeg = 40.f;

	/** 타겟 위치 기준 위쪽 오프셋 (cm). 대략 상체 높이를 바라보게 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float TargetAimHeightOffset = 40.f;

private:
	TWeakObjectPtr<ALNPEnemyCharacter> LockOnTarget;
	TWeakObjectPtr<ULNPPawnGravityComponent> GravityComponent;
	TWeakObjectPtr<ULNPControlRotationComponent> ControlRotationComponent;

	/** 화면 중앙에서 가장 가까운 각도의 적을 반환 */
	ALNPEnemyCharacter* FindBestTarget() const;

	void SetTarget(ALNPEnemyCharacter* NewTarget);
	void ClearTarget();

	/** 타겟이 아직 유효한지 확인 (사망·거리 이탈 체크) */
	bool IsTargetStillValid() const;

	/** GravityComponent 이후에 소프트 회전 보정을 PlayerController에 적용 */
	void ApplySoftRotation(float DeltaTime);
};

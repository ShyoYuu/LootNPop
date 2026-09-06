#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HitDetection/LNPTargetQuerySubsystem.h"
#include "LNPLockOnComponent.generated.h"

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 락온 토글. 활성 상태면 해제, 비활성이면 최적 타겟으로 락온. */
	void ToggleLockOn();

	bool IsLockOnActive() const { return LockOnEntity.IsSet(); }
	/** 락온 대상의 최신 월드 좌표. 락온 중이 아니면 false. (InputCmd로 서버에 실려 근접 보정이 읽는다) */
	bool GetLockOnTargetLocation(FVector& OutLocation) const;

	/** 최대 락온 탐색 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float MaxLockOnRange = 1000.f;

	/**
	 * 후보로 인정할 카메라 축 기준 최대 각도. 옛 구현의 "화면 밖 제외"를 대신한다 —
	 * 뷰포트 투영은 게임 스레드 전용이라 Mass 워커에서 부를 수 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float LockOnSearchAngleDeg = 45.f;

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
	float AutoBreakRange = 6000.f;

	/** 락온 중 카메라 전방과 타겟 방향의 최대 허용 각도 (도). 이 각도를 초과하는 시선 이탈은 강제로 되돌린다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float MaxDeviationDeg = 40.f;

	/** 타겟 위치 기준 위쪽 오프셋 (cm). 대략 상체 높이를 바라보게 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LockOn")
	float TargetAimHeightOffset = 40.f;

private:
	/**
	 * 락온 대상은 **엔티티 핸들**이다. Actor 포인터가 아닌 이유는 순수 엔티티(CombatMode::PureEntity)에
	 * Actor가 없기 때문이고, 승격 Actor도 엔티티를 갖고 있으므로 한 경로로 덮인다.
	 */
	FMassEntityHandle LockOnEntity;

	/** 대상의 최신 위치. Track 질의가 매 프레임 갱신한다. */
	FVector LockOnTargetLocation = FVector::ZeroVector;

	/**
	 * 질의 슬롯 하나를 상태에 따라 바꿔 쓴다 — 락온 전에는 Cone(후보 탐색), 락온 중에는 Track(추적).
	 * 락온 중에는 후보 탐색이 필요 없다(토글은 해제만 한다).
	 */
	FLNPTargetQueryHandle QueryHandle;

	TWeakObjectPtr<ULNPPawnGravityComponent> GravityComponent;
	TWeakObjectPtr<ULNPControlRotationComponent> ControlRotationComponent;

	/** 질의 슬롯을 확보하고 이번 프레임 파라미터를 채운다. */
	void UpdateQuery();

	void SetTarget(FMassEntityHandle NewTarget, const FVector& TargetLocation);
	void ClearTarget();


	/** GravityComponent 이후에 소프트 회전 보정을 PlayerController에 적용 */
	void ApplySoftRotation(float DeltaTime);
};

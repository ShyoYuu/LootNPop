// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "MoverTypes.h"
#include "LNPAnimInstance.generated.h"

class ALNPCharacterBase;
class ULNPCharacterMoverComponent;
class UMoverTrajectoryPredictor;
class USkeletalMeshComponent;

/*
 * 로코모션 상태 enum — GASP SandboxCharacter_Mover_ABP의 E_* 와 같은 의미(AnimationOverhaul design/GASPReference.md §2.3·§2.8).
 * 2-2부터 Chooser 행 입력으로 쓴다. GASP에만 있는 값(Sliding·Traversing·Ragdoll·Flying, 방향 LL/LR/RL/RR 구분)은 없다.
 */

UENUM(BlueprintType)
enum class ELNPMovementMode : uint8
{
	OnGround,
	InAir
};

UENUM(BlueprintType)
enum class ELNPMovementState : uint8
{
	Idle,
	Moving
};

UENUM(BlueprintType)
enum class ELNPGait : uint8
{
	Walk,
	Run,
	Sprint
};

UENUM(BlueprintType)
enum class ELNPStance : uint8
{
	Stand,
	Crouch
};

UENUM(BlueprintType)
enum class ELNPRotationMode : uint8
{
	OrientToMovement,
	Strafe,
	Aim
};

/** 캐릭터 정면 기준 이동 방향. GASP 폰의 임계값 판정도 F·B·좌·우 넷만 돌려준다. */
UENUM(BlueprintType)
enum class ELNPMovementDirection : uint8
{
	F,
	B,
	L,
	R
};

/**
 * ULNPAnimInstance
 * LootNPop 프로젝트용 커스텀 AnimInstance 클래스.
 */
UCLASS()
class LOOTNPOP_API ULNPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 오프셋 그립의 왼손이 그립을 떠나야 하는 정도(0~1). LeftHandIKAlpha 주석 참조. */
	float ComputeLeftHandOffDemand() const;

	/** 이 인스턴스가 AnimSourceMesh의 메인 인스턴스인가. 링크 레이어·표시 메시 ABP도 이 클래스를 부모로 쓰므로 로코모션 공급층(B0)은 메인에서만 돈다. */
	bool IsLocomotionSource() const;

	/** 메인 인스턴스가 되면 한 번: 궤적 예측기 생성, based movement 델리게이트 연결. */
	void InitializeLocomotionSource();

	/** B0 공급층 — GASP Update_Logic(Trajectory → EssentialValues → States → AdditiveLean)에 해당. */
	void UpdateLocomotionValues(float DeltaSeconds);
	void UpdateTrajectoryValues(float DeltaSeconds);
	void UpdateStates(float DeltaSeconds);
	void DrawLocomotionDebug() const;

	UFUNCTION()
	void HandleBasedMovementApplied(const FTransform& TransformDelta, const FMoverTimeStep& TimeStep);

	/** Owner 캐릭터 참조 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<ALNPCharacterBase> OwningMoverCharacter;

	/** 캐릭터의 MoverComponent 참조 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	/** 캐릭터의 현재 속도 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	FVector Velocity;

	/** 캐릭터의 수평 속력 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsOnGround;

	/** 캐릭터가 이동 애니메이션을 재생해야 하는지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldMove;

	/** 캐릭터가 현재 낙하 상태인가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsFalling;

	/** 캐릭터가 현재 공중(지면 비접지 상태)에 있는가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsAirborne;

	/** 캐릭터 회전 기준 이동 방향 (-180 ~ 180) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float Direction;

	/** 캐릭터가 스트레이핑 애니메이션을 사용해야 하는가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldStrafe;

	/** 캐릭터가 현재 수영 중인가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSwimming;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsCrouching;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSprinting;

	/** 캐릭터가 현재 가드 중인가? Guard Modifier 활성 여부로 판단. ABP 분기에 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	bool bIsGuarding;

	/** Aim Offset: 카메라 Yaw - 캐릭터 Yaw 차이 (-180 ~ 180) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|AimOffset")
	float AimYaw = 0.0f;

	/** Aim Offset: 카메라 Pitch (-90 ~ 90) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|AimOffset")
	float AimPitch = 0.0f;

	/** 현재 장착 무기 메시 컴포넌트. 왼손 IK 소켓 위치 쿼리에 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp;

	/** 무기의 LeftHandGrip 소켓 월드 위치. Two Bone IK 타겟으로 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	FVector LeftHandGripLocation = FVector::ZeroVector;

	/** 왼손 그립 목표가 있는가(LeftHandGrip 소켓 또는 표현 세트의 그립 오프셋). false면 왼손 IK 비활성. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	bool bHasLeftHandGrip = false;

	/**
	 * 표시 메시(VisualMesh) ABP의 왼손 IK 알파(0~1).
	 * 소켓 그립(롱소드)은 원본 쪽 서브 ABP IK와 같이 항상 1이다.
	 * 오프셋 그립(총기)은 손이 그립을 떠날 수 있는 구간에 내린다 — `AnimSourceMesh`의
	 * `DisableLHandIK` 커브(Lyra 라이플 재장전)와 DefaultSlot·FullBody·UpperBody 몽타주 가중치 중 큰 값만큼.
	 * 사격(FullBodyAdditivePreAim)·피격(AdditiveHitReact)은 가산 슬롯이라 IK를 유지한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	float LeftHandIKAlpha = 0.0f;

	/**
	 * 견착 보정(`ULNPWeaponVisualSet::RightHandCorrection`)을 무기 부착 본(hand_r) 공간으로 옮긴 값(cm).
	 * 플레이어 표시 ABP의 오른팔 Two Bone IK가 "현재 hand_r + 이 값"을 목표로 쓴다(Effector = BoneSpace hand_r).
	 * 무기가 손에 강체로 붙어 있어 프레임마다 같은 값이다(이전 프레임 IK 결과가 되먹임되지 않는다).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	FVector RightHandIKOffset = FVector::ZeroVector;

	/** 오른팔 견착 보정 IK 알파(0~1). 보정이 있을 때만, 왼손 오프셋 그립과 같은 몽타주·커브 차단을 따른다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	float RightHandIKAlpha = 0.0f;

	// ---------------------------------------------------------------------------------------------
	// 로코모션 공급층(B0). 메인 인스턴스에서만 채운다(IsLocomotionSource).
	// 벡터는 월드 공간이고, 이름에 Ground가 붙지 않은 것도 캐릭터 Up 접평면에 투영한 값이다(구면 월드).
	// 계산식·임계값 출처는 AnimationOverhaul design/GASPReference.md §2.8.
	// ---------------------------------------------------------------------------------------------

	/**
	 * 궤적(이력 0.033s×15 + 예측 0.1s×15, GASP 값). Mover 예측기로 만든다.
	 * ABP_Main_UEFN의 PoseHistoryCollector `TransformTrajectory` 핀이 이 이름으로 바인딩돼 있다 — 이름을 바꾸지 말 것.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FTransformTrajectory Trajectory;

	UPROPERTY(Transient)
	TObjectPtr<UMoverTrajectoryPredictor> TrajectoryPredictor;

	float PreviousDesiredControllerYaw = 0.0f;

	/** 접평면 속도가 HasVelocity(> 5)였던 마지막 값 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector LastNonZeroVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	bool bHasVelocity = false;

	/** 입력 가속 = Mover 이동 의도(접평면) × 최대 가속 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector Acceleration = FVector::ZeroVector;

	/** |입력 가속| / 최대 가속(0~1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	float AccelerationAmount = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	bool bHasAcceleration = false;

	/** 속도 차분 가속(접평면, cm/s²) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector VelocityAcceleration = FVector::ZeroVector;

	/** VelocityAcceleration을 캐릭터 로컬로 옮긴 값 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector RelativeAcceleration = FVector::ZeroVector;

	/** 직전 프레임 3D 속도(착지 판정용, 투영하지 않음) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector Velocity_LastFrame = FVector::ZeroVector;

	/** 착지 직전 3D 속도. Up 성분이 HeavyLandSpeedThreshold 이하면 무거운 착지 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector LandVelocity = FVector::ZeroVector;

	/** 바닥 법선(바닥이 없으면 캐릭터 Up) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	FVector GroundNormal = FVector::UpVector;

	/** GroundNormal을 VInterpTo(속도 5)로 평활한 값 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	FVector SmoothedGroundNormal = FVector::UpVector;

	/** 바닥 접점(바닥이 없으면 메시 위치) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	FVector GroundLocation = FVector::ZeroVector;

	/** 경사 각도(도). X = 좌우 기울기(Roll), Y = 전후 기울기(Pitch). GetSlopeDegreeAngles(캐릭터 Right·Up) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	float SlopeAngle_X = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	float SlopeAngle_Y = 0.0f;

	/** 지난 애니 갱신 이후 움직이는 base가 캐릭터에 준 월드 변환 누적(Mover OnBasedMovementApplied). 매 갱신 뒤 비운다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Ground")
	FTransform BasedMovementDelta = FTransform::Identity;

	/** 측면 가속 비율(-1~1)을 이동 방향에 맞춰 Lean 블렌드스페이스 입력으로 옮긴 값(레이어는 2-6) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Locomotion")
	FVector2D LeanAmount = FVector2D::ZeroVector;

	// --- 궤적 파생값 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FVector Trj_PastVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FVector Trj_NearFutureVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FVector Trj_FutureVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FVector Trj_PreviousFutureVelocity = FVector::ZeroVector;

	/** 현재 속도 → 0.4~0.5s 뒤 속도의 회전각(도, Up 축 부호각, +는 오른쪽) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float Trj_TurnAngle = 0.0f;

	/** 1.5s 뒤 예측 facing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	FQuat Trj_FutureFacing = FQuat::Identity;

	/** Up 축 각속도(도/s). 과거 -0.4~-0.3s, 현재 0~0.1s */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float Trj_PastYawRate = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float Trj_CurrentYawRate = 0.0f;

	/** 같은 방향으로 계속 도는 중(두 각속도가 같은 부호로 200°/s 초과, |TurnAngle| > 50) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	bool bTrj_IsCircling = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float Trj_CirclingTime = 0.0f;

	/** 지금 메시 facing부터 예측 0·0.25·0.75·1.5s facing까지 누적 회전각(도, ±180을 넘을 수 있다) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float FutureFacingDelta = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Trajectory")
	float FutureFacingDelta_LastFrame = 0.0f;

	// --- 상태와 이력. _LastFrame = 직전 프레임 값, _Recent = 제한 시간 이상 유지된 마지막 값, _Time = 현재 값 유지 시간 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementMode MovementMode = ELNPMovementMode::OnGround;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementMode MovementMode_LastFrame = ELNPMovementMode::OnGround;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementMode MovementMode_Recent = ELNPMovementMode::OnGround;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float MovementMode_Time = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementState MovementState = ELNPMovementState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementState MovementState_LastFrame = ELNPMovementState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementState MovementState_Recent = ELNPMovementState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float MovementState_Time = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPGait Gait = ELNPGait::Run;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPGait Gait_LastFrame = ELNPGait::Run;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPGait Gait_Recent = ELNPGait::Run;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float Gait_Time = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPStance Stance = ELNPStance::Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPStance Stance_LastFrame = ELNPStance::Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPStance Stance_Recent = ELNPStance::Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float Stance_Time = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPRotationMode RotationMode = ELNPRotationMode::OrientToMovement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPRotationMode RotationMode_LastFrame = ELNPRotationMode::OrientToMovement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPRotationMode RotationMode_Recent = ELNPRotationMode::OrientToMovement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float RotationMode_Time = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementDirection MovementDirection = ELNPMovementDirection::F;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementDirection MovementDirection_LastFrame = ELNPMovementDirection::F;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	ELNPMovementDirection MovementDirection_Recent = ELNPMovementDirection::F;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|States")
	float MovementDirection_Time = 0.0f;

	// --- 상태 술어(AN-003: 분기 판정은 C++가 하고 ABP·Chooser는 읽기만 한다) ---

	/** 궤적 0.9~1.0s 접평면 속도 > 10 → MovementState Moving */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bIsMoving = false;

	/** 움직이는 중이고 0.4~0.5s 뒤 속력이 지금보다 100 이상 크다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bIsStarting = false;

	/** Moving이고 |Trj_TurnAngle| ≥ 75, 회전 중(circling)이 아님 — GASP MM 경로 조건 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bIsPivoting = false;

	/** |FutureFacingDelta| ≥ 50, 속력 < 50 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bShouldTurnInPlace = false;

	/** |FutureFacingDelta| ≥ 130, 속력 ≥ 150 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bShouldSpinTransition = false;

	/** 이번 프레임에 InAir → OnGround, 직전 Up 속도 > -500(가벼운) / ≤ -500(무거운) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bJustLanded_Light = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bJustLanded_Heavy = false;

	/** 이번 프레임 메시 이동 거리 > 50cm + 두 프레임 속력 중 큰 값 × dt(리스폰·텔레포트·큰 보정). 시뮬 프록시 Actor 스냅을 피하려고 메시 위치로 잰다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Predicates")
	bool bJustTeleported = false;

private:
	bool bLocomotionSourceInitialized = false;
	bool bHasLocomotionHistory = false;
	FVector Location_LastFrame = FVector::ZeroVector;
	FVector PreviousVelocity = FVector::ZeroVector;
	/** GASP: 예측 facing 누적각이 한 프레임에 200° 넘게 뛰면(±180 경계 넘김) 방향을 B로 한 번 바꿔 DB 인터럽트를 일으킨다 */
	bool bForceMovementDirectionChange = false;
	FTransform PendingBasedMovementDelta = FTransform::Identity;
	ELNPMovementDirection ComputeMovementDirection(ELNPRotationMode NewRotationMode, ELNPGait NewGait) const;
};

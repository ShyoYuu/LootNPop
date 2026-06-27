// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/LNPAnimInstance.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPPlayerCharacter.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "LootNPop.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "KismetAnimationLibrary.h"
#include "Components/SkeletalMeshComponent.h"

void ULNPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 성능 향상을 위한 참조 Cache
	OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());
	if (OwningMoverCharacter != nullptr)
	{
		MoverComponent  = OwningMoverCharacter->GetMoverComponent();
		WeaponMeshComp  = OwningMoverCharacter->GetWeaponMesh();
	}
}

void ULNPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 아직 Cache되지 않은 경우 캐릭터 Cache 시도 (지연 초기화)
	if (OwningMoverCharacter == nullptr)
	{
		OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());

		if (OwningMoverCharacter != nullptr)
		{
			if (MoverComponent == nullptr)
				MoverComponent = OwningMoverCharacter->GetMoverComponent();
			if (WeaponMeshComp == nullptr)
				WeaponMeshComp = OwningMoverCharacter->GetWeaponMesh();
		}
	}

	if (OwningMoverCharacter == nullptr || MoverComponent == nullptr)
	{
		return;
	}

	// 1. 속도 및 지상 속력 업데이트
	Velocity = MoverComponent->GetVelocity();
	
	/**
	 * 구형 세계 보정:
	 * Size2D()는 World XY 평면에서만 동작한다.
	 * 캐릭터의 로컬 Up 벡터에 수직인 평면으로 속도를 투영해야 한다.
	 */
	FVector CharacterUp = OwningMoverCharacter->GetActorUpVector();
	FVector GroundVelocity = FVector::VectorPlaneProject(Velocity, CharacterUp);
	GroundSpeed = GroundVelocity.Size();

	// 2. 캐릭터가 이동해야 하는지 판단
	// 로직: 충분한 수평 속력과 의도적인 이동 입력이 있는 경우
	bShouldMove = (3.0f < GroundSpeed) && (!MoverComponent->GetMovementIntent().IsZero()); // 

	// 3. MoverComponent를 통한 공중 및 낙하 상태
	bIsOnGround = MoverComponent->IsOnGround();
	bIsAirborne = MoverComponent->IsAirborne();
	bIsFalling = MoverComponent->IsFalling();
	bIsSwimming = MoverComponent->IsSwimming();
	bIsCrouching  = MoverComponent->IsCrouching();
	bIsSprinting  = MoverComponent->IsSprinting();
	bIsGuarding   = MoverComponent->IsGuarding();

	// 4. 이동 방향 계산
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningMoverCharacter->GetActorRotation());

	// 5. 캐릭터가 스트레이핑해야 하는지 판단
	// 이동 방향으로 회전하지 않으면 스트레이핑 중으로 간주 (예: 타겟을 보며 이동).
	bShouldStrafe = !OwningMoverCharacter->GetFaceMoveDirection();

	// 6. Aim Offset용 Yaw/Pitch 계산
	// GetBaseAimRotation()은 플레이어의 경우 Control Rotation, AI는 별도 로직을 따름.
	// 구형 세계 보정: 월드 공간 Euler 각도 뺄셈은 캐릭터 Up이 World Up과 벌어질 때 오차가 생긴다.
	// 에임 방향 벡터를 캐릭터 로컬 좌표계로 변환한 뒤 Pitch/Yaw를 추출해 항상 올바른 로컬 에임 각도를 얻는다.
	FRotator BaseAimRotation = OwningMoverCharacter->GetBaseAimRotation();
	FVector LocalAimDir = OwningMoverCharacter->GetActorTransform().InverseTransformVectorNoScale(BaseAimRotation.Vector());
	FRotator LocalAimRot = LocalAimDir.Rotation();
	AimPitch = FMath::ClampAngle(LocalAimRot.Pitch, -90.0f, 90.0f);
	AimYaw   = FRotator::NormalizeAxis(LocalAimRot.Yaw);

	// 7. 왼손 IK 타겟 위치 업데이트
	static const FName LeftHandGripSocketName(TEXT("LeftHandGrip"));
	if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(LeftHandGripSocketName))
	{
		bHasLeftHandGrip    = true;
		LeftHandGripLocation = WeaponMeshComp->GetSocketLocation(LeftHandGripSocketName);
	}
	else
	{
		bHasLeftHandGrip = false;
	}
}

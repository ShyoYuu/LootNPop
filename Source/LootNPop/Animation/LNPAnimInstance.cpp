// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/LNPAnimInstance.h"
#include "Character/LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "KismetAnimationLibrary.h"

void ULNPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 성능 향상을 위한 참조 Cache
	OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());
	if (OwningMoverCharacter != nullptr)
	{
		MoverComponent = OwningMoverCharacter->GetMoverComponent();
	}
}

void ULNPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 아직 Cache되지 않은 경우 캐릭터 Cache 시도 (지연 초기화)
	if (OwningMoverCharacter == nullptr)
	{
		OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());

		if (MoverComponent == nullptr && OwningMoverCharacter != nullptr)
		{
			MoverComponent = OwningMoverCharacter->GetMoverComponent();
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
	bIsCrouching = MoverComponent->IsCrouching();
	bIsSprinting = MoverComponent->IsSprinting();

	// 4. 이동 방향 계산
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningMoverCharacter->GetActorRotation());

	// 5. 캐릭터가 스트레이핑해야 하는지 판단
	// 이동 방향으로 회전하지 않으면 스트레이핑 중으로 간주 (예: 타겟을 보며 이동).
	bShouldStrafe = !OwningMoverCharacter->GetFaceMoveDirection();
}

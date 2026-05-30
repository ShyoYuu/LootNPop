// Fill out your copyright notice in the Description page of Project Settings.


#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "SmartObjectComponent.h"
#include "NiagaraComponent.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"

// 기본값 설정
ALNPLootPod::ALNPLootPod()
{
	// 매 프레임 Tick() 호출 설정.
	PrimaryActorTick.bCanEverTick = true;

	// 1. Static Mesh (루트)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	// 2. Smart Object Component
	SmartObjectComponent = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObjectComponent"));

	// 3. 루팅 구역 구체
	LootingZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("LootingZoneSphere"));
	LootingZoneSphere->SetupAttachment(RootComponent);
	LootingZoneSphere->SetSphereRadius(500.0f); 
	LootingZoneSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	LootingZoneSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 4. Niagara VFX (루트 기둥)
	LootPillarVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LootPillarVFX"));
	LootPillarVFX->SetupAttachment(RootComponent);

	// 5. Mass Agent Component (MassEntity 연결)
	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));
}

void ALNPLootPod::BeginPlay()
{
	Super::BeginPlay();
	
	// 초기 비주얼 상태 설정
	UpdateVisuals(ELNPLootPodState::Idle);
}

void ALNPLootPod::StartLooting()
{
	// 로컬 상태 및 비주얼 업데이트
	UpdateVisuals(ELNPLootPodState::Looting);
}

void ALNPLootPod::UpdateVisuals(ELNPLootPodState NewState)
{
	CurrentState = NewState;
	if (!LootPillarVFX)
		return;

	FLinearColor TargetColor = IdleColor;

	switch (NewState)
	{
	case ELNPLootPodState::Looting:
		TargetColor = LootingColor;
		break;
	case ELNPLootPodState::Popped:
		TargetColor = PoppedColor;
		break;
	case ELNPLootPodState::Idle:
	default:
		TargetColor = IdleColor;
		break;
	}

	LootPillarVFX->SetVariableLinearColor(ColorParameterName, TargetColor);
}

bool ALNPLootPod::CanInteract_Implementation(const APawn* Interactor) const
{
	if (!Interactor)
		return false;

	// Idle이거나 이미 Looting 중인 경우만 상호작용 허용 (멀티Player)
	if (CurrentState == ELNPLootPodState::Popped || CurrentState == ELNPLootPodState::Interrupted)
	{
		return false;
	}

	// 1. 거리 체크
	const float DistSq = FVector::DistSquared(GetActorLocation(), Interactor->GetActorLocation());
	const float MaxDist = LootingZoneSphere->GetScaledSphereRadius();
	
	if (DistSq > FMath::Square(MaxDist))
	{
		return false;
	}

	// 2. 각도 체크
	const FVector DirToInteractor = (Interactor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(GetActorForwardVector(), DirToInteractor);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	if (AngleDeg > MaxInteractionAngle)
	{
		return false;
	}

	return true;
}

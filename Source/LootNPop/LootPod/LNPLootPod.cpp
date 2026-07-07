// Copyright (c) 2026 LootNPop. All rights reserved.


#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "SmartObjectComponent.h"
#include "NiagaraComponent.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "Net/UnrealNetwork.h"

// 기본값 설정
ALNPLootPod::ALNPLootPod()
{
	// 매 프레임 Tick() 호출 설정.
	PrimaryActorTick.bCanEverTick = true;

	// Actor 복제 (Phase 7 §5.4): PodState·게이지를 근접 클라이언트에 전파한다.
	// 엔티티 존재·초기 위치는 MassReplication bubble이 전 클라이언트에 담당 (이중 복제).
	bReplicates = true;
	SetNetCullDistanceSquared(20000.f * 20000.f);
	// 게이지는 2% 임계값으로만 변하므로 높은 갱신 빈도가 필요 없다
	SetNetUpdateFrequency(10.f);

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

	// 초기 비주얼 상태 설정 — 클라이언트는 이미 복제된 CurrentState를 존중한다
	// (BeginPlay와 초기 OnRep의 실행 순서가 고정되지 않으므로, 무조건 Idle로 덮으면
	//  먼저 도착한 상태가 유실된다 — Phase 3 무기 레이어 레이스와 동일 유형)
	UpdateVisuals(CurrentState);
}

void ALNPLootPod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Iris 활성화 상태에서도 이 선언이 그대로 Iris Descriptor 생성 입력이 된다
	// (AActor::RegisterReplicationFragments → CreateAndRegisterFragmentsForObject)
	DOREPLIFETIME(ALNPLootPod, CurrentState);
	DOREPLIFETIME(ALNPLootPod, CurrentGaugePercent);
}

void ALNPLootPod::OnRep_PodState()
{
	// 서버가 확정한 상태로 비주얼 갱신 (클라이언트 전용 경로)
	UpdateVisuals(CurrentState);
}

void ALNPLootPod::SetGaugePercent(float NewPercent)
{
	if (!HasAuthority())
		return;

	NewPercent = FMath::Clamp(NewPercent, 0.0f, 1.0f);

	// 2% 미만 변화는 무시하되, 0/1 경계값은 항상 반영한다 (완료·리셋 시점 정확성)
	const bool bBoundary = (NewPercent <= 0.0f || 1.0f <= NewPercent);
	if (!bBoundary && FMath::Abs(NewPercent - CurrentGaugePercent) < 0.02f)
		return;

	CurrentGaugePercent = NewPercent;
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

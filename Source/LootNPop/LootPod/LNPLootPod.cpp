// Copyright (c) 2026 LootNPop. All rights reserved.


#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootPod/LNPLootPodSubsystem.h"
#include "LootNPop.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Interaction/LNPInteractionPromptWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SetRootComponent(SceneComponent);

	// 1. Static Mesh
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

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

	// 6. 상호작용 프롬프트 위젯 — 로컬 판정으로만 표시되므로 복제와 무관, 기본 숨김
	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(RootComponent);
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawAtDesiredSize(true);
	InteractionPromptWidget->SetWidgetClass(ULNPInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetVisibility(false);

	// 7. 루팅 존 홀로그램 링 — 메시(SM_Plane)·머티리얼(M_LootZoneRing)은 BP에서 지정, 기본 숨김
	ZoneRingComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneRingComponent"));
	ZoneRingComponent->SetupAttachment(RootComponent);
	ZoneRingComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneRingComponent->SetVisibility(false);
}

void ALNPLootPod::BeginPlay()
{
	Super::BeginPlay();

	// 상호작용 탐색용 레지스트리 등록 (→ ULNPLootPodSubsystem)
	if (ULNPLootPodSubsystem* PodSubsystem = UWorld::GetSubsystem<ULNPLootPodSubsystem>(GetWorld()))
	{
		PodSubsystem->RegisterPod(this);
	}

	// 존 링 스케일을 루팅 존 반경과 동기화 (SM_Plane 100×100 기준) + 게이지 파라미터 구동용 MID 생성
	if (ZoneRingComponent != nullptr && LootingZoneSphere != nullptr)
	{
		const float RingScale = LootingZoneSphere->GetScaledSphereRadius() * 2.0f / 100.0f;
		ZoneRingComponent->SetRelativeScale3D(FVector(RingScale, RingScale, 1.0f));

		if (UMaterialInterface* RingMaterial = ZoneRingComponent->GetMaterial(0))
		{
			ZoneRingMID = UMaterialInstanceDynamic::Create(RingMaterial, this);
			ZoneRingComponent->SetMaterial(0, ZoneRingMID);
		}
	}

	// 초기 비주얼 상태 설정 — 클라이언트는 이미 복제된 CurrentState를 존중한다
	// (BeginPlay와 초기 OnRep의 실행 순서가 고정되지 않으므로, 무조건 Idle로 덮으면
	//  먼저 도착한 상태가 유실된다 — Phase 3 무기 레이어 레이스와 동일 유형)
	UpdateVisuals(CurrentState);
}

void ALNPLootPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 게이지 진행률을 존 링 머티리얼에 반영 — 복제된 CurrentGaugePercent를 읽으므로 서버·클라이언트 공통
	if (ZoneRingMID != nullptr && CurrentState == ELNPLootPodState::Looting)
	{
		ZoneRingMID->SetScalarParameterValue(TEXT("GaugePercent"), CurrentGaugePercent);
	}
}

void ALNPLootPod::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPLootPodSubsystem* PodSubsystem = UWorld::GetSubsystem<ULNPLootPodSubsystem>(GetWorld()))
	{
		PodSubsystem->UnregisterPod(this);
	}

	Super::EndPlay(EndPlayReason);
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

	// 게이지 진행 테스트 로그 — 10% 단위 구간이 바뀔 때만 출력 (증가·감쇠 모두 추적 가능)
	const int32 Decile = FMath::FloorToInt(NewPercent * 10.0f);
	if (Decile != LastLoggedGaugeDecile)
	{
		LastLoggedGaugeDecile = Decile;
		UE_LOG(LogLootNPop, Log, TEXT("[LootPod] %s gauge %.0f%%"), *GetName(), NewPercent * 100.0f);
	}
}

void ALNPLootPod::StartLooting()
{
	// 로컬 상태 및 비주얼 업데이트
	UpdateVisuals(ELNPLootPodState::Looting);
}

void ALNPLootPod::SetInteractionPromptVisible(bool bVisible)
{
	if (InteractionPromptWidget == nullptr)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[Interaction] %s: InteractionPromptWidget component is null"), *GetName());
		return;
	}

	// 위젯 인스턴스 생성 실패 진단 — WidgetClass 미설정/생성 실패 시 표시가 조용히 누락되는 것을 잡는다
	if (bVisible && InteractionPromptWidget->GetWidget() == nullptr)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[Interaction] %s: prompt widget instance not created (WidgetClass=%s)"),
			*GetName(), *GetNameSafe(InteractionPromptWidget->GetWidgetClass()));
	}

	InteractionPromptWidget->SetVisibility(bVisible);
}

FString ALNPLootPod::GetInteractDiagnosticString(const APawn* Interactor) const
{
	if (Interactor == nullptr)
		return TEXT("Interactor=null");

	// CanInteract_Implementation과 동일한 판정 값을 계산해 로그로 노출한다
	const float Dist = FVector::Dist(GetActorLocation(), Interactor->GetActorLocation());
	const float MaxDist = InteractionRadius;

	const FVector DirToInteractor = (Interactor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(GetActorForwardVector(), DirToInteractor);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	return FString::Printf(TEXT("%s: Dist=%.0f/%.0f, Angle=%.1f/%.1f, State=%s, PodLoc=%s, PlayerLoc=%s"),
		*GetName(), Dist, MaxDist, AngleDeg, MaxInteractionAngle, *UEnum::GetValueAsString(CurrentState),
		*GetActorLocation().ToCompactString(), *Interactor->GetActorLocation().ToCompactString());
}

void ALNPLootPod::UpdateVisuals(ELNPLootPodState NewState)
{
	CurrentState = NewState;

	// 존 링은 루팅 존 활성(Looting) 동안만 표시 — 감쇠 중에도 유지된다
	if (ZoneRingComponent != nullptr)
	{
		ZoneRingComponent->SetVisibility(NewState == ELNPLootPodState::Looting);
	}

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
	if (CurrentState == ELNPLootPodState::Popped)
	{
		return false;
	}

	// 1. 거리 체크 — 단말기 조작 컨셉의 초근접 반경 (루팅 존 반경과 별개)
	const float DistSq = FVector::DistSquared(GetActorLocation(), Interactor->GetActorLocation());

	if (DistSq > FMath::Square(InteractionRadius))
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

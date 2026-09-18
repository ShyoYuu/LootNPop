// Copyright (c) 2026 LootNPop. All rights reserved.

#include "WorldDevice/LNPSpringLauncher.h"
#include "DataAsset/LNPWorldDeviceConfig.h"
#include "Interaction/LNPInteractableRegistrySubsystem.h"
#include "Interaction/LNPInteractionPromptWidget.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"

ALNPSpringLauncher::ALNPSpringLauncher()
{
	PrimaryActorTick.bCanEverTick = false;

	// 복제하는 이유는 상태가 아니라 NetGUID다 — 헤더 주석 참조. 복제 프로퍼티는 0개이므로
	// 초기 스폰 번치 이후 트래픽이 발생하지 않는다. 갱신 빈도를 최소로 두어 릴러번시 계산만 남긴다.
	bReplicates = true;
	SetNetCullDistanceSquared(30000.f * 30000.f);
	SetNetUpdateFrequency(1.f);
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	LaunchSlot = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchSlot"));
	LaunchSlot->SetupAttachment(RootComponent);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(RootComponent);
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawAtDesiredSize(true);
	InteractionPromptWidget->SetWidgetClass(ULNPInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetVisibility(false);
}

void ALNPSpringLauncher::BeginPlay()
{
	Super::BeginPlay();

	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->RegisterInteractable(this);
	}
}

void ALNPSpringLauncher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

FVector ALNPSpringLauncher::GetFacingDirection() const
{
	// 구 내벽이므로 Up은 중심을 향한다. 런처 전방에서 Up 성분을 빼 접평면 성분만 남긴다.
	const FVector Up = -GetActorLocation().GetSafeNormal();
	return FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal();
}

FVector ALNPSpringLauncher::GetLaunchVelocity() const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	const FVector Forward = GetFacingDirection();
	if (Config == nullptr || Forward.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	// 발사 방향을 슬롯의 로컬 축이 아니라 **접평면 기준 앙각**으로 정의한다.
	// 구 내벽에서는 "지면"이 곧 접평면이라 이쪽이 기획 의도("지면으로부터 45도")와 직접 대응하고,
	// 배치가 결정하는 것은 나침반 방향(Yaw) 하나로 줄어든다.
	const FVector Up = -GetActorLocation().GetSafeNormal();
	const float AngleRad = FMath::DegreesToRadians(Config->LaunchAngleDegrees);
	const FVector Dir = (Forward * FMath::Cos(AngleRad) + Up * FMath::Sin(AngleRad)).GetSafeNormal();
	return Dir * Config->LaunchSpeed;
}

float ALNPSpringLauncher::GetInteractionSearchRadius() const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	return Config ? Config->LauncherInteractionRadius : 0.f;
}

bool ALNPSpringLauncher::CanInteract_Implementation(const APawn* Interactor) const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	if (Interactor == nullptr || Config == nullptr)
	{
		return false;
	}

	// **거리만 본다 — 각도 제한은 없다.** 어느 방향에서 올라타든 같은 방향으로 날아가므로
	// 접근 각도를 제한할 이유가 없고, 제한하면 서버 재검증이 RTT 중의 미세한 위치 차이로
	// 정당한 입력을 기각하는 위험만 남는다 (LootPod은 "단말기를 조작한다"는 컨셉이라 사정이 다르다).
	const FVector ToInteractor = Interactor->GetActorLocation() - GetActorLocation();
	return ToInteractor.SizeSquared() <= FMath::Square(Config->LauncherInteractionRadius);
}

void ALNPSpringLauncher::SetInteractionPromptVisible(bool bVisible)
{
	if (InteractionPromptWidget != nullptr)
	{
		InteractionPromptWidget->SetVisibility(bVisible);
	}
}

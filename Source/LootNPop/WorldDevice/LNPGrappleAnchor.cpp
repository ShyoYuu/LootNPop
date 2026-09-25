// Copyright (c) 2026 LootNPop. All rights reserved.

#include "WorldDevice/LNPGrappleAnchor.h"
#include "DataAsset/LNPWorldDeviceConfig.h"
#include "GameLogic/LNPWorldDeviceSpawnSubsystem.h"
#include "Interaction/LNPInteractableRegistrySubsystem.h"
#include "Interaction/LNPInteractionPromptWidget.h"

#include "Components/WidgetComponent.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ALNPGrappleAnchor::ALNPGrappleAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 런처와 같은 이유로 복제한다 — 상태가 아니라 존재·위치·ID를 알려야 하기 때문이다.
	// AnchorID 하나만 복제되고 스폰 이후 변하지 않으므로 초기 번치 뒤 트래픽은 0이다.
	bReplicates = true;
	SetNetCullDistanceSquared(30000.f * 30000.f);
	SetNetUpdateFrequency(1.f);
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EffectComponent"));
	EffectComponent->SetupAttachment(RootComponent);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(RootComponent);
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawAtDesiredSize(true);
	InteractionPromptWidget->SetWidgetClass(ULNPInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetVisibility(false);
}

void ALNPGrappleAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALNPGrappleAnchor, AnchorID);
}

void ALNPGrappleAnchor::InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id)
{
	if (ULNPWorldDeviceSpawnSubsystem* DeviceSub = GetWorld()->GetSubsystem<ULNPWorldDeviceSpawnSubsystem>())
	{
		AnchorID = DeviceSub->AllocateAnchorID();
	}
}

void ALNPGrappleAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->RegisterInteractable(this);
	}
}

void ALNPGrappleAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

ALNPGrappleAnchor* ALNPGrappleAnchor::FindByID(const UWorld* World, int32 InAnchorID)
{
	if (World == nullptr || InAnchorID == INDEX_NONE)
	{
		return nullptr;
	}

	// 별도 ID 맵을 두지 않는다 — 앵커는 수십 개이고 이 조회는 그래플 발동 프레임에만 일어난다.
	const ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(World);
	if (Registry == nullptr)
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<AActor>& WeakActor : Registry->GetInteractables())
	{
		ALNPGrappleAnchor* Anchor = Cast<ALNPGrappleAnchor>(WeakActor.Get());
		if (Anchor != nullptr && Anchor->AnchorID == InAnchorID)
		{
			return Anchor;
		}
	}
	return nullptr;
}

float ALNPGrappleAnchor::GetMaxGrappleDistance() const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	return Config ? Config->MaxGrappleDistance : 0.f;
}

bool ALNPGrappleAnchor::CanInteract_Implementation(const APawn* Interactor) const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	if (Interactor == nullptr || Config == nullptr || AnchorID == INDEX_NONE)
	{
		return false;
	}

	// **거리만 본다.** 서버 재검증이 이 함수를 쓰므로 카메라처럼 소유 클라만 아는 값을 넣으면
	// 정당한 입력이 기각된다. 조준은 WantsInteractionPrompt(로컬)와 Mover 시뮬(권위)이 나눠 맡는다.
	const float DistSq = FVector::DistSquared(GetGrapplePoint(), Interactor->GetActorLocation());
	return DistSq <= FMath::Square(Config->MaxGrappleDistance)
		&& DistSq >= FMath::Square(Config->MinGrappleDistance);
}

bool ALNPGrappleAnchor::WantsInteractionPrompt(const APawn* Interactor) const
{
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	if (Interactor == nullptr || Config == nullptr)
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	if (PC == nullptr)
	{
		return false;
	}

	// 원점은 카메라, 축은 ControlRotation — 크로스헤어 트레이스와 같은 관례다
	// (카메라 노드가 프레이밍으로 시선을 기울여도 조준이 따라 내려가지 않는다).
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector ToAnchor = GetGrapplePoint() - CamLoc;
	const float Dist = ToAnchor.Size();
	if (Dist <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float AimCosine = FMath::Cos(FMath::DegreesToRadians(Config->AnchorAimHalfAngle));
	if (FVector::DotProduct(Interactor->GetControlRotation().Vector(), ToAnchor / Dist) < AimCosine)
	{
		return false;
	}

	// 가림 검사는 각도까지 통과한 앵커에만 붙는다 — 화면 중앙 ±8°에 드는 앵커는 사실상 0~1개다.
	if (Config->bRequireLineOfSight)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPGrappleAnchorLOS), /*bTraceComplex=*/false, Interactor);
		Params.AddIgnoredActor(this);
		if (GetWorld()->LineTraceTestByChannel(CamLoc, GetGrapplePoint(), ECC_Visibility, Params))
		{
			return false;
		}
	}

	return true;
}

void ALNPGrappleAnchor::SetInteractionPromptVisible(bool bVisible)
{
	if (InteractionPromptWidget != nullptr)
	{
		InteractionPromptWidget->SetVisibility(bVisible);
	}
}

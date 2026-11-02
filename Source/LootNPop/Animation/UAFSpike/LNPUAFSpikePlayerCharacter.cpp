// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikePlayerCharacter.h"

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"
#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "LootNPop.h"

ULNPUAFSpikeInputComponent::ULNPUAFSpikeInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULNPUAFSpikeInputComponent::BeginPlay()
{
	Super::BeginPlay();
	AddTickPrerequisiteActor(GetOwner());
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPInputHandlerComponent>());
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPControlRotationComponent>());
	if (auto* UAF = GetOwner()->FindComponentByClass<UUAFComponent>())
		UAF->AddComponentPrerequisite(this, TEXT("PrePhysics"));
}

void ULNPUAFSpikeInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const auto* Character = CastChecked<ALNPUAFSpikePlayerCharacter>(GetOwner());
	if (auto* Source = Cast<ULNPUAFSpikeAnimInstance>(Character->GetAnimInstance()))
		Source->UpdateFromDriver(DeltaTime);

	// 애니 평가가 꺼진 표시 메시는 랙돌 물리 블렌드 뒤 본 버퍼 교체가 요청되지 않아(엔진은 애니 평가 경로에서만 요청)
	// 렌더가 마지막 UAF 포즈에 멈춘다. 랙돌 동안은 ABP 없이 엔진 기본 평가(참조 포즈)로 되돌려 표준 블렌드 경로를 쓴다.
	// SetEnableAnimation은 프레임 끝에 반영되므로 다음 틱까지 같은 요청이 반복될 수 있다(값이 같으면 무시된다).
	const bool bRagdoll = Character->IsRagdollActive();
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() != TEXT("VisualMesh") || Mesh->bEnableAnimation == bRagdoll)
			continue;
		if (bRagdoll)
			Mesh->SetAnimInstanceClass(nullptr);
		Mesh->SetEnableAnimation(bRagdoll);
	}
}

ALNPUAFSpikePlayerCharacter::ALNPUAFSpikePlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	CreateDefaultSubobject<ULNPUAFSpikeInputComponent>(TEXT("UAFSpikeInputs"));
	AnimSourceMesh->bEnableAnimation = false;
	AnimSourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UActorComponent* ALNPUAFSpikePlayerCharacter::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	// UAF 몽타주·ANS 참조와 GAS가 같은 원본 메시를 찾게 한다. 다른 컴포넌트 질의는 그대로 둔다.
	if (ComponentClass.Get() == USkeletalMeshComponent::StaticClass())
		return AnimSourceMesh;
	return Super::FindComponentByClass(ComponentClass);
}

void ALNPUAFSpikePlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetWorld()->IsGameWorld())
		InitializeSourceDriver();
}

bool ALNPUAFSpikePlayerCharacter::InitializeSourceDriver()
{
	const UClass* SourceClass = AnimSourceMesh->AnimClass.Get();
	if (AnimSourceMesh->bEnableAnimation || !SourceClass || !SourceClass->IsChildOf(ULNPUAFSpikeAnimInstance::StaticClass()))
	{
		if (!bReportedDriverError)
		{
			UE_LOG(LogLootNPop, Error, TEXT("[UAFSpikeDriver] %s requires AnimSourceMesh animation disabled and a UAF spike AnimClass."), *GetName());
			bReportedDriverError = true;
		}
		return false;
	}

	// 일반 평가가 꺼져 있으면 InitAnim은 인스턴스를 만들지 않는다. UAF 그래프 생성 전에 공급층을 준비한다.
	if (!AnimSourceMesh->GetAnimInstance())
		AnimSourceMesh->AnimScriptInstance = NewObject<ULNPUAFSpikeAnimInstance>(AnimSourceMesh, AnimSourceMesh->AnimClass);
	ULNPUAFSpikeAnimInstance* Source = Cast<ULNPUAFSpikeAnimInstance>(AnimSourceMesh->GetAnimInstance());
	if (!Source)
		return false;
	Source->InitializeExplicitDriver();
	return true;
}

ULNPPhysicalHitReactAnimInstance* ALNPUAFSpikePlayerCharacter::GetPhysicalHitReactInstance() const
{
	// 기존 표시 ABP가 남아 있으면 그쪽 RBWC가 계속 맡는다(표시 경로 비교용).
	if (ULNPPhysicalHitReactAnimInstance* Display = Super::GetPhysicalHitReactInstance())
		return Display;
	return Cast<ULNPUAFSpikeAnimInstance>(AnimSourceMesh->GetAnimInstance());
}

void ALNPUAFSpikePlayerCharacter::ShutdownUAFNotifies(const TCHAR* Phase)
{
	UUAFComponent* UAF = Cast<UUAFComponent>(FindComponentByClass(UUAFComponent::StaticClass()));
	FLNPUAFNotifyCleanup::EndBeforeOwnerShutdown(UAF, AnimSourceMesh, Phase);
}

void ALNPUAFSpikePlayerCharacter::Destroyed()
{
	// APawn::Destroyed가 PlayerState를 해제하기 전에 태그/근접 판정 소유 ANS를 종료한다.
	ShutdownUAFNotifies(TEXT("ActorDestroyed"));
	Super::Destroyed();
}

void ALNPUAFSpikePlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownUAFNotifies(TEXT("ActorEndPlay"));
	Super::EndPlay(EndPlayReason);
}

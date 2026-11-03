// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeDriver.h"

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"
#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"

static TAutoConsoleVariable<int32> CVarDestroyBeforeFirstEvent(
	TEXT("LNP.UAFSpike.DestroyBeforeFirstEvent"), 0,
	TEXT("Test only. 1: a UAF spike enemy destroys itself on its first input tick, before its first UAF PrePhysics event runs.\n")
	TEXT("Probe for the UAF module AsShared assert. This timing did not reproduce it (467 destroys, no assert)."));

ULNPUAFSpikeInputComponent::ULNPUAFSpikeInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULNPUAFSpikeInputComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlayFrame = GFrameCounter;
	AddTickPrerequisiteActor(GetOwner());
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPInputHandlerComponent>());
	// 적에는 ControlRotation 컴포넌트가 없다(null이면 엔진이 무시한다).
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPControlRotationComponent>());
	if (auto* UAF = GetOwner()->FindComponentByClass<UUAFComponent>())
		UAF->AddComponentPrerequisite(this, TEXT("PrePhysics"));
}

void ULNPUAFSpikeInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	const auto* Character = CastChecked<ALNPCharacterBase>(Owner);

	// 재현 시험: 월드 틱 도중, 시스템의 첫 PrePhysics 이벤트(이 틱이 선행 조건) 직전에 적 Actor를 파괴한다.
	// BeginPlay의 시스템 활성은 다음 월드 틱 직전에 반영되므로, 스폰 다음 프레임의 첫 틱이 그 이벤트 직전이다.
	if (GFrameCounter > BeginPlayFrame && CVarDestroyBeforeFirstEvent.GetValueOnGameThread() > 0
		&& Owner->IsA<ALNPEnemyCharacter>() && !bTestDestroyChecked)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[UAFSpikeDriver] %s test destroy before first UAF event frame=%llu spawnFrame=%llu"),
			*Owner->GetName(), GFrameCounter, BeginPlayFrame);
		Owner->Destroy();
		return;
	}
	bTestDestroyChecked |= GFrameCounter > BeginPlayFrame;

	// Mass 표현이 적 Actor를 끄면(ISKM 전환 중 유지) 기존 ABP는 원본 메시 틱과 함께 멈춘다. UAF 시스템도 같은 시점에 멈춘다.
	// 시스템 활성은 다음 월드 틱 직전에 반영된다. 시스템은 UAF 컴포넌트 BeginPlay에서 이미 활성이다.
	const bool bActorEnabled = Owner->IsActorTickEnabled();
	if (auto* UAF = Owner->FindComponentByClass<UUAFComponent>(); UAF && UAF->IsActive() != bActorEnabled)
	{
		UAF->SetActive(bActorEnabled);
		UE_LOG(LogLootNPop, Log, TEXT("[UAFSpikeDriver] %s system=%s frame=%llu"),
			*Owner->GetName(), bActorEnabled ? TEXT("enabled") : TEXT("disabled"), GFrameCounter);
	}
	if (!bActorEnabled)
		return;

	if (auto* Source = Cast<ULNPUAFSpikeAnimInstance>(Character->GetAnimInstance()))
		Source->UpdateFromDriver(DeltaTime);

	// 애니 평가가 꺼진 표시 메시는 랙돌 물리 블렌드 뒤 본 버퍼 교체가 요청되지 않아(엔진은 애니 평가 경로에서만 요청)
	// 렌더가 마지막 UAF 포즈에 멈춘다. 랙돌 동안은 ABP 없이 엔진 기본 평가(참조 포즈)로 되돌려 표준 블렌드 경로를 쓴다.
	// SetEnableAnimation은 프레임 끝에 반영되므로 다음 틱까지 같은 요청이 반복될 수 있다(값이 같으면 무시된다).
	const bool bRagdoll = Character->IsRagdollActive();
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() != TEXT("VisualMesh") || Mesh->bEnableAnimation == bRagdoll)
			continue;
		if (bRagdoll)
			Mesh->SetAnimInstanceClass(nullptr);
		Mesh->SetEnableAnimation(bRagdoll);
	}
}

bool LNPUAFSpikeDriver::InitializeSource(USkeletalMeshComponent& Source, bool& bReportedError)
{
	const UClass* SourceClass = Source.AnimClass.Get();
	if (Source.bEnableAnimation || !SourceClass || !SourceClass->IsChildOf(ULNPUAFSpikeAnimInstance::StaticClass()))
	{
		if (!bReportedError)
		{
			UE_LOG(LogLootNPop, Error, TEXT("[UAFSpikeDriver] %s requires AnimSourceMesh animation disabled and a UAF spike AnimClass."), *GetNameSafe(Source.GetOwner()));
			bReportedError = true;
		}
		return false;
	}

	// 일반 평가가 꺼져 있으면 InitAnim은 인스턴스를 만들지 않는다. UAF 그래프 생성 전에 공급층을 준비한다.
	if (!Source.GetAnimInstance())
		Source.AnimScriptInstance = NewObject<ULNPUAFSpikeAnimInstance>(&Source, Source.AnimClass);
	ULNPUAFSpikeAnimInstance* Instance = Cast<ULNPUAFSpikeAnimInstance>(Source.GetAnimInstance());
	if (!Instance)
		return false;
	Instance->InitializeExplicitDriver();
	return true;
}

ULNPPhysicalHitReactAnimInstance* LNPUAFSpikeDriver::ResolveHitReactInstance(ULNPPhysicalHitReactAnimInstance* Display, const USkeletalMeshComponent& Source)
{
	if (Display)
		return Display;
	return Cast<ULNPUAFSpikeAnimInstance>(Source.GetAnimInstance());
}

void LNPUAFSpikeDriver::ShutdownNotifies(AActor& Owner, USkeletalMeshComponent* Source, const TCHAR* Phase)
{
	FLNPUAFNotifyCleanup::EndBeforeOwnerShutdown(Owner.FindComponentByClass<UUAFComponent>(), Source, Phase);
}

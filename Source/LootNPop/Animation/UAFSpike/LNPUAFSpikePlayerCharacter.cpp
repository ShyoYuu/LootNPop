// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikePlayerCharacter.h"

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"
#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "LootNPop.h"

ALNPUAFSpikePlayerCharacter::ALNPUAFSpikePlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.TickGroup = TG_PrePhysics;
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

void ALNPUAFSpikePlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (InitializeSourceDriver())
		CastChecked<ULNPUAFSpikeAnimInstance>(AnimSourceMesh->GetAnimInstance())->UpdateFromDriver(DeltaSeconds);
}

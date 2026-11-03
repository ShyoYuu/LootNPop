// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikePlayerCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

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
		LNPUAFSpikeDriver::InitializeSource(*AnimSourceMesh, bReportedDriverError);
}

ULNPPhysicalHitReactAnimInstance* ALNPUAFSpikePlayerCharacter::GetPhysicalHitReactInstance() const
{
	return LNPUAFSpikeDriver::ResolveHitReactInstance(Super::GetPhysicalHitReactInstance(), *AnimSourceMesh);
}

void ALNPUAFSpikePlayerCharacter::Destroyed()
{
	// APawn::Destroyed가 PlayerState를 해제하기 전에 태그/근접 판정 소유 ANS를 종료한다.
	LNPUAFSpikeDriver::ShutdownNotifies(*this, AnimSourceMesh, TEXT("ActorDestroyed"));
	Super::Destroyed();
}

void ALNPUAFSpikePlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LNPUAFSpikeDriver::ShutdownNotifies(*this, AnimSourceMesh, TEXT("ActorEndPlay"));
	Super::EndPlay(EndPlayReason);
}

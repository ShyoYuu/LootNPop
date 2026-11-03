// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeEnemyCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

ALNPUAFSpikeEnemyCharacter::ALNPUAFSpikeEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	CreateDefaultSubobject<ULNPUAFSpikeInputComponent>(TEXT("UAFSpikeInputs"));
	AnimSourceMesh->bEnableAnimation = false;
	AnimSourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UActorComponent* ALNPUAFSpikeEnemyCharacter::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	// UAF 몽타주·ANS 참조와 GAS가 같은 원본 메시를 찾게 한다. MassCrowd 활성/비활성 처리도 원래 첫 메시(원본)를 받는다.
	if (ComponentClass.Get() == USkeletalMeshComponent::StaticClass())
		return AnimSourceMesh;
	return Super::FindComponentByClass(ComponentClass);
}

void ALNPUAFSpikeEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetWorld()->IsGameWorld())
		LNPUAFSpikeDriver::InitializeSource(*AnimSourceMesh, bReportedDriverError);
}

ULNPPhysicalHitReactAnimInstance* ALNPUAFSpikeEnemyCharacter::GetPhysicalHitReactInstance() const
{
	return LNPUAFSpikeDriver::ResolveHitReactInstance(Super::GetPhysicalHitReactInstance(), *AnimSourceMesh);
}

void ALNPUAFSpikeEnemyCharacter::Destroyed()
{
	// LOD 강등·사망 반납으로 Actor가 파괴될 때 근접 판정 엔티티·차단 태그를 소유한 ANS를 먼저 종료한다.
	LNPUAFSpikeDriver::ShutdownNotifies(*this, AnimSourceMesh, TEXT("ActorDestroyed"));
	Super::Destroyed();
}

void ALNPUAFSpikeEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LNPUAFSpikeDriver::ShutdownNotifies(*this, AnimSourceMesh, TEXT("ActorEndPlay"));
	Super::EndPlay(EndPlayReason);
}

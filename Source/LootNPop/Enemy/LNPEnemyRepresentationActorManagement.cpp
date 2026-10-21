// Copyright LootNPop. All Rights Reserved.

#include "Enemy/LNPEnemyRepresentationActorManagement.h"
#include "Mass/EntityHandle.h"
#include "GameFramework/Actor.h"

AActor* ULNPEnemyRepresentationActorManagement::GetOrSpawnActor(UMassRepresentationSubsystem& RepresentationSubsystem,
	FMassEntityManager& EntityManager, const FMassEntityHandle MassAgent, const FTransform& Transform,
	const int16 TemplateActorIndex, FMassActorSpawnRequestHandle& SpawnRequestHandle, const float Priority) const
{
	// MassCrowd의 발밑→중심 보정(월드 +Z 반높이)을 건너뛴다. Entity Transform은 이미 캡슐 중심이다.
	return UMassRepresentationActorManagement::GetOrSpawnActor(RepresentationSubsystem, EntityManager,
		MassAgent, Transform, TemplateActorIndex, SpawnRequestHandle, Priority);
}

void ULNPEnemyRepresentationActorManagement::TeleportActor(const FTransform& Transform, AActor& Actor,
	FMassCommandBuffer& CommandBuffer) const
{
	// 게스트의 Actor 위치는 Mover 복제가 소유한다. ISM 보간 위치로 프록시를 재배치하면 안 된다.
	if (!Actor.HasAuthority())
		return;

	// 서버의 Mover 자세는 ActorInitializer가 같은 활성화의 Entity Transform으로 함께 초기화한다.
	UMassRepresentationActorManagement::TeleportActor(Transform, Actor, CommandBuffer);
}

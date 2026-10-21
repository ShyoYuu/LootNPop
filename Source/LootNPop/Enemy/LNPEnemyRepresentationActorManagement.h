// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassCrowdRepresentationActorManagement.h"
#include "LNPEnemyRepresentationActorManagement.generated.h"

/** 캡슐 중심 좌표를 사용하는 적 표현. 메시 활성화·Puppet 수명주기는 MassCrowd 구현을 유지한다. */
UCLASS()
class LOOTNPOP_API ULNPEnemyRepresentationActorManagement : public UMassCrowdRepresentationActorManagement
{
	GENERATED_BODY()

public:
	virtual AActor* GetOrSpawnActor(UMassRepresentationSubsystem& RepresentationSubsystem, FMassEntityManager& EntityManager,
		const FMassEntityHandle MassAgent, const FTransform& Transform, const int16 TemplateActorIndex,
		FMassActorSpawnRequestHandle& SpawnRequestHandle, const float Priority) const override;

	virtual void TeleportActor(const FTransform& Transform, AActor& Actor, FMassCommandBuffer& CommandBuffer) const override;
};

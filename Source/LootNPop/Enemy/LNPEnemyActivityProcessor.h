// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyActivityProcessor.generated.h"

struct FLNPSurfaceDataSnapshot;

/** PostPhysics에서 깨움과 대역을 확정하고 다음 PrePhysics 전 휴면 태그를 반영한다. */
UCLASS()
class LOOTNPOP_API ULNPEnemyActivityProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPEnemyActivityProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PodQuery;
	FMassEntityQuery PlayerQuery;
	double NextDistanceTime = 0.0;
	TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> LastSurfaceSnapshot;
};

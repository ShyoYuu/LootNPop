// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyWanderProcessor.generated.h"

/** Pod 귀속 배회 대역의 구간 생성과 시간 보간. 다른 이동 프로세서는 처리된 개체를 건너뛴다. */
UCLASS()
class LOOTNPOP_API ULNPEnemyWanderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPEnemyWanderProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EnemyQuery;
};

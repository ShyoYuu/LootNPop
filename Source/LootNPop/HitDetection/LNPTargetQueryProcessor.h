// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPTargetQueryProcessor.generated.h"

/**
 * ULNPTargetQuerySubsystem에 등록된 상시 질의를 매 프레임 평가한다.
 *
 * 적 청크 순회 비용은 등록된 질의 수와 무관하게 한 번이고, 질의별 계산만 안쪽 루프에서 곱해진다.
 * 상세 설계는 `.context/TechDesign_TargetQuery.md`.
 */
UCLASS()
class LOOTNPOP_API ULNPTargetQueryProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPTargetQueryProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EnemyQuery;
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPPositionHistoryProcessors.generated.h"

/**
 * 서버 전용. FLNPPositionHistoryFragment를 가진 모든 엔티티(Enemy NPC·플레이어 캐릭터)의 위치를
 * 50ms 간격으로 기록한다. Lag Compensation(HitDetection)이 과거 위치를 되감아 조회하는 데 사용한다.
 */
UCLASS()
class LOOTNPOP_API ULNPPositionHistoryRecordProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPPositionHistoryRecordProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	static constexpr double RecordInterval = 0.05; // 50ms
};

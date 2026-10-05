// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyUpDeviationProcessor.generated.h"

/**
 * 진단 전용. 적 엔티티 회전 Z와 구 중심 방향 사이의 최대 각도를 상태별로 모아 주기적으로 로그에 남긴다.
 *
 * 몸 방향 Up을 회전 Z에서 읽는 리팩터(Phase 14 단위 1)는 "지상·비행 적의 회전 Z는 구 중심 방향"이라는
 * 전제 위에서만 동작을 바꾸지 않는다. 이 전제를 서버·게스트 양쪽에서 수치로 확인하는 장치다.
 * `LNP.Enemy.UpDeviationCheck`가 0이면 아무것도 하지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyUpDeviationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyUpDeviationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	enum class ECategory : uint8 { Dying, Flying, Airborne, Promoted, Dormant, Wander, Grounded, Num };

	struct FStat
	{
		int64  Samples = 0;
		double MaxDeg = 0.0;
		uint32 MaxNetID = 0;
	};

	FMassEntityQuery EnemyQuery;
	FStat Stats[static_cast<int32>(ECategory::Num)];
	double WindowStartTime = -1.0;
};

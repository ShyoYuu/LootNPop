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
	/** 같은 Pod 구성원의 현재 위치와 예약 선분. 매 실행 다시 채우되 할당은 실행 사이에 재사용한다. */
	struct FReservation
	{
		FMassEntityHandle Pod;
		FMassEntityHandle Entity;
		FVector Start = FVector::ZeroVector;
		FVector Goal = FVector::ZeroVector;
		float Radius = 0.f;
	};

	FMassEntityQuery EnemyQuery;
	/** (Pod, Entity.Index) 순으로 정렬한 전체 구성원. 한 Pod의 구성원은 연속 구간이다. */
	TArray<FReservation> Reservations;
	/** Pod → Reservations 안의 (시작, 개수). */
	TMap<FMassEntityHandle, TPair<int32, int32>> PodRanges;
};

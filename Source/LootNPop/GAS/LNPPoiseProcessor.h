// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPPoiseProcessor.generated.h"

/**
 * 경직도 감쇠와 임계값 판정. 서버 전용.
 *
 * 누적은 히트 판정 Processor가 하고(같은 프레임의 StartPhysics), 여기서는 그 결과를 읽어
 * 감쇠시키고 임계 돌파만 판정한다. 발동에 필요한 ASC 접근은 FLNPStaggerCommand가 게임 스레드로 미룬다.
 *
 * Player·Enemy를 가리지 않고 FLNPPoiseFragment를 가진 모든 엔티티에 동일하게 작용한다 —
 * Low LOD 적(Actor 없음)도 게이지는 똑같이 쌓이고 줄어든다.
 */
UCLASS()
class LOOTNPOP_API ULNPPoiseProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPPoiseProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery PoiseQuery;
};

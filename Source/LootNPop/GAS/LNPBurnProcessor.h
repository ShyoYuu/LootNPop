// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPBurnProcessor.generated.h"

/**
 * 적 화상 DoT 정산. 서버 전용.
 *
 * 걸기는 화염 원뿔 판정(StartPhysics)이 하고, 여기서는 틱 피해·만료·사망 정지만 한다.
 * 피해 수단은 원뿔 피격과 같다 — Actor 적은 데미지 GE 커맨드(HitReact 억제), 순수 엔티티는 Health 직접 차감.
 *
 * 사망 중인 적도 쿼리에 남긴다 — 걸린 중첩을 0으로 지워 그 값이 게스트에 복제되게 하기 위해서다.
 * 그래서 화상 해제는 만료·사망 모두 "Stacks = 0" 한 경로로 모인다.
 */
UCLASS()
class LOOTNPOP_API ULNPBurnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPBurnProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery BurnQuery;
};

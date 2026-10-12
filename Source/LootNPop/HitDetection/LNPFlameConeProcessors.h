// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPFlameConeProcessors.generated.h"

/**
 * 화염방사기 원뿔 피격 판정 (서버 전용).
 * FLNPFlameConeFragment 엔티티마다 적 캡슐을 원뿔과 대조하고, 총구→대상 사선이 월드에 막히지 않은 대상에 틱 피해를 준다.
 * 판정이 끝난 원뿔 엔티티는 여기서 파괴한다 — 원뿔은 1프레임짜리 요청이다.
 */
UCLASS()
class LOOTNPOP_API ULNPFlameConeHitDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPFlameConeHitDetectionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ConeQuery;
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PlayerQuery;   // 아군 사격(bFriendlyFire) 대상
};

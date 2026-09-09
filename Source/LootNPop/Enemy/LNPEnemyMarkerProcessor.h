// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPEnemyMarkerProcessor.generated.h"

/**
 * 적 HP 바 표시 후보를 매 프레임 모아 ULNPEnemyMarkerSubsystem에 넣는다.
 *
 * 서버·클라이언트 모두에서 돈다 — 리슨 호스트도 적 HP 바를 봐야 하기 때문이다.
 * 표시 여부와 무관하게 FLNPEnemyHealthDisplayFragment의 "HP가 변한 시각"을 함께 갱신한다.
 *
 * 상세 설계는 `.context/TechDesign_HUD.md` §11.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyMarkerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyMarkerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EnemyQuery;
};

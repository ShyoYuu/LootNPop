// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPPoiseDebugDrawProcessor.generated.h"

/**
 * 경직도 시각화. **검증 전용이며 에디터 빌드에서만 활성화된다** — 인게임 정식 UI가 아니다.
 * `LNP.Debug.DrawPoise 1`로 켠다 (기본 0).
 *
 * 경직도는 복제하지 않으므로 서버 화면에서만 의미가 있다 (클라이언트 월드에서는 조기 반환).
 * FLNPPoiseFragment를 가진 모든 엔티티를 한 경로로 그린다 — 플레이어·High LOD 적·**Actor 없는
 * Low LOD 적**까지 포함된다. 위젯으로는 마지막 항목이 불가능해서 프로세서로 둔다.
 */
UCLASS()
class LOOTNPOP_API ULNPPoiseDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPPoiseDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery PoiseQuery;
	FMassEntityQuery PlayerQuery;
};

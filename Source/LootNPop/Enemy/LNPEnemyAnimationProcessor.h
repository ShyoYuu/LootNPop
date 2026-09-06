// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyAnimationProcessor.generated.h"

/**
 * 행동 상태(`FLNPEnemyActionFragment`)를 ISKM 애니 데이터(`FMassRepresentationAnimationFragment`)로
 * 옮기는 유일한 프로세서. 엔진의 `UMassConsumeInstancedSkinnedMeshAnimationProcessor`가 이 값을
 * 읽어 `UInstancedSkinnedMeshComponent`에 일괄 반영한다.
 *
 * 입력이 이미 서버·게스트 공용 채널이므로 **넷 모드 분기가 없다** — 호스트와 게스트가 같은 값을
 * 보고 같은 그림을 그린다(`ULNPEnemyActionDebugDrawProcessor`와 같은 이유).
 *
 * ⚠️ **페이즈는 PrePhysics다.** 엔진의 표현 체인
 * (`UMassCrowdVisualizationProcessor` · `UMassConsumeInstancedSkinnedMeshAnimationProcessor`)은
 * `ProcessingPhase`를 설정하지 않아 **기본값 PrePhysics**로 돌고, Mass는 프로세서를 페이즈별로
 * 따로 버킷팅해 각 페이즈를 독립적으로 의존성 해소한다. 다른 페이즈에서 건 `ExecuteBefore`는
 * **에러도 경고도 없이 무시된다** — 그룹만 맞추면 선언은 그럴듯한데 실행은 안 되는 상태가 된다
 * (→ `EngineAnalysis_MassEntity.md` §4.2·§4.3, `TechDesign_EnemyNPC.md` §7.10).
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyAnimationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyAnimationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery AnimQuery;
};

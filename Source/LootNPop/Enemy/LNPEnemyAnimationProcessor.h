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
 * 입력이 이미 서버·게스트 공용 채널이므로 **넷 모드별 값 분기가 없다** — 호스트와 게스트가 같은 값을
 * 보고 같은 그림을 그린다(`ULNPEnemyActionDebugDrawProcessor`와 같은 이유).
 * 다만 *실행 여부*는 가른다: `ExecutionFlags = Client | Standalone`이라 그리지 않는 데디 서버에서는
 * 아예 돌지 않는다(엔진의 소비 프로세서도 같은 플래그다).
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

/**
 * 순수 엔티티의 공격이 플레이어에게 닿았을 때(가드·피격 무관) ISKM 트랙의 재생 속도를 잠깐 누른다.
 *
 * 입력은 `FLNPEnemyActionFragment::HitStopSeq` 하나이고, 호스트는 서버가 올린 값을 그대로,
 * 게스트는 복제된 같은 값을 읽으므로 **넷 모드 분기가 없다** (행동 상태 채널과 같은 규약).
 * 길이·배율은 `FLNPEntityAttackConfig`에서 각 머신이 읽는다 — 시간을 와이어에 싣지 않는 이유는
 * 개체 수만큼 곱해지는 비용에 비해 그 정밀도가 필요 없기 때문이다.
 *
 * ⚠️ **`FMassRepresentationAnimationFragment::AnimData.PlayRate`를 바꾸는 것으로는 안 된다.**
 * 엔진의 반영 지점(`UMassVisualizationComponent::EndVisualChanges`)은 **`SequenceIndex`가 달라졌을
 * 때만** `SetAutoPlayData`를 호출하므로, 재생 속도만 바뀐 갱신은 경고 한 줄 없이 버려진다.
 * 그래서 `UAnimSequenceTransformProviderDataInstance::SetPlayRate`로 트랙에 직접 건다.
 *
 * ⚠️ **애니 프로세서(PrePhysics)와 페이즈를 나눠 놓은 것이 요점이다.** 판정이 미는 커맨드는
 * StartPhysics가 **끝날 때** 플러시되므로 PrePhysics에서 읽으면 이번 프레임의 적중을 놓치고,
 * 엔진의 트랙 반영도 PostPhysics 시작에 일어난다 — 두 이유가 같은 답(PostPhysics)을 가리킨다.
 * AnimData를 **쓰는** 쪽은 소비 프로세서보다 앞서야 하므로 PrePhysics에 그대로 남는다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyHitStopProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyHitStopProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery HitStopQuery;
};

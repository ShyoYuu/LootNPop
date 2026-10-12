// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "Mass/EntityHandle.h"
#include "LNPBurnVisualizationProcessor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 적 화상 표현. `FLNPBurnFragment::Stacks`가 0보다 큰 적 몸통에 루프 Niagara를 붙인다.
 *
 * 서버·Standalone은 원본 값을, 게스트는 버블로 복제된 값을 **같은 필드**에서 읽으므로 넷 모드 분기가 없다.
 *
 * 해제는 한 곳뿐이다 — 매 프레임 쿼리에서 "중첩 > 0"으로 보인 엔티티를 표시하고, 표시되지 않은 항목은 전부 회수한다.
 * 만료·사망(둘 다 서버가 Stacks를 0으로 지워 복제), 엔티티 파괴(버블 이탈·사망 팝)가 모두 이 경로로 모인다.
 * 표현 LOD가 바뀌어도 엔티티 핸들은 그대로라 영향이 없다.
 */
UCLASS()
class LOOTNPOP_API ULNPBurnVisualizationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPBurnVisualizationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	struct FBurnVisual
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		uint8  AppliedStacks = 0;
		uint32 SeenFrame     = 0;
	};

	/** 이어지던 불길은 수명대로 사라지게 두고 컴포넌트는 그 뒤 스스로 파괴된다. */
	static void Release(FBurnVisual& Visual);

	FMassEntityQuery BurnQuery;

	TMap<FMassEntityHandle, FBurnVisual> ActiveVisuals;
	uint32 Frame = 0;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> BurnSystem;
	bool bBurnSystemResolved = false;
};

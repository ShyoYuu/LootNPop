// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Enemy/LNPEnemyCharacter.h"
#include "Animation/UAFSpike/LNPUAFSpikeDriver.h"
#include "LNPUAFSpikeEnemyCharacter.generated.h"

/**
 * `ActorPromoted` 적의 UAF 시험 셸(Phase 5 ⓓ). 플레이어 시험 폰과 같은 원본 공급층·시스템을 쓰고,
 * 이동 입력은 컨트롤러 없이 Mass AI 입력(InputHandler)에서, 게스트에서는 Mover 재시뮬레이션에서 온다.
 */
UCLASS()
class LOOTNPOP_API ALNPUAFSpikeEnemyCharacter : public ALNPEnemyCharacter
{
	GENERATED_BODY()

public:
	ALNPUAFSpikeEnemyCharacter(const FObjectInitializer& ObjectInitializer);
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const override;
	virtual void PostInitializeComponents() override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 표시 ABP 없이 UAF가 VisualMesh에 쓰므로 원본 공급층이 피격 반응을 계산해 물리 CR 입력으로 넘긴다. */
	virtual ULNPPhysicalHitReactAnimInstance* GetPhysicalHitReactInstance() const override;

private:
	bool bReportedDriverError = false;
};

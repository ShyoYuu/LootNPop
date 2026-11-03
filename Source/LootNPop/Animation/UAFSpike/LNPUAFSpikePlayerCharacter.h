// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Character/LNPPlayerCharacter.h"
#include "Animation/UAFSpike/LNPUAFSpikeDriver.h"
#include "LNPUAFSpikePlayerCharacter.generated.h"

/** UAF 시험에서만 원본 메시 선택과 포즈 평가 없는 입력 공급을 명시한다. */
UCLASS()
class LOOTNPOP_API ALNPUAFSpikePlayerCharacter : public ALNPPlayerCharacter
{
	GENERATED_BODY()

public:
	ALNPUAFSpikePlayerCharacter(const FObjectInitializer& ObjectInitializer);
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

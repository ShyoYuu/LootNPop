// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Character/LNPPlayerCharacter.h"
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
	virtual void Tick(float DeltaSeconds) override;

private:
	bool InitializeSourceDriver();
	bool bReportedDriverError = false;
};

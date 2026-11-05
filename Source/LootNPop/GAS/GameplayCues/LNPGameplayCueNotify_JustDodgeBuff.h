// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "LNPGameplayCueNotify_JustDodgeBuff.generated.h"

/** APawn 기반 캐릭터의 표시용 메시 소켓에 버프 오라를 붙인다. 수명은 기존 지속 큐가 관리한다. */
UCLASS()
class LOOTNPOP_API ALNPGameplayCueNotify_JustDodgeBuff : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

protected:
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
};

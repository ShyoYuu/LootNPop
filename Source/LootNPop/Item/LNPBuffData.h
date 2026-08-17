// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "LNPBuffData.generated.h"

namespace LNPBuff
{
	/** 만료 없는 영구 버프를 뜻하는 지속 시간 값. UI도 이 값이면 남은 시간을 표시하지 않는다. */
	inline constexpr float PermanentDuration = -1.0f;
}

UCLASS(BlueprintType)
class LOOTNPOP_API ULNPBuffData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
public:
	/**
	 * 최대 지속 시간(초). **-1 = 영구**(만료 없음, UI 시간 표시 없음), 양수 = 기간제.
	 * 0은 유효하지 않다 — 설정하면 경고 로그 후 영구로 취급한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff", meta = (ClampMin = "-1"))
	float Duration = LNPBuff::PermanentDuration;
};

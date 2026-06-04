// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_LNPCancelMontageOnMovement.generated.h"

/**
 * 이동 입력이 감지되면 현재 몽타주를 취소하는 AnimNotifyState.
 *
 * 근접 공격 몽타주의 후반부(이동 허용 구간)에 배치한다.
 * NotifyTick에서 이동 입력 감지 시 Montage_Stop을 호출해 발 미끄러짐을 방지한다.
 */
UCLASS(meta = (DisplayName = "LNP Cancel Montage On Movement"))
class LOOTNPOP_API UANS_LNPCancelMontageOnMovement : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	/** Montage_Stop 시 블렌드아웃 시간 (초). */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee", meta = (ClampMin = "0"))
	float BlendOutTime = 0.3f;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("Cancel Montage On Movement"); }
};

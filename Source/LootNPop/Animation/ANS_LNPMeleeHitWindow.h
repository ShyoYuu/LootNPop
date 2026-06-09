// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MassEntityHandle.h"
#include "ANS_LNPMeleeHitWindow.generated.h"

/**
 * 근거리 공격 애니메이션의 피격 활성 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : Mass 엔티티 생성 + FLNPWeaponTraceFragment 초기화
 * NotifyTick  : 매 프레임 칼날 본(Bone) 위치를 Fragment에 기록 (Swept Volume 스윕 선분 생성)
 * NotifyEnd   : 엔티티 파괴
 */
UCLASS(meta = (DisplayName = "LNP Melee Hit Window"))
class LOOTNPOP_API UANS_LNPMeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	/** 칼끝 본 이름. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FName BoneTipName = TEXT("sword_tip");

	/** 칼밑(손잡이 쪽) 본 이름. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FName BoneRootName = TEXT("sword_root");

	/** 0이면 무기 DataAsset의 ProjectileHitRadius를 사용한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee", meta = (ClampMin = "0"))
	float HitRadiusOverride = 0.f;

	UPROPERTY(EditAnywhere, Category = "LNP|Melee", meta = (ClampMin = "0"))
	float ParryRadiusOverride = 0.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("LNP Melee Hit Window"); }

private:
	FMassEntityHandle MeleeEntity;
	bool bEntityActive = false;
};

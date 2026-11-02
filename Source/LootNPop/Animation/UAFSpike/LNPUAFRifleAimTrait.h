// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "EvaluationVM/EvaluationTask.h"
#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitInterfaces/IEvaluate.h"
#include "LNPUAFRifleAimTrait.generated.h"

class ULNPMassWorldCollisionSubsystem;
class ALNPCharacterBase;
struct FLNPUAFRootFeedback;

namespace LNPUAFRifleAim
{
	/** 기존 발사 진단이 켜졌을 때만 호출한다. 완료 사본과 실제 표시 총열을 비교한다. */
	void LogFireComparison(const ALNPCharacterBase& Character, const FVector& Muzzle, const FVector& Barrel,
		const FVector& Spawn, const FVector& Shot, const FVector& View, const FVector* AimTarget);
}

/** 게임 스레드에서 복사한 조준 캐시와 부착 기하. 포즈는 평가 task가 이번 프레임에서 읽는다. */
USTRUCT(BlueprintType)
struct FLNPUAFRifleAimInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector AimTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector ViewDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FTransform MeshWorld = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FTransform MuzzleInWeaponBone = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector MuzzleOffsetWorld = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector ActorLocation = FVector::ZeroVector;

	/** 수명은 공유 입력이 보유한다. 워커는 모든 스레드용 동기 질의 API만 호출한다. */
	UPROPERTY(Transient)
	TObjectPtr<ULNPMassWorldCollisionSubsystem> WorldCollision;
};

USTRUCT(meta = (DisplayName = "LNP Rifle Aim"))
struct FLNPUAFRifleAimSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rifle Aim", meta = (PinShownByDefault))
	FLNPUAFRifleAimInputs Inputs;

#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(Inputs)
	GENERATE_TRAIT_LATENT_PROPERTIES(FLNPUAFRifleAimSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::UAF
{
	/** 최종 CR 블렌드 위에서만 사용한다. 슬롯 갱신 뒤의 실제 가중치를 같은 평가에 전달한다. */
	struct FLNPUAFRifleAimTrait : FAdditiveTrait, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FLNPUAFRifleAimTrait, FAdditiveTrait)
		using FSharedData = FLNPUAFRifleAimSharedData;
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
	};
}

/** 상체 피벗만 회전한다. 자식 로컬 포즈·하체·커브·속성은 보존한다. */
USTRUCT()
struct FLNPUAFRifleAimTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPUAFRifleAimTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	FLNPUAFRifleAimInputs Inputs;
	float MontageDemand = 1.0f;
	bool bLog = false;
	FLNPUAFRootFeedback* Feedback = nullptr;
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Module/UAFModuleInstanceComponent.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IUpdate.h"
#include "LNPUAFMontageIKTrait.generated.h"

/** 완료된 세 슬롯의 실제 그래프 기여도. 커브는 출력 메시에서 별도로 읽는다. */
struct FLNPUAFHandIKSnapshot
{
	float Weights[3] = {};
	int64 Serial = 0;
	bool bValid = false;

	float GetDemand(float Curve) const
	{
		const float Montage = bValid ? FMath::Max3(Weights[0], Weights[1], Weights[2]) : 0.0f;
		return FMath::Clamp(FMath::Max(Curve, Montage), 0.0f, 1.0f);
	}
};

/** 갱신 중 가중치와 GT가 읽는 완료 사본을 분리한다. */
USTRUCT()
struct FLNPUAFHandIKFeedback : public FUAFModuleInstanceComponent
{
	GENERATED_BODY()
	DECLARE_UAF_ASSET_INSTANCE_COMPONENT()

	virtual void OnBeginExecution(float InDeltaTime) override;
	virtual void OnEndExecution(float InDeltaTime) override;
	void RecordSlot(int32 Index, float LocalWeight, float GlobalWeight);
	FLNPUAFHandIKSnapshot Read() const;
	/** UAF의 갱신 순회 완료 뒤, 같은 실행의 평가 순회에서만 읽는다. GT는 Read를 사용한다. */
	FLNPUAFHandIKSnapshot ReadCurrentUpdate() const;

private:
	FLNPUAFHandIKSnapshot Pending;
	uint8 SeenSlots = 0;
	bool bDuplicateSlot = false;
	mutable FCriticalSection Mutex;
	FLNPUAFHandIKSnapshot Completed;
};

template<>
struct TStructOpsTypeTraits<FLNPUAFHandIKFeedback> : public TStructOpsTypeTraitsBase2<FLNPUAFHandIKFeedback>
{
	enum { WithCopy = false };
};

USTRUCT(meta = (DisplayName = "LNP Montage IK Feedback"))
struct FLNPUAFMontageIKSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::UAF
{
	/** native 슬롯의 갱신/자식/몽타주 정책은 그대로 두고 가중치만 관찰한다. */
	struct FLNPUAFMontageIKTrait : FAdditiveTrait, IUpdate
	{
		DECLARE_ANIM_TRAIT(FLNPUAFMontageIKTrait, FAdditiveTrait)
		using FSharedData = FLNPUAFMontageIKSharedData;
		struct FInstanceData : FTrait::FInstanceData
		{
			bool bReportedConfigurationError = false;
		};
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding,
			const FTraitUpdateState& TraitState) const override;
	};
}

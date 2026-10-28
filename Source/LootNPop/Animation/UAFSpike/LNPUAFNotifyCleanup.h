// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/AnimNotifyQueue.h"
#include "Module/UAFModuleInstanceComponent.h"
#include "LNPUAFNotifyCleanup.generated.h"

class USkeletalMeshComponent;

/** 기본 dispatcher 뒤의 활성 상태 사본만 보관해, 모듈 해제 때 남은 ANS를 종료한다. */
USTRUCT()
struct FLNPUAFNotifyCleanup : public FUAFModuleInstanceComponent
{
	GENERATED_BODY()
	DECLARE_UAF_ASSET_INSTANCE_COMPONENT()

	virtual ~FLNPUAFNotifyCleanup() override;
	virtual void OnEndExecution(float InDeltaTime) override;

private:
	UPROPERTY()
	TArray<FAnimNotifyEventReference> ActiveNotifyReferences;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> DispatchMesh;
};

template<>
struct TStructOpsTypeTraits<FLNPUAFNotifyCleanup> : public TStructOpsTypeTraitsBase2<FLNPUAFNotifyCleanup>
{
	enum { WithCopy = false };
};

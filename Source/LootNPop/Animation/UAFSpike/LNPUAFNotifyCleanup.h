// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/AnimNotifyQueue.h"
#include "HAL/CriticalSection.h"
#include "Module/UAFModuleInstanceComponent.h"
#include "LNPUAFNotifyCleanup.generated.h"

class USkeletalMeshComponent;
class UUAFComponent;

/** 기본 dispatcher의 활성 사본을 보관해 소유 Actor 종료 전 또는 모듈 해제 때 ANS를 종료한다. */
USTRUCT()
struct FLNPUAFNotifyCleanup : public FUAFModuleInstanceComponent
{
	GENERATED_BODY()
	DECLARE_UAF_ASSET_INSTANCE_COMPONENT()

	virtual ~FLNPUAFNotifyCleanup() override;
	virtual void OnEndExecution(float InDeltaTime) override;

	/** 원본 메시/PlayerState 연결이 해제되기 전에 종료하며, 모듈 소멸자의 중복 호출을 막는다. */
	static void EndBeforeOwnerShutdown(UUAFComponent* Component, USkeletalMeshComponent* Mesh, const TCHAR* Phase);

private:
	void CopyForOwnerShutdown(USkeletalMeshComponent* Mesh, TArray<FAnimNotifyEventReference>& OutReferences) const;

	mutable FCriticalSection Mutex;
	mutable bool bOwnerShuttingDown = false;

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

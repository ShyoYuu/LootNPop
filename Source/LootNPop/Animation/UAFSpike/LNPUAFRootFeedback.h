// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Module/UAFModuleInstanceComponent.h"
#include "LNPUAFRootFeedback.generated.h"

/** 평가 task의 완료된 루트만 게임 스레드에 사본으로 전달한다. UObject나 trait 상태를 노출하지 않는다. */
USTRUCT()
struct FLNPUAFRootFeedback : public FUAFModuleInstanceComponent
{
	GENERATED_BODY()
	DECLARE_UAF_ASSET_INSTANCE_COMPONENT()

	void Publish(const FTransform& Transform, int64 Serial, bool bValid)
	{
		FScopeLock Lock(&Mutex);
		RootTransform = Transform;
		InputSerial = Serial;
		bHasRoot = bValid;
	}

	bool Read(FTransform& OutTransform, int64& OutSerial) const
	{
		FScopeLock Lock(&Mutex);
		OutTransform = RootTransform;
		OutSerial = InputSerial;
		return bHasRoot;
	}

private:
	mutable FCriticalSection Mutex;
	FTransform RootTransform = FTransform::Identity;
	int64 InputSerial = 0;
	bool bHasRoot = false;
};

template<>
struct TStructOpsTypeTraits<FLNPUAFRootFeedback> : public TStructOpsTypeTraitsBase2<FLNPUAFRootFeedback>
{
	enum { WithCopy = false };
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Module/UAFModuleInstanceComponent.h"
#include "LNPUAFRootFeedback.generated.h"

/** 발사 진단이 비교하는 마지막 총열 평가 사본. UObject나 워커 포인터를 포함하지 않는다. */
struct FLNPUAFRifleAimSnapshot
{
	uint64 EvaluationFrame = 0;
	int64 InputSerial = 0;
	float Alpha = 0.0f;
	bool bHasPose = false;
	FTransform MeshWorld = FTransform::Identity;
	FVector AimTarget = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	FVector MuzzleWorld = FVector::ZeroVector;
	FVector SpawnWorld = FVector::ZeroVector;
	FVector BarrelWorld = FVector::ForwardVector;
};

/** 완료된 루트와 선택적 총열 진단을 게임 스레드에 사본으로 전달한다. */
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

	void PublishRifleAim(const FLNPUAFRifleAimSnapshot& Snapshot)
	{
		FScopeLock Lock(&Mutex);
		RifleAim = Snapshot;
		RifleAim.InputSerial = InputSerial;
	}

	FLNPUAFRifleAimSnapshot ReadRifleAim() const
	{
		FScopeLock Lock(&Mutex);
		return RifleAim;
	}

private:
	mutable FCriticalSection Mutex;
	FTransform RootTransform = FTransform::Identity;
	int64 InputSerial = 0;
	bool bHasRoot = false;
	FLNPUAFRifleAimSnapshot RifleAim;
};

template<>
struct TStructOpsTypeTraits<FLNPUAFRootFeedback> : public TStructOpsTypeTraitsBase2<FLNPUAFRootFeedback>
{
	enum { WithCopy = false };
};

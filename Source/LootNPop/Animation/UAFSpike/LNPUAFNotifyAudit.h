// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

class UAnimNotifyState;
class UAnimSequenceBase;
class USkeletalMeshComponent;
struct FAnimNotifyEventReference;

/** 스파이크 폰의 실제 콜백/대상과 엔티티 정리만 관찰하며 게임 상태를 바꾸지 않는다. */
namespace LNPUAFNotifyAudit
{
	void Trace(const TCHAR* Phase, const UAnimNotifyState* Notify, USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference);
	void TraceMeleeEntity(USkeletalMeshComponent* MeshComp, FMassEntityHandle Entity, bool bDestroy);
}

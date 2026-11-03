// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "LNPUAFRagdollPoseAnimInstance.generated.h"

/**
 * UAF 시험 폰의 랙돌 동안 표시 메시 엔진 평가가 참조 포즈 대신 사망 순간 포즈를 내게 한다.
 * 그래프 없는 네이티브 인스턴스다. 초기화 때 읽기 버퍼(UAF가 마지막으로 쓴 포즈)를 로컬 포즈로 잡아 두고 매 평가 그대로 낸다.
 * 물리 바디가 있는 본은 물리 블렌드가 덮으므로, 실제로 남는 것은 바디 없는 본(손가락 등)과 텔레포트가 바디를 옮길 때의 기준 포즈다.
 */
UCLASS(Transient, NotBlueprintable)
class ULNPUAFRagdollPoseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

namespace LNPUAFRagdollPose
{
	/** 랙돌 동안 표시 메시에 걸 애님 클래스. LNP.UAFSpike.RagdollHoldPose 0이면 nullptr(이전 동작 — 참조 포즈 평가). */
	UClass* GetRagdollAnimClass();

	/** LNP.UAFSpike.DebugRagdoll: 표시 메시가 텔레포트 이동할 때마다 본 버퍼·물리 바디의 손 간격을 남긴다. RagdollStartFrame이 0이면 기록하지 않는다. */
	void BindTeleportProbe(USkeletalMeshComponent& VisualMesh, UObject& Owner, const uint64* RagdollStartFrame);

	/** LNP.UAFSpike.DebugRagdoll: 랙돌 초반 프레임마다 같은 값을 남긴다. */
	void TickProbe(const USkeletalMeshComponent& VisualMesh, uint64 RagdollStartFrame);
}

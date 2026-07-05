// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentComponent.h"
#include "LNPMassAgentComponent.generated.h"

/**
 * UMassAgentComponent의 NetID 캐싱 타이밍 갭을 보정하는 서브클래스 (Phase 6.5).
 *
 * 엔진의 SetEntityHandleInternal은 엔티티 핸들이 설정되는 순간 FMassNetworkIDFragment를 읽어
 * 복제 프로퍼티 NetID에 1회 캐싱한다. 그런데 에이전트 경로(컴포넌트 등록이 엔티티를 생성하는
 * 플레이어 폰)는 그 시점에 UMassNetworkIDFragmentInitializer(Add 옵저버)가 아직 실행되기 전이라
 * NetID가 무효(0)로 남고, 이후 갱신 경로가 없어 클라이언트 퍼펫 링크가 영원히
 * PuppetPendingReplication 상태에 머문다 (PIE 실측: netIDValid=0 pendingRep=1).
 * Enemy처럼 엔티티가 먼저 생성되고 액터가 나중에 링크되는 경로는 프래그먼트가 이미 채워져 있어 무관.
 *
 * 옵저버 실행 이후 시점에 프래그먼트를 재조회해 NetID를 채우는 것으로 해결한다.
 */
UCLASS()
class LOOTNPOP_API ULNPMassAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

protected:
	virtual void SetEntityHandleInternal(const FMassEntityHandle NewHandle) override;

private:
	/** NetworkID 옵저버가 프래그먼트를 채운 뒤 NetID를 재조회한다. 미해결 시 0.1s 간격 재시도 (최대 10회). */
	void TryResolveNetIDFromFragment(int32 AttemptCount);

	FTimerHandle NetIDRetryTimerHandle;
};

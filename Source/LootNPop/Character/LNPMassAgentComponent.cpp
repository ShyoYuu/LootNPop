// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPMassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassReplicationFragments.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "LootNPop.h"

void ULNPMassAgentComponent::SetEntityHandleInternal(const FMassEntityHandle NewHandle)
{
	Super::SetEntityHandleInternal(NewHandle);

	// 서버의 에이전트 생성 경로에서만 필요하다: 핸들은 유효한데 Super가 NetID를 캐싱하지 못한 경우
	// (NetworkID 옵저버 미실행 시점). 클라이언트(퍼펫)와 Standalone(복제 트레잇 미적용)은 해당 없음.
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Standalone || World->GetNetMode() == NM_Client)
		return;

	if (!NewHandle.IsSet() || GetNetID().IsValid())
		return;

	TryResolveNetIDFromFragment(0);
}

void ULNPMassAgentComponent::TryResolveNetIDFromFragment(int32 AttemptCount)
{
	UWorld* World = GetWorld();
	const FMassEntityHandle Entity = GetEntityHandle();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem || !Entity.IsSet())
		return; // 엔티티가 해제된 경우 등 — 재시도 무의미

	const FMassEntityManager& EntityManager = MassSubsystem->GetEntityManager();
	if (EntityManager.IsEntityValid(Entity))
	{
		if (const FMassNetworkIDFragment* NetIDFragment = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity))
		{
			if (NetIDFragment->NetID.IsValid())
			{
				NetID = NetIDFragment->NetID; // 복제 프로퍼티 — polled 방식이라 별도 dirty 마킹 불필요
				return;
			}
		}
		else
		{
			return; // NetworkID 프래그먼트가 없는 아키타입 (복제 대상 아님) — 재시도 무의미
		}
	}

	if (AttemptCount >= 10)
	{
		// 이 경고가 나오면 해당 폰은 클라이언트 퍼펫 링크가 성립하지 않는다 (클라 예측 판정 대상에서 제외됨).
		UE_LOG(LogLootNPop, Warning, TEXT("NetID resolve failed after %d attempts: owner=%s — client puppet linking will not work for this pawn"),
			AttemptCount, *GetNameSafe(GetOwner()));
		return;
	}

	World->GetTimerManager().SetTimer(NetIDRetryTimerHandle,
		FTimerDelegate::CreateUObject(this, &ULNPMassAgentComponent::TryResolveNetIDFromFragment, AttemptCount + 1),
		0.1f, false);
}

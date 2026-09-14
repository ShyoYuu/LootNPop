// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPMassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassReplicationFragments.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "LootNPop.h"

namespace
{
	/**
	 * UMassAgentSubsystem::ReplicatedAgentComponents에서 해당 NetID의 현재 주인을 찾는다.
	 * 공개 접근자가 없어 리플렉션으로 원본을 그대로 읽는다 — 사본 장부를 두면 엔진의 등록·해제 시점과
	 * 어긋나 멀쩡한 등록을 막을 수 있다.
	 */
	UMassAgentComponent* FindReplicatedAgentByNetID(const UWorld* World, const FMassNetworkID NetID)
	{
		UMassAgentSubsystem* AgentSubsystem = UWorld::GetSubsystem<UMassAgentSubsystem>(World);
		if (!AgentSubsystem)
			return nullptr;

		static const FMapProperty* MapProperty = CastField<FMapProperty>(
			UMassAgentSubsystem::StaticClass()->FindPropertyByName(TEXT("ReplicatedAgentComponents")));
		if (!MapProperty)
			return nullptr;

		FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(AgentSubsystem));
		for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
		{
			if (!MapHelper.IsValidIndex(Index))
				continue;

			if (*reinterpret_cast<const FMassNetworkID*>(MapHelper.GetKeyPtr(Index)) == NetID)
				return reinterpret_cast<const TObjectPtr<UMassAgentComponent>*>(MapHelper.GetValuePtr(Index))->Get();
		}

		return nullptr;
	}
}

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
	if (EntityManager.IsEntityActive(Entity))
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

void ULNPMassAgentComponent::OnRep_NetID()
{
	// 엔진은 같은 NetID가 두 번 등록되면 그 자리에서 죽는다
	// (UMassAgentSubsystem::NotifyMassAgentComponentReplicated의 check). NetID는 서버의 단조 증가
	// 카운터에서 나오고 Iris는 값이 그대로면 RepNotify를 다시 부르지 않으므로, 중복이 보인다는 것은
	// **서로 다른 두 컴포넌트가 같은 엔티티에 물려 있다**는 뜻이다. 그 조건을 아직 특정하지 못했으므로
	// 크래시 대신 양쪽 주인을 로그로 남기고 두 번째 등록만 건너뛴다.
	// 건너뛴 컴포넌트는 Mass 퍼펫 링크만 성립하지 않으며 Actor 복제 표현은 그대로 동작한다.
	if (const UMassAgentComponent* Existing = FindReplicatedAgentByNetID(GetWorld(), GetNetID()))
	{
		// 두 컴포넌트의 엔티티 핸들을 같이 남긴다 — 같으면 "한 엔티티에 Actor 둘",
		// 다르면 "두 엔티티가 같은 NetID"로 원인 갈래가 바로 갈린다.
		UE_LOG(LogLootNPop, Error, TEXT("Duplicate Mass NetID %u on replicated agent: incoming owner=%s entity=%s, already registered owner=%s entity=%s, same component=%d. Skipping puppet registration."),
			GetNetID().GetValue(),
			*GetNameSafe(GetOwner()), *GetEntityHandle().DebugGetDescription(),
			*GetNameSafe(Existing->GetOwner()), *Existing->GetEntityHandle().DebugGetDescription(),
			Existing == this ? 1 : 0);
		return;
	}

	UE_LOG(LogLootNPop, Log, TEXT("[MassAgent] Replicated agent registered: NetID=%u owner=%s entity=%s"),
		GetNetID().GetValue(), *GetNameSafe(GetOwner()), *GetEntityHandle().DebugGetDescription());

	Super::OnRep_NetID();
}

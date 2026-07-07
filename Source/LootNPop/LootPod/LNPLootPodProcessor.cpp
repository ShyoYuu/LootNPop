// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPLootPodProcessor.h"
#include "LNPLootPodMassTypes.h"
#include "LNPLootPod.h"
#include "LNPMassUtils.h"
#include "LootNPop.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassActorSubsystem.h"

// --- 상태 전환 알림 및 로직 처리를 위한 통합 커맨드 ---
struct FLNPPodStateTransitionCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<ALNPLootPod> Pod;
		int32 PodID;
		ELNPLootPodState OldState;
		ELNPLootPodState NewState;
		FVector Location;
	};

	FLNPPodStateTransitionCommand()
		: FMassBatchedCommand(EMassCommandOperationType::None)
	{}

	void Add(ALNPLootPod* InPod, int32 InPodID, ELNPLootPodState InOldState, ELNPLootPodState InNewState, const FVector& InLocation)
	{
		Entries.Add({ InPod, InPodID, InOldState, InNewState, InLocation });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			ALNPLootPod* Pod = Entry.Pod.Get();
			
			// 1. 비주얼/Actor 상태 알림
			if (Pod != nullptr)
			{
				Pod->UpdateVisuals(Entry.NewState);
			}

			if (Entry.OldState != Entry.NewState)
			{
				UE_LOG(LogLootNPop, Log, TEXT("[LootPod] PodID %d transitioned from %s to %s at location %s"), 
					Entry.PodID, 
					*UEnum::GetValueAsString(Entry.OldState), 
					*UEnum::GetValueAsString(Entry.NewState), 
					*Entry.Location.ToString());
			}

			// 2. 상태 전환에 따른 특수 처리
			if (Entry.NewState == ELNPLootPodState::Popped)
			{
				// TODO: 통합 보상 드롭 — 실제 보상 Actor/아이템을 여기에 스폰
			}
		}
	}

	virtual void Reset() override
	{
		Entries.Reset();
		FMassBatchedCommand::Reset();
	}

	virtual SIZE_T GetAllocatedSize() const override { return Entries.GetAllocatedSize(); }

	virtual int32 GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// --- 1. ULNPIdleToLootingProcessor (Idle → Looting 전환) ---

ULNPIdleToLootingProcessor::ULNPIdleToLootingProcessor()
	: EntityQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void ULNPIdleToLootingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodIdleTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

	// 잠재적 루터 쿼리
	PlayerQuery.AddTagRequirement<FLNPPlayerLootingTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FLNPPlayerLootingFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPIdleToLootingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// LootPod MassReplication(Phase 7) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — 상태 전환은 서버 전용(Actor 복제로 전달).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	// Player 위치 Cache
	struct FLooterInfo { FVector Location; };
	TArray<FLooterInfo> ActiveLooters;
	
	PlayerQuery.ForEachEntityChunk(Context, [&ActiveLooters](FMassExecutionContext& PlayerContext)
	{
		const int32 NumPlayers = PlayerContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();

		for (int32 i = 0; i < NumPlayers; ++i)
		{
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation() });
		}
	});

	EntityQuery.ForEachEntityChunk(Context, [this, &ActiveLooters](FMassExecutionContext& IterContext)
	{
		const TArrayView<FLNPLootPodFragment> LootPods = IterContext.GetMutableFragmentView<FLNPLootPodFragment>();
		const TConstArrayView<FTransformFragment> Transforms = IterContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = IterContext.GetMutableFragmentView<FMassActorFragment>();

		for (FMassExecutionContext::FEntityIterator i = IterContext.CreateEntityIterator(); i; ++i)
		{
			const FVector PodLocation = Transforms[i].GetTransform().GetLocation();
			const float MaxDistSq = LootPods[i].LootableDistSquared;

			bool bPlayerDetected = false;
			for (const FLooterInfo& Looter : ActiveLooters)
			{
				if (FVector::DistSquared(PodLocation, Looter.Location) <= MaxDistSq)
				{
					bPlayerDetected = true;
					break;
				}
			}

			if (bPlayerDetected)
			{
				LootPods[i].State = ELNPLootPodState::Looting;

				// 상태 전환 알림
				ALNPLootPod* PodActor = Cast<ALNPLootPod>(ActorFragments[i].GetMutable());
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, LootPods[i].PodID, ELNPLootPodState::Idle, ELNPLootPodState::Looting, PodLocation);

				// Tag 변경 지연
				IterContext.Defer().RemoveTag<FLNPLootPodIdleTag>(IterContext.GetEntity(i));
				IterContext.Defer().AddTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));
			}
		}
	});
}

// --- 2. ULNPLootingProcessor (루팅 처리) ---

ULNPLootingProcessor::ULNPLootingProcessor()
	: EntityQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
}

void ULNPLootingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodLootingTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

	// 루터 쿼리
	PlayerQuery.AddTagRequirement<FLNPPlayerLootingTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FLNPPlayerLootingFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPLootingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// LootPod MassReplication(Phase 7) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — 게이지·상태 판정은 서버 전용(Actor 복제로 전달).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// 1. Player 정보 Cache (실행당 한 번)
	struct FLooterInfo { FVector Location; float BuffedLootSpeed; };
	TArray<FLooterInfo> ActiveLooters;
	PlayerQuery.ForEachEntityChunk(Context, [&ActiveLooters](FMassExecutionContext& PlayerContext)
	{
		const int32 NumPlayers = PlayerContext.GetNumEntities();
		const TConstArrayView<FLNPPlayerLootingFragment> LootingFragments = PlayerContext.GetFragmentView<FLNPPlayerLootingFragment>();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < NumPlayers; ++i)
		{
			// TODO: Player 스탯/버프에서 실제 BuffedLootSpeed 가져오기
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation(), LootingFragments[i].BuffedLootSpeed });
		}
	});

	// 2. 모든 루팅 중인 LootPod 처리
	EntityQuery.ForEachEntityChunk(Context, [DeltaTime, &ActiveLooters](FMassExecutionContext& IterContext)
	{
		const int32 NumEntities = IterContext.GetNumEntities();
		const TArrayView<FLNPLootPodFragment> LootPods = IterContext.GetMutableFragmentView<FLNPLootPodFragment>();
		const TConstArrayView<FTransformFragment> Transforms = IterContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = IterContext.GetMutableFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ALNPLootPod* PodActor = Cast<ALNPLootPod>(ActorFragments[i].GetMutable());
			const FVector PodLocation = Transforms[i].GetTransform().GetLocation();
			const int32 PodID = LootPods[i].PodID;
			const float MaxDistSq = LootPods[i].LootableDistSquared;

			// 근접 체크: 범위 내 루터의 루팅 속도를 합산한다 (여러 명이 함께 루팅하면 그만큼 빨라짐)
			float FinalLootSpeed = 0.0f;
			bool  bHasValidLooter = false;
			for (const auto& Looter : ActiveLooters)
			{
				if (FVector::DistSquared(PodLocation, Looter.Location) <= MaxDistSq)
				{
					FinalLootSpeed += Looter.BuffedLootSpeed;
					bHasValidLooter = true;
				}
			}

			// A. 유효한 루터가 있으면: 게이지 업데이트 및 완료 체크
			if (bHasValidLooter)
			{
				LootPods[i].CurrentGauge = FMath::Min(LootPods[i].MaxGauge, LootPods[i].CurrentGauge + (FinalLootSpeed * DeltaTime));

				// Actor 복제 프로퍼티 동기화 (Phase 7) — 게임 스레드 지연 실행, 2% 임계값은 SetGaugePercent가 처리
				if (PodActor)
				{
					const float GaugePercent = (0.0f < LootPods[i].MaxGauge) ? LootPods[i].CurrentGauge / LootPods[i].MaxGauge : 0.0f;
					const TWeakObjectPtr<ALNPLootPod> WeakPod = PodActor;
					IterContext.Defer().PushCommand<FMassDeferredSetCommand>([WeakPod, GaugePercent](FMassEntityManager&)
					{
						if (ALNPLootPod* Pod = WeakPod.Get())
							Pod->SetGaugePercent(GaugePercent);
					});
				}

				// 완료 체크 — 엔티티를 즉시 파괴하므로 Fragment 상태/Tag 갱신은 불필요하다.
				// Popped 상태 전파(비주얼·복제)는 전환 커맨드의 UpdateVisuals가 담당한다.
				if (LootPods[i].MaxGauge <= LootPods[i].CurrentGauge)
				{
					IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Popped, PodLocation);
					IterContext.Defer().DestroyEntity(IterContext.GetEntity(i));
				}
			}
			// B. 범위 내 유효한 루터 없음: 중단
			else
			{
				LootPods[i].State = ELNPLootPodState::Idle;
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Idle, PodLocation);

				IterContext.Defer().RemoveTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));
				IterContext.Defer().AddTag<FLNPLootPodIdleTag>(IterContext.GetEntity(i));
			}
		}
	});
}

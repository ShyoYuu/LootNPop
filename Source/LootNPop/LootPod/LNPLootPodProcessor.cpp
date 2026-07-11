// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPLootPodProcessor.h"
#include "LNPLootPodMassTypes.h"
#include "LNPLootPod.h"
#include "LNPMassUtils.h"
#include "LootNPop.h"
#include "Enemy/LNPEnemyMassTypes.h"

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

	// 루팅 존 활성화 요청 쿼리 — Interaction Input이 부여한 1회성 Tag 보유자
	PlayerQuery.AddTagRequirement<FLNPPlayerLootingTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPIdleToLootingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// LootPod MassReplication(Phase 7) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — 상태 전환은 서버 전용(Actor 복제로 전달).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	// 활성화 요청자 위치 Cache
	struct FLooterInfo { FVector Location; };
	TArray<FLooterInfo> ActiveLooters;
	TArray<FMassEntityHandle> RequesterEntities;

	PlayerQuery.ForEachEntityChunk(Context, [&ActiveLooters, &RequesterEntities](FMassExecutionContext& PlayerContext)
	{
		const int32 NumPlayers = PlayerContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();

		for (int32 i = 0; i < NumPlayers; ++i)
		{
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation() });
			RequesterEntities.Add(PlayerContext.GetEntity(i));
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

	// 활성화 요청 Tag는 1회성 — 활성화 성사 여부와 무관하게 이번 실행에서 소비한다.
	// (이미 Looting 중인 Pod 앞에서 누른 입력은 프레즌스 기반 기여가 이어받으므로 별도 처리가 필요 없다)
	for (const FMassEntityHandle& Requester : RequesterEntities)
	{
		Context.Defer().RemoveTag<FLNPPlayerLootingTag>(Requester);
	}
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
	// Execute에서 GetMutableFragmentView로 Actor 포인터를 꺼내므로 ReadWrite로 선언해야 한다 (ReadOnly면 assert)
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

	// 루터 쿼리 — 기여는 프레즌스 기반: 활성화된 존 범위 안의 모든 플레이어가 루팅에 기여한다.
	// FLNPPlayerLootingFragment는 Optional — 한 번도 상호작용하지 않은 플레이어는 기본 속도 1.0으로 기여.
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FLNPPlayerLootingFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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
		// Optional Fragment — 없는 청크는 빈 뷰가 반환된다
		const TConstArrayView<FLNPPlayerLootingFragment> LootingFragments = PlayerContext.GetFragmentView<FLNPPlayerLootingFragment>();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < NumPlayers; ++i)
		{
			// BuffedLootSpeed는 LootSpeed Attribute와 동기화된다 (ALNPPlayerCharacter::PushLootSpeedToEntity).
			// Fragment가 없는 플레이어(버프 이력·상호작용 이력 없음)는 기본 속도 1.0으로 기여.
			const float LootSpeed = LootingFragments.IsEmpty() ? 1.0f : LootingFragments[i].BuffedLootSpeed;
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation(), LootSpeed });
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

			// A. 게이지 갱신 — 루터가 있으면 합산 속도로 증가, 전원 이탈 시 감쇠 (존은 활성 유지)
			if (bHasValidLooter)
			{
				LootPods[i].CurrentGauge = FMath::Min(LootPods[i].MaxGauge, LootPods[i].CurrentGauge + (FinalLootSpeed * DeltaTime));
			}
			else
			{
				const float DecayPerSecond = LootPods[i].MaxGauge * LNPLootPodGaugeDecayFractionPerSecond;
				LootPods[i].CurrentGauge = FMath::Max(0.0f, LootPods[i].CurrentGauge - (DecayPerSecond * DeltaTime));
			}

			// Actor 복제 프로퍼티 동기화 (Phase 7) — 게임 스레드 지연 실행, 2% 임계값은 SetGaugePercent가 처리
			if (PodActor)
			{
				const float GaugePercent = (0.0f < LootPods[i].MaxGauge) ? LootPods[i].CurrentGauge / LootPods[i].MaxGauge : 0.0f;
				const TWeakObjectPtr<ALNPLootPod> WeakPod = PodActor;
				IterContext.Defer().PushCommand<FMassDeferredSetCommand>([WeakPod, GaugePercent](FMassEntityManager&)
				{
					if (ALNPLootPod* Pod = WeakPod.Get())
					{
						Pod->SetGaugePercent(GaugePercent);

						// Actor 재스폰 자기치유 — LOD로 소멸했다 재스폰된 Actor는 기본값(Idle)로 시작하므로,
						// 엔티티가 Looting인데 Actor 비주얼이 다르면 상태를 밀어 넣는다 (§5.6).
						// 방치하면 Idle로 보이는 Pod가 프레즌스 기여로 몰래 차올라 "갑자기 Pop"하는 버그가 된다.
						if (Pod->GetCurrentState() != ELNPLootPodState::Looting)
						{
							Pod->UpdateVisuals(ELNPLootPodState::Looting);
						}
					}
				});
			}

			// B. 완료 체크 — 엔티티를 즉시 파괴하므로 Fragment 상태/Tag 갱신은 불필요하다.
			// Popped 상태 전파(비주얼·복제)는 전환 커맨드의 UpdateVisuals가 담당한다.
			if (LootPods[i].MaxGauge <= LootPods[i].CurrentGauge)
			{
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Popped, PodLocation);
				IterContext.Defer().DestroyEntity(IterContext.GetEntity(i));
			}
			// C. 감쇠 끝에 게이지 0 도달 — 루팅 프로세스 완전 취소, 재활성화는 Interaction Input부터
			else if (!bHasValidLooter && LootPods[i].CurrentGauge <= 0.0f)
			{
				LootPods[i].State = ELNPLootPodState::Idle;
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Idle, PodLocation);

				IterContext.Defer().RemoveTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));
				IterContext.Defer().AddTag<FLNPLootPodIdleTag>(IterContext.GetEntity(i));
			}
		}
	});
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPTargetingSubsystem.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "LootNPop.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassStateTreeTypes.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationTypes.h"
#include "MassMovementFragments.h"
#include "MassRepresentationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassNavigationFragments.h"
#if WITH_EDITOR
#include "MassDebugDrawHelpers.h"
#endif

// --- Scoring Processor (점수 산정) ---

ULNPEnemyScoringProcessor::ULNPEnemyScoringProcessor()
	: ScoringQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	// 모두 이동한 후 다음 프레임의 후보를 준비
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
}

void ULNPEnemyScoringProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ScoringQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyTargetingCandidateFragment>(EMassFragmentAccess::ReadWrite);
	ScoringQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	ScoringQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	ScoringQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	ScoringQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
}

void ULNPEnemyScoringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 1. 모든 Player 수집
	struct FPlayerData { FMassEntityHandle Handle; FVector Location; };
	TArray<FPlayerData> Players;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& PlayerContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < PlayerContext.GetNumEntities(); ++i)
		{
			Players.Add({ PlayerContext.GetEntity(i), Transforms[i].GetTransform().GetLocation() });
		}
	});

	if (Players.Num() == 0)
	{
		return;
	}

	// 2. Enemy 처리
	ScoringQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyFragment> EnemyFragments = EnemyContext.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyTargetingCandidateFragment> CandidateFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyTargetingCandidateFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const FLNPEnemyTargetingConfig& TConfig = SharedFragment.Config->TargetingConfig;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FTransform& EnemyTransform = Transforms[i].GetTransform();
			const FVector EnemyLoc = EnemyTransform.GetLocation();
			const FVector EnemyForward = EnemyTransform.GetUnitAxis(EAxis::X);
			const FLNPEnemyFragment& EnemyData = EnemyFragments[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			FLNPEnemyTargetingCandidateFragment& CandidateData = CandidateFragments[i];
			const ELNPTargetingState PreviousState = Targeting.State;

			CandidateData.Reset();

			// Leash 페널티: Enemy가 ParentPod에서 MaxLeashDistance에 가까워질수록 점수가 급격히 0으로 감소
			const float DistToLeash = FVector::Dist(EnemyLoc, EnemyData.ParentPodLocation);
			const float LeashFactor = FMath::Square(FMath::Clamp(1.0f - (DistToLeash / TConfig.MaxLeashDistance), 0.0f, 1.0f));

			struct FCandidate { FMassEntityHandle Handle; FVector Location; float DistSq; };
			TArray<FCandidate, TInlineAllocator<8>> VisiblePlayers;

			for (const auto& Player : Players)
			{
				const float DistSq = FVector::DistSquared(EnemyLoc, Player.Location);

				if (FMath::Square(TConfig.MaxTargetingDistance) < DistSq)
					continue;

				bool bVisible = false;
				if (PreviousState == ELNPTargetingState::Confirmed)
				{
					bVisible = true;
				}
				else if (DistSq <= FMath::Square(TConfig.AwarenessDistance))
				{
					bVisible = true;
				}
				else if (DistSq <= FMath::Square(TConfig.VisionDistance))
				{
					const FVector DirToTarget = (Player.Location - EnemyLoc).GetSafeNormal();
					const float DotToTarget = FVector::DotProduct(EnemyForward, DirToTarget);
					const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(DotToTarget));

					if (AngleToTarget <= (TConfig.VisionAngle * 0.5f))
					{
						bVisible = true;
					}
				}

				if (bVisible)
				{
					VisiblePlayers.Add({ Player.Handle, Player.Location, DistSq });
				}
			}

			if (VisiblePlayers.Num() > 0)
			{
				VisiblePlayers.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });

				CandidateData.NumPotentialTargets = FMath::Min(VisiblePlayers.Num(), 4);

				for (int32 TargetIdx = 0; TargetIdx < CandidateData.NumPotentialTargets; ++TargetIdx)
				{
					const auto& Candidate = VisiblePlayers[TargetIdx];
					CandidateData.PotentialTargets[TargetIdx] = Candidate.Handle;

					float Score = 1000000.0f / (FMath::Sqrt(Candidate.DistSq) + 1.0f);
					Score *= LeashFactor;

					//if (Score > KINDA_SMALL_NUMBER)
					{
						//bool bIsMelee = SharedFragment.Config->EnemyTypeTag.ToString().Contains(TEXT("Melee"), ESearchCase::IgnoreCase);
						bool bIsMelee = true; // 테스트용

						FMassEntityHandle EnemyEntity = EnemyContext.GetEntity(i);
						FMassEntityHandle PlayerHandle = Candidate.Handle;

						Context.Defer().PushCommand<FMassDeferredSetCommand>([EnemyEntity, PlayerHandle, Score, bIsMelee](const FMassEntityManager& InOutEntityManager)
						{
							ULNPTargetingSubsystem* TargetingSubsystem = UWorld::GetSubsystem<ULNPTargetingSubsystem>(InOutEntityManager.GetWorld());
							if (TargetingSubsystem != nullptr)
								TargetingSubsystem->RegisterEnemyInterest(EnemyEntity, PlayerHandle, Score, bIsMelee);
						});
					}
				}
			}
		}
	});
}


// --- Targeting Processor (타게팅) ---

ULNPEnemyTargetingProcessor::ULNPEnemyTargetingProcessor()
	: TargetingQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void ULNPEnemyTargetingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	TargetingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	TargetingQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadWrite);
	TargetingQuery.AddRequirement<FLNPEnemyTargetingCandidateFragment>(EMassFragmentAccess::ReadOnly);
	TargetingQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	TargetingQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	TargetingQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	TargetingQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPTargetingSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyTargetingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPTargetingSubsystem& TargetingSubsystem = Context.GetMutableSubsystemChecked<ULNPTargetingSubsystem>();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	// 1. Player 위치 수집
	TMap<FMassEntityHandle, FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& PlayerContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < PlayerContext.GetNumEntities(); ++i)
		{
			PlayerLocations.Add(PlayerContext.GetEntity(i), Transforms[i].GetTransform().GetLocation());
		}
	});

	// 2. 전역 재균형 수행
	TargetingSubsystem.RebalanceSlots();

	TArray<FMassEntityHandle> EntitiesToSignal;

	// 3. 결과 동기화 및 특정 타겟 정보 업데이트
	TargetingQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FLNPEnemyTargetingCandidateFragment> CandidateFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingCandidateFragment>();

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FVector EnemyLocation = Transforms[i].GetTransform().GetLocation();
			FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			const FLNPEnemyTargetingCandidateFragment& CandidateData = CandidateFragments[i];
			const ELNPTargetingState OldState = Targeting.State;
			const FMassEntityHandle OldTarget = Targeting.TargetPlayer;
			Targeting.ResetTargeting();

			bool bFoundConfirmed = false;
			
			// 확정된 최선의 잠재적 타겟 탐색
			for (int32 TargetIdx = 0; TargetIdx < CandidateData.NumPotentialTargets; ++TargetIdx)
			{
				FMassEntityHandle PotentialTarget = CandidateData.PotentialTargets[TargetIdx];
				if (TargetingSubsystem.IsSlotConfirmed(EnemyContext.GetEntity(i), PotentialTarget))
				{
					Targeting.TargetPlayer = PotentialTarget;
					Targeting.State = ELNPTargetingState::Confirmed;
					
					// 선택된 타겟의 정밀 정보 업데이트
					if (const FVector* PLoc = PlayerLocations.Find(PotentialTarget))
					{
						Targeting.TargetLocation = *PLoc;
						Targeting.DistanceToTargetSq = FVector::DistSquared(EnemyLocation, *PLoc);
					}

					bFoundConfirmed = true;
					break;
				}
			}

			if (!bFoundConfirmed)
			{
				// 잠재적 타겟이 있지만 확정되지 않은 경우, Alert 상태 진입
				if (CandidateData.NumPotentialTargets > 0)
				{
					Targeting.TargetPlayer = CandidateData.PotentialTargets[0];
					Targeting.State = ELNPTargetingState::Alert;

					if (const FVector* PLoc = PlayerLocations.Find(Targeting.TargetPlayer))
					{
						Targeting.TargetLocation = *PLoc;
						Targeting.DistanceToTargetSq = FVector::DistSquared(EnemyLocation, *PLoc);
					}

				}
				else
				{
					Targeting.State = ELNPTargetingState::None;
					Targeting.ResetTargeting();
				}
			}

			if (OldState != Targeting.State)
			{
				UE_LOG(LogLootNPop, Log, TEXT("Entity %d changed state from %s to %s"), EnemyContext.GetEntity(i).Index, *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(Targeting.State));
			}

			if (OldState != Targeting.State || OldTarget != Targeting.TargetPlayer)
			{
				EntitiesToSignal.Add(EnemyContext.GetEntity(i));
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

// --- Target Follow Processor (타겟 추적) ---

ULNPEnemyTargetFollowProcessor::ULNPEnemyTargetFollowProcessor()
	: FollowQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 타게팅 후 Behavior 단계에서 Intent 처리
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetingProcessor::StaticClass()->GetFName());
}

void ULNPEnemyTargetFollowProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	FollowQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FollowQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	FollowQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	FollowQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	FollowQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	FollowQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	FollowQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	FollowQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyTargetFollowProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	TArray<FMassEntityHandle> EntitiesToSignal;

	FollowQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FVector EntityLocation = Transforms[i].GetTransform().GetLocation();
			FMassMoveTargetFragment& MoveTarget = MoveTargets[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			// 1. MoveTarget과 타게팅 데이터 동기화
			if (Targeting.TargetPlayer.IsValid())
			{
				const float ActualDistance = FMath::Sqrt(Targeting.DistanceToTargetSq);
				// StopBuffer >= ArrivalBuffer(30)으로 AttackRange 내에서 도착 신호 발생 보장
				const float StopBuffer = FMath::Max(30.f, FMath::Min(AttackRange * 0.1f, 100.f));
				const float StopDist = FMath::Max(0.f, AttackRange - StopBuffer);

				if (ActualDistance <= StopDist)
				{
					// 정지 구역: 타겟 방향 전환 (방향은 MoveTarget.Center 방향 사용)
					MoveTarget.Center = Targeting.TargetLocation;
					MoveTarget.DistanceToGoal = 0.f;
					// StateTree 신호 발송으로 SteeringTask가 DistanceToTarget <= AttackRange 평가 가능
					if (Targeting.State == ELNPTargetingState::Confirmed)
						EntitiesToSignal.Add(EnemyContext.GetEntity(i));
				}
				else
				{
					const FVector DirToTarget = (Targeting.TargetLocation - EntityLocation).GetSafeNormal();
					MoveTarget.Center = Targeting.TargetLocation - DirToTarget * StopDist;
					MoveTarget.DistanceToGoal = ActualDistance - StopDist;
				}
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

// --- Movement Processor (이동) ---

ULNPEnemyMovementProcessor::ULNPEnemyMovementProcessor()
	: MovementQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void ULNPEnemyMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	MovementQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	MovementQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	MovementQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	MovementQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	MovementQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	MovementQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	MovementQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPSurfaceCacheSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPEnemyMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const ULNPSurfaceCacheSubsystem& SurfaceCache = Context.GetSubsystemChecked<ULNPSurfaceCacheSubsystem>();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	TArray<FMassEntityHandle> EntitiesToSignal;

	MovementQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TArrayView<FMassActorFragment> ActorFragments = EnemyContext.GetMutableFragmentView<FMassActorFragment>();
		const TArrayView<FTransformFragment> Transforms = EnemyContext.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> VelocityFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyVelocityFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float RotationRate = SharedFragment.Config->MovementConfig.RotationRate;
		const FVector GravityOrigin = SharedFragment.Config->MovementConfig.GravityOrigin;
		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;
		const float BaseMoveSpeed = SharedFragment.Config->MovementConfig.MoveSpeed;
		const float GravityStrength = SharedFragment.Config->MovementConfig.GravityStrength;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			FTransform& EntityTransform = Transforms[i].GetMutableTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();
			const FMassMoveTargetFragment& MoveTarget = MoveTargets[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			const FVector UpDir = (GravityOrigin - EntityLocation).GetSafeNormal(); // 내부 구형 세계에서 중심 방향 = Up
			const FQuat CurrentRotation = EntityTransform.GetRotation();

			const FVector TargetPos = MoveTarget.Center;
			const float DistSq = FVector::DistSquared(TargetPos, EntityLocation);
			const FVector DirToTarget = (TargetPos - EntityLocation).GetSafeNormal();
			const FVector TargetDirOnPlane = FVector::VectorPlaneProject(DirToTarget, UpDir).GetSafeNormal();

			float EffectiveSpeed = 0.0f;
			FVector OrientationIntent = FVector::ZeroVector;

			switch (Targeting.State)
			{
			case ELNPTargetingState::None:
				// 대기 이동: 타겟 지점으로 천천히 이동
				EffectiveSpeed = BaseMoveSpeed * 0.3f; // 느린 걷기
				OrientationIntent = TargetDirOnPlane;
				break;

			case ELNPTargetingState::Alert:
				// Alert: Player 방향 전환, ST 태스크에 의해 밀리지 않으면 이동 안 함
				EffectiveSpeed = 0.0f;
				OrientationIntent = TargetDirOnPlane;
				break;

			case ELNPTargetingState::Confirmed:
			{
				const float ActualDistance = Targeting.TargetPlayer.IsValid() ? FMath::Sqrt(Targeting.DistanceToTargetSq) : 0.0f;
				const float StopBuffer = FMath::Max(30.f, FMath::Min(AttackRange * 0.1f, 100.f));
				const float StopDist = FMath::Max(0.f, AttackRange - StopBuffer);
				EffectiveSpeed = (ActualDistance <= StopDist) ? 0.0f : BaseMoveSpeed;
				OrientationIntent = TargetDirOnPlane;
				break;
			}
			}

			// 목적지 도달 시 StateTree 신호 (None/Confirmed 상태용)
			if (EffectiveSpeed > 0.0f && DistSq < FMath::Square(30.0f)) // 도착 임계값
			{
				EntitiesToSignal.Add(EnemyContext.GetEntity(i));
				EffectiveSpeed = 0.0f;
			}

			// StateTree에서 명시적으로 속도를 설정한 경우 Override (예: SteeringTask)
			// Alert 상태는 StateTree 속도와 관계없이 항상 정지
			if (MoveTarget.DesiredSpeed.Get() > 0.0f && Targeting.State != ELNPTargetingState::Alert)
			{
				EffectiveSpeed = MoveTarget.DesiredSpeed.Get();
			}

			if (AActor* Actor = ActorFragments[i].GetMutable())
			{
				TWeakObjectPtr<AActor> WeakActor(Actor);
				const FVector CapturedOrientation = OrientationIntent;
				const FVector CapturedMoveInput   = EffectiveSpeed > 0.0f ? OrientationIntent : FVector::ZeroVector;
				EnemyContext.Defer().PushCommand<FMassDeferredSetCommand>(
					[WeakActor, CapturedOrientation, CapturedMoveInput](const FMassEntityManager&)
					{
						if (ALNPCharacterBase* LNPCharacter = Cast<ALNPCharacterBase>(WeakActor.Get()))
						{
							LNPCharacter->SetAIOrientationIntent(CapturedOrientation);
							LNPCharacter->SetAIMoveInput(CapturedMoveInput);
						}
					});
			}
			else
			{
				FVector& PhysVelocity = VelocityFragments[i].Velocity;

				// 상태는 전용 Tag(예: FLNPEnemyAirborneTag)가 아닌 PhysVelocity로 판단한다.
				// Tag 분리 방식(Archetype Chunk별 별도 Processor)이 Mass에서 더 관용적이며
				// 비행이 지속적이거나 고빈도 상태가 된다면 (예: 비행 Enemy) 고려할 가치가 있다.
				// 현재 넉백 전용 케이스에서 분기 비용은 무시할 수 있으며
				// 매 피격/착지 시 반복적인 Deferred AddTag/RemoveTag Archetype 마이그레이션을 피할 수 있다.
				if (!PhysVelocity.IsNearlyZero())
				{
					// 공중 물리: 중력 적용 및 속도 적분
					const FVector GravityDir = (EntityLocation - GravityOrigin).GetSafeNormal(); // 외향 = 아래
					PhysVelocity += GravityDir * GravityStrength * DeltaTime;

					const FVector NewPos = EntityLocation + PhysVelocity * DeltaTime;
					const FVector NewDir = (NewPos - GravityOrigin).GetSafeNormal();

					FVector SurfacePoint;
					if (SurfaceCache.GetSurfacePoint(NewDir, SurfacePoint))
					{
						const float SurfaceRadius = FVector::Dist(GravityOrigin, SurfacePoint);
						const float DistFromCenter = FVector::Dist(GravityOrigin, NewPos);

						if (DistFromCenter >= SurfaceRadius)
						{
							// 착지: 표면에 스냅하고 물리 정지
							EntityTransform.SetLocation(GravityOrigin + NewDir * SurfaceRadius);
							PhysVelocity = FVector::ZeroVector;
						}
						else
						{
							// 아직 공중: 자유 이동 및 Up 정렬 회전 유지
							EntityTransform.SetLocation(NewPos);
							const FVector NewUp = (GravityOrigin - NewPos).GetSafeNormal();
							const FVector HorizForward = FVector::VectorPlaneProject(EntityTransform.GetRotation().GetForwardVector(), NewUp).GetSafeNormal();
							if (!HorizForward.IsNearlyZero())
								EntityTransform.SetRotation(FRotationMatrix::MakeFromXZ(HorizForward, NewUp).ToQuat());
						}
					}
					else
					{
						EntityTransform.SetLocation(NewPos);
					}
				}
				else
				{
					// 지면: 일반 Intent 기반 이동
					FVector Velocity = FVector::ZeroVector;

					if (!OrientationIntent.IsNearlyZero())
					{
						const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(OrientationIntent, UpDir).ToQuat();
						const FQuat NewRotation = FMath::QInterpConstantTo(CurrentRotation, TargetQuat, DeltaTime, FMath::DegreesToRadians(RotationRate));
						EntityTransform.SetRotation(NewRotation);

						Velocity = (EffectiveSpeed > 0.0f) ? (OrientationIntent * EffectiveSpeed) : FVector::ZeroVector;
					}
					else
					{
						const FVector Forward = CurrentRotation.GetForwardVector();
						const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(Forward, UpDir).ToQuat();
						EntityTransform.SetRotation(TargetQuat);
					}

					// 경사 체크: ~45도보다 가파른 경사 오름 이동 차단 (MaxWalkSlopeCosine = 0.71f, Mover CommonLegacyMovementSettings 기준)
					constexpr float MaxWalkSlopeCosine = 0.71f;
					if (!Velocity.IsNearlyZero())
					{
						const FVector CurrentSurfaceDir = (EntityLocation - GravityOrigin).GetSafeNormal();
						const FVector TargetSurfaceDir = (EntityLocation + Velocity * DeltaTime - GravityOrigin).GetSafeNormal();
						FVector CurrentSurface, TargetSurface;
						if (SurfaceCache.GetSurfacePoint(CurrentSurfaceDir, CurrentSurface) &&
							SurfaceCache.GetSurfacePoint(TargetSurfaceDir, TargetSurface))
						{
							const FVector SlopeDelta = TargetSurface - CurrentSurface;
							if (FVector::DotProduct(SlopeDelta, UpDir) > 0.f) // 오름 경사만
							{
								const float TotalDist = SlopeDelta.Size();
								if (TotalDist > KINDA_SMALL_NUMBER)
								{
									const float HorizDist = FVector::VectorPlaneProject(SlopeDelta, UpDir).Size();
									if (HorizDist / TotalDist < MaxWalkSlopeCosine)
										Velocity = FVector::ZeroVector;
								}
							}
						}
					}

					const FVector DesiredPos = EntityLocation + Velocity * DeltaTime;
					const FVector DirToSurface = (DesiredPos - GravityOrigin).GetSafeNormal();

					FVector FinalPos = DesiredPos;
					FVector SurfacePoint;
					if (SurfaceCache.GetSurfacePoint(DirToSurface, SurfacePoint))
					{
						const float SurfaceRadius = FVector::Dist(GravityOrigin, SurfacePoint);
						FinalPos = GravityOrigin + DirToSurface * SurfaceRadius;
					}
					EntityTransform.SetLocation(FinalPos);
				}
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

#if WITH_EDITOR
// --- Debug Draw Processor (디버그 드로우) ---

ULNPEnemyDebugDrawProcessor::ULNPEnemyDebugDrawProcessor()
	: DebugQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetingProcessor::StaticClass()->GetFName());
}

void ULNPEnemyDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DebugQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	DebugQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
}

void ULNPEnemyDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UE::Mass::Debug::FLineBatcher LineBatcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

	DebugQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetFragmentView<FMassMoveTargetFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			FColor StateColor = FColor::Green; // 대기

			if (Targeting.State == ELNPTargetingState::Confirmed)
			{
				const float ActualDistance = FMath::Sqrt(Targeting.DistanceToTargetSq);
				if (ActualDistance <= AttackRange)
				{
					StateColor = FColor::Red; // 공격
				}
				else
				{
					StateColor = FColor::Blue; // 추격
				}
			}
			else if (Targeting.State == ELNPTargetingState::Alert)
			{
				StateColor = FColor::Yellow; // 경계
			}
			else
			{
				StateColor = FColor::Green; // 대기
			}

			const FVector Offset = (FVector::ZeroVector - EntityLocation).GetSafeNormal() * 50.0f;
			LineBatcher.DrawSolidBox(EntityLocation + Offset, FVector(25.0), StateColor);
			LineBatcher.DrawArrow(EntityTransform, 75.0f, FColor::Black);
		}
	});
}
#else
ULNPEnemyDebugDrawProcessor::ULNPEnemyDebugDrawProcessor()
	: DebugQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPEnemyDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPEnemyDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}
#endif

// --- Health Processor (HP) ---

ULNPHealthProcessor::ULNPHealthProcessor()
	: HealthQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPHealthProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	HealthQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	HealthQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	HealthQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	HealthQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	HealthQuery.RegisterWithProcessor(*this);
}

void ULNPHealthProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> DyingEntities;

	HealthQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPEnemyFragment>  Enemies    = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			if (Enemies[i].Health > 0.f)
				continue;

			Enemies[i].DeathCountdown = 2.5f;
			DyingEntities.Add(Ctx.GetEntity(i));

			if (!ActorFrags.IsEmpty())
			{
				if (AActor* RawActor = ActorFrags[i].GetMutable())
				{
					TWeakObjectPtr<AActor> WeakActor(RawActor);
					Ctx.Defer().PushCommand<FMassDeferredSetCommand>([WeakActor](const FMassEntityManager&)
					{
						if (ALNPEnemyCharacter* EnemyChar = Cast<ALNPEnemyCharacter>(WeakActor.Get()))
							EnemyChar->TriggerRagdoll();
					});
				}
			}
		}
	});

	for (const FMassEntityHandle Entity : DyingEntities)
		Context.Defer().AddTag<FLNPEnemyDyingTag>(Entity);
}

// --- LOD Override Processor (LOD Override) ---

ULNPEnemyLODOverrideProcessor::ULNPEnemyLODOverrideProcessor()
	: LODOverrideQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassDistanceLODProcessor"));
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void ULNPEnemyLODOverrideProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	LODOverrideQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	LODOverrideQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	LODOverrideQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
}

void ULNPEnemyLODOverrideProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	LODOverrideQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& LODContext)
	{
		const TArrayView<FMassRepresentationFragment> Representations = LODContext.GetMutableFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = LODContext.GetFragmentView<FLNPEnemyTargetingFragment>();

		for (int32 i = 0; i < LODContext.GetNumEntities(); ++i)
		{
			if (TargetingFragments[i].State == ELNPTargetingState::Confirmed)
			{
				//Representations[i].CurrentRepresentation = EMassRepresentationType::HighResSpawnedActor;
			}
		}
	});
}

// --- Actor Initializer Processor (Actor 초기화) ---

ULNPEnemyActorInitializerProcessor::ULNPEnemyActorInitializerProcessor()
	: InitializerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void ULNPEnemyActorInitializerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	InitializerQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	InitializerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	InitializerQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	InitializerQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	InitializerQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
	InitializerQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	InitializerQuery.AddTagRequirement<FLNPEnemyActorInitializedTag>(EMassFragmentPresence::None);
}

void ULNPEnemyActorInitializerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	InitializerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = EnemyContext.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPEnemyFragment> EnemyFragments = EnemyContext.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> VelocityFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyVelocityFragment>();

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			if (AActor* RawActor = ActorFragments[i].GetMutable())
			{
				FMassEntityHandle Entity = EnemyContext.GetEntity(i);
				ULNPEnemyConfig* Config = SharedFragment.Config;
				float Health = EnemyFragments[i].Health;
				ELNPTargetingState TState = TargetingFragments[i].State;
				FVector Velocity = VelocityFragments[i].Velocity;
				VelocityFragments[i].Velocity = FVector::ZeroVector; // Actor가 소비

				Context.Defer().PushCommand<FMassDeferredAddCommand>([Entity, RawActor, Config, Health, TState, Velocity](FMassEntityManager& InEntityManager)
				{
					if (ALNPEnemyCharacter* EnemyActor = Cast<ALNPEnemyCharacter>(RawActor))
					{
						EnemyActor->InitializeFromConfig(Config);
						EnemyActor->SyncFromEntity(Entity, Health, TState, Velocity);
						InEntityManager.AddTagToEntity(Entity, FLNPEnemyActorInitializedTag::StaticStruct());
					}
				});
			}
		}
	});
}

// --- ActorSync Processor (Actor 동기화) ---

ULNPEnemyActorSyncProcessor::ULNPEnemyActorSyncProcessor()
	: SyncQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::LOD);
}

void ULNPEnemyActorSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SyncQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	SyncQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	SyncQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
	SyncQuery.AddTagRequirement<FLNPEnemyActorInitializedTag>(EMassFragmentPresence::All);
	SyncQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyActorSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> ToCleanup;

	SyncQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FMassActorFragment> ActorFrags  = Ctx.GetFragmentView<FMassActorFragment>();
		TArrayView<FLNPEnemyFragment>             EnemyFrags  = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FLNPEnemyVelocityFragment>     VelocityFrags = Ctx.GetMutableFragmentView<FLNPEnemyVelocityFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			if (const ALNPEnemyCharacter* EnemyChar = Cast<ALNPEnemyCharacter>(ActorFrags[i].Get()))
				EnemyChar->SyncToEntity(EnemyFrags[i].Health, VelocityFrags[i].Velocity);
			else
				ToCleanup.Add(Ctx.GetEntity(i));
		}
	});

	for (const FMassEntityHandle Entity : ToCleanup)
		Context.Defer().RemoveTag<FLNPEnemyActorInitializedTag>(Entity);
}

// --- DeathTimer Processor (사망 Timer) ---

ULNPEnemyDeathTimerProcessor::ULNPEnemyDeathTimerProcessor()
	: DeathTimerQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPHealthProcessor::StaticClass()->GetFName());
}

void ULNPEnemyDeathTimerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DeathTimerQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	DeathTimerQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::All);
	DeathTimerQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyDeathTimerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	TArray<FMassEntityHandle> ToDestroy;

	DeathTimerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPEnemyFragment> Enemies = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			Enemies[i].DeathCountdown -= DeltaTime;
			if (Enemies[i].DeathCountdown <= 0.f)
				ToDestroy.Add(Ctx.GetEntity(i));
		}
	});

	if (ToDestroy.Num() > 0)
		Context.Defer().DestroyEntities(MoveTemp(ToDestroy));
}
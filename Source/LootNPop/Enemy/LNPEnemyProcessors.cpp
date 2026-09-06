// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPTargetingSubsystem.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "Config/LNPSettings.h"
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
#include "MassRepresentationProcessor.h"
#include "MassReplicationFragments.h"   // FMassNetworkIDFragment — 호스트/게스트 로그 대조용 공유 식별자
#include "LNPMassUtils.h"
#include "GAS/LNPPoiseTypes.h"
#if WITH_EDITOR
#include "MassDebugDrawHelpers.h"
#endif

// Chase 정지 거리 공식은 FLNPEnemyMovementConfig::ComputeStopDistance로 일원화되어 있다
// (TargetFollow/Movement/SteeringTask가 동일 값을 공유해야 정지 지점이 일치).

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
	// 사망한 플레이어는 타겟 후보에서 제외한다 — 적 쪽 FLNPEnemyDyingTag 배제와 대칭.
	PlayerQuery.AddTagRequirement<FLNPPlayerDeadTag>(EMassFragmentPresence::None);
}

void ULNPEnemyScoringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — AI 로직은 서버 전용.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const float DeltaTime = Context.GetDeltaTimeSeconds();

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
		const FLNPEnemyMovementConfig& MConfig = SharedFragment.Config->MovementConfig;  // 피격 인지의 상하 게이트

		// 슬롯 풀은 Config가 CombatMode·AttackType에서 파생시킨다 — 판별 원본이 하나여야 한다.
		// (예전에는 EnemyTypeTag에 "Melee"가 들어 있는지로 봤는데, 이름과 거동이 조용히 어긋난다.)
		const ELNPTargetSlotPool SlotPool = SharedFragment.Config->GetSlotPool();

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

			// 인지 게이트 세 개. 모두 루프 **전에** 확정한다 — 값 갱신은 루프 뒤다.
			//   bCanAcquire  : 포기 직후의 재발견 금지 창이 열려 있는가 (새 대상 발견에만 관여)
			//   bExhausted   : 인내가 다 찼는가. 이 프레임에 **유지까지 끊어** Idle로 내려보낸다.
			//   bHitReacting : 피격 주시 중인가. 시야 중심을 정면에서 피격 방향으로 옮긴다.
			const bool bCanAcquire = (CandidateData.DisengageTimer <= 0.0f);
			const bool bExhausted = (CandidateData.AlertDwellTime >= TConfig.AlertPatienceTime);
			const bool bHitReacting = (EnemyData.HitReactTimer > 0.0f);

			struct FCandidate { FMassEntityHandle Handle; FVector Location; float DistSq; };
			TArray<FCandidate, TInlineAllocator<8>> VisiblePlayers;
			bool bAnyChaseEligible = false;

			for (const auto& Player : Players)
			{
				const float DistSq = FVector::DistSquared(EnemyLoc, Player.Location);

				// 시야 재검사 면제는 **이미 추적 중인 그 플레이어에게만** 준다 (등 뒤로 돌아도 유지).
				// PreviousState는 엔티티 단위 값이라, 대상을 한정하지 않으면 한 번 인지한 순간부터
				// 다른 플레이어까지 거리·시야각 검사를 건너뛰고 전부 후보가 된다 — 이것이
				// "교전 중 갑자기 멀리 있는 플레이어에게 달려가는" 증상의 원인이었다.
				//
				// Alert도 포함한다(Confirmed만이 아니라). 슬롯을 못 얻어 경계 중인 개체가 시야각을
				// 벗어나는 순간 Idle로 떨어지면, 경계 상태가 사실상 존재하지 않게 된다.
				const bool bIsTrackedTarget = (PreviousState != ELNPTargetingState::None
					&& Player.Handle == Targeting.TargetPlayer);

				bool bVisible = false;
				if (DistSq <= FMath::Square(TConfig.AwarenessDistance))
				{
					// 초근접은 FOV도 재발견 금지도 무시한다 — 어떤 상태에서도 눈앞의 상대는 본다.
					bVisible = true;
				}
				else if (bExhausted)
				{
					// 인내 소진: 유지 조건까지 끊어야 Idle로 내려간다. 발견만 막으면
					// 추적 중인 타겟이 유지 거리 안에 남아 영원히 경계가 풀리지 않는다.
					bVisible = false;
				}
				else if (bIsTrackedTarget)
				{
					bVisible = (DistSq <= FMath::Square(TConfig.AlertRetentionDistance));
				}
				else if (bHitReacting && DistSq <= FMath::Square(TConfig.VisionDistance))
				{
					// 피격 주시 중에는 시야 중심을 **정면이 아니라 맞은 방향**으로 옮긴다.
					// 몸통은 접평면에서만 돌아서므로(회전축이 로컬 Up 하나뿐이다) 위·아래에서 날아온
					// 공격은 아무리 돌아봐도 정면 시야 원뿔 안에 들어오지 않는다 — 피격 주시 시간이
					// 약속한 "돌아본 결과 시야 안에 있으면 발견한다"가 고저차에서만 성립하지 않던 이유다.
					// 각도 예산은 그대로 VisionAngle을 쓴다. 축만 바뀌고 넓어지지는 않는다.
					//
					// ⚠️ 각도 판정을 빼고 "피격 중이면 다 보인다"로 두면 안 된다. HitReactTimer는
					// 대상이 아니라 **엔티티 단위** 값이라, 대상을 한정하지 않으면 교전 중 한 대 맞는
					// 것만으로 멀리 있는 다른 플레이어까지 전부 후보가 된다 — 바로 위 주석이 말하는
					// "교전 중 갑자기 멀리 있는 플레이어에게 달려가는" 그 함정이다.
					//
					// 재발견 금지 창(bCanAcquire)은 보지 않는다. 초근접과 같은 이유다 — 등을 돌릴
					// 시간을 벌어 주는 장치가 "맞고도 누가 쐈는지 못 찾는" 근거가 될 수는 없다.
					const FVector DirToTarget = (Player.Location - EnemyLoc).GetSafeNormal();
					const float DotToHitDir = FVector::DotProduct(EnemyData.HitReactDirection, DirToTarget);
					const float AngleToHitDir = FMath::RadiansToDegrees(FMath::Acos(DotToHitDir));

					// 상하 게이트: **겨눌 수 없는 각도에서 온 공격은 인지하지도 않는다.**
					// 이 경로는 정면 시야 원뿔을 우회하므로, 막지 않으면 조준 클램프 밖(정수리 위·발밑)의
					// 공격자를 발견해 놓고 클램프된 각도로 영원히 헛쏘는 상태가 된다.
					// 조준·발사와 **같은 값**(MovementConfig)을 읽어야 "못 겨누는 각도 = 못 알아채는 각도"가 성립한다.
					const FVector LocalDir = EnemyTransform.InverseTransformVectorNoScale(Player.Location - EnemyLoc);
					const float TargetPitch = static_cast<float>(LocalDir.Rotation().Pitch);
					const bool bWithinAimPitch = (MConfig.AimPitchMinDeg <= TargetPitch) && (TargetPitch <= MConfig.AimPitchMaxDeg);

					bVisible = bWithinAimPitch && (AngleToHitDir <= (TConfig.VisionAngle * 0.5f));
				}
				else if (bCanAcquire && DistSq <= FMath::Square(TConfig.VisionDistance))
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

					// 추격 자격이 없으면 **슬롯 경쟁에 참가시키지 않는다.** 후보 목록에는 남으므로
					// TargetingProcessor가 Alert로 잡아 준다 — 이것이 Confirmed → Alert 강등 경로 전부다.
					//
					// ⚠️ 세력권은 **플레이어**가 Pod에서 얼마나 떨어졌는지로 잰다. NPC 자신의 거리로
					// 재면 NPC가 움직일 때마다 자기 자격이 뒤집혀 경계선에서 자기진동한다(실측) —
					// 히스테리시스를 걸어도 진동 주기가 늘어날 뿐 사라지지 않는다.
					// 코앞(AwarenessDistance)까지 들어온 상대에게는 세력권과 무관하게 반격한다.
					const float PlayerToPodSq = FVector::DistSquared(Candidate.Location, EnemyData.ParentPodLocation);
					const bool bChaseEligible = (PlayerToPodSq <= FMath::Square(TConfig.ChaseRadius))
						|| (Candidate.DistSq <= FMath::Square(TConfig.AwarenessDistance));
					if (!bChaseEligible)
						continue;

					bAnyChaseEligible = true;

					const float Score = 1000000.0f / (FMath::Sqrt(Candidate.DistSq) + 1.0f);

					const FMassEntityHandle EnemyEntity = EnemyContext.GetEntity(i);
					const FMassEntityHandle PlayerHandle = Candidate.Handle;

					// TargetingSubsystem 갱신은 게임 스레드 커맨드로 지연 실행 (Processor는 워커 스레드에서 돌 수 있음)
					Context.Defer().PushCommand<FMassDeferredSetCommand>([EnemyEntity, PlayerHandle, Score, SlotPool](const FMassEntityManager& InOutEntityManager)
					{
						ULNPTargetingSubsystem* TargetingSubsystem = UWorld::GetSubsystem<ULNPTargetingSubsystem>(InOutEntityManager.GetWorld());
						if (TargetingSubsystem != nullptr)
							TargetingSubsystem->RegisterEnemyInterest(EnemyEntity, PlayerHandle, Score, SlotPool);
					});
				}
			}

			// 경계 인내 타이머. 추격 자격이 하나라도 있으면(= 슬롯 대기 중이면) 초기화한다 —
			// 전투 대기열은 시간이 지난다고 흩어져선 안 된다. 추격·공격 중(Confirmed)이나
			// 이미 Idle(None)일 때도 초기화되므로, "교전에 성공하면 인내는 새로 시작"이 성립한다.
			// 피격 반응 중에도 재지 않는다 — 맞고 두리번거리는 시간은 대치가 아니다.
			const bool bStandoff = (PreviousState == ELNPTargetingState::Alert)
				&& !bAnyChaseEligible
				&& (EnemyData.HitReactTimer <= 0.0f);
			CandidateData.AlertDwellTime = bStandoff
				? FMath::Min(CandidateData.AlertDwellTime + DeltaTime, TConfig.AlertPatienceTime)
				: 0.0f;

			// 인내를 소진한 프레임에 재발견 금지 창을 연다. 이 창이 벌어 주는 것은 **등을 돌릴 시간**이다 —
			// 없으면 포기한 다음 프레임에 정면의 플레이어를 그대로 재발견해 Pod 쪽으로 한 발짝도 못 걷는다.
			if (bExhausted)
			{
				CandidateData.DisengageTimer = FMath::Max(CandidateData.DisengageTimer, TConfig.AlertRecoveryTime);
			}
			else
			{
				CandidateData.DisengageTimer = FMath::Max(CandidateData.DisengageTimer - DeltaTime, 0.0f);
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
	// 사망한 플레이어는 타겟 후보에서 제외한다 — 적 쪽 FLNPEnemyDyingTag 배제와 대칭.
	PlayerQuery.AddTagRequirement<FLNPPlayerDeadTag>(EMassFragmentPresence::None);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPTargetingSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyTargetingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — AI 로직은 서버 전용.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

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

			// 잠재적 타겟이 있지만 확정되지 않은 경우 Alert 진입.
			// 후보가 없으면 루프 초입의 ResetTargeting 상태(None)를 그대로 유지한다.
			if (!bFoundConfirmed && CandidateData.NumPotentialTargets > 0)
			{
				Targeting.TargetPlayer = CandidateData.PotentialTargets[0];
				Targeting.State = ELNPTargetingState::Alert;

				if (const FVector* PLoc = PlayerLocations.Find(Targeting.TargetPlayer))
				{
					Targeting.TargetLocation = *PLoc;
					Targeting.DistanceToTargetSq = FVector::DistSquared(EnemyLocation, *PLoc);
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
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — AI 로직은 서버 전용.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

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
				const float StopDist = FLNPEnemyMovementConfig::ComputeStopDistance(AttackRange);

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
	MovementQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);                   // HitReactTimer 감소
	MovementQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite); // 배회 타임아웃 계측
	MovementQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional); // 경직 중 정지
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
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — 이동 시뮬레이션은 서버 전용(위치는 복제로 전달).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

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
		const TArrayView<FLNPEnemyFragment> EnemyFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> VelocityFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyVelocityFragment>();
		const TArrayView<FLNPEnemyIdleFragment> IdleFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyIdleFragment>();
		const TConstArrayView<FLNPPoiseFragment> PoiseFragments = EnemyContext.GetFragmentView<FLNPPoiseFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float RotationRate = SharedFragment.Config->MovementConfig.RotationRate;
		const FVector GravityOrigin = SharedFragment.Config->MovementConfig.GravityOrigin;
		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;
		const float BaseMoveSpeed = SharedFragment.Config->MovementConfig.MoveSpeed;
		const float GravityStrength = SharedFragment.Config->MovementConfig.GravityStrength;

		// 엔티티 Transform의 기준점은 **캡슐 중심**이다 — 발밑이 아니다.
		// Actor가 붙어 있는 동안에는 MassAgentCapsuleCollisionSyncTrait(ActorToMass)가
		// 캡슐 컴포넌트 Transform을 그대로 넣으므로 중심 기준이고, 피격 판정 프로세서와
		// Actor 승격 시의 TeleportActor도 중심을 가정한다. 여기(Actor 없는 경로)만 표면점을
		// 그대로 쓰면 LOD가 바뀔 때마다 좌표가 HalfHeight만큼 튄다.
		// 구 내벽이라 Up은 중심 방향 = 반지름이 줄어드는 쪽이므로 표면 반지름에서 빼준다.
		const float CapsuleHalfHeight = SharedFragment.Config->CapsuleHalfHeight;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			FTransform& EntityTransform = Transforms[i].GetMutableTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();
			const FMassMoveTargetFragment& MoveTarget = MoveTargets[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			const FVector UpDir = (GravityOrigin - EntityLocation).GetSafeNormal(); // 내부 구형 세계에서 중심 방향 = Up
			const FQuat CurrentRotation = EntityTransform.GetRotation();

			const FVector TargetPos = MoveTarget.Center;
			// 구면 위에서 목적지까지의 거리는 **접평면 성분으로만** 잰다.
			// 반경 방향 차이(캡슐 중심 보정 96cm, 지형 높이차)는 걸어서 좁힐 수 있는 거리가 아니므로
			// 거리에 포함시키면 도착 판정이 영영 성립하지 않는다. 실제로 목적지가 순수 반경 방향으로만
			// 어긋나면 접평면 투영이 0이 되어 방향 벡터까지 사라져 엔티티가 완전히 굳는다.
			const FVector ToTargetOnPlane = FVector::VectorPlaneProject(TargetPos - EntityLocation, UpDir);
			const float DistSq = ToTargetOnPlane.SizeSquared();
			const FVector TargetDirOnPlane = ToTargetOnPlane.GetSafeNormal();

			float EffectiveSpeed = 0.0f;
			FVector OrientationIntent = FVector::ZeroVector;

			// 피격 반응 타이머는 **상태와 무관하게** 매 프레임 감소시킨다. Alert/Confirmed 중에 맞아
			// 남은 타이머가 나중에 Idle이 될 때 엉뚱하게 발동하는 것을 막기 위해서다.
			FLNPEnemyFragment& EnemyData = EnemyFragments[i];
			const bool bHitReacting = (EnemyData.HitReactTimer > 0.0f);
			if (bHitReacting)
			{
				EnemyData.HitReactTimer = FMath::Max(EnemyData.HitReactTimer - DeltaTime, 0.0f);
			}

			switch (Targeting.State)
			{
			case ELNPTargetingState::None:
				if (bHitReacting)
				{
					// 피격 직후: 그 자리에 서서 맞은 방향을 바라본다. 돌아본 결과 시야에 플레이어가
					// 있으면 평소의 발견 → 경계 플로우를 그대로 타므로 별도 전이 규칙이 필요 없다.
					OrientationIntent = FVector::VectorPlaneProject(EnemyData.HitReactDirection, UpDir).GetSafeNormal();
					EffectiveSpeed = 0.0f;
				}
				else
				{
					// 대기 이동: 타겟 지점으로 천천히 이동
					EffectiveSpeed = BaseMoveSpeed * 0.3f; // 느린 걷기
					OrientationIntent = TargetDirOnPlane;
				}
				break;

			case ELNPTargetingState::Alert:
				// Alert: Player 방향 전환, ST 태스크에 의해 밀리지 않으면 이동 안 함
				EffectiveSpeed = 0.0f;
				OrientationIntent = TargetDirOnPlane;
				break;

			case ELNPTargetingState::Confirmed:
			{
				const float ActualDistance = Targeting.TargetPlayer.IsValid() ? FMath::Sqrt(Targeting.DistanceToTargetSq) : 0.0f;
				EffectiveSpeed = (ActualDistance <= FLNPEnemyMovementConfig::ComputeStopDistance(AttackRange)) ? 0.0f : BaseMoveSpeed;
				OrientationIntent = TargetDirOnPlane;
				break;
			}
			}

			// 목적지 도달 시 StateTree 신호 (None/Confirmed 상태용)
			const bool bArrived = DistSq < FMath::Square(FLNPEnemyMovementConfig::ArrivalTolerance);
			if (EffectiveSpeed > 0.0f && bArrived)
			{
				EntitiesToSignal.Add(EnemyContext.GetEntity(i));
				EffectiveSpeed = 0.0f;
			}

			// 배회 교착 복구 — Idle(None) 상태에서 타임아웃까지 도착하지 못하면 StateTree를 깨워
			// 목표를 재추첨하게 한다. 도착 신호만으로는 복구할 수 없다: 도달 불가능한 지점을 한 번
			// 뽑으면 신호가 영영 오지 않고, 신호가 없으면 IdleTask의 Tick도 돌지 않아 영구 정지한다.
			// 시간 계측을 여기 두는 것은 매 프레임 도는 경로가 이쪽뿐이기 때문이다.
			FLNPEnemyIdleFragment& IdleData = IdleFragments[i];
			if (Targeting.State == ELNPTargetingState::None && !bArrived)
			{
				IdleData.TimeSinceWanderIssued += DeltaTime;
				if (IdleData.TimeSinceWanderIssued > FLNPEnemyMovementConfig::WanderTimeout)
				{
					// 여기서 바로 0으로 되돌려 신호가 매 프레임 반복되지 않게 한다.
					// 실제 목표 폐기·재추첨은 IdleTask가 한다(배회 목표의 단일 결정 주체).
					IdleData.TimeSinceWanderIssued = 0.0f;
					IdleData.bWanderTargetTimedOut = true;
					EntitiesToSignal.Add(EnemyContext.GetEntity(i));
				}
			}
			else
			{
				IdleData.TimeSinceWanderIssued = 0.0f;
			}

			// StateTree에서 명시적으로 속도를 설정한 경우 Override (예: SteeringTask)
			// Alert 상태는 StateTree 속도와 관계없이 항상 정지
			if (MoveTarget.DesiredSpeed.Get() > 0.0f && Targeting.State != ELNPTargetingState::Alert)
			{
				EffectiveSpeed = MoveTarget.DesiredSpeed.Get();
			}

			// 그로기·다운 중에는 어떤 경로로도 움직이지 않는다. StateTree 속도 override 뒤에 두어야 다시 살아나지 않는다.
			// Actor가 있으면 GA_Stagger의 TAG_Block_MovementInput이 입력을 이미 지우지만,
			// Actor가 없는 Low LOD 적에게는 이 검사가 유일한 정지 경로다.
			// 다운은 게이지를 0으로 리셋하므로 bIsGroggy만으로는 안 잡힌다 — 면역 잔여로 함께 본다.
			if (PoiseFragments.IsValidIndex(i)
				&& (PoiseFragments[i].bIsGroggy || PoiseFragments[i].ImmunityTimeRemaining > 0.f))
				EffectiveSpeed = 0.0f;

			if (AActor* Actor = ActorFragments[i].GetMutable())
			{
				TWeakObjectPtr<AActor> WeakActor(Actor);
				const FVector CapturedOrientation = OrientationIntent;

				// 이동 의도는 **방향만** 담는다(단위 벡터). 속도는 MaxSpeed로 따로 지정한다.
				// Mover의 ComputeVelocity가 의도 벡터를 정규화하지 않고 방향 전환 항에 그대로 써서,
				// 크기 s(<1)를 지속적으로 넣으면 매 프레임 속도가 s배로 깎이기 때문이다
				// (실측: s=0.3에서 180cm/s 기대 → 27cm/s). 상세는 ULNPInputHandlerComponent::SetAIMoveInput 주석.
				const FVector CapturedMoveInput   = EffectiveSpeed > 0.0f ? OrientationIntent : FVector::ZeroVector;
				const float   CapturedDesiredSpeed = EffectiveSpeed;
				EnemyContext.Defer().PushCommand<FMassDeferredSetCommand>(
					[WeakActor, CapturedOrientation, CapturedMoveInput, CapturedDesiredSpeed](const FMassEntityManager&)
					{
						if (ALNPCharacterBase* LNPCharacter = Cast<ALNPCharacterBase>(WeakActor.Get()))
						{
							LNPCharacter->SetAIOrientationIntent(CapturedOrientation);
							LNPCharacter->SetAIMoveInput(CapturedMoveInput);
							LNPCharacter->SetAIDesiredSpeed(CapturedDesiredSpeed);
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
						// 접지 상태의 캡슐 중심 반지름 — 발이 표면에 닿았을 때의 중심 위치
						const float SurfaceRadius = FVector::Dist(GravityOrigin, SurfacePoint) - CapsuleHalfHeight;
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
									{
										Velocity = FVector::ZeroVector;
									}
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
						const float SurfaceRadius = FVector::Dist(GravityOrigin, SurfacePoint) - CapsuleHalfHeight;
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
	HealthQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();   // 사망 처리가 CombatMode로 갈린다
	HealthQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	HealthQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	HealthQuery.RegisterWithProcessor(*this);
}

void ULNPHealthProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — HP 판정은 서버 전용(GAS Attribute로 복제).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	TArray<FMassEntityHandle> DyingEntities;

	// 랙돌이 보일 시간을 준다 — 너무 짧으면 시체가 무너지기도 전에 엔티티가 사라진다.
	const float RagdollDuration = GetDefault<ULNPSettings>()->EnemyRagdollDuration;

	HealthQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPEnemyFragment>  Enemies    = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

		// 랙돌은 ActorPromoted 전용 연출로 남는다. 순수 엔티티는 Actor가 없으므로 무너질 몸이 없고,
		// 대신 행동 상태 채널이 Dying을 전파해 게스트가 사망을 본다 — 그 전파가 나갈 시간을 벌어야
		// 하므로 소멸까지의 지연을 Config에서 따로 받는다.
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		const bool bPureEntity = Config && Config->CombatMode == ELNPEnemyCombatMode::PureEntity;
		const float DeathDelay = bPureEntity ? Config->PureEntityDeathDuration : RagdollDuration;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			if (Enemies[i].Health > 0.f)
				continue;

			Enemies[i].DeathCountdown = DeathDelay;
			DyingEntities.Add(Ctx.GetEntity(i));

			// 순수 엔티티에는 Actor가 없다 — 승격된 개체가 남긴 Actor가 붙어 있을 수도 없으므로
			// 랙돌 방송 자체를 건너뛴다.
			if (!bPureEntity && !ActorFrags.IsEmpty())
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

/** Actor를 스폰하지 않는 첫 표현 단계. 클라이언트 LOD를 여기까지 눌러 Mass의 Actor 스폰을 막는다. */
static EMassLOD::Type FindLowestNonActorLOD(const FMassRepresentationParameters& Params)
{
	for (int32 LODIndex = 0; LODIndex < EMassLOD::Max; ++LODIndex)
	{
		const EMassRepresentationType Type = Params.LODRepresentation[LODIndex];
		if (Type != EMassRepresentationType::HighResSpawnedActor && Type != EMassRepresentationType::LowResSpawnedActor)
			return (EMassLOD::Type)LODIndex;
	}
	return EMassLOD::Off;
}

ULNPEnemyLODOverrideProcessor::ULNPEnemyLODOverrideProcessor()
	: LODOverrideQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 판단 근거인 타게팅 상태를 ULNPEnemyScoringProcessor(PostPhysics)가 채우므로 그보다 뒤여야 한다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassDistanceLODProcessor"));
}

void ULNPEnemyLODOverrideProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	LODOverrideQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadWrite);
	LODOverrideQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	LODOverrideQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	LODOverrideQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	LODOverrideQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyLODOverrideProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 —
	// 전투 판단 기반 LOD 강제는 서버 전용 개념이다(게스트 표현 정책은 별도 프로세서가 맡는다).
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	LODOverrideQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& LODContext)
	{
		// 승격하지 않기로 한 개체는 전투로 LOD를 끌어올리지 않는다. 그 뒤의 표현 선택은
		// EntityConfig의 MassCrowdVisualizationTrait이 데이터로 정한다 — LODRepresentation에
		// Actor 단계가 없으면 거리와 무관하게 Actor가 스폰되지 않는다.
		// LOD 값을 눌러 강제하지 않는 이유는, LOD가 표현뿐 아니라 유의도·틱 레이트까지
		// 함께 결정하는 값이라 표현 하나를 막으려고 나머지까지 끌어내리게 되기 때문이다.
		const ULNPEnemyConfig* Config = LODContext.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config && Config->CombatMode == ELNPEnemyCombatMode::PureEntity)
			return;

		const TArrayView<FMassRepresentationLODFragment> RepresentationLODs = LODContext.GetMutableFragmentView<FMassRepresentationLODFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = LODContext.GetFragmentView<FLNPEnemyTargetingFragment>();

		for (int32 i = 0; i < LODContext.GetNumEntities(); ++i)
		{
			// CurrentRepresentation은 건드리지 않는다. WantedRepresentationType의 원천인 LOD 값만 변경해
			// RepresentationProcessor가 전환을 최초 1회만 감지하도록 한다.
			if (TargetingFragments[i].State == ELNPTargetingState::Confirmed)
				RepresentationLODs[i].LOD = EMassLOD::High;
		}
	});
}

// --- Client Representation Processor (게스트 표현 일원화) ---

ULNPEnemyClientRepresentationProcessor::ULNPEnemyClientRepresentationProcessor()
	: ClientRepresentationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 게스트 전용 정책이다. 리슨 호스트는 넷 모드가 Client | Server라 이 플래그에도 매칭되므로
	// Execute 첫 줄에서 다시 거른다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client;

	// ⚠️ **이 한 줄이 이 프로세서가 따로 존재하는 이유다.** 표현을 실제로 정하는 엔진 프로세서들이
	//    ProcessingPhase를 설정하지 않아 기본값 PrePhysics로 돌고, Mass는 페이즈별로 따로
	//    의존성을 해소하므로 다른 페이즈에서 선언한 ExecuteBefore는 조용히 무시된다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	// 거리로 LOD를 계산하는 쪽보다 뒤 — 안 그러면 우리가 쓴 값이 같은 프레임에 덮인다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassCrowdVisualizationLODProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassDistanceLODProcessor"));
	// 그 값을 읽어 Actor를 스폰하는 쪽보다 앞. UMassVisualizationProcessor는 Representation이 아니라
	// 그 하위 그룹(Representation.VisualizationProcessing)에 들어가므로 둘 다 못 박는다.
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);
}

void ULNPEnemyClientRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ClientRepresentationQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadWrite);
	ClientRepresentationQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	ClientRepresentationQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	ClientRepresentationQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	ClientRepresentationQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyClientRepresentationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!LNPMass::IsClientWorld(EntityManager))
		return;

	ClientRepresentationQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& RepContext)
	{
		const FMassRepresentationParameters& RepParams = RepContext.GetConstSharedFragment<FMassRepresentationParameters>();
		const TArrayView<FMassRepresentationLODFragment> RepresentationLODs = RepContext.GetMutableFragmentView<FMassRepresentationLODFragment>();
		const TConstArrayView<FMassActorFragment> ActorFrags = RepContext.GetFragmentView<FMassActorFragment>();

		const int32 NonActorLOD = (int32)FindLowestNonActorLOD(RepParams);

		for (int32 i = 0; i < RepContext.GetNumEntities(); ++i)
		{
			// 복제로 도착한 Actor(= Mass 소유가 아닌 Actor)가 있으면 그것을 표현으로 채택한다.
			// 엔진의 bForceActorRepresentationForExternalActors와 같은 처방을 LOD 쪽에서 건다.
			const bool bHasReplicatedActor = ActorFrags[i].IsValid() && !ActorFrags[i].IsOwnedByMass();
			RepresentationLODs[i].LOD = bHasReplicatedActor
				? EMassLOD::High
				: (EMassLOD::Type)FMath::Max((int32)RepresentationLODs[i].LOD, NonActorLOD);
		}
	});
}

// --- Actor Initializer Processor (Actor 초기화) ---

ULNPEnemyActorInitializerProcessor::ULNPEnemyActorInitializerProcessor()
	: ActivationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
	ExecutionOrder.ExecuteAfter.Add(UMassRepresentationProcessor::StaticClass()->GetFName());
}

void ULNPEnemyActorInitializerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ActivationQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	ActivationQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ActivationQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	ActivationQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	ActivationQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ActivationQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	ActivationQuery.AddTagRequirement<FLNPEnemyActorInitializedTag>(EMassFragmentPresence::None);
	ActivationQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyActorInitializerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다.
	// 클라이언트의 Actor 존재 여부는 일반 Actor Relevancy 복제가 결정하므로, Mass LOD 기반 Actor 스폰/초기화는 서버 전용 개념이다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	ActivationQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& SharedFragment = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		const TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPEnemyFragment> EnemyFrags = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFrags = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> VelocityFrags = Ctx.GetMutableFragmentView<FLNPEnemyVelocityFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			if (AActor* RawActor = ActorFrags[i].GetMutable())
			{
				FMassEntityHandle Entity = Ctx.GetEntity(i);
				TWeakObjectPtr<AActor> WeakActor(RawActor);
				TWeakObjectPtr<ULNPEnemyConfig> WeakConfig(SharedFragment.Config);
				float Health = EnemyFrags[i].Health;
				ELNPTargetingState TState = TargetingFrags[i].State;
				FVector Velocity = VelocityFrags[i].Velocity;
				VelocityFrags[i].Velocity = FVector::ZeroVector;

				Context.Defer().PushCommand<FMassDeferredAddCommand>([Entity, WeakActor, WeakConfig, Health, TState, Velocity](FMassEntityManager& InEntityManager)
				{
					if (ALNPEnemyCharacter* Enemy = Cast<ALNPEnemyCharacter>(WeakActor.Get()))
					{
						Enemy->InitializeOnce(WeakConfig.Get());
						Enemy->SyncFromEntity(Health, TState, Velocity);
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
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — Mass<->Actor 동기화는 서버 전용 개념.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

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
	// Enemy MassReplication(Phase 6) 이후 클라이언트에도 이 아키타입의 엔티티가 존재한다 — 소멸 결정은 서버 전용.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

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

// --- Action Processor (행동 상태 채널) ---

namespace
{
	/**
	 * Idle <-> Move 데드밴드(cm/s). 진입이 이탈보다 높아 경계에서 자기진동하지 않는다.
	 *
	 * ⚠️ **이 히스테리시스는 연출 장치이자 대역폭 장치다.** 데드밴드가 없으면 멈췄다 걷기를 반복하는
	 * 배회 개체가 초당 여러 번 전이를 만들고, 전이 하나하나가 복제 갱신 후보가 된다.
	 * 값의 근거: 배회 속도는 `MoveSpeed * 0.3`(기본 180 cm/s), 추격은 `MoveSpeed`(600 cm/s)이므로
	 * 실제 이동은 두 역치를 여유 있게 넘고, 회전만 하는 정지 상태는 두 역치 아래에 머문다.
	 */
	constexpr float ActionMoveEnterSpeed = 40.f;
	constexpr float ActionMoveExitSpeed  = 15.f;
}

ULNPEnemyActionProcessor::ULNPEnemyActionProcessor()
	: ActionQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 같은 프레임에 진행된 공격 위상을 읽어야 전이가 한 프레임 늦지 않는다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEntityAttackProcessor"));
}

void ULNPEnemyActionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ActionQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadWrite);
	ActionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ActionQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);
	ActionQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ActionQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// ⚠️ FLNPEnemyDyingTag를 None으로 걸지 않는다 — 죽는 순간 쿼리에서 빠지면 Dying을 아무도 못 싣는다.
	ActionQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyActionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 행동 상태의 원본은 서버다. 게스트는 이 값을 복제로 받아 같은 프래그먼트에 쓴다
	// (FLNPMassClientBubbleHandler) — 그래서 이 프로세서만 서버 전용이고, 소비처는 양쪽 공용이다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
		return;

	ActionQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Ctx)
	{
		const TArrayView<FLNPEnemyActionFragment> Actions      = Ctx.GetMutableFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FTransformFragment> Transforms   = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEntityAttackFragment> Attacks = Ctx.GetFragmentView<FLNPEntityAttackFragment>();
		const TConstArrayView<FLNPPoiseFragment> PoiseFrags    = Ctx.GetFragmentView<FLNPPoiseFragment>();

		// 태그는 청크 단위 속성이라 엔티티마다 물어볼 필요가 없다.
		const bool bChunkIsDying = Ctx.DoesArchetypeHaveTag<FLNPEnemyDyingTag>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPEnemyActionFragment& ActionFrag = Actions[i];
			const FVector Position = Transforms[i].GetTransform().GetLocation();

			// 첫 프레임은 변위를 신뢰할 수 없다 — 원점에서 스폰 위치까지의 거리가 통째로 속도로 잡힌다.
			const bool bHasPrev = !ActionFrag.PrevPosition.IsNearlyZero();
			const float Speed = bHasPrev
				? static_cast<float>(FVector::Dist(Position, ActionFrag.PrevPosition)) / DeltaTime
				: 0.f;
			ActionFrag.PrevPosition = Position;

			// 경직·다운 판별은 ULNPEntityAttackProcessor와 같은 규약이다 — 다운은 게이지를 0으로
			// 리셋하므로 bIsGroggy만으로는 안 잡혀 면역 잔여도 함께 본다.
			const bool bStaggered = PoiseFrags.IsValidIndex(i)
				&& (PoiseFrags[i].bIsGroggy || PoiseFrags[i].ImmunityTimeRemaining > 0.f);

			ELNPEnemyAction NewAction;
			if (bChunkIsDying)
			{
				NewAction = ELNPEnemyAction::Dying;
			}
			// ⚠️ 경직보다 **먼저** 본다. 패링은 ApplyParryBreak으로 경직도 함께 밀어 넣으므로,
			//    순서를 바꾸면 Parried가 한 번도 나가지 못하고 전부 Stagger로 덮인다.
			else if (Attacks[i].ParriedTimeRemaining > 0.f)
			{
				NewAction = ELNPEnemyAction::Parried;
			}
			else if (bStaggered)
			{
				NewAction = ELNPEnemyAction::Stagger;
			}
			else if (Attacks[i].Phase != ELNPEntityAttackPhase::None)
			{
				NewAction = ELNPEnemyAction::Attack;
			}
			else
			{
				// 데드밴드 — 현재 상태에 따라 다른 역치를 본다. 어느 쪽도 넘지 않으면 그대로 유지한다.
				const bool bWasMoving = (ActionFrag.Action == ELNPEnemyAction::Move);
				const bool bMoving = bWasMoving ? (Speed > ActionMoveExitSpeed) : (Speed > ActionMoveEnterSpeed);
				NewAction = bMoving ? ELNPEnemyAction::Move : ELNPEnemyAction::Idle;
			}

			if (NewAction != ActionFrag.Action)
			{
				ActionFrag.Action = NewAction;
				// 연속 공격(Attack -> Attack)의 두 번째 시작을 놓치지 않으려면 전이마다 카운터가 올라야 한다.
				// 위상이 Recovery -> None -> Windup을 거치므로 Attack은 반드시 다른 상태를 경유해 재진입한다.
				++ActionFrag.Seq;
			}
		}
	});
}

#if WITH_EDITOR
// --- Action Debug Draw Processor (행동 상태 채널의 눈) ---

namespace
{
	TAutoConsoleVariable<int32> CVarDrawEnemyAction(
		TEXT("LNP.Debug.DrawEnemyAction"), 0,
		TEXT("Show the behaviour action channel above every enemy entity.\n")
		TEXT("Runs on the server and the client with no branch, so comparing the two screens is the\n")
		TEXT("proof that the channel replicates.\n")
		TEXT("  0: off (default)\n")
		TEXT("  1: coloured marker only\n")
		TEXT("  2: marker + one log line per transition (compare host and guest logs)"),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarDrawEnemyActionDistance(
		TEXT("LNP.Debug.DrawEnemyActionDistance"), 5000.f,
		TEXT("Max distance (cm) from the local pawn at which the enemy action marker is drawn."),
		ECVF_Cheat);

	/** 캡슐 중심에서 마커를 띄우는 높이(cm) — 경직 게이지(130)보다 위에 둬서 겹치지 않게 한다. */
	constexpr float ActionMarkerHeightOffset = 170.f;
	constexpr float ActionMarkerExtent       = 12.f;

	FColor ActionMarkerColor(const ELNPEnemyAction Action)
	{
		switch (Action)
		{
		case ELNPEnemyAction::Move:    return FColor::Green;
		case ELNPEnemyAction::Attack:  return FColor::Red;
		case ELNPEnemyAction::Parried: return FColor::Cyan;
		case ELNPEnemyAction::Stagger: return FColor::Yellow;
		case ELNPEnemyAction::Dying:   return FColor::Black;
		default:                       return FColor::Silver;   // Idle
		}
	}
}

ULNPEnemyActionDebugDrawProcessor::ULNPEnemyActionDebugDrawProcessor()
	: ActionQuery(*this)
{
	// ⚠️ 기본값(Server | Standalone)이면 게스트에서 아예 돌지 않는다 — 두 화면을 비교하는 것이
	//    목적이므로 반드시 All이어야 한다. 같은 함정을 ULNPEnemyLODOverrideProcessor가 먼저 밟았다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPEnemyActionDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ActionQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly);
	ActionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	// 호스트와 게스트의 엔티티 핸들은 서로 무관하다 — 두 로그를 대조하려면 **공유 식별자**가 필요하다.
	// Standalone에서는 복제 트레이트가 조기 반환해 이 프래그먼트가 아예 없으므로 Optional이다.
	ActionQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ActionQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	ActionQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyActionDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const int32 DrawMode = CVarDrawEnemyAction.GetValueOnGameThread();
	if (DrawMode == 0)
	{
		// 껐다 켜면 다시 처음부터 관측한다 — 꺼져 있는 동안의 전이를 켜는 순간 쏟아내지 않는다.
		LastLoggedSeq.Reset();
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
		return;

	// 로컬 시점 기준으로 자른다 — 리슨 호스트와 게스트가 각자 자기 화면 근처만 그린다.
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* ViewPawn = PC ? PC->GetPawn() : nullptr;
	if (ViewPawn == nullptr)
		return;

	const UE::Mass::Debug::FLineBatcher LineBatcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);
	const FVector ViewLocation = ViewPawn->GetActorLocation();
	const float MaxDistSq = FMath::Square(CVarDrawEnemyActionDistance.GetValueOnGameThread());
	const bool bLogTransitions = (DrawMode >= 2);

	// 이번 프레임에 본 엔티티만 남긴다 — 사라진 엔티티의 항목이 계속 쌓이지 않게 한다.
	TMap<FMassEntityHandle, uint8> SeenThisFrame;

	ActionQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FLNPEnemyActionFragment> Actions = Ctx.GetFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FTransformFragment> Transforms   = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassNetworkIDFragment> NetIDs   = Ctx.GetFragmentView<FMassNetworkIDFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Center = Transforms[i].GetTransform().GetLocation();
			if (FVector::DistSquared(Center, ViewLocation) > MaxDistSq)
				continue;

			const FLNPEnemyActionFragment& ActionFrag = Actions[i];

			// 구 내벽이라 머리 방향은 구 중심을 향한다 (Mass 판정 경로 전체가 쓰는 규약).
			const FVector Up = (-Center).GetSafeNormal();
			LineBatcher.DrawSolidBox(Center + Up * ActionMarkerHeightOffset,
				FVector(ActionMarkerExtent), ActionMarkerColor(ActionFrag.Action));

			if (!bLogTransitions)
				continue;

			const FMassEntityHandle Entity = Ctx.GetEntity(i);
			SeenThisFrame.Add(Entity, ActionFrag.Seq);

			const uint8* Previous = LastLoggedSeq.Find(Entity);
			// 처음 본 엔티티는 기준선만 잡는다. 카운터는 wrap하므로 반드시 "같은가"로만 비교한다.
			if (Previous != nullptr && *Previous != ActionFrag.Seq)
			{
				const uint32 NetID = NetIDs.IsValidIndex(i) ? NetIDs[i].NetID.GetValue() : 0u;
				UE_LOG(LogLootNPop, Log, TEXT("EnemyAction netid=%u action=%s seq=%u (%s)"),
					NetID, *UEnum::GetValueAsString(ActionFrag.Action), ActionFrag.Seq,
					*Entity.DebugGetDescription());
			}
		}
	});

	if (bLogTransitions)
		LastLoggedSeq = MoveTemp(SeenThisFrame);
}

// --- Debug Draw Processor (디버그 드로우) ---

ULNPEnemyDebugDrawProcessor::ULNPEnemyDebugDrawProcessor()
	: EnemyQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetingProcessor::StaticClass()->GetFName());
}

void ULNPEnemyDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UE::Mass::Debug::FLineBatcher LineBatcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);
	const float MeleeProximityDistSq = GetDefault<ULNPSettings>()->DebugDrawProximityDistSq;

	// Player 위치 전체 수집
	TArray<FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			PlayerLocations.Add(Transforms[i].GetTransform().GetLocation());
	});

	auto IsNearAnyPlayer = [&](const FVector& Pos) -> bool
	{
		for (const FVector& PL : PlayerLocations)
		{
			if (FVector::DistSquared(Pos, PL) < MeleeProximityDistSq)
				return true;
		}
		return false;
	};

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
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
			const FTransform&                 EntityTransform = Transforms[i].GetTransform();
			const FVector                     EntityLocation  = EntityTransform.GetLocation();
			const FLNPEnemyTargetingFragment& Targeting       = TargetingFragments[i];

			if (!IsNearAnyPlayer(EntityLocation))
				continue;

			FColor StateColor = FColor::Green; // 대기 (기본값)

			if (Targeting.State == ELNPTargetingState::Confirmed)
			{
				const float ActualDistance = FMath::Sqrt(Targeting.DistanceToTargetSq);
				StateColor = (ActualDistance <= AttackRange) ? FColor::Red /*공격*/ : FColor::Blue /*추격*/;
			}
			else if (Targeting.State == ELNPTargetingState::Alert)
			{
				StateColor = FColor::Yellow; // 경계
			}

			const FVector Offset = (FVector::ZeroVector - EntityLocation).GetSafeNormal() * 50.0f;
			LineBatcher.DrawSolidBox(EntityLocation + Offset, FVector(15.0), StateColor);
			LineBatcher.DrawArrow(EntityTransform, 75.0f, FColor::Black);
		}
	});
}
#else
ULNPEnemyDebugDrawProcessor::ULNPEnemyDebugDrawProcessor()
	: EnemyQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPEnemyDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPEnemyDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}
#endif

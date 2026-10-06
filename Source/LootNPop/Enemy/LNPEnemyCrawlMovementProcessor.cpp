// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyCrawlMovementProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPEnemySpatialGrid.h"
#include "Enemy/LNPSurfaceCrawl.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "GAS/LNPPoiseTypes.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"
#include "LNPMassUtils.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

ULNPEnemyCrawlMovementProcessor::ULNPEnemyCrawlMovementProcessor()
	: CrawlQuery(*this)
	, PodQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	// 지상 이동 프로세서와 같은 이유 — 같은 프레임의 공격 위상을 읽어야 정지가 한 박자 늦지 않는다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEntityAttackProcessor"));
	// 3D 분리가 이번 프레임에 지은 벽보행 격자를 읽는다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemySpatialGridProcessor"));
}

void ULNPEnemyCrawlMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	CrawlQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	CrawlQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	CrawlQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	CrawlQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);          // 피격 반응 시계
	CrawlQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);  // 넉백·표면 상실·사망 낙하
	CrawlQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite);      // 배회 타임아웃 계측
	CrawlQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);    // 공격 중 정지·회전 고정
	CrawlQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	CrawlQuery.AddRequirement<FLNPEnemyCrawlFragment>(EMassFragmentAccess::ReadWrite);      // 표면 법선·사망 착지
	CrawlQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	CrawlQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	CrawlQuery.AddTagRequirement<FLNPEnemyDormantTag>(EMassFragmentPresence::None);
	CrawlQuery.AddTagRequirement<FLNPEnemyCrawlTag>(EMassFragmentPresence::All);
	// ⚠️ 지상과 같이 FLNPEnemyDyingTag를 None으로 걸지 않는다 — 시체의 낙하를 이 프로세서가 적분한다.
	CrawlQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	CrawlQuery.RegisterWithProcessor(*this);

	PodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PodQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadOnly);
	PodQuery.AddTagRequirement<FLNPLootPodTag>(EMassFragmentPresence::All);
	PodQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPCrawlSpatialGridSubsystem>(EMassFragmentAccess::ReadOnly);
}

namespace
{
	void ClearFailedPods(FLNPEnemyCrawlFragment& CrawlData)
	{
		for (FMassEntityHandle& Failed : CrawlData.FailedPods)
			Failed = FMassEntityHandle();
		CrawlData.NextFailedPod = 0;
	}
}

void ULNPEnemyCrawlMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
		return;

	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	const ULNPMassWorldCollisionSubsystem& WorldCollision = Context.GetSubsystemChecked<ULNPMassWorldCollisionSubsystem>();
	const ULNPCrawlSpatialGridSubsystem& CrawlGrid = Context.GetSubsystemChecked<ULNPCrawlSpatialGridSubsystem>();

	// 재귀속 후보: 터지지 않은 Pod 전부(D-066 — Nav 도달성은 보지 않는다). 지상 경로 프로세서와 같은 "활성" 기준이다.
	TArray<FMassEntityHandle> PodEntities;
	TArray<FVector> PodPositions;
	PodQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> PodTransforms = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPLootPodFragment> Pods = Ctx.GetFragmentView<FLNPLootPodFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			if (Pods[i].State == ELNPLootPodState::Popped)
				continue;
			PodEntities.Add(Ctx.GetEntity(i));
			PodPositions.Add(PodTransforms[i].GetTransform().GetLocation());
		}
	});
	const UWorld* World = EntityManager.GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// 청크 사이의 공유 쓰기는 신호 목록 하나뿐이다 — 지상 이동 프로세서와 같은 방식으로 모은다.
	TArray<FMassEntityHandle> EntitiesToSignal;
	FCriticalSection SignalLock;

	const auto ExecuteChunk = [&](FMassExecutionContext& Ctx)
	{
		TArray<FMassEntityHandle> ChunkEntitiesToSignal;
		ON_SCOPE_EXIT
		{
			if (ChunkEntitiesToSignal.Num() > 0)
			{
				FScopeLock ScopeLock(&SignalLock);
				EntitiesToSignal.Append(ChunkEntitiesToSignal);
			}
		};

		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr)
			return;

		const TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = Ctx.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyFragment> EnemyFragments = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> VelocityFragments = Ctx.GetMutableFragmentView<FLNPEnemyVelocityFragment>();
		const TArrayView<FLNPEnemyIdleFragment> IdleFragments = Ctx.GetMutableFragmentView<FLNPEnemyIdleFragment>();
		const TConstArrayView<FLNPEntityAttackFragment> AttackFragments = Ctx.GetFragmentView<FLNPEntityAttackFragment>();
		const TConstArrayView<FLNPPoiseFragment> PoiseFragments = Ctx.GetFragmentView<FLNPPoiseFragment>();
		const TArrayView<FLNPEnemyCrawlFragment> CrawlFragments = Ctx.GetMutableFragmentView<FLNPEnemyCrawlFragment>();

		const FLNPEnemyMovementConfig& MoveConfig = Config->MovementConfig;
		const FVector GravityOrigin = MoveConfig.GravityOrigin;
		const float RotationRateRad = FMath::DegreesToRadians(MoveConfig.RotationRate);
		const float StopDistance = FLNPEnemyMovementConfig::ComputeStopDistance(MoveConfig.AttackRange);

		LNPSurfaceCrawl::FParams CrawlParams;
		CrawlParams.BodyRadius = Config->CapsuleRadius;
		LNPSurfaceCrawl::FDetourParams DetourParams;
		DetourParams.StallRadius = Config->CapsuleRadius;
		LNPSurfaceCrawl::FHomeApproachParams HomeParams;
		HomeParams.FarDistance = MoveConfig.WanderMaxDistance + Config->CapsuleRadius;
		HomeParams.ProgressMargin = Config->CapsuleRadius;

		// 낙하는 지상 공중 분기와 같은 함수를 쓴다. 몸이 구라 반높이도 반지름이다(캡슐 = 구).
		LNPEnemyExactMovement::FParams FallParams;
		FallParams.GravityOrigin = GravityOrigin;
		FallParams.GravityStrength = MoveConfig.GravityStrength;
		FallParams.CapsuleRadius = Config->CapsuleRadius;
		FallParams.CapsuleHalfHeight = Config->CapsuleRadius;

		// 착지·스폰 직후 붙을 표면을 찾는 거리. 스폰은 지면점에서 반높이만큼 올린 자리다.
		const float AttachReach = Config->CapsuleRadius;

		const bool bChunkIsDying = Ctx.DoesArchetypeHaveTag<FLNPEnemyDyingTag>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// 배회 대역의 구간 배회(ULNPEnemyWanderProcessor)가 이번 프레임 위치·회전을 정했다.
			if (IdleFragments[i].bWanderHandled)
				continue;
			FTransform& EntityTransform = Transforms[i].GetMutableTransform();
			FVector Location = EntityTransform.GetLocation();
			const FVector GravityUp = (GravityOrigin - Location).GetSafeNormal();

			FLNPEnemyCrawlFragment& CrawlData = CrawlFragments[i];
			FVector& PhysVelocity = VelocityFragments[i].Velocity;

			FLNPEnemyFragment& EnemyData = EnemyFragments[i];
			const bool bHitReacting = EnemyData.TickReactionTimers(DeltaTime);

			// 착지하면 그 자리 표면에 다시 붙는다. 못 찾으면 구 중심 방향을 법선으로 두고 다음 이동의 probe가 정한다.
			auto AttachHere = [&](const FVector& At)
			{
				FVector Attached, AttachedNormal;
				if (LNPSurfaceCrawl::Attach(WorldCollision, CrawlParams, At, -(GravityOrigin - At).GetSafeNormal(), AttachReach, Attached, AttachedNormal))
				{
					CrawlData.SurfaceNormal = AttachedNormal;
					return Attached;
				}
				CrawlData.SurfaceNormal = (GravityOrigin - At).GetSafeNormal();
				return At;
			};

			if (bChunkIsDying)
			{
				// 붙어 있던 표면을 놓고 착지할 때까지 떨어진다. 사망 팝이 0이어도 중력이 적분되므로 천장에 남지 않는다.
				if (!CrawlData.bDeathLanded)
				{
					FVector NewLocation;
					CrawlData.bDeathLanded = LNPEnemyExactMovement::StepAirborne(WorldCollision, FallParams, Location, PhysVelocity, DeltaTime, NewLocation);
					CrawlData.SurfaceNormal = (GravityOrigin - NewLocation).GetSafeNormal();
					EntityTransform.SetLocation(NewLocation);
					EntityTransform.SetRotation(FRotationMatrix::MakeFromZX(CrawlData.SurfaceNormal, EntityTransform.GetRotation().GetForwardVector()).ToQuat());
				}
				continue;
			}

			// 넉백·표면 상실: 지상과 같은 공중 분기. 스스로 점프하지 않으므로 이 속도의 생산자는 외부(넉백)와 표면 상실뿐이다.
			if (!PhysVelocity.IsNearlyZero())
			{
				LNPSurfaceCrawl::ResetDetour(CrawlData.Detour);
				FVector NewLocation;
				if (LNPEnemyExactMovement::StepAirborne(WorldCollision, FallParams, Location, PhysVelocity, DeltaTime, NewLocation))
				{
					NewLocation = AttachHere(NewLocation);
					// 착지는 Home 판정이 끝날 때까지 Pod를 활성으로 붙잡는다(지상과 같은 플래그 — 활동 대역이 강제 활성하고
					// 구간 배회가 건너뛴다). 그러지 않으면 도망친 플레이어 때문에 Pod가 배회·휴면으로 내려가 재귀속 계측이 멈춘다.
					EnemyData.bNeedsHomeCheck |= EnemyData.ParentLootPod.IsValid();
				}
				else
				{
					CrawlData.SurfaceNormal = (GravityOrigin - NewLocation).GetSafeNormal();
				}
				EntityTransform.SetLocation(NewLocation);
				EntityTransform.SetRotation(FRotationMatrix::MakeFromZX(CrawlData.SurfaceNormal, EntityTransform.GetRotation().GetForwardVector()).ToQuat());
				continue;
			}

			// 스폰 직후에는 법선을 모른다. 발밑 표면을 한 번 찾는다.
			if (CrawlData.SurfaceNormal.IsNearlyZero())
			{
				Location = AttachHere(Location);
				EntityTransform.SetLocation(Location);
			}
			const FVector Normal = CrawlData.SurfaceNormal;

			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			FVector Goal = Location;
			float Speed = 0.f;
			FVector FacingDir = FVector::ZeroVector;   // 월드 방향. 접평면 성분만 몸의 전방이 된다.

			switch (Targeting.State)
			{
			case ELNPTargetingState::None:
				if (bHitReacting)
				{
					FacingDir = EnemyData.HitReactDirection;
				}
				else
				{
					Goal = MoveTargets[i].Center;
					Speed = MoveConfig.MoveSpeed * 0.3f;
					FacingDir = Goal - Location;
				}
				break;

			case ELNPTargetingState::Alert:
				FacingDir = Targeting.TargetLocation - Location;
				break;

			case ELNPTargetingState::Confirmed:
				if (Targeting.TargetPlayer.IsValid())
				{
					// 원거리 전용이다(D-066). 사거리 안 정지 거리까지 다가가 선다.
					// 사거리 안이어도 사선이 막혔으면(섬 밑면·벽 뒤) 서지 않고 타겟 쪽으로 계속 기어간다 — 표면을 따라 돌면
					// 대개 사선이 트인다. 계속 막히면 아래 단순 우회가 끼어든다. 플래그는 소비하지 않는다:
					// 공격 프로세서가 0.25초마다 다시 보고 트이면 0으로 되돌린다.
					FacingDir = Targeting.TargetLocation - Location;
					if (FMath::Sqrt(Targeting.DistanceToTargetSq) > StopDistance || AttackFragments[i].bLineOfSightBlocked)
					{
						Goal = Targeting.TargetLocation;
						Speed = MoveConfig.MoveSpeed;
					}
				}
				break;
			}

			// 도착 신호와 배회 타임아웃은 지상과 같은 규약이다. 신호 없이는 IdleTask Tick이 돌지 않는다.
			const bool bArrived = LNPSurfaceCrawl::GoalDistanceSq(Location, Normal, Goal, CrawlParams.BodyRadius)
				< FMath::Square(FLNPEnemyMovementConfig::ArrivalTolerance);
			if (Speed > 0.f && bArrived)
			{
				if (Targeting.State == ELNPTargetingState::None)
					ChunkEntitiesToSignal.Add(Ctx.GetEntity(i));
				Speed = 0.f;
			}

			FLNPEnemyIdleFragment& IdleData = IdleFragments[i];
			if (Targeting.State == ELNPTargetingState::None && !bArrived)
			{
				IdleData.TimeSinceWanderIssued += DeltaTime;
				if (IdleData.TimeSinceWanderIssued > FLNPEnemyMovementConfig::WanderTimeout)
				{
					IdleData.TimeSinceWanderIssued = 0.0f;
					IdleData.bWanderTargetTimedOut = true;
					ChunkEntitiesToSignal.Add(Ctx.GetEntity(i));
				}
			}
			else
			{
				IdleData.TimeSinceWanderIssued = 0.0f;
			}

			// 재귀속(단위 4d, D-066): 비교전 중 Home 쪽으로 기어가다 가까워지지 않으면 직선거리로 가장 가까운 활성 Pod로 바꾼다.
			// 배회 목표가 Home 주변이라 따로 귀환 목표를 만들지 않는다. 후보가 없으면 그 자리를 배회 중심으로 두고(Orphaned)
			// 1초마다 다시 본다. 공중 프레임은 위에서 continue하므로 계측이 이어진다.
			if (Targeting.State == ELNPTargetingState::None)
			{
				const float HomeDistance = FVector::Dist(Location, EnemyData.ParentPodLocation);
				// Home 배회 거리 안으로 돌아왔으면 착지 판정은 끝났다. 실제 Pod에 도착했으면 실패 기억도 비운다
				// (Orphaned의 Home은 제자리라 늘 안이므로 비우지 않는다 — 비우면 1초마다 같은 섬 Pod를 다시 고른다).
				if (HomeDistance <= HomeParams.FarDistance || EnemyData.bOrphaned)
					EnemyData.bNeedsHomeCheck = false;
				if (HomeDistance <= HomeParams.FarDistance && !EnemyData.bOrphaned)
					ClearFailedPods(CrawlData);
				const bool bRehome = EnemyData.bOrphaned
					? (Now - EnemyData.LastHomeCheckTime >= 1.0 && !PodPositions.IsEmpty())
					: LNPSurfaceCrawl::UpdateHomeApproach(CrawlData.HomeApproach, HomeParams, HomeDistance, DeltaTime);
				if (bRehome)
				{
					const FMassEntityHandle OldParent = EnemyData.ParentLootPod;
					const float OldDistance = FVector::Dist(Location, EnemyData.ParentPodLocation);
					if (OldParent.IsSet())
					{
						CrawlData.FailedPods[CrawlData.NextFailedPod] = OldParent;
						CrawlData.NextFailedPod = (CrawlData.NextFailedPod + 1) % FLNPEnemyCrawlFragment::MaxFailedPods;
					}
					TArray<bool, TInlineAllocator<128>> Excluded;
					Excluded.SetNumZeroed(PodEntities.Num());
					for (int32 PodIdx = 0; PodIdx < PodEntities.Num(); ++PodIdx)
						for (const FMassEntityHandle& Failed : CrawlData.FailedPods)
							Excluded[PodIdx] |= Failed == PodEntities[PodIdx];
					const int32 PodIndex = LNPSurfaceCrawl::SelectNearestPod(Location, PodPositions, Excluded);
					EnemyData.LastHomeCheckTime = Now;
					EnemyData.bNeedsHomeCheck = false;
					EnemyData.bOrphaned = PodIndex == INDEX_NONE;
					EnemyData.ParentLootPod = PodIndex != INDEX_NONE ? PodEntities[PodIndex] : FMassEntityHandle();
					EnemyData.ParentPodLocation = PodIndex != INDEX_NONE ? PodPositions[PodIndex] : Location;
					if (OldParent != EnemyData.ParentLootPod)
					{
						IdleData.bNeedNewWanderTarget = true;
						IdleData.bWanderTargetTimedOut = true;
						ChunkEntitiesToSignal.Add(Ctx.GetEntity(i));
						UE_LOG(LogLootNPop, Log, TEXT("[SurfaceCrawlHome] enemy=%d oldPod=%d newPod=%d orphaned=%d oldDist=%.0f newDist=%.0f"),
							Ctx.GetEntity(i).Index, OldParent.Index, EnemyData.ParentLootPod.Index, EnemyData.bOrphaned,
							OldDistance, FVector::Dist(Location, EnemyData.ParentPodLocation));
					}
				}
			}
			else
			{
				LNPSurfaceCrawl::ResetHomeApproach(CrawlData.HomeApproach);
				ClearFailedPods(CrawlData);
			}

			// 그로기·다운 중에는 제자리. 다운은 게이지를 0으로 되돌리므로 면역 잔여로 함께 본다.
			if (PoiseFragments.IsValidIndex(i)
				&& (PoiseFragments[i].bIsGroggy || PoiseFragments[i].ImmunityTimeRemaining > 0.f))
			{
				Speed = 0.f;
			}

			// 공격 중에는 제자리. 회전은 Active부터 잠근다(지상과 같은 규약 — 선딜만 조준이 따라온다).
			const ELNPEntityAttackPhase AttackPhase = AttackFragments[i].Phase;
			if (AttackPhase != ELNPEntityAttackPhase::None)
			{
				Speed = 0.f;
				if (AttackPhase != ELNPEntityAttackPhase::Windup)
					FacingDir = FVector::ZeroVector;
			}

			// 교전 추격 중에만 막힘을 재고 단순 우회를 시도한다(단위 2e). 배회는 배회 타임아웃이 목표를 다시 뽑는다.
			FVector MoveDir = LNPSurfaceCrawl::SteerDirection(Location, Normal, Goal, GravityUp, CrawlParams.BodyRadius);
			if (Targeting.State == ELNPTargetingState::Confirmed && Speed > 0.f)
			{
				MoveDir = LNPSurfaceCrawl::UpdateDetour(CrawlData.Detour, DetourParams, Location, Normal, MoveDir, Goal - Location,
					GravityUp, EntityTransform.GetRotation().GetRightVector(), (Ctx.GetEntity(i).Index & 1) ? 1 : -1, DeltaTime);
			}
			else
			{
				LNPSurfaceCrawl::ResetDetour(CrawlData.Detour);
			}

			// 벽보행끼리의 3D 분리. 거리는 3D로 재고(다른 면의 개체도 몸이 겹칠 수 있다) 미는 방향은 지금 표면의
			// 접평면에 눕힌다 — 법선 성분은 표면을 떠나게 할 뿐이다. 이웃이 없으면 0이라 정지 개체의 query 0회가 유지된다.
			FVector SeparationPush = FVector::ZeroVector;
			if (MoveConfig.SeparationRadius > 0.f && MoveConfig.SeparationStrength > 0.f)
			{
				const FMassEntityHandle Self = Ctx.GetEntity(i);
				const float Radius = MoveConfig.SeparationRadius;
				CrawlGrid.ForEachNeighbor(Location, Radius, [&](const FMassEntityHandle Neighbor, const FVector& NeighborPos)
				{
					if (Neighbor == Self)
						return;
					const FVector Delta = FVector::VectorPlaneProject(Location - NeighborPos, Normal);
					const float Dist = (Location - NeighborPos).Size();
					if (Dist >= Radius)
						return;
					// 접평면에서 겹친 쌍(같은 자리 또는 바로 위아래 면)은 핸들 순서로 좌우를 갈라 서로 반대로 흩어지게 한다.
					const float PlaneDist = Delta.Size();
					const FVector Right = EntityTransform.GetRotation().GetRightVector();
					const FVector PushDir = PlaneDist > 1.f ? Delta / PlaneDist : (Self.Index < Neighbor.Index ? Right : -Right);
					SeparationPush += PushDir * (1.f - Dist / Radius);
				});
				SeparationPush = FVector::VectorPlaneProject(SeparationPush.GetClampedToMaxSize(1.f), Normal) * MoveConfig.SeparationStrength;
			}

			FVector NewLocation = Location;
			FVector NewNormal = Normal;
			const FVector Velocity = MoveDir * Speed + SeparationPush;
			if (LNPSurfaceCrawl::Step(WorldCollision, CrawlParams, Location, Normal, Velocity, DeltaTime, NewLocation, NewNormal)
				== LNPSurfaceCrawl::EStepResult::Lost)
			{
				// 붙을 표면이 없다. 기어가던 속도를 실은 채 구 중심 중력으로 떨어진다. 속도 0은 붙음을 뜻하므로 중력 한 스텝을 더한다.
				PhysVelocity = FVector::VectorPlaneProject(Velocity, Normal) - GravityUp * MoveConfig.GravityStrength * DeltaTime;
				NewNormal = (GravityOrigin - NewLocation).GetSafeNormal();
			}
			CrawlData.SurfaceNormal = NewNormal;
			EntityTransform.SetLocation(NewLocation);

			// 몸의 Up(회전 Z)은 언제나 표면 법선이다. 전방만 회전 속도로 바라볼 방향을 따라간다 —
			// 판정 캡슐·공격 기저가 모서리에서 늦게 따라오지 않게 한다.
			// 표면을 갈아탄 프레임에는 법선이 돈 만큼 몸 전체를 같이 기울인다. 그대로 두면 벽을 향하던 전방이 새 법선과
			// 평행해져 전방을 잃는다(바닥 → 벽에서 전방 = -벽 법선).
			const FQuat CurrentRotation = FQuat::FindBetweenNormals(Normal, NewNormal) * EntityTransform.GetRotation();
			FVector Forward = CurrentRotation.GetForwardVector();
			const FVector FacingOnPlane = FVector::VectorPlaneProject(FacingDir, NewNormal).GetSafeNormal();
			if (!FacingOnPlane.IsNearlyZero())
			{
				const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(FacingOnPlane, NewNormal).ToQuat();
				Forward = FMath::QInterpConstantTo(CurrentRotation, TargetQuat, DeltaTime, RotationRateRad).GetForwardVector();
			}
			EntityTransform.SetRotation(FRotationMatrix::MakeFromZX(NewNormal, Forward).ToQuat());
		}
	};

	if (LNPEnemyExactMovement::IsParallelMovementEnabled())
	{
		CrawlQuery.ParallelForEachEntityChunk(Context, ExecuteChunk, FMassEntityQuery::EParallelExecutionFlags::AutoBalance);
	}
	else
	{
		CrawlQuery.ForEachEntityChunk(Context, ExecuteChunk);
	}

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

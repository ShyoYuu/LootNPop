// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyCrawlMovementProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPSurfaceCrawl.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "GAS/LNPPoiseTypes.h"
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
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	// 지상 이동 프로세서와 같은 이유 — 같은 프레임의 공격 위상을 읽어야 정지가 한 박자 늦지 않는다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEntityAttackProcessor"));
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

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
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
					// 원거리 전용이다(D-066). 사거리 안 정지 거리까지 다가가 선다. LoS 재배치는 단위 4다.
					FacingDir = Targeting.TargetLocation - Location;
					if (FMath::Sqrt(Targeting.DistanceToTargetSq) > StopDistance)
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

			FVector NewLocation = Location;
			FVector NewNormal = Normal;
			const FVector Velocity = MoveDir * Speed;
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

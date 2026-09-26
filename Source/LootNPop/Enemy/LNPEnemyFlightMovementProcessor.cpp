// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyFlightMovementProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPFlightSteering.h"
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

ULNPEnemyFlightMovementProcessor::ULNPEnemyFlightMovementProcessor()
	: FlightQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	// 지상 이동 프로세서와 같은 이유 — 같은 프레임의 공격 위상을 읽어야 정지가 한 박자 늦지 않는다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEntityAttackProcessor"));
}

void ULNPEnemyFlightMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	FlightQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	FlightQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	FlightQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	FlightQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);          // 피격 반응 시계
	FlightQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);  // 사망 낙하
	FlightQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite);      // 배회 타임아웃 계측
	FlightQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);    // 공격 중 정지·회전 고정
	FlightQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	FlightQuery.AddRequirement<FLNPEnemyFlightFragment>(EMassFragmentAccess::ReadWrite);     // 교전 위치 유지
	FlightQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	FlightQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	FlightQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::All);
	// ⚠️ 지상과 같이 FLNPEnemyDyingTag를 None으로 걸지 않는다 — 시체의 낙하를 이 프로세서가 적분한다.
	FlightQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	FlightQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPEnemyFlightMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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
		const TArrayView<FLNPEnemyFlightFragment> FlightFragments = Ctx.GetMutableFragmentView<FLNPEnemyFlightFragment>();

		const FLNPEnemyMovementConfig& MoveConfig = Config->MovementConfig;
		const FLNPEnemyFlightConfig& FlightConfig = Config->FlightConfig;
		const FVector GravityOrigin = MoveConfig.GravityOrigin;
		const float RotationRateRad = FMath::DegreesToRadians(MoveConfig.RotationRate);
		const float EngageAltitude = 0.5f * (FlightConfig.EngageAltitudeMin + FlightConfig.EngageAltitudeMax);
		// 타겟이 교전 지점을 올려다보는 각도가 EngageElevationDeg가 되는 수평 거리.
		const float EngageStandoff = EngageAltitude / FMath::Tan(FMath::DegreesToRadians(FlightConfig.EngageElevationDeg));
		const float AttackRangeSq = FMath::Square(MoveConfig.AttackRange);
		const float AimTargetUpOffset = Config->EntityAttackConfig.AimTargetUpOffset;

		LNPFlightSteering::FParams SteeringParams;
		SteeringParams.BodyRadius = FMath::Max(Config->CapsuleRadius, Config->CapsuleHalfHeight);
		SteeringParams.Clearance = FlightConfig.Clearance;
		SteeringParams.LookaheadTime = FlightConfig.LookaheadTime;

		// 시체의 낙하는 지상 공중 분기와 같은 함수를 쓴다(Phase03c §3.5).
		LNPEnemyExactMovement::FParams FallParams;
		FallParams.GravityOrigin = GravityOrigin;
		FallParams.GravityStrength = MoveConfig.GravityStrength;
		FallParams.CapsuleRadius = Config->CapsuleRadius;
		FallParams.CapsuleHalfHeight = Config->CapsuleHalfHeight;

		const bool bChunkIsDying = Ctx.DoesArchetypeHaveTag<FLNPEnemyDyingTag>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FTransform& EntityTransform = Transforms[i].GetMutableTransform();
			const FVector Location = EntityTransform.GetLocation();
			const FVector UpDir = (GravityOrigin - Location).GetSafeNormal();

			FLNPEnemyFragment& EnemyData = EnemyFragments[i];
			const bool bHitReacting = EnemyData.TickReactionTimers(DeltaTime);

			if (bChunkIsDying)
			{
				// 비행을 끊고 떨어진다. 착지하면 속도가 0이 되어 그 자리에 멈춘다.
				FVector& FallVelocity = VelocityFragments[i].Velocity;
				if (!FallVelocity.IsNearlyZero())
				{
					FVector NewLocation;
					LNPEnemyExactMovement::StepAirborne(WorldCollision, FallParams, Location, FallVelocity, DeltaTime, NewLocation);
					EntityTransform.SetLocation(NewLocation);
				}
				continue;
			}

			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			FLNPEnemyFlightFragment& FlightData = FlightFragments[i];
			if (Targeting.State != ELNPTargetingState::Confirmed)
			{
				FlightData.bHoldingFirePosition = false;
			}

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
					// 배회점은 IdleTask가 Home 위 고도 대역에서 뽑은 3D 점이다. 속도 비율은 지상 배회와 같다.
					Goal = MoveTargets[i].Center;
					Speed = FlightConfig.FlightSpeed * 0.3f;
					FacingDir = Goal - Location;
				}
				break;

			case ELNPTargetingState::Alert:
				FacingDir = Targeting.TargetLocation - Location;
				break;

			case ELNPTargetingState::Confirmed:
				if (Targeting.TargetPlayer.IsValid())
				{
					FacingDir = Targeting.TargetLocation - Location;

					// 지금 자리에서 쏠 수 있는가: 사거리 안이고, 조준점이 조준 가용 각도 안이다.
					// 발사 방향은 "몸 전방 + 타겟에서 뽑은 Pitch"라, 타겟이 거의 바로 아래면 총구(전방 오프셋)보다 뒤에 있어
					// 탄이 앞으로 빗나간다. AimPitchMinDeg가 그 한계이자 발사 클램프·피격 인지 게이트의 공용 원본이다.
					const FVector AimPoint = Targeting.TargetLocation + (GravityOrigin - Targeting.TargetLocation).GetSafeNormal() * AimTargetUpOffset;
					const FVector ToAim = AimPoint - Location;
					const float AimUp = FVector::DotProduct(ToAim, UpDir);
					const float AimPitchDeg = FMath::RadiansToDegrees(FMath::Atan2(AimUp, (ToAim - UpDir * AimUp).Size()));
					const bool bCanFireHere = ToAim.SizeSquared() <= AttackRangeSq
						&& AimPitchDeg >= MoveConfig.AimPitchMinDeg && AimPitchDeg <= MoveConfig.AimPitchMaxDeg;

					// 쏠 수 있는 자리에 선 뒤에는 타겟이 조금 움직여도 따라 옮기지 않는다. 사거리·각도를 벗어날 때만 새 교전 지점으로 간다.
					if (FlightData.bHoldingFirePosition && bCanFireHere)
						break;
					FlightData.bHoldingFirePosition = false;

					// 교전 지점은 타겟 위 교전 고도(D-053)이되, 바로 위가 아니라 드론이 있는 쪽으로 수평 거리만큼 떨어진 점이다 —
					// 타겟이 EngageElevationDeg만큼만 올려다보면 된다. 대역 안의 선회·LoS 재배치는 구현 단위 3이다.
					const FVector TargetUp = (GravityOrigin - Targeting.TargetLocation).GetSafeNormal();
					FVector AwayFromTarget = FVector::VectorPlaneProject(Location - Targeting.TargetLocation, TargetUp).GetSafeNormal();
					if (AwayFromTarget.IsNearlyZero())
					{
						// 타겟 바로 위에 있으면 수평 방향이 없다. 현재 전방의 반대쪽으로 물러난다.
						AwayFromTarget = FVector::VectorPlaneProject(-EntityTransform.GetRotation().GetForwardVector(), TargetUp).GetSafeNormal();
					}
					Goal = Targeting.TargetLocation + TargetUp * EngageAltitude + AwayFromTarget * EngageStandoff;
					Speed = FlightConfig.FlightSpeed;

					// 움직이는 타겟을 쫓으면 30cm 도착 판정이 좀처럼 성립하지 않는다. 교전 지점은 이 정도면 도착으로 본다.
					constexpr float EngageArrivalTolerance = 100.f;
					if (bCanFireHere && FVector::DistSquared(Goal, Location) < FMath::Square(EngageArrivalTolerance))
					{
						FlightData.bHoldingFirePosition = true;
						Goal = Location;
						Speed = 0.f;
					}
				}
				break;
			}

			// 도착 신호와 배회 타임아웃은 지상과 같은 규약이다. 신호 없이는 IdleTask Tick이 돌지 않는다.
			const bool bArrived = FVector::DistSquared(Goal, Location) < FMath::Square(FLNPEnemyMovementConfig::ArrivalTolerance);
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

			// 그로기·다운 중에는 제자리 호버. 다운은 게이지를 0으로 되돌리므로 면역 잔여로 함께 본다.
			if (PoiseFragments.IsValidIndex(i)
				&& (PoiseFragments[i].bIsGroggy || PoiseFragments[i].ImmunityTimeRemaining > 0.f))
			{
				Speed = 0.f;
			}

			// 공격 중에는 제자리 호버. 회전은 Active부터 잠근다(지상과 같은 규약 — 선딜만 조준이 따라온다).
			const ELNPEntityAttackPhase AttackPhase = AttackFragments[i].Phase;
			if (AttackPhase != ELNPEntityAttackPhase::None)
			{
				Speed = 0.f;
				if (AttackPhase != ELNPEntityAttackPhase::Windup)
					FacingDir = FVector::ZeroVector;
			}

			const FVector DesiredVelocity = LNPFlightSteering::ComputeArrivalVelocity(Location, Goal, Speed, DeltaTime);
			FVector NewLocation;
			LNPFlightSteering::Step(WorldCollision, SteeringParams, Location, DesiredVelocity, DeltaTime, NewLocation);
			EntityTransform.SetLocation(NewLocation);

			// 몸은 늘 새 위치의 Up에 선다. 전방은 바라볼 방향의 접평면 성분이고, 없으면 현재 전방을 유지한다.
			// 복제 Yaw가 접평면 로컬 Yaw라 pitch·bank는 게스트에 전달되지 않는다(Phase03c §2).
			const FVector NewUp = (GravityOrigin - NewLocation).GetSafeNormal();
			const FVector FacingOnPlane = FVector::VectorPlaneProject(FacingDir, NewUp).GetSafeNormal();
			const FQuat CurrentRotation = EntityTransform.GetRotation();
			if (!FacingOnPlane.IsNearlyZero())
			{
				const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(FacingOnPlane, NewUp).ToQuat();
				EntityTransform.SetRotation(FMath::QInterpConstantTo(CurrentRotation, TargetQuat, DeltaTime, RotationRateRad));
			}
			else
			{
				const FVector Forward = FVector::VectorPlaneProject(CurrentRotation.GetForwardVector(), NewUp).GetSafeNormal();
				if (!Forward.IsNearlyZero())
					EntityTransform.SetRotation(FRotationMatrix::MakeFromXZ(Forward, NewUp).ToQuat());
			}
		}
	};

	if (LNPEnemyExactMovement::IsParallelMovementEnabled())
	{
		FlightQuery.ParallelForEachEntityChunk(Context, ExecuteChunk, FMassEntityQuery::EParallelExecutionFlags::AutoBalance);
	}
	else
	{
		FlightQuery.ForEachEntityChunk(Context, ExecuteChunk);
	}

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

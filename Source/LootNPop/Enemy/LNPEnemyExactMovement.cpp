// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyExactMovement.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

#include "HAL/IConsoleManager.h"

namespace LNPEnemyExactMovement
{
	namespace
	{
		int32 GEnemyExactLateralSweep = 1;
		FAutoConsoleVariableRef CVarEnemyExactLateralSweep(
			TEXT("LNP.SurfaceNav.EnemyExactLateralSweep"), GEnemyExactLateralSweep,
			TEXT("Exact enemy grounding only. 1 = lateral capsule sweep before the support probe, 0 = skip it (measurement factor)."));

		int32 GEnemyParallelMovement = 1;
		FAutoConsoleVariableRef CVarEnemyParallelMovement(
			TEXT("LNP.SurfaceNav.EnemyParallelMovement"), GEnemyParallelMovement,
			TEXT("Server-only. 1 = run the enemy movement processor with ParallelForEachEntityChunk, 0 = single-threaded (measurement factor, Phase03b 3.6.1)."));

		/** 시작 겹침을 풀 때 겹침 깊이에 더하는 여유(cm). */
		constexpr float DepenetrationSkin = 0.5f;

		/**
		 * 수평 sweep이 막힌 자리에서 벽 바깥으로 물러나는 거리(cm). 벽에 딱 붙어 서면 캡슐과 같은 반지름의
		 * 하향 probe가 벽을 먼저 맞혀 지지면을 잃는다(Rejected로 멈춤).
		 */
		constexpr float WallSkin = 1.0f;

		FVector UpAt(const FParams& Params, const FVector& Location)
		{
			return (Params.GravityOrigin - Location).GetSafeNormal();
		}

		/** 캡슐 축(로컬 Z)을 Up에 맞춘 회전. */
		FQuat CapsuleRotation(const FVector& Up)
		{
			return FQuat::FindBetweenNormals(FVector::UpVector, Up);
		}

		/**
		 * 하향 probe 구는 캡슐 바닥 구와 같다. 이보다 작으면 절벽 끝에서 probe가 빠졌는데 캡슐 옆면은 아직
		 * 모서리 위에 걸쳐, 수직 낙하 sweep이 모서리에 착지하고 다음 프레임 다시 지지면을 잃는 진동이 생긴다.
		 * 같은 구면 probe가 빠질 때 캡슐 전체가 모서리를 비켜 간다.
		 */
		float ProbeToCenter(const FParams& Params)
		{
			return Params.CapsuleHalfHeight - Params.CapsuleRadius;
		}

		bool ProbeAt(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params, const FVector& CapsuleCenter,
			const FVector& Up, const float StepUp, const float Drop, FLNPSupportProbeResult& OutResult)
		{
			FLNPSupportProbeQuery Query;
			Query.Position = CapsuleCenter - Up * ProbeToCenter(Params);
			Query.Up = Up;
			Query.MaxStepUp = StepUp;
			Query.MaxDrop = Drop;
			Query.Radius = Params.CapsuleRadius;
			Query.WalkableMinDot = Params.WalkableMinDot;
			return Collision.ProbeSupport(Query, FLNPWorldQueryParams(ELNPWorldQueryClass::GroundRiskFallback), OutResult);
		}

		/**
		 * 올린 캡슐로 Delta만큼 수평 sweep한다. 막히면 hit 법선의 접평면으로 남은 이동을 한 번 미끄러뜨린다.
		 * 반환값은 올리기 전 높이의 캡슐 중심이다.
		 */
		/** 이동에서 법선 쪽으로 파고드는 성분을 지우고 접평면에 남긴다. */
		FVector SlideAlong(const FVector& Move, const FVector& Normal, const FVector& Up)
		{
			const FVector Unblocked = FVector::DotProduct(Move, Normal) < 0.0 ? FVector::VectorPlaneProject(Move, Normal) : Move;
			return FVector::VectorPlaneProject(Unblocked, Up);
		}

		/**
		 * 캡슐을 From에서 접평면 이동 Move만큼 sweep하고 멈춘 위치를 돌려준다. OutHit.bBlockingHit이면 그 hit에서 멈췄다.
		 * 시작부터 겹쳐 있으면 겹침을 풀고 파고드는 성분을 지운 뒤 다시 sweep한다. 벽에 붙어 선 개체는 매 프레임
		 * 이 경우라, 이동 전체를 막으면 벽을 따라 미끄러지지 못한다. 다시 겹치면 From에 머문다.
		 */
		FVector SweepTangent(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params, const FVector& Up,
			const FVector& From, const FVector& Move, FLNPWorldHit& OutHit)
		{
			const FLNPWorldQueryParams QueryParams(ELNPWorldQueryClass::GroundRiskFallback);
			const FQuat Rotation = CapsuleRotation(Up);
			auto PulledBack = [&Up](const FLNPWorldHit& Hit)
			{
				return Hit.Location + FVector::VectorPlaneProject(Hit.ImpactNormal, Up) * WallSkin;
			};

			if (!Collision.SweepCapsuleWorld(From, From + Move, Rotation, Params.CapsuleRadius, Params.CapsuleHalfHeight, QueryParams, OutHit))
				return From + Move;
			if (!OutHit.bStartPenetrating)
				return PulledBack(OutHit);

			const FVector Depenetrated = From + OutHit.ImpactNormal * (OutHit.PenetrationDepth + DepenetrationSkin);
			const FVector Slide = SlideAlong(Move, OutHit.ImpactNormal, Up);
			if (!Collision.SweepCapsuleWorld(Depenetrated, Depenetrated + Slide, Rotation, Params.CapsuleRadius, Params.CapsuleHalfHeight, QueryParams, OutHit))
				return Depenetrated + Slide;
			return OutHit.bStartPenetrating ? From : PulledBack(OutHit);
		}

		/**
		 * 올린 캡슐로 Delta만큼 수평 sweep한다. 막히면 hit 법선의 접평면으로 남은 이동을 한 번 미끄러뜨린다.
		 * 반환값은 올리기 전 높이의 캡슐 중심이다.
		 */
		FVector SweepLateral(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
			const FVector& Location, const FVector& Up, const FVector& Delta)
		{
			const FVector Raise = Up * Params.MaxStepUp;
			const FVector Start = Location + Raise;

			FLNPWorldHit Hit;
			const FVector Reached = SweepTangent(Collision, Params, Up, Start, Delta, Hit);
			if (!Hit.bBlockingHit || Hit.bStartPenetrating)
				return Reached - Raise;

			const FVector Slide = SlideAlong(Start + Delta - Reached, Hit.ImpactNormal, Up);
			if (Slide.SizeSquared() < 1.0)
				return Reached - Raise;

			FLNPWorldHit SlideHit;
			return SweepTangent(Collision, Params, Up, Reached, Slide, SlideHit) - Raise;
		}
	}

	bool IsLateralSweepEnabled()
	{
		return GEnemyExactLateralSweep != 0;
	}

	bool IsParallelMovementEnabled()
	{
		return GEnemyParallelMovement != 0;
	}

	FVector MoveGroundedLaterally(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Velocity, const float DeltaTime)
	{
		const FVector Up = UpAt(Params, Location);
		const FVector Delta = FVector::VectorPlaneProject(Velocity, Up) * DeltaTime;

		FVector Target = Location + Delta;
		if (Params.bLateralSweep && Delta.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			Target = SweepLateral(Collision, Params, Location, Up, Delta);
		}
		return Target;
	}

	EGroundResult ProbeGroundedAt(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& OriginalLocation, const FVector& TargetLocation, const float DeltaTime,
		FVector& OutLocation, FVector& OutVelocity, FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = FLNPExactHitIdentity();
		}

		const FVector TargetUp = UpAt(Params, TargetLocation);
		FLNPSupportProbeResult Probe;
		if (ProbeAt(Collision, Params, TargetLocation, TargetUp, Params.MaxStepUp, Params.MaxStepDown, Probe))
		{
			OutLocation = Probe.Hit.Location + TargetUp * ProbeToCenter(Params);
			OutVelocity = FVector::ZeroVector;
			if (OutSurfaceIdentity != nullptr)
			{
				*OutSurfaceIdentity = Probe.Hit.Identity;
			}
			return EGroundResult::Grounded;
		}

		if (!Probe.Hit.bBlockingHit)
		{
			// 절벽 끝. probe 구 전체가 가장자리를 벗어났으므로 곧장 떨어져도 가장자리에 걸리지 않는다.
			// 속도 0은 접지를 뜻하므로 중력 한 스텝을 실어 공중으로 넘긴다.
			OutLocation = TargetLocation;
			OutVelocity = -TargetUp * Params.GravityStrength * DeltaTime;
			return EGroundResult::LostSupport;
		}

		if (Probe.Hit.bStartPenetrating)
		{
			// probe 구가 처음부터 지형 안에 있다(묻힌 채 스폰됐거나 겹친 자리로 밀려났다). 겹침 법선으로 풀어 두면
			// 다음 프레임 probe가 지형 밖에서 시작해 지지면을 다시 찾는다. 이 자리에서 그대로 서면 영영 묻혀 있다.
			OutLocation = TargetLocation + Probe.Hit.ImpactNormal * (Probe.Hit.PenetrationDepth + DepenetrationSkin);
			OutVelocity = FVector::ZeroVector;
			return EGroundResult::Rejected;
		}

		// 가파른 경사·Blocker·Unknown. 지금 위치는 직전 프레임에 지지면이었으므로 그대로 선다.
		OutLocation = OriginalLocation;
		OutVelocity = FVector::ZeroVector;
		return EGroundResult::Rejected;
	}

	EGroundResult StepGrounded(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Velocity, const float DeltaTime, FVector& OutLocation, FVector& OutVelocity,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		const FVector Target = MoveGroundedLaterally(Collision, Params, Location, Velocity, DeltaTime);
		return ProbeGroundedAt(Collision, Params, Location, Target, DeltaTime, OutLocation, OutVelocity, OutSurfaceIdentity);
	}

	bool StepAirborne(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, FVector& InOutVelocity, const float DeltaTime, FVector& OutLocation,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = FLNPExactHitIdentity();
		}
		const FVector Up = UpAt(Params, Location);
		const FQuat Rotation = CapsuleRotation(Up);
		const FLNPWorldQueryParams QueryParams(ELNPWorldQueryClass::AirborneMandatory);
		InOutVelocity -= Up * Params.GravityStrength * DeltaTime;

		auto RemoveInwardComponent = [&InOutVelocity, &Params, DeltaTime](const FVector& Normal, const FVector& HitUp)
		{
			if (FVector::DotProduct(InOutVelocity, Normal) < 0.0)
			{
				InOutVelocity = FVector::VectorPlaneProject(InOutVelocity, Normal);
			}
			// 속도 0은 접지를 뜻한다. 착지하지 않은 개체는 중력 한 스텝으로 공중에 남긴다.
			if (InOutVelocity.IsNearlyZero())
			{
				InOutVelocity = -HitUp * Params.GravityStrength * DeltaTime;
			}
		};

		FVector From = Location;
		FLNPWorldHit Hit;
		bool bHit = Collision.SweepCapsuleWorld(From, From + InOutVelocity * DeltaTime, Rotation, Params.CapsuleRadius, Params.CapsuleHalfHeight, QueryParams, Hit);
		if (bHit && Hit.bStartPenetrating)
		{
			// 이미 겹쳐 있다(접지 자리에서 넉백을 받았거나 측벽에 붙어 미끄러지는 중). sweep 없이 옮기면 겹친 면 너머
			// 다른 면(측벽을 따라 내려가다 만나는 바닥 등)을 뚫으므로, 겹침을 풀고 파고드는 성분을 지운 뒤 한 번 더 sweep한다.
			From = Location + Hit.ImpactNormal * (Hit.PenetrationDepth + DepenetrationSkin);
			RemoveInwardComponent(Hit.ImpactNormal, UpAt(Params, From));
			bHit = Collision.SweepCapsuleWorld(From, From + InOutVelocity * DeltaTime, Rotation, Params.CapsuleRadius, Params.CapsuleHalfHeight, QueryParams, Hit);
			if (bHit && Hit.bStartPenetrating)
			{
				OutLocation = From;
				return false;
			}
		}

		if (!bHit)
		{
			OutLocation = From + InOutVelocity * DeltaTime;
			return false;
		}

		OutLocation = Hit.Location;
		const FVector HitUp = UpAt(Params, Hit.Location);
		const bool bWalkable = FVector::DotProduct(Hit.ImpactNormal, HitUp) >= Params.WalkableMinDot;
		const bool bSupport = Hit.Identity.IsKnown() && (Hit.Identity.Roles & ELNPExactSourceRole::Support) != 0;
		if (bWalkable && bSupport)
		{
			InOutVelocity = FVector::ZeroVector;
			if (OutSurfaceIdentity != nullptr)
			{
				*OutSurfaceIdentity = Hit.Identity;
			}
			return true;
		}

		// 섬 측벽·밑면·Blocker·Unknown. 멈추면 섬 밑면에 달라붙으므로 미끄러진다. Unknown에는 착지하지 않는다(D-037).
		RemoveInwardComponent(Hit.ImpactNormal, HitUp);
		return false;
	}

	bool ProjectToSameLayer(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& ReferenceLocation, const FVector& Direction, const float Reach, FVector& OutCapsuleCenter,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = FLNPExactHitIdentity();
		}
		const FVector Dir = Direction.GetSafeNormal();
		const FVector Up = -Dir;
		const FVector SameRadius = Params.GravityOrigin + Dir * FVector::Dist(Params.GravityOrigin, ReferenceLocation);

		FLNPSupportProbeResult Probe;
		if (!ProbeAt(Collision, Params, SameRadius, Up, Reach, Reach, Probe))
			return false;

		OutCapsuleCenter = Probe.Hit.Location + Up * ProbeToCenter(Params);
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = Probe.Hit.Identity;
		}
		return true;
	}
}

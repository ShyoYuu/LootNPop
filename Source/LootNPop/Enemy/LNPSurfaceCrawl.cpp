// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPSurfaceCrawl.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemySurfaceMovement.h"

namespace LNPSurfaceCrawl
{
	namespace
	{
		/** 시작 겹침을 풀 때 겹침 깊이에 더하는 여유(cm). */
		constexpr float DepenetrationSkin = 0.5f;

		bool Sweep(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
			const FVector& Start, const FVector& End, FLNPWorldHit& OutHit)
		{
			return Collision.SweepSphereWorld(Start, End, Params.BodyRadius,
				FLNPWorldQueryParams(ELNPWorldQueryClass::SurfaceCrawl), OutHit);
		}

		/**
		 * 구의 접촉 법선(접점 → 구 중심). 면 법선(ImpactNormal)은 모서리에 닿았을 때 두 면 중 하나로 튀지만,
		 * 접촉 법선은 모서리를 축으로 연속으로 돈다.
		 */
		FVector ContactNormal(const FLNPWorldHit& Hit)
		{
			const FVector Normal = (Hit.Location - Hit.ImpactPoint).GetSafeNormal();
			return Normal.IsNearlyZero() ? Hit.ImpactNormal : Normal;
		}

		/**
		 * hit 자리에 틈을 남기고 붙는다. SweepStart는 그 hit를 낸 sweep의 시작점이다.
		 * 틈은 법선 방향이 아니라 sweep이 지나온 길로 물러나 만든다 — 지나온 길은 비어 있음이 확인됐으므로, 프랍 사이 구석·주머니에서도
		 * 옆 면에 파고들지 않는다(법선으로 띄우면 옆 면 쪽으로 최대 SurfaceGap × 두 면 법선 내적만큼 들어간다, Phase 14 단위 6b 실측).
		 * 법선 거리로 SurfaceGap만큼 물러나되 비스듬한 hit에서 접선으로 멀리 미끄러지지 않게 4배까지만, 지나온 거리까지만 물러난다.
		 */
		void SettleOn(const FLNPWorldHit& Hit, const FVector& SweepStart, const FParams& Params, FVector& OutLocation, FVector& OutNormal,
			FLNPExactHitIdentity* OutIdentity = nullptr)
		{
			if (OutIdentity != nullptr)
				*OutIdentity = Hit.Identity;
			OutNormal = ContactNormal(Hit);
			const FVector Back = SweepStart - Hit.Location;
			const double Traveled = Back.Size();
			if (Traveled <= UE_KINDA_SMALL_NUMBER)
			{
				OutLocation = Hit.Location;
				return;
			}
			const FVector BackDir = Back / Traveled;
			const double Approach = FVector::DotProduct(BackDir, OutNormal);
			const double MaxBack = Params.SurfaceGap * 4.0;
			const double Distance = Approach > Params.SurfaceGap / MaxBack ? Params.SurfaceGap / Approach : MaxBack;
			OutLocation = Hit.Location + BackDir * FMath::Min(Distance, Traveled);
		}
	}

	namespace
	{
		/**
		 * Step·정지 해소의 겹침 해소. 먼저 지지면 보호(붙은 면 안쪽 금지)로 풀고, 실패했는데 Dynamic 겹침이 없으면 보호 없이 한 번 더 푼다 —
		 * 덮치는 것이 없으면 정적 면 법선으로 미는 것이 맞고, 붙어 있던 패널이 지나간 뒤 남은 오래된 법선이 지면에서 빠져나오는 방향을
		 * 막는 경우가 있다(단위 6e 패키지 실측). 둘 다 실패하면 제자리이고, Dynamic 겹침이 있었으면 압사다.
		 * 반환: 풀었으면 true. bOutCrushed는 실패 + Dynamic 겹침.
		 */
		bool ResolveOverlap(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params, const FVector& From,
			const FVector& Up, FVector& OutLocation, bool& bOutCrushed)
		{
			bOutCrushed = false;
			bool bTouchedDynamic = false;
			if (Depenetrate(Collision, Params, From, OutLocation, Up, &bTouchedDynamic, Up))
				return true;
			if (bTouchedDynamic)
			{
				bOutCrushed = true;
				return false;
			}
			return Depenetrate(Collision, Params, From, OutLocation, Up, &bTouchedDynamic);
		}

		/** SteerDirection의 정규화 전 조향. 크기로 평형점(오르막과 내리막이 상쇄)을 알아본다. */
		FVector SteerRaw(const FVector& Location, const FVector& Normal, const FVector& Goal, const FVector& GravityUp,
			const float ReachDistance, const bool bSeekLineOfSight)
		{
			const FVector ToGoal = Goal - Location;
			const double Along = FVector::DotProduct(ToGoal, Normal);
			FVector Direction = FVector::VectorPlaneProject(ToGoal, Normal);
			if (Along > ReachDistance)
			{
				Direction += FVector::VectorPlaneProject(bSeekLineOfSight ? GravityUp : -GravityUp, Normal) * Along;
			}
			return Direction;
		}

		/** 경사선 성분이 이보다 작으면 바닥·천장으로 본다(경사선이 퇴화). */
		constexpr double MinSlopeSize = 0.3;

		uint8 BranchBit(const EChaseBranch Branch, const int8 Side)
		{
			switch (Branch)
			{
			case EChaseBranch::Lateral: return Side > 0 ? 1 : 2;
			case EChaseBranch::Down: return 4;
			case EChaseBranch::Up: return 8;
			default: return 0;
			}
		}

		/** 지금 면에서 갈래 방향. 옆 갈래는 State.Side·BranchDir를 갱신한다. 방향을 정할 수 없으면 0. */
		FVector BranchDirection(FChaseDetourState& State, const FVector& Normal, const FVector& GravityUp)
		{
			const FVector Slope = FVector::VectorPlaneProject(GravityUp, Normal);
			const bool bWall = Slope.Size() >= MinSlopeSize;
			FVector Direction = FVector::ZeroVector;
			if (State.Branch == EChaseBranch::Lateral)
			{
				if (bWall)
				{
					// 등고선. 면이 바뀌어도 회전 방향을 잇는다 — 볼록 모서리에서는 법선이 연속으로 돌아 내적이 양수로 이어지고,
					// 오목 모서리에서는 내적이 0 근처라 부호를 그대로 둔다(Cross(Normal, Up) 방향 규약이 같은 회전을 준다).
					const FVector Contour = FVector::CrossProduct(Normal, GravityUp).GetSafeNormal();
					const double Continuity = FVector::DotProduct(Contour, State.BranchDir);
					if (FMath::Abs(Continuity) > 0.1)
						State.Side = Continuity > 0.0 ? 1 : -1;
					Direction = Contour * State.Side;
				}
				else
				{
					Direction = FVector::VectorPlaneProject(State.BranchDir, Normal).GetSafeNormal();
				}
			}
			else if (State.Branch == EChaseBranch::Down || State.Branch == EChaseBranch::Up)
			{
				Direction = bWall ? (State.Branch == EChaseBranch::Up ? Slope : -Slope).GetSafeNormal()
					: FVector::VectorPlaneProject(State.BranchDir, Normal).GetSafeNormal();
			}
			if (!Direction.IsNearlyZero())
				State.BranchDir = Direction;
			return Direction;
		}

		/**
		 * 지금 자리에서 다음 갈래를 고른다. 순서는 치우친 쪽 옆 → 아래 → 반대쪽 옆 → 위(바닥·천장은 옆 둘). 이미 끝난 갈래와,
		 * 일반 추격 방향이 뚜렷할 때 그와 같은 갈래는 뺀다. 고르면 true.
		 */
		bool SelectBranch(FChaseDetourState& State, const FChaseDetourParams& Params, const FVector& Location, const FVector& Normal,
			const FVector& Goal, const FVector& GravityUp, const float ReachDistance, const int8 FallbackSide)
		{
			const FVector ToTargetPlane = FVector::VectorPlaneProject(Goal - Location, Normal);
			const FVector Slope = FVector::VectorPlaneProject(GravityUp, Normal);
			const bool bWall = Slope.Size() >= MinSlopeSize;
			FVector Axis = bWall ? FVector::CrossProduct(Normal, GravityUp) : FVector::CrossProduct(Normal, ToTargetPlane);
			if (Axis.IsNearlyZero())
				Axis = FVector::CrossProduct(Normal, FMath::Abs(Normal.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector);
			Axis = Axis.GetSafeNormal();
			const bool bFirstSelection = State.AttemptSide == 0;
			if (bFirstSelection)
			{
				const double Bias = FVector::DotProduct(ToTargetPlane.GetSafeNormal(), Axis);
				State.AttemptSide = State.RetrySide != 0 ? State.RetrySide
					: FMath::Abs(Bias) >= Params.BiasDot ? (Bias > 0.0 ? 1 : -1) : (FallbackSide >= 0 ? 1 : -1);
				State.RetrySide = 0;
			}
			const int8 FirstSide = State.AttemptSide;

			struct FCandidate { EChaseBranch Branch; int8 Side; FVector Direction; };
			TArray<FCandidate, TInlineAllocator<4>> Candidates;
			Candidates.Add({ EChaseBranch::Lateral, FirstSide, Axis * FirstSide });
			if (bWall)
				Candidates.Add({ EChaseBranch::Down, 0, -Slope.GetSafeNormal() });
			Candidates.Add({ EChaseBranch::Lateral, static_cast<int8>(-FirstSide), Axis * -FirstSide });
			if (bWall)
				Candidates.Add({ EChaseBranch::Up, 0, Slope.GetSafeNormal() });

			// 일반 추격이 갇힌 자리에서 이미 시도한 갈래는 시도 내내 뺀다. 갇힌 자리에서 한 번만 잰다 — 갈래를 따라 멀리 간 뒤에는
			// 일반 추격 방향이 달라져 뜻이 없다. 평형점에서는 정규화 전 조향이 거의 0이라 그 방향을 기준으로 쓰지 않는다.
			if (bFirstSelection)
			{
				const FVector Raw = SteerRaw(Location, Normal, Goal, GravityUp, ReachDistance, false);
				if (Raw.Size() > 0.5 * ToTargetPlane.Size())
				{
					const FVector ChaseDir = Raw.GetSafeNormal();
					for (const FCandidate& Candidate : Candidates)
					{
						if (FVector::DotProduct(Candidate.Direction, ChaseDir) > Params.ExcludeDot)
							State.TriedMask |= BranchBit(Candidate.Branch, Candidate.Side);
					}
				}
			}

			for (const FCandidate& Candidate : Candidates)
			{
				if ((State.TriedMask & BranchBit(Candidate.Branch, Candidate.Side)) != 0)
					continue;
				State.Branch = Candidate.Branch;
				State.Side = Candidate.Side != 0 ? Candidate.Side : State.Side;
				State.BranchDir = Candidate.Direction;
				State.BranchNormal = Normal;
				State.BranchOrigin = Location;
				State.LastLocation = Location;
				State.BranchTravel = 0.f;
				return true;
			}
			return false;
		}
	}

	float GoalDistanceSq(const FVector& Location, const FVector& Normal, const FVector& Goal, const float BodyRadius)
	{
		const FVector Delta = Goal - Location;
		const double Along = FVector::DotProduct(Delta, Normal);
		const double Excess = FMath::Max(0.0, FMath::Abs(Along) - BodyRadius);
		return static_cast<float>((Delta - Normal * Along).SizeSquared() + Excess * Excess);
	}

	FVector SteerDirection(const FVector& Location, const FVector& Normal, const FVector& Goal,
		const FVector& GravityUp, const float ReachDistance, const bool bSeekLineOfSight)
	{
		return SteerRaw(Location, Normal, Goal, GravityUp, ReachDistance, bSeekLineOfSight).GetSafeNormal();
	}

	FVector UpdateDetour(FDetourState& State, const FDetourParams& Params, const FVector& Location, const FVector& Normal,
		const FVector& SteerDir, const FVector& ToGoal, const FVector& GravityUp, const FVector& FallbackSide, const int8 InitialSide,
		const float DeltaTime)
	{
		if (State.Side == 0)
		{
			State.Side = InitialSide >= 0 ? 1 : -1;
		}

		// 막힘 계측은 우회 중에도 돈다 — 우회 방향이 또 막히면 반대로 시도한다.
		if (State.StallAnchor.IsZero() || FVector::DistSquared(Location, State.StallAnchor) > FMath::Square(Params.StallRadius))
		{
			State.StallAnchor = Location;
			State.StallTime = 0.f;
		}
		else
		{
			State.StallTime += DeltaTime;
		}
		if (State.StallTime >= Params.StallTimeout)
		{
			if (State.DetourTime > 0.f)
			{
				State.Side = -State.Side;
			}
			State.StallAnchor = Location;
			State.StallTime = 0.f;
			State.DetourTime = Params.Duration;
			State.DetourNormal = Normal;
			State.DetourOrigin = Location;
		}

		if (State.DetourTime <= 0.f)
			return SteerDir;

		State.DetourTime -= DeltaTime;
		// 시작 위치 근처(떨리는 모서리)에서는 기준 면을 계속 갱신한다. 기준 면은 그 자리를 떠날 때 붙어 있던 면이다.
		if (FVector::DistSquared(Location, State.DetourOrigin) <= FMath::Square(Params.StallRadius))
		{
			State.DetourNormal = Normal;
		}
		else if (FVector::DotProduct(Normal, State.DetourNormal) < Params.NewSurfaceDot)
		{
			// 다른 면에 올라섰다. 새 면에서는 일반 조향이 목표 쪽으로 데려간다.
			State.DetourTime = 0.f;
			return SteerDir;
		}

		// 목표 방향이 접평면에서 0이면(목표가 바로 위·아래) 목표 원방향과 몸의 옆으로 차례로 대신한다.
		FVector Toward = SteerDir;
		if (Toward.IsNearlyZero())
		{
			Toward = FVector::VectorPlaneProject(ToGoal, Normal).GetSafeNormal();
		}
		FVector Lateral = FVector::CrossProduct(Normal, Toward).GetSafeNormal();
		if (Lateral.IsNearlyZero())
		{
			Lateral = FVector::VectorPlaneProject(FallbackSide, Normal).GetSafeNormal();
		}
		Lateral *= State.Side;

		const bool bCeiling = FVector::DotProduct(State.DetourNormal, GravityUp) < -0.5;
		return bCeiling ? (Lateral * 0.5 - Toward).GetSafeNormal() : Lateral;
	}

	void ResetDetour(FDetourState& State)
	{
		State.StallAnchor = FVector::ZeroVector;
		State.StallTime = 0.f;
		State.DetourTime = 0.f;
	}

	bool UpdateChaseProgress(FChaseProgressState& State, const FChaseProgressParams& Params, const float DistanceToTarget,
		const FVector& TargetLocation, const float DeltaTime)
	{
		if (State.BestDistance <= 0.f
			|| FVector::DistSquared(TargetLocation, State.TargetAnchor) > FMath::Square(Params.RetargetDistance)
			|| DistanceToTarget < State.BestDistance - Params.ProgressMargin)
		{
			State.BestDistance = DistanceToTarget;
			State.StallTime = 0.f;
			State.TargetAnchor = TargetLocation;
			return false;
		}
		State.StallTime += DeltaTime;
		if (State.StallTime < Params.StallWindow)
			return false;
		ResetChaseProgress(State);
		return true;
	}

	void ResetChaseProgress(FChaseProgressState& State)
	{
		State.BestDistance = 0.f;
		State.StallTime = 0.f;
		State.TargetAnchor = FVector::ZeroVector;
	}

	FVector UpdateChaseDetour(FChaseDetourState& State, const FChaseDetourParams& Params, const FVector& Location,
		const FVector& Normal, const FVector& Goal, const FVector& GravityUp, const float ReachDistance, const int8 FallbackSide,
		const float DeltaTime, EChaseDetourMode& OutMode)
	{
		const double RetargetSq = FMath::Square(Params.Progress.RetargetDistance);
		// 대기: 내리막 없는 조향으로 지금 면에서 타겟의 수선 발 쪽(대개 윗면 가장자리)에 가서 선다.
		const auto Wait = [&]()
		{
			OutMode = EChaseDetourMode::Wait;
			const FVector Hold = FVector::VectorPlaneProject(Goal - Location, Normal);
			return Hold.SizeSquared() <= FMath::Square(Params.BodyRadius) ? FVector::ZeroVector : Hold.GetSafeNormal();
		};
		const auto GiveUp = [&]()
		{
			const int8 RetrySide = static_cast<int8>(-State.AttemptSide);
			ResetChaseDetour(State);
			State.RetrySide = RetrySide;
			State.bWaiting = true;
			State.WaitTargetAnchor = Goal;
			return Wait();
		};

		if (State.bWaiting)
		{
			State.WaitTime += DeltaTime;
			const bool bTargetStayed = FVector::DistSquared(Goal, State.WaitTargetAnchor) <= RetargetSq;
			if (State.WaitTime < Params.GiveUpCooldown && bTargetStayed)
				return Wait();
			// 타겟이 그대로면 지난 시도의 첫 옆 반대쪽부터 다시 시도한다. 타겟이 움직였으면 처음부터 판단한다.
			const int8 RetrySide = bTargetStayed ? State.RetrySide : static_cast<int8>(0);
			ResetChaseDetour(State);
			State.RetrySide = RetrySide;
		}

		const float Distance = FVector::Dist(Location, Goal);
		if (State.bAttempt)
		{
			// 갇힌 거리보다 확실히 가까워졌으면 성공이다. 타겟이 크게 움직였으면 처음부터 다시 판단한다.
			if (Distance < State.StuckDistance - Params.Progress.ProgressMargin
				|| FVector::DistSquared(Goal, State.AttemptTargetAnchor) > RetargetSq)
			{
				ResetChaseDetour(State);
			}
			else
			{
				State.AttemptTime += DeltaTime;
				if (State.AttemptTime >= Params.TotalBudget)
					return GiveUp();
			}
		}

		if (State.Branch != EChaseBranch::None)
		{
			State.BranchTravel += FVector::Dist(Location, State.LastLocation);
			State.LastLocation = Location;
			const bool bLeftOrigin = FVector::DistSquared(Location, State.BranchOrigin) > FMath::Square(Params.BodyRadius);
			if (State.Branch != EChaseBranch::Lateral && bLeftOrigin && FVector::DotProduct(Normal, State.BranchNormal) < Params.NewSurfaceDot)
			{
				// 위·아래가 다른 면에 닿았다. 새 면에서는 일반 추격이 타겟 쪽으로 데려간다. 같은 갈래는 이번 시도에서 다시 쓰지 않는다.
				State.TriedMask |= BranchBit(State.Branch, State.Side);
				State.Branch = EChaseBranch::None;
				ResetChaseProgress(State.Progress);
			}
			else if (State.BranchTravel >= Params.BranchBudget
				|| (Params.MaxTargetDistance > 0.f && Distance > Params.MaxTargetDistance))
			{
				// 예산 안에 끝 조건을 못 채웠거나 타겟을 놓칠 만큼 멀어졌다. 갇힌 자리로 돌아가지 않고 지금 자리에서 다음 갈래를 시작한다.
				State.TriedMask |= BranchBit(State.Branch, State.Side);
				State.Branch = EChaseBranch::None;
				if (!SelectBranch(State, Params, Location, Normal, Goal, GravityUp, ReachDistance, FallbackSide))
					return GiveUp();
			}
		}
		else if (UpdateChaseProgress(State.Progress, Params.Progress, Distance, Goal, DeltaTime))
		{
			if (!State.bAttempt)
			{
				State.bAttempt = true;
				State.AttemptTime = 0.f;
				State.StuckDistance = Distance;
				State.AttemptTargetAnchor = Goal;
				State.TriedMask = 0;
			}
			if (!SelectBranch(State, Params, Location, Normal, Goal, GravityUp, ReachDistance, FallbackSide))
				return GiveUp();
		}

		if (State.Branch != EChaseBranch::None)
		{
			const FVector Direction = BranchDirection(State, Normal, GravityUp);
			if (!Direction.IsNearlyZero())
			{
				OutMode = EChaseDetourMode::Branch;
				return Direction;
			}
			// 방향을 정할 수 없는 면(바닥·천장에서 진행 방향이 법선과 나란함)이다. 실패로 기억하고 일반 추격으로 돌아간다.
			State.TriedMask |= BranchBit(State.Branch, State.Side);
			State.Branch = EChaseBranch::None;
			ResetChaseProgress(State.Progress);
		}
		OutMode = EChaseDetourMode::Chase;
		return SteerDirection(Location, Normal, Goal, GravityUp, ReachDistance);
	}

	void InterruptChaseDetour(FChaseDetourState& State)
	{
		State.Branch = EChaseBranch::None;
		ResetChaseProgress(State.Progress);
	}

	void ResetChaseDetour(FChaseDetourState& State)
	{
		State = FChaseDetourState();
	}

	bool UpdateHomeApproach(FHomeApproachState& State, const FHomeApproachParams& Params, const float DistanceToHome, const float DeltaTime)
	{
		if (DistanceToHome <= Params.FarDistance)
		{
			ResetHomeApproach(State);
			return false;
		}
		if (State.BestDistance <= 0.f || DistanceToHome < State.BestDistance - Params.ProgressMargin)
		{
			State.BestDistance = DistanceToHome;
			State.StallTime = 0.f;
			return false;
		}
		State.StallTime += DeltaTime;
		if (State.StallTime < Params.Timeout)
			return false;
		ResetHomeApproach(State);
		return true;
	}

	void ResetHomeApproach(FHomeApproachState& State)
	{
		State.BestDistance = 0.f;
		State.StallTime = 0.f;
	}

	int32 SelectNearestPod(const FVector& From, const TConstArrayView<FVector> PodPositions, const TConstArrayView<bool> Excluded)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = DBL_MAX;
		for (int32 Index = 0; Index < PodPositions.Num(); ++Index)
		{
			if (Excluded.IsValidIndex(Index) && Excluded[Index])
				continue;
			const double DistSq = FVector::DistSquared(From, PodPositions[Index]);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Index;
			}
		}
		return Best;
	}

	bool IsCrawlable(const FLNPExactHitIdentity& Identity)
	{
		return Identity.IsKnown() && (Identity.Roles & (ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker)) != 0;
	}

	EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, const FVector& Velocity, const float DeltaTime,
		FVector& OutLocation, FVector& OutNormal, FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		OutLocation = Location;
		OutNormal = Normal;

		const FVector Up = Normal.GetSafeNormal();
		const FVector Move = FVector::VectorPlaneProject(Velocity, Up) * DeltaTime;
		if (Up.IsNearlyZero() || Move.SizeSquared() < UE_KINDA_SMALL_NUMBER)
			return EStepResult::Attached;

		// 1. 접평면 sweep. StepUp만큼 띄워 턱을 넘는다. 띄운 자리가 천장에 겹치면(낮은 틈) 띄우지 않는다.
		FVector Lift = Up * Params.StepUp;
		FLNPWorldHit Hit;
		bool bHit = Sweep(Collision, Params, Location + Lift, Location + Lift + Move, Hit);
		bool bLowClearance = false;
		if (bHit && Hit.bStartPenetrating)
		{
			Lift = FVector::ZeroVector;
			bLowClearance = true;
			bHit = Sweep(Collision, Params, Location, Location + Move, Hit);
			if (bHit && Hit.bStartPenetrating)
			{
				// 이미 지형 안에 있다(스폰·넉백 착지 직후 등). 겹침만 풀고 이번 프레임은 전진하지 않는다.
				// 풀지 못하면 제자리 — 마지막으로 민 자리는 다른 면(패널과 지면 사이면 지면) 안일 수 있다(Phase 14 PIE).
				bool bCrushed = false;
				if (!ResolveOverlap(Collision, Params, Location, Up, OutLocation, bCrushed))
				{
					OutLocation = Location;
					if (bCrushed)
						return EStepResult::Crushed;
				}
				return EStepResult::Attached;
			}
		}

		// 낮은 틈(법선 쪽에 띄울 자리가 없음)에서는 이후 sweep이 모두 그 면에서 멀어지거나 나란히 움직인다. Chaos trimesh는
		// 이때 얕은 시작 겹침을 보고하지 않아, 좁아지는 쐐기(섬 밑면과 지각 사이 등)로 기어가면 매 틱 그 면 안으로 들어간다
		// (단위 6e 패키지 실측 2~4cm, 틱당 3.6cm). 이 프레임에만 결과 자리를 법선 쪽으로 검사하고, 겹치면 전진하지 않는다.
		const auto Finish = [&](const EStepResult Result)
		{
			FLNPWorldHit Check;
			if (!bLowClearance || Result != EStepResult::Attached
				|| !Sweep(Collision, Params, OutLocation, OutLocation + Up * 0.1f, Check) || !Check.bStartPenetrating)
			{
				return Result;
			}
			OutNormal = Normal;
			if (OutSurfaceIdentity != nullptr)
				*OutSurfaceIdentity = FLNPExactHitIdentity();
			bool bCrushed = false;
			if (!ResolveOverlap(Collision, Params, Location, Up, OutLocation, bCrushed))
			{
				OutLocation = Location;
				if (bCrushed)
					return EStepResult::Crushed;
			}
			return EStepResult::Blocked;
		};

		if (bHit)
		{
			// Unknown에는 붙지 않는다(D-037). 미끄러지지 않고 선다 — 다음 프레임 조향이 다른 방향을 고를 때까지다.
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;

			// 오목 모서리: 앞을 막은 면으로 갈아탄다. 띄운 높이만큼 원래 표면 쪽으로 내려앉아 두 면이 만나는 구석에 선다.
			FVector Corner;
			FVector WallNormal;
			SettleOn(Hit, Location + Lift, Params, Corner, WallNormal, OutSurfaceIdentity);
			if (!Lift.IsZero())
			{
				FLNPWorldHit Down;
				const FVector DownEnd = Corner - Lift;
				if (!Sweep(Collision, Params, Corner, DownEnd, Down))
				{
					Corner = DownEnd;
				}
				else if (!Down.bStartPenetrating)
				{
					const FVector DownStart = Corner;
					FVector DownNormal;
					SettleOn(Down, DownStart, Params, Corner, DownNormal);
				}
			}
			OutLocation = Corner;
			OutNormal = WallNormal;
			return Finish(EStepResult::Attached);
		}

		// 2. 이동한 자리에서 -법선 probe. 평면·완만한 굴곡·볼록 모서리(구가 모서리를 축으로 굴러 돈다)를 모두 이 hit가 맡는다.
		const FVector Moved = Location + Move;
		const FVector ProbeEnd = Moved - Up * (Params.StepDown + Params.SurfaceGap);
		bHit = Sweep(Collision, Params, Moved + Lift, ProbeEnd, Hit);
		if (bHit)
		{
			if (Hit.bStartPenetrating)
			{
				bool bCrushed = false;
				if (!ResolveOverlap(Collision, Params, Moved + Lift, Up, OutLocation, bCrushed))
				{
					OutLocation = Location;
					if (bCrushed)
						return EStepResult::Crushed;
				}
				return EStepResult::Attached;
			}
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;
			SettleOn(Hit, Moved + Lift, Params, OutLocation, OutNormal, OutSurfaceIdentity);
			return Finish(EStepResult::Attached);
		}

		// 3. 한 프레임에 모서리를 지나쳤다. probe 끝(빈 공간으로 확인된 자리)에서 원래 표면 아래·이동 반대쪽으로 감싸 sweep한다.
		const FVector WrapEnd = ProbeEnd - Up * Params.BodyRadius - Move.GetSafeNormal() * (Move.Size() + Params.BodyRadius);
		bHit = Sweep(Collision, Params, ProbeEnd, WrapEnd, Hit);
		// 감싸기 시작점이 이미 지형 안이면 표면을 잃은 게 아니라 구석에 몰린 것이다. 공중으로 넘기면 겹친 채 떨어지며
		// 지면을 뚫는다(단위 6e 자동화 — 내려오는 패널 아래에서 기어가던 개체). 제자리에 서고 다음 프레임 겹침 해소에 맡긴다.
		if (bHit && Hit.bStartPenetrating)
		{
			OutLocation = Location;
			return EStepResult::Blocked;
		}
		if (bHit)
		{
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;
			SettleOn(Hit, ProbeEnd, Params, OutLocation, OutNormal, OutSurfaceIdentity);
			return Finish(EStepResult::Attached);
		}

		OutLocation = Moved;
		return EStepResult::Lost;
	}

	bool Attach(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& SearchDirection, const float Reach, FVector& OutLocation, FVector& OutNormal,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		const FVector Direction = SearchDirection.GetSafeNormal();
		FLNPWorldHit Hit;
		if (Direction.IsNearlyZero()
			|| !Sweep(Collision, Params, Location, Location + Direction * (Params.StepDown + Reach), Hit)
			|| Hit.bStartPenetrating || !IsCrawlable(Hit.Identity))
		{
			return false;
		}
		SettleOn(Hit, Location, Params, OutLocation, OutNormal, OutSurfaceIdentity);
		return true;
	}

	bool CarryOnDynamicSupport(const FLNPDynamicSupportFrame& DynamicFrame, FLNPEnemyDynamicSupportContact& InOutContact,
		FTransform& InOutTransform, FVector& InOutNormal)
	{
		const FQuat Before = InOutTransform.GetRotation();
		if (!LNPEnemySurfaceMovement::ApplyDynamicSupportDelta(DynamicFrame, InOutContact, InOutTransform))
			return false;
		InOutNormal = (InOutTransform.GetRotation() * Before.Inverse()).RotateVector(InOutNormal).GetSafeNormal();
		return true;
	}

	bool Depenetrate(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, FVector& OutLocation, const FVector& ProbeDirection, bool* bOutTouchedDynamic,
		const FVector& SupportNormal)
	{
		if (bOutTouchedDynamic != nullptr)
			*bOutTouchedDynamic = false;
		constexpr int32 MaxIterations = 8;
		// 짧은 sweep의 시작 겹침으로 제자리 겹침을 본다. 방향은 의심되는 면 쪽이어야 한다 — Chaos trimesh는 면에서 멀어지는 sweep의
		// 얕은 겹침을 보고하지 않거나 뒷면 법선을 낸다. 밀어낸 자리는 민 방향으로 검사한다: 내려오는 패널에서 벗어나려고 민 자리가
		// 지면 안이면 그 지면은 민 방향에 있다(Phase 14 단위 6e — 위쪽 검사로는 못 보고 매 틱 패널 속도만큼 지면에 밀어 넣었다).
		const auto Overlap = [&](const FVector& At, const FVector& Direction, FLNPWorldHit& OutHit)
		{
			const bool bOverlaps = Sweep(Collision, Params, At, At + Direction.GetSafeNormal() * 0.1f, OutHit) && OutHit.bStartPenetrating;
			if (bOverlaps && bOutTouchedDynamic != nullptr && OutHit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic)
				*bOutTouchedDynamic = true;
			return bOverlaps;
		};

		// 같은 면에 다시 걸렸는지. 깊이 비교는 같은 면일 때만 뜻이 있다 — 다른 면(주머니의 맞은편)이면 정상 진행이다.
		const auto SameSurface = [](const FLNPWorldHit& A, const FLNPWorldHit& B)
		{
			return A.Identity.Lifetime == B.Identity.Lifetime && A.Identity.Roles == B.Identity.Roles
				&& A.Identity.FaceIndex == B.Identity.FaceIndex && A.Identity.InstanceIndex == B.Identity.InstanceIndex
				&& A.Identity.Slot == B.Identity.Slot && FMath::Abs(FVector::DotProduct(A.ImpactNormal, B.ImpactNormal)) > 0.99;
		};

		OutLocation = Location;
		FLNPWorldHit Hit;
		if (!Overlap(OutLocation, ProbeDirection, Hit))
			return true;
		FVector LastPush = FVector::ZeroVector;
		for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
		{
			const FVector Normal = Hit.ImpactNormal;
			const double Depth = Hit.PenetrationDepth + DepenetrationSkin;
			// 직전에 민 방향과 마주 보는 면에 걸리면(주머니) 두 방향의 이등분으로 민다 — 주머니 출구 쪽이다.
			// 면 법선으로만 밀면 마주 보는 두 면 사이를 오가며 빠져나오지 못한다(자동화 실측).
			FVector Push = Normal;
			if (FVector::DotProduct(LastPush, Normal) < 0.0)
			{
				const FVector Bisector = (LastPush + Normal).GetSafeNormal();
				if (!Bisector.IsNearlyZero())
					Push = Bisector;
			}
			// 지지면 안쪽으로는 밀지 않는다(위 SupportNormal 설명).
			if (!SupportNormal.IsZero() && FVector::DotProduct(Push, SupportNormal) < -0.1)
				return false;
			const FVector Offset = Push * (Depth / FMath::Max(FVector::DotProduct(Push, Normal), 0.1));

			// 민 자리의 겹침이 줄지 않으면 법선이 뒤집혀 보고된 것으로 본다. Chaos trimesh는 검사 sweep이 면에서 멀어지는 방향일 때
			// 뒷면 법선을 낸다 — 그대로 밀면 매 틱 지면을 뚫고 들어간다(Phase 14 단위 6e 패키지 실측 틱당 5.4cm). 반대로 밀어 보고,
			// 어느 쪽도 나아지지 않으면 실패다(호출자는 제자리에 둔다).
			FVector Candidate = OutLocation + Offset;
			FLNPWorldHit CandidateHit;
			bool bCandidateOverlaps = Overlap(Candidate, Push, CandidateHit);
			if (bCandidateOverlaps && SameSurface(CandidateHit, Hit) && CandidateHit.PenetrationDepth >= Hit.PenetrationDepth)
			{
				const FVector Flipped = OutLocation - Offset;
				FLNPWorldHit FlippedHit;
				const bool bFlippedOverlaps = Overlap(Flipped, -Push, FlippedHit);
				if (bFlippedOverlaps && SameSurface(FlippedHit, Hit) && FlippedHit.PenetrationDepth >= Hit.PenetrationDepth)
					return false;
				if (!SupportNormal.IsZero() && FVector::DotProduct(-Push, SupportNormal) < -0.1)
					return false;
				Candidate = Flipped;
				CandidateHit = FlippedHit;
				bCandidateOverlaps = bFlippedOverlaps;
				Push = -Push;
			}
			OutLocation = Candidate;
			LastPush = Push;
			if (!bCandidateOverlaps)
				return true;
			Hit = CandidateHit;
		}
		return false;
	}

	EStepResult ResolveStationaryOverlap(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, FVector& OutLocation)
	{
		// 덮치는 쪽은 붙은 면의 반대편(법선 쪽)이다. 그쪽으로 검사해야 Chaos가 겹침을 보고한다.
		bool bCrushed = false;
		if (ResolveOverlap(Collision, Params, Location, Normal, OutLocation, bCrushed))
			return EStepResult::Attached;
		OutLocation = Location;
		return bCrushed ? EStepResult::Crushed : EStepResult::Blocked;
	}

	bool ValidateWanderSegment(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Start, const FVector& Normal, const FVector& Goal, const float MaxGapError, const float MinNormalDot)
	{
		FLNPWorldHit Hit;
		if (Sweep(Collision, Params, Start, Goal, Hit))
			return false;
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Distance(Start, Goal) / FMath::Max(Params.BodyRadius, 1.f)));
		for (int32 Index = 1; Index <= Steps; ++Index)
		{
			const FVector Sample = FMath::Lerp(Start, Goal, static_cast<double>(Index) / Steps);
			FVector Attached, AttachedNormal;
			if (!Attach(Collision, Params, Sample, -Normal, MaxGapError, Attached, AttachedNormal)
				|| FVector::DotProduct(AttachedNormal, Normal) < MinNormalDot
				|| FVector::DistSquared(Attached, Sample) > FMath::Square(MaxGapError))
			{
				return false;
			}
		}
		return true;
	}
}

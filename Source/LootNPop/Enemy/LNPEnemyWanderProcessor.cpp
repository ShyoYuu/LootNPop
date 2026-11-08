// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyWanderProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "Enemy/LNPSurfaceCrawl.h"
#include "Replication/LNPWanderReplication.h"
#include "Replication/LNPMassReplication.h"
#include "GAS/LNPPoiseTypes.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassExecutionContext.h"
#include "Algo/BinarySearch.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "LNPMassUtils.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "LootNPop.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWanderDiagnostics(
		TEXT("LNP.Enemy.Wander.Diagnostics"), 0,
		TEXT("Record per-execution Wander reselection counters in the default CSV category. Diagnostic only; server/standalone."));

	/** 재선택 한 번에 검사하는 후보 수. 막힌 후보는 버리지 않고 갈 수 있는 데까지 잘라 쓰므로 둘이면 충분하다. */
	constexpr int32 MaxCandidateAttempts = 2;
	/** 한 실행의 재선택 상한. 넘친 개체는 후보 검사 없이 잠깐 서고 다음 프레임들에 흩어져 다시 뽑는다(순간 부하 상한). */
	constexpr int32 MaxReselectionsPerExecute = 16;
	constexpr double DeferredHoldMin = 0.2;
	constexpr double DeferredHoldMax = 1.0;
	/** 채택한 지상·비행 구간의 도착 뒤 정지 시간(초). 범위로 두어 같은 프레임에 뽑은 개체들이 다시 모이지 않게 한다. */
	constexpr double HoldSecondsMin = 3.0;
	constexpr double HoldSecondsMax = 6.0;
	/** 벽보행 구간(Phase 14 단위 4e)은 지금 붙은 표면 안의 짧은 직선이다 — 이동 약 1초, 정지 9초. */
	constexpr double CrawlTravelSeconds = 1.0;
	constexpr double CrawlHoldSeconds = 9.0;
	/** 연속 n번째 실패의 정지 시간 = FailureHoldSeconds × 2^min(n−1, MaxFailureDoublings) + 개체별 지터. 코덱 Hold(uint16 ms) 안이다. */
	constexpr double FailureHoldSeconds = 3.0;
	constexpr int32 MaxFailureDoublings = 3;
	constexpr double FailureJitterSeconds = 1.5;
	/** 지상 경로의 정적 blocker sweep은 Atlas 표본(반지름 × 0.5 간격) 이 개수마다 한 번이다. Sweep은 연속 판정이라 사이의 장애물을 건너뛰지 않는다. */
	constexpr int32 GroundSweepStride = 4;

	/** 후보 거부 단계(진단 계수). */
	enum class EReject : uint8
	{
		GoalSupport,
		Endpoint,
		DirectWalk,
		PathSurface,
		PathSweep,
		FlightSweep,
		Crawl,
		Reservation,
		TileOverflow,
		Count
	};

	double FailureHold(const uint8 Failures, const int32 EntityIndex)
	{
		const int32 Doublings = FMath::Min(FMath::Max<int32>(Failures, 1) - 1, MaxFailureDoublings);
		// 난수열을 쓰지 않는 개체·횟수별 지터 — 같은 프레임에 실패한 무리가 같은 기한으로 다시 모이지 않는다.
		const uint32 Hash = HashCombineFast(GetTypeHash(EntityIndex), GetTypeHash(Failures));
		return FailureHoldSeconds * (1 << Doublings) + FailureJitterSeconds * ((Hash & 0xFFFF) / 65535.0);
	}
}

ULNPEnemyWanderProcessor::ULNPEnemyWanderProcessor() : EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEntityAttackProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemyPathProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("LNPEnemyMovementProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("LNPEnemyFlightMovementProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("LNPEnemyCrawlMovementProcessor"));
}

void ULNPEnemyWanderProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	EnemyQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPSurfaceDataSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPNavPathSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPDynamicTerrainSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPEnemyWanderProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
		return;
	const double Now = EntityManager.GetWorld()->GetTimeSeconds();
	const bool bDiagnostics = CVarWanderDiagnostics.GetValueOnGameThread() != 0;
	struct FWanderDiagnostics
	{
		int32 Eligible = 0;
		int32 Invalid = 0;
		int32 Expired = 0;
		int32 OverlapRejected = 0;
		int32 CandidateAttempts = 0;
		int32 Accepted = 0;
		int32 Failed = 0;
		int32 CodecRejected = 0;
		int32 Deferred = 0;
		int32 Truncated = 0;
		int32 Rejects[static_cast<int32>(EReject::Count)] = {};
		/** 0 지상, 1 비행, 2 벽보행. */
		int32 ModeAttempts[3] = {};
		int32 ModeFailed[3] = {};
	} Diagnostics;
	const auto Snapshot = Context.GetSubsystemChecked<ULNPSurfaceDataSubsystem>().TakeSnapshot();
	const auto Overlay = Context.GetSubsystemChecked<ULNPNavPathSubsystem>().TakeOverlay();
	const auto& Collision = Context.GetSubsystemChecked<ULNPMassWorldCollisionSubsystem>();
	const auto DynamicFrame = Context.GetSubsystemChecked<ULNPDynamicTerrainSubsystem>().GetDynamicSupportFrame();

	// 동적 지형 위험은 지지면이 속한 slot(옥탄트)만 본다. 개체마다 Supports 전체를 돌지 않도록 slot 목록으로 줄여 둔다.
	bool bAnyUnslottedSupport = false;
	TArray<int32, TInlineAllocator<8>> RiskSlots;
	for (const auto& Support : DynamicFrame->Supports)
	{
		if (Support.Id.Slot == INDEX_NONE)
			bAnyUnslottedSupport = true;
		else if (Snapshot.IsValid() && Snapshot->Slots.IsValidIndex(Support.Id.Slot))
			RiskSlots.AddUnique(Support.Id.Slot);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LNPWander_Collect);
		Reservations.Reset();
		PodRanges.Reset();
		EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const auto Enemies = Ctx.GetFragmentView<FLNPEnemyFragment>();
			const auto Idle = Ctx.GetMutableFragmentView<FLNPEnemyIdleFragment>();
			const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const auto* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config.Get();
			for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
			{
				Idle[I].bWanderHandled = false;
				if (!Config || !Enemies[I].ParentLootPod.IsValid())
					continue;
				const auto& Segment = Idle[I].WanderSegment;
				const FVector Position = Transforms[I].GetTransform().GetLocation();
				Reservations.Add({ Enemies[I].ParentLootPod, Ctx.GetEntity(I), Position,
					Segment.bValid ? Segment.Goal : Position, FMath::Max(Config->CapsuleHalfHeight, Config->CapsuleRadius) });
			}
		});
		// Pod 안에서는 Entity.Index 오름차순이 부채꼴 순번이다.
		Reservations.Sort([](const FReservation& A, const FReservation& B)
		{
			if (A.Pod.Index != B.Pod.Index)
				return A.Pod.Index < B.Pod.Index;
			if (A.Pod.SerialNumber != B.Pod.SerialNumber)
				return A.Pod.SerialNumber < B.Pod.SerialNumber;
			return A.Entity.Index < B.Entity.Index;
		});
		TPair<int32, int32>* Range = nullptr;
		for (int32 Index = 0; Index < Reservations.Num(); ++Index)
		{
			if (Index == 0 || Reservations[Index].Pod != Reservations[Index - 1].Pod)
				Range = &PodRanges.Add(Reservations[Index].Pod, { Index, 0 });
			++Range->Value;
		}
	}

	int32 Reselections = 0;
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPWander_Update);
	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto Enemies = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const auto Idle = Ctx.GetMutableFragmentView<FLNPEnemyIdleFragment>();
		const auto Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		const auto Moves = Ctx.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Targets = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const auto Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const auto Attacks = Ctx.GetFragmentView<FLNPEntityAttackFragment>();
		const auto Actors = Ctx.GetFragmentView<FMassActorFragment>();
		const auto Poise = Ctx.GetFragmentView<FLNPPoiseFragment>();
		const auto* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config.Get();
		if (!Config)
			return;
		LNPEnemyExactMovement::FParams Params;
		Params.GravityOrigin = Config->MovementConfig.GravityOrigin;
		Params.CapsuleRadius = Config->CapsuleRadius;
		Params.CapsuleHalfHeight = Config->CapsuleHalfHeight;
		const bool bFlying = Config->IsFlying();
		const bool bCrawl = Config->IsCrawling();
		LNPSurfaceCrawl::FParams CrawlParams;
		CrawlParams.BodyRadius = Config->CapsuleRadius;
		const float BodyExtent = FMath::Max(Params.CapsuleRadius, Params.CapsuleHalfHeight);
		// 잘라 쓴 구간이 이보다 짧으면 이동하지 않는다. 벽보행 구간은 원래 1~2m라 기준을 낮춘다.
		const float MinTravel = Params.CapsuleRadius * (bCrawl ? 0.5f : 1.f);
		const float WanderSpeed = 0.3f * (bFlying ? Config->FlightConfig.FlightSpeed : Config->MovementConfig.MoveSpeed);
		const int32 Mode = bFlying ? 1 : bCrawl ? 2 : 0;
		TArray<int32> Nodes;

		// 동적 지형은 시작 때의 sweep만으로 미래 자세를 보장할 수 없다. 지지면이 있는 slot만 정확성을 우선한다.
		auto IsDynamicRisk = [&](const FVector& At)
		{
			if (bAnyUnslottedSupport)
				return true;
			for (const int32 Slot : RiskSlots)
			{
				const FVector Local = Snapshot->Slots[Slot].WorldToSlotRotation.RotateVector(At - Params.GravityOrigin);
				if (Local.X >= 0.0 && Local.Y >= 0.0 && Local.Z >= 0.0)
					return true;
			}
			return false;
		};
		auto IsDynamicallyBlocked = [&](const FVector& From, const FVector& To)
		{
			const FVector Up = (Params.GravityOrigin - From).GetSafeNormal();
			FLNPWorldHit Hit;
			return bFlying
				? Collision.SweepSphereWorld(From, To, BodyExtent + Config->FlightConfig.Clearance,
					FLNPWorldQueryParams(ELNPWorldQueryClass::FlightSteering), Hit)
				: bCrawl
				? Collision.SweepSphereWorld(From, To, Params.CapsuleRadius, FLNPWorldQueryParams(ELNPWorldQueryClass::SurfaceCrawl), Hit)
				: Collision.SweepCapsuleWorld(From + Up * 2.f, To + Up * 2.f, FQuat::FindBetweenNormals(FVector::UpVector, Up),
					Params.CapsuleRadius, Params.CapsuleHalfHeight, FLNPWorldQueryParams(ELNPWorldQueryClass::DynamicSupportContact), Hit);
		};

		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			auto& Enemy = Enemies[I];
			auto& Data = Idle[I];
			auto& Segment = Data.WanderSegment;
			auto& Transform = Transforms[I].GetMutableTransform();
			const FMassEntityHandle Self = Ctx.GetEntity(I);
			const FVector Position = Transform.GetLocation();
			const bool bSafe = Enemy.Health > 0.f && !Enemy.bActivityWakeRequested
				&& Enemy.HitReactTimer <= 0.f && Enemy.FlinchTimeRemaining <= 0.f && !Enemy.bNeedsHomeCheck
				&& !Enemy.DynamicSupportContact.IsValid() && Actors[I].Get() == nullptr
				&& Targets[I].State == ELNPTargetingState::None && Velocities[I].Velocity.IsNearlyZero()
				&& Attacks[I].Phase == ELNPEntityAttackPhase::None && !Attacks[I].bAttackRequested
				&& (!Poise.IsValidIndex(I) || (Poise[I].Current <= 0.f && !Poise[I].bIsGroggy && Poise[I].ImmunityTimeRemaining <= 0.f));
			const bool bWander = Enemy.ActivityBand == ELNPEnemyActivityBand::Wander
				&& Config->CombatMode == ELNPEnemyCombatMode::PureEntity && Enemy.ParentLootPod.IsValid();
			if (!bWander || !bSafe || !Snapshot.IsValid()
				|| (Segment.bValid && !Segment.IsCurrent(Snapshot->Generation, Overlay.Get())))
			{
				Data.bWanderHoldApplied = false;
				Data.ConsecutiveWanderFailures = 0;
				if (Segment.bValid)
				{
					Segment = FLNPEnemyWanderSegment();
					Moves[I].Center = Position;
					Data.bNeedNewWanderTarget = true;
					Data.LastWanderTime = 0.0;
				}
				if (bWander)
				{
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=UnsafeOrRevision Safe=%d"), Self.Index, Enemy.ParentLootPod.Index, bSafe);
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
				}
				continue;
			}
			Data.bWanderHandled = true;
			if (bDiagnostics)
				++Diagnostics.Eligible;
			if (!Segment.bValid || Now >= Segment.StopUntil)
			{
				if (bDiagnostics)
				{
					Diagnostics.Invalid += !Segment.bValid ? 1 : 0;
					Diagnostics.Expired += Segment.bValid ? 1 : 0;
				}
				Data.bWanderHoldApplied = false;
				const TPair<int32, int32>& Range = PodRanges.FindChecked(Enemy.ParentLootPod);
				const TArrayView<FReservation> Group(Reservations.GetData() + Range.Key, Range.Value);
				const int32 SelfIndex = Algo::LowerBoundBy(Group, Self.Index, [](const FReservation& R) { return R.Entity.Index; });
				check(Group.IsValidIndex(SelfIndex) && Group[SelfIndex].Entity == Self);
				bool bInitiallyOverlapping = false;
				for (const FReservation& R : Group)
				{
					if (R.Entity != Self && FVector::DistSquared(Position, R.Start) < FMath::Square(R.Radius + BodyExtent))
					{
						bInitiallyOverlapping = true;
						break;
					}
				}
				if (bInitiallyOverlapping)
				{
					if (bDiagnostics)
						++Diagnostics.OverlapRejected;
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Overlap Flight=%d"), Self.Index, Enemy.ParentLootPod.Index, bFlying);
					Segment = FLNPEnemyWanderSegment();
					Data.bWanderHandled = false;
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
					Moves[I].Center = Position;
					continue;
				}
				Segment = FLNPEnemyWanderSegment();
				Segment.Start = Segment.Goal = Position;
				Segment.StartTime = Segment.ArrivalTime = Now;
				Segment.Generation = Snapshot->Generation;
				Segment.bValid = true;
				Segment.bCrawl = bCrawl;
				// 벽보행 Up은 엔티티 회전 Z(= 붙은 면의 법선)다. 구간 동안 회전 Z를 바꾸지 않는다.
				const FVector CrawlNormal = LNPMass::GetEntityUp(Transform);
				if (Reselections >= MaxReselectionsPerExecute)
				{
					// 이번 실행의 재선택 상한을 넘었다. 후보 검사 없이 제자리에 잠깐 서고, 다음 프레임들에 흩어져 다시 뽑는다.
					if (bDiagnostics)
						++Diagnostics.Deferred;
					Segment.StopUntil = Now + FMath::FRandRange(DeferredHoldMin, DeferredHoldMax);
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LNPWander_Reselect);
					++Reselections;
					const double Sector = 2.0 * PI / Group.Num();
					const double SectorStart = Sector * SelfIndex;
					const FVector Out = (Enemy.ParentPodLocation - Params.GravityOrigin).GetSafeNormal();
					const FVector Axis = FMath::Abs(Out.X) < 0.9 ? FVector::ForwardVector : FVector::RightVector;
					const FVector Tangent = FVector::CrossProduct(Out, Axis).GetSafeNormal();
					const FVector Other = FVector::CrossProduct(Out, Tangent).GetSafeNormal();
					// 같은 Pod의 현재 위치와 예약된 전체 선분을 함께 검사한다. 잘라 쓴 구간은 이 선분의 앞부분이라 겹치지 않는다.
					auto OverlapsReservation = [&](const FVector& Goal)
					{
						for (const FReservation& R : Group)
						{
							if (R.Entity == Self)
								continue;
							FVector A, B;
							FMath::SegmentDistToSegmentSafe(Position, Goal, R.Start, R.Goal, A, B);
							if (FVector::DistSquared(A, B) < FMath::Square(R.Radius + BodyExtent + 10.f))
								return true;
						}
						return false;
					};
					// 지나는 Nav cell의 Tile 리비전을 구간에 싣는다. 잘라 쓴 지상 구간은 원래 직선의 cell을 그대로 싣는다(보수적 무효화).
					auto FillTiles = [&]()
					{
						Segment.TileCount = 0;
						for (const int32 Node : Nodes)
						{
							const uint32 Tile = LNPNavGraph::GetTileKey(Snapshot->Nav, Node);
							bool bFound = false;
							for (uint8 T = 0; T < Segment.TileCount && !bFound; ++T)
								bFound = Segment.Tiles[T].Key == Tile;
							if (bFound)
								continue;
							if (Segment.TileCount == UE_ARRAY_COUNT(Segment.Tiles))
								return false;
							Segment.Tiles[Segment.TileCount++] = { Tile, LNPNavOverlay::GetTileRevision(Overlay.Get(), Tile) };
						}
						return true;
					};
					LNPNavGraph::FStartCache StartCache;
					bool bAccepted = false;
					bool bTruncated = false;
					for (int32 Attempt = 0; Attempt < MaxCandidateAttempts && !bAccepted; ++Attempt)
					{
						if (bDiagnostics)
						{
							++Diagnostics.CandidateAttempts;
							++Diagnostics.ModeAttempts[Mode];
						}
						const double Angle = SectorStart + Sector * FMath::FRandRange(0.15f, 0.85f);
						const float Distance = FMath::FRandRange(Config->MovementConfig.WanderMinDistance, Config->MovementConfig.WanderMaxDistance);
						FVector Goal = Enemy.ParentPodLocation + (Tangent * FMath::Cos(Angle) + Other * FMath::Sin(Angle)) * Distance;
						Nodes.Reset();
						Segment.TileCount = 0;
						EReject Reject = EReject::Count;
						bool bCut = false;
						if (bFlying)
						{
							Goal -= Out * FMath::FRandRange(Config->FlightConfig.IdleAltitudeMin, FMath::Max(Config->FlightConfig.IdleAltitudeMin, Config->FlightConfig.IdleAltitudeMax));
							FLNPWorldHit Hit;
							if (OverlapsReservation(Goal))
								Reject = EReject::Reservation;
							else if (Collision.SweepSphereWorld(Position, Goal, BodyExtent + Config->FlightConfig.Clearance,
								FLNPWorldQueryParams(ELNPWorldQueryClass::FlightSteering), Hit))
							{
								// 막히면 버리지 않고 hit 앞에서 몸 반지름만큼 남기고 선다. sweep이 지나온 길은 비어 있다.
								const double Reach = Hit.bStartPenetrating ? 0.0 : Hit.Distance - Params.CapsuleRadius;
								if (Reach < MinTravel)
									Reject = EReject::FlightSweep;
								else
								{
									Goal = Position + (Goal - Position).GetSafeNormal() * Reach;
									bCut = true;
								}
							}
						}
						else if (bCrawl)
						{
							// 부채꼴 배회점 쪽을 접평면에 눕혀 짧게 걷는다. 매 구간 배회점 쪽으로 가므로 Home 주변을 벗어나지 않는다.
							const FVector Toward = FVector::VectorPlaneProject(Goal - Position, CrawlNormal);
							if (Toward.IsNearlyZero())
								Reject = EReject::Crawl;
							else
							{
								const double MaxLength = 0.3 * Config->MovementConfig.MoveSpeed * CrawlTravelSeconds * FMath::FRandRange(0.5f, 1.f);
								Goal = Position + Toward.GetClampedToMaxSize(MaxLength);
								FVector Trimmed;
								if (OverlapsReservation(Goal))
									Reject = EReject::Reservation;
								else if (!LNPSurfaceCrawl::TrimWanderSegment(Collision, CrawlParams, Position, CrawlNormal, Goal, MinTravel, Trimmed))
									Reject = EReject::Crawl;
								else
								{
									bCut = !Trimmed.Equals(Goal, 0.01);
									Goal = Trimmed;
								}
							}
						}
						else
						{
							Goal = Params.GravityOrigin + (Goal - Params.GravityOrigin).GetSafeNormal() * (Position - Params.GravityOrigin).Size();
							FLNPSurfaceHandle Surface = Enemy.SurfaceHandle;
							if (!LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Goal, Surface, Goal))
								Reject = EReject::GoalSupport;
							else if (OverlapsReservation(Goal))
								Reject = EReject::Reservation;
							else
							{
								FLNPNavEndpoints Ends;
								{
									TRACE_CPUPROFILER_EVENT_SCOPE(LNPWander_Endpoints);
									FLNPNavEndpointQuery Query;
									Query.StartPosition = Position;
									Query.StartSurface = &Enemy.SurfaceHandle;
									Query.GoalPosition = Goal;
									Query.GoalSurface = &Surface;
									Query.ApproachRadius = 0.0;
									Ends = LNPNavGraph::ResolveEndpointsCached(Snapshot->Nav, Query, StartCache, Overlay.Get());
								}
								const FVector Direction = (Goal - Position).GetSafeNormal();
								const double FullDistance = FVector::Distance(Position, Goal);
								double Reach = FullDistance;
								if (Ends.Status != ELNPNavEndpointStatus::Reachable)
									Reject = EReject::Endpoint;
								else if (!LNPNavGraph::CollectDirectWalkNodes(Snapshot->Nav, Ends.StartNode, Ends.GoalNode, Nodes, Overlay.Get()))
								{
									// 직선 보행이 막혔다. 막히기 전 마지막 cell 앞까지만 간다. 지나온 cell이 둘 이상이어야 갈 자리가 있다.
									if (Nodes.Num() >= 2)
									{
										const int32 Last = Nodes.Last();
										const FVector LastPoint(Snapshot->Nav.Graph.GetWorldPoint(Snapshot->Nav.Graph.GetSlot(Last), Last));
										Reach = FMath::Min(Reach, FVector::DotProduct(LastPoint - Position, Direction) - Params.CapsuleRadius);
										bCut = true;
									}
									if (Nodes.Num() < 2 || Reach < MinTravel)
										Reject = EReject::DirectWalk;
								}
								if (Reject == EReject::Count && !FillTiles())
									Reject = EReject::TileOverflow;
								if (Reject == EReject::Count)
								{
									// 실제 보간 직선을 같은 Layer에 투영하고 정적 blocker를 확인한다. 실패하면 그 직전까지만 간다.
									TRACE_CPUPROFILER_EVENT_SCOPE(LNPWander_PathSample);
									const FVector End = FullDistance > UE_KINDA_SMALL_NUMBER ? Position + (Goal - Position) * (Reach / FullDistance) : Position;
									const int32 Steps = FMath::Max(1, FMath::CeilToInt(Reach / FMath::Max(Params.CapsuleRadius * 0.5f, 1.f)));
									FVector Swept = Position;
									FVector Confirmed = Position;
									FVector Pending = Position;
									bool bPending = false;
									EReject PathReject = EReject::Count;
									auto SweepClear = [&](const FVector& To)
									{
										FLNPWorldHit Hit;
										const FVector Up = (Params.GravityOrigin - Swept).GetSafeNormal();
										return !Collision.SweepCapsuleWorld(Swept + Up * 2.f, To + Up * 2.f,
											FQuat::FindBetweenNormals(FVector::UpVector, Up), Params.CapsuleRadius, Params.CapsuleHalfHeight,
											FLNPWorldQueryParams(ELNPWorldQueryClass::GroundRiskFallback), Hit);
									};
									for (int32 Step = 1; Step <= Steps; ++Step)
									{
										FVector Sample = FMath::Lerp(Position, End, static_cast<double>(Step) / Steps);
										FLNPSurfaceHandle SampleSurface = Enemy.SurfaceHandle;
										if (!LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Sample, SampleSurface, Sample)
											|| SampleSurface.OctantSlot != Enemy.SurfaceHandle.OctantSlot || SampleSurface.LocalLayerId != Enemy.SurfaceHandle.LocalLayerId)
										{
											PathReject = EReject::PathSurface;
											break;
										}
										Pending = Sample;
										bPending = true;
										if (Step % GroundSweepStride == 0 || Step == Steps)
										{
											bPending = false;
											if (!SweepClear(Sample))
											{
												PathReject = EReject::PathSweep;
												break;
											}
											Swept = Confirmed = Sample;
										}
									}
									// 표면이 끊긴 자리 앞의 아직 sweep하지 않은 표본까지 확인해 쓴다.
									if (bPending && SweepClear(Pending))
										Confirmed = Pending;
									if (FVector::Distance(Position, Confirmed) < MinTravel)
										Reject = PathReject == EReject::Count ? EReject::PathSurface : PathReject;
									else
									{
										bCut |= PathReject != EReject::Count;
										Goal = Confirmed;
									}
								}
							}
						}
						if (Reject != EReject::Count)
						{
							if (bDiagnostics)
								++Diagnostics.Rejects[static_cast<int32>(Reject)];
							Segment.TileCount = 0;
							continue;
						}
						Segment.Goal = Goal;
						Segment.ArrivalTime = Now + FVector::Distance(Position, Goal) / FMath::Max(WanderSpeed, 1.f);
						Segment.StopUntil = Segment.ArrivalTime + (bCrawl ? CrawlHoldSeconds : FMath::FRandRange(HoldSecondsMin, HoldSecondsMax));
						bAccepted = true;
						bTruncated = bCut;
					}
					if (bAccepted)
					{
						Data.ConsecutiveWanderFailures = 0;
					}
					else
					{
						// 실패가 이어질수록 오래 선다. 월드가 그대로면 같은 부채꼴은 다시 실패하기 쉽다.
						Data.ConsecutiveWanderFailures = static_cast<uint8>(FMath::Min<int32>(Data.ConsecutiveWanderFailures + 1, MAX_uint8));
						Segment.StopUntil = Now + FailureHold(Data.ConsecutiveWanderFailures, Self.Index);
					}
					if (bDiagnostics)
					{
						Diagnostics.Accepted += bAccepted ? 1 : 0;
						Diagnostics.Truncated += bAccepted && bTruncated ? 1 : 0;
						Diagnostics.Failed += bAccepted ? 0 : 1;
						Diagnostics.ModeFailed[Mode] += bAccepted ? 0 : 1;
					}
				}
				Group[SelfIndex].Goal = Segment.Goal;
				const auto Quantized = FLNPReplicatedWanderData::Encode(Segment, bFlying, Enemy.SurfaceHandle, bCrawl
					? LNP::Replication::EncodeReplicatedLocalYaw(Transform, LNP::Replication::EncodeOctNormal(CrawlNormal))
					: LNP::Replication::EncodeSphereLocalYaw(Transform));
				if (!Quantized.IsEnabled())
				{
					if (bDiagnostics)
						++Diagnostics.CodecRejected;
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Codec"), Self.Index, Enemy.ParentLootPod.Index);
					Segment = FLNPEnemyWanderSegment();
					Data.bWanderHandled = false;
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
					continue;
				}
				Quantized.ApplyTo(Segment, Now);
			}
			if (Data.bWanderHoldApplied && Data.WanderHoldSnapshot == Snapshot.Get())
			{
				// 정지 위치·회전·이동 목표는 이미 적용한 값 그대로다. 동적 지형 위험만 매 프레임 본다.
				if (IsDynamicRisk(Position) && IsDynamicallyBlocked(Position, Position))
				{
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=DynamicBlock"), Self.Index, Enemy.ParentLootPod.Index);
					Segment = FLNPEnemyWanderSegment();
					Data.bWanderHoldApplied = false;
					Data.bWanderHandled = false;
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
					Moves[I].Center = Position;
				}
				continue;
			}
			FVector Next = Segment.Evaluate(Now);
			if (!bFlying && !bCrawl && !LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Next, Enemy.SurfaceHandle, Next))
			{
				if (LNPLoadBaseline::IsActivityDiagnostics())
					UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Support"), Self.Index, Enemy.ParentLootPod.Index);
				Segment = FLNPEnemyWanderSegment();
				Data.bWanderHoldApplied = false;
				Data.bWanderHandled = false;
				Enemy.bActivityWakeRequested = true;
				Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
				continue;
			}
			if (IsDynamicRisk(Next) && IsDynamicallyBlocked(Position, Next))
			{
				if (LNPLoadBaseline::IsActivityDiagnostics())
					UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=DynamicBlock"), Self.Index, Enemy.ParentLootPod.Index);
				Segment = FLNPEnemyWanderSegment();
				Data.bWanderHoldApplied = false;
				Data.bWanderHandled = false;
				Enemy.bActivityWakeRequested = true;
				Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
				Moves[I].Center = Position;
				continue;
			}
			Transform.SetLocation(Next);
			const FVector Up = bCrawl ? LNPMass::GetEntityUp(Transform) : (Params.GravityOrigin - Next).GetSafeNormal();
			FVector Forward = FVector::VectorPlaneProject(Segment.Goal - Segment.Start, Up).GetSafeNormal();
			if (Forward.IsNearlyZero())
				Forward = FVector::VectorPlaneProject(Transform.GetRotation().GetForwardVector(), Up).GetSafeNormal();
			Transform.SetRotation(Segment.Start.Equals(Segment.Goal)
				? LNP::Replication::DecodeReplicatedRotation(Next, bCrawl ? LNP::Replication::EncodeOctNormal(Up) : 0, Segment.LocalYaw)
				: FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat());
			Moves[I].Center = Segment.Goal;
			Moves[I].DistanceToGoal = FVector::Distance(Next, Segment.Goal);
			Moves[I].DesiredSpeed = FMassInt16Real(Now < Segment.ArrivalTime ? WanderSpeed : 0.f);
			if (Now >= Segment.ArrivalTime)
			{
				// 도착 뒤의 위치·회전은 같은 스냅샷이면 다음 프레임에도 같다.
				Data.bWanderHoldApplied = true;
				Data.WanderHoldSnapshot = Snapshot.Get();
			}
		}
	});
	if (bDiagnostics)
	{
		// 프로세서 실행마다 초기화한 계수다. 후보 선택 결과와 인코딩 거부는 구분한다.
		const auto Rejects = [&Diagnostics](const EReject Reject) { return Diagnostics.Rejects[static_cast<int32>(Reject)]; };
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_WorldSeconds, Now, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Eligible, Diagnostics.Eligible, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Invalid, Diagnostics.Invalid, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Expired, Diagnostics.Expired, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_OverlapRejected, Diagnostics.OverlapRejected, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_CandidateAttempts, Diagnostics.CandidateAttempts, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Accepted, Diagnostics.Accepted, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Failed, Diagnostics.Failed, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_CodecRejected, Diagnostics.CodecRejected, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Deferred, Diagnostics.Deferred, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_Truncated, Diagnostics.Truncated, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_GroundAttempts, Diagnostics.ModeAttempts[0], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_FlightAttempts, Diagnostics.ModeAttempts[1], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_CrawlAttempts, Diagnostics.ModeAttempts[2], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_GroundFailed, Diagnostics.ModeFailed[0], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_FlightFailed, Diagnostics.ModeFailed[1], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_CrawlFailed, Diagnostics.ModeFailed[2], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectGoalSupport, Rejects(EReject::GoalSupport), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectEndpoint, Rejects(EReject::Endpoint), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectDirectWalk, Rejects(EReject::DirectWalk), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectPathSurface, Rejects(EReject::PathSurface), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectPathSweep, Rejects(EReject::PathSweep), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectFlightSweep, Rejects(EReject::FlightSweep), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectCrawl, Rejects(EReject::Crawl), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectReservation, Rejects(EReject::Reservation), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPWander_RejectTileOverflow, Rejects(EReject::TileOverflow), ECsvCustomStatOp::Set);
	}
}

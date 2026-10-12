// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyCrawlMovementProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "Enemy/LNPEntityAttackShared.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "HAL/IConsoleManager.h"
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

namespace
{
	std::atomic<uint64> GStatDetourEntries{0};
	std::atomic<uint64> GStatSurfaceLost{0};
	std::atomic<uint64> GStatLandings{0};
	std::atomic<uint64> GStatRehomes{0};
	std::atomic<uint64> GStatCrushes{0};

	/** 정지 개체도 제자리 겹침을 검사하는 패널 중심 반경(cm). 패널 크기와 운반 경로를 덮을 만큼 넉넉히 둔다. */
	constexpr double StationaryCrushCheckRadius = 1500.0;

	int32 GCrawlEngageLog = 0;
	FAutoConsoleVariableRef CVarCrawlEngageLog(
		TEXT("LNP.SurfaceNav.CrawlEngageLog"), GCrawlEngageLog,
		TEXT("Server-only diagnostic. 1 = log every engaged wall crawler's chase state twice a second ([CrawlEngage])."));

	/** 교전 진단 로그를 찍을 프레임인가(0.5초 간격). 게임 스레드의 Execute에서 한 번 정한다. */
	double GCrawlEngageLogLastTime = -1.0;
}

LNPSurfaceCrawlStats::FCounts LNPSurfaceCrawlStats::Get()
{
	FCounts Counts;
	Counts.DetourEntries = GStatDetourEntries.load(std::memory_order_relaxed);
	Counts.SurfaceLost = GStatSurfaceLost.load(std::memory_order_relaxed);
	Counts.Landings = GStatLandings.load(std::memory_order_relaxed);
	Counts.Rehomes = GStatRehomes.load(std::memory_order_relaxed);
	Counts.Crushes = GStatCrushes.load(std::memory_order_relaxed);
	return Counts;
}

void LNPSurfaceCrawlStats::Reset()
{
	GStatDetourEntries.store(0, std::memory_order_relaxed);
	GStatSurfaceLost.store(0, std::memory_order_relaxed);
	GStatLandings.store(0, std::memory_order_relaxed);
	GStatRehomes.store(0, std::memory_order_relaxed);
	GStatCrushes.store(0, std::memory_order_relaxed);
}

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
	// 움직이는 패널 운반(D-013). 패널 자세는 이 페이즈보다 먼저 게시된다(D-050).
	ProcessorRequirements.AddSubsystemRequirement<ULNPDynamicTerrainSubsystem>(EMassFragmentAccess::ReadOnly);
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
	const TSharedRef<const FLNPDynamicSupportFrame, ESPMode::ThreadSafe> DynamicFrame =
		Context.GetSubsystemChecked<ULNPDynamicTerrainSubsystem>().GetDynamicSupportFrame();

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
	const bool bEngageLogFrame = GCrawlEngageLog != 0 && (Now - GCrawlEngageLogLastTime >= 0.5 || Now < GCrawlEngageLogLastTime);
	if (bEngageLogFrame)
		GCrawlEngageLogLastTime = Now;

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

		// 측정 누계는 청크 끝에 한 번만 더한다.
		LNPSurfaceCrawlStats::FCounts ChunkStats;
		ON_SCOPE_EXIT
		{
			GStatDetourEntries.fetch_add(ChunkStats.DetourEntries, std::memory_order_relaxed);
			GStatSurfaceLost.fetch_add(ChunkStats.SurfaceLost, std::memory_order_relaxed);
			GStatLandings.fetch_add(ChunkStats.Landings, std::memory_order_relaxed);
			GStatRehomes.fetch_add(ChunkStats.Rehomes, std::memory_order_relaxed);
			GStatCrushes.fetch_add(ChunkStats.Crushes, std::memory_order_relaxed);
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
		LNPSurfaceCrawl::FChaseDetourParams ChaseDetourParams;
		ChaseDetourParams.BodyRadius = Config->CapsuleRadius;
		ChaseDetourParams.Progress.ProgressMargin = Config->CapsuleRadius * 2.f;
		// 갈래가 타겟을 놓치기 전에 끝낸다. 놓치면 비교전 프레임이 시도 기억을 지운다.
		ChaseDetourParams.MaxTargetDistance = Config->TargetingConfig.AlertRetentionDistance * 0.9f;
		LNPSurfaceCrawl::FHomeApproachParams HomeParams;
		HomeParams.FarDistance = MoveConfig.WanderMaxDistance + Config->CapsuleRadius;
		HomeParams.ProgressMargin = Config->CapsuleRadius;

		// 낙하는 지상 공중 분기와 같은 함수를 쓴다. 몸이 구라 반높이도 반지름이다(캡슐 = 구).
		LNPEnemyExactMovement::FParams FallParams;
		FallParams.GravityOrigin = GravityOrigin;
		FallParams.GravityStrength = MoveConfig.GravityStrength;
		FallParams.CapsuleRadius = Config->CapsuleRadius;
		FallParams.CapsuleHalfHeight = Config->CapsuleRadius;
		// D-066: 넉백 낙하는 걸을 수 없는 Blocker 면에도 착지해 붙는다. 그러지 않으면 프랍 둘 사이 골에 끼어 바닥에 닿지 못하고
		// 두 경사면 사이에서 겹침 풀기를 오가며 공중에 남는다(Phase 14 단위 6b 실측).
		FallParams.bLandOnAnyKnownSolid = true;

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

			// 착지하면 그 자리 표면에 다시 붙는다. Up은 붙을 면의 바깥 방향(착지 면 법선, 스폰은 구 중심 방향)이다.
			// 못 찾으면 Up을 법선으로 두고 다음 이동의 probe가 정한다. 붙은 면이 움직이는 패널이면 운반 접촉을 만든다.
			auto AttachHere = [&](const FVector& At, const FVector& Up)
			{
				FVector Attached, AttachedNormal;
				FLNPExactHitIdentity Identity;
				if (LNPSurfaceCrawl::Attach(WorldCollision, CrawlParams, At, -Up, AttachReach, Attached, AttachedNormal, &Identity))
				{
					CrawlData.SurfaceNormal = AttachedNormal;
					LNPEnemySurfaceMovement::MakeDynamicSupportContact(Identity, *DynamicFrame, Attached, EnemyData.DynamicSupportContact);
					return Attached;
				}
				// 착지 hit 자리가 볼록 hull 안쪽으로 조금 들어가 probe가 시작 겹침으로 실패할 수 있다(단위 6b 실측 0.13cm).
				// 겹침을 풀고 한 번 더 붙는다. 정지한 개체는 query를 하지 않아 겹침이 그대로 남기 때문이다.
				// 풀지 못하면 제자리 — 마지막으로 민 자리는 다른 면 안일 수 있다.
				FVector Free;
				if (!LNPSurfaceCrawl::Depenetrate(WorldCollision, CrawlParams, At, Free, Up, nullptr, Up))
					Free = At;
				if (!Free.Equals(At) && LNPSurfaceCrawl::Attach(WorldCollision, CrawlParams, Free, -Up, AttachReach, Attached, AttachedNormal, &Identity))
				{
					CrawlData.SurfaceNormal = AttachedNormal;
					LNPEnemySurfaceMovement::MakeDynamicSupportContact(Identity, *DynamicFrame, Attached, EnemyData.DynamicSupportContact);
					return Attached;
				}
				CrawlData.SurfaceNormal = Up;
				return Free;
			};

			if (bChunkIsDying)
			{
				// 붙어 있던 표면을 놓고 착지할 때까지 떨어진다. 사망 팝이 0이어도 중력이 적분되므로 천장에 남지 않는다.
				// 착지 때 속도가 0이 되므로, 착지 뒤 속도가 생겼다면 시체가 넉백을 받은 것이다 — 다시 떨어진다.
				if (CrawlData.bDeathLanded && !PhysVelocity.IsNearlyZero())
					CrawlData.bDeathLanded = false;
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

			// 움직이는 패널 위(밑면·옆면 포함)에 붙어 있으면 먼저 이번 프레임 패널 자세로 옮긴다(D-013, 지상과 같은 로컬 접촉).
			// 패널이 snapshot에서 사라지거나 비보행이 되면 마지막 패널 속도를 실어 공중으로 넘긴다.
			if (PhysVelocity.IsNearlyZero() && EnemyData.DynamicSupportContact.IsValid())
			{
				const FLNPDynamicSupportSnapshot* Panel = DynamicFrame->Find(EnemyData.DynamicSupportContact.SupportId);
				const uint32 PanelActorId = Panel ? Panel->ActorId : 0;
				if (LNPSurfaceCrawl::CarryOnDynamicSupport(*DynamicFrame, EnemyData.DynamicSupportContact, EntityTransform, CrawlData.SurfaceNormal))
				{
					// 운반 경로를 패널을 뺀 지형으로 검사한다. 밑면·옆면에 붙은 개체는 패널이 지면·벽 쪽으로 가면 그 안으로
					// 끌려 들어간다(윗면만 타는 지상 적에는 없는 경우, 단위 6e 패키지 실측 2~4cm). 막히면 그 앞에 서고 운반을 끊는다 —
					// 패널이 계속 밀고 오면 겹침 해소가 맡는다(풀지 못하면 제자리, 압사는 백로그).
					FLNPWorldQueryParams CarryQuery(ELNPWorldQueryClass::SurfaceCrawl);
					if (PanelActorId != 0)
						CarryQuery.IgnoredActorIds.Add(PanelActorId);
					FLNPWorldHit CarryHit;
					const FVector Carried = EntityTransform.GetLocation();
					if (!Carried.Equals(Location, 0.01)
						&& WorldCollision.SweepSphereWorld(Location, Carried, CrawlParams.BodyRadius, CarryQuery, CarryHit))
					{
						EnemyData.DynamicSupportContact.Reset();
						const FVector Back = (Location - Carried).GetSafeNormal();
						EntityTransform.SetLocation(CarryHit.bStartPenetrating ? Location
							: CarryHit.Location + Back * FMath::Min(CrawlParams.SurfaceGap, CarryHit.Distance));
					}
					Location = EntityTransform.GetLocation();
				}
				else
				{
					PhysVelocity = EnemyData.DynamicSupportContact.LastLinearVelocity;
					EnemyData.DynamicSupportContact.Reset();
					if (PhysVelocity.IsNearlyZero())
						PhysVelocity = -GravityUp * MoveConfig.GravityStrength * DeltaTime;
				}
			}

			// 넉백·표면 상실: 지상과 같은 공중 분기. 스스로 점프하지 않으므로 이 속도의 생산자는 외부(넉백)와 표면 상실뿐이다.
			if (!PhysVelocity.IsNearlyZero())
			{
				LNPSurfaceCrawl::ResetDetour(CrawlData.Detour);
				// 넉백 중 갈래는 끝내고 착지 뒤 다시 잰다. 시도·대기 기억은 유지한다(Phase 14b 설계 5).
				LNPSurfaceCrawl::InterruptChaseDetour(CrawlData.ChaseDetour);
				EnemyData.DynamicSupportContact.Reset();
				FVector NewLocation;
				FVector LandingNormal = (GravityOrigin - Location).GetSafeNormal();
				if (bEngageLogFrame && TargetingFragments[i].State == ELNPTargetingState::Confirmed)
				{
					UE_LOG(LogLootNPop, Log, TEXT("[CrawlEngage] enemy=%d airborne speed=%.0f dist=%.0f"), Ctx.GetEntity(i).Index,
						PhysVelocity.Size(), FMath::Sqrt(TargetingFragments[i].DistanceToTargetSq));
				}
				if (LNPEnemyExactMovement::StepAirborne(WorldCollision, FallParams, Location, PhysVelocity, DeltaTime, NewLocation,
					nullptr, &LandingNormal))
				{
					++ChunkStats.Landings;
					NewLocation = AttachHere(NewLocation, LandingNormal);
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
				Location = AttachHere(Location, (GravityOrigin - Location).GetSafeNormal());
				EntityTransform.SetLocation(Location);
			}
			const FVector Normal = CrawlData.SurfaceNormal;

			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			FVector Goal = Location;
			float Speed = 0.f;
			FVector FacingDir = FVector::ZeroVector;   // 월드 방향. 접평면 성분만 몸의 전방이 된다.
			bool bSeekLineOfSight = false;
			bool bInRange = false;
			const bool bWasHoldingFirePosition = CrawlData.bHoldingFirePosition;
			CrawlData.bHoldingFirePosition = false;

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
					// 대개 사선이 트인다. 이때 조향은 내리막 대신 오르막을 더한다(SteerDirection) — 건너편 절벽 위 타겟은
					// 틈 아래가 아니라 이쪽 윗면 가장자리에서 보인다. 계속 막히면 아래 단순 우회가 끼어든다. 플래그는 소비하지 않는다:
					// 공격 프로세서가 0.25초마다 다시 보고 트이면 0으로 되돌린다.
					FacingDir = Targeting.TargetLocation - Location;
					const float Distance = FMath::Sqrt(Targeting.DistanceToTargetSq);
					bInRange = Distance <= MoveConfig.AttackRange;
					bSeekLineOfSight = bInRange && AttackFragments[i].bLineOfSightBlocked;
					// 사선 탐색 중에는 매 프레임 직접 보고 처음 트인 자리에서 선다. 게이트의 0.25초 재검사만 기다리면 그 사이 윗면
					// 가장자리를 넘었다 되돌아오며 한 점(타겟의 수선 발)으로 모인다(PIE 2026-10-10). 플래그는 게이트가 다음 검사에서 내린다.
					const bool bFoundFirePosition = bSeekLineOfSight && LNPEntityAttack::HasClearShotFrom(WorldCollision,
						EntityTransform, GravityOrigin, Targeting.TargetLocation, Config->EntityAttackConfig);
					bSeekLineOfSight &= !bFoundFirePosition;
					// 사거리 안에서 공격을 시작했으면 그 자리를 지킨다. 정지 거리까지 마저 다가가면 윗면 가장자리를 넘어 벽으로 내려가
					// 사선을 잃는다(같은 높이 절벽 사이 PIE 2026-10-10).
					CrawlData.bHoldingFirePosition = bInRange && !bSeekLineOfSight
						&& (bWasHoldingFirePosition || bFoundFirePosition || AttackFragments[i].Phase != ELNPEntityAttackPhase::None);
					// StateTree는 신호로만 틱한다. 정지 거리 안은 TargetFollow가 신호하지만, 그 밖의 사거리 안에서도 공격 Task가
					// 돌아야 요청·LoS 재검사가 이어진다 — 없으면 사거리 안 윗면에서 쏘지 않고 가장자리를 넘는다.
					if (bInRange && Distance > StopDistance)
						ChunkEntitiesToSignal.Add(Ctx.GetEntity(i));
					if ((Distance > StopDistance && !CrawlData.bHoldingFirePosition) || bSeekLineOfSight)
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
						++ChunkStats.Rehomes;
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
			// 교전 추격은 이 면에서 사거리 안에 들 수 있으면 내리막을 더하지 않는다(SteerDirection의 ReachDistance).
			// 문턱은 정지 거리가 아니라 사거리다 — 면까지 법선 거리가 둘 사이(752, PIE 로그)면 정지 거리 문턱은 내리막을 남겨
			// 사거리 밖 평형점에 모인다. 사거리 안에 들면 공격이 시작되고 자리 지키기가 멈춰 세운다.
			// 사선 탐색의 오르막은 면 앞쪽 타겟 전부에 적용한다.
			const bool bChaseApproach = Targeting.State == ELNPTargetingState::Confirmed && !bSeekLineOfSight;
			const float ReachDistance = bChaseApproach ? FMath::Max(CrawlParams.BodyRadius, MoveConfig.AttackRange) : CrawlParams.BodyRadius;
			const int8 FallbackSide = (Ctx.GetEntity(i).Index & 1) ? 1 : -1;
			FVector MoveDir = LNPSurfaceCrawl::SteerDirection(Location, Normal, Goal, GravityUp, ReachDistance, bSeekLineOfSight);
			// 사거리 밖 교전 추격은 진행 기반 고정 방향 우회(Phase 14b)가 조향을 정한다. 위치 기준 우회(국소 장애물)는 그것이 일반 추격을
			// 돌려주고 타겟이 면 앞쪽으로 ReachDistance 안일 때(내리막 꺼짐)만 덧붙인다 — 내리막이 켜진 구간의 막힘은 평형점이다.
			// 그로기·공격 잔여 등으로 이번 프레임 멈췄으면 계측을 잠시 세운다(시도·대기 기억 유지).
			const bool bOutOfRangeChase = bChaseApproach && !bInRange;
			LNPSurfaceCrawl::EChaseDetourMode ChaseMode = LNPSurfaceCrawl::EChaseDetourMode::Chase;
			bool bLocalDetour = Targeting.State == ELNPTargetingState::Confirmed && Speed > 0.f;
			if (bOutOfRangeChase && Speed > 0.f)
			{
				MoveDir = LNPSurfaceCrawl::UpdateChaseDetour(CrawlData.ChaseDetour, ChaseDetourParams, Location, Normal, Goal, GravityUp,
					ReachDistance, FallbackSide, DeltaTime, ChaseMode);
				bLocalDetour = ChaseMode == LNPSurfaceCrawl::EChaseDetourMode::Chase
					&& FVector::DotProduct(Goal - Location, Normal) <= ReachDistance;
			}
			else if (!bOutOfRangeChase)
			{
				LNPSurfaceCrawl::ResetChaseDetour(CrawlData.ChaseDetour);
			}
			if (bLocalDetour)
			{
				const bool bWasDetouring = CrawlData.Detour.DetourTime > 0.f;
				MoveDir = LNPSurfaceCrawl::UpdateDetour(CrawlData.Detour, DetourParams, Location, Normal, MoveDir, Goal - Location,
					GravityUp, EntityTransform.GetRotation().GetRightVector(), FallbackSide, DeltaTime);
				ChunkStats.DetourEntries += !bWasDetouring && CrawlData.Detour.DetourTime > 0.f ? 1 : 0;
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
			if (bEngageLogFrame && Targeting.State == ELNPTargetingState::Confirmed)
			{
				// 법선 거리(along)·타겟 높이(height)·면 기울기(normalUp)가 조향을 정한다. 위치는 원점 기준 반지름(r)으로 본다.
				const FVector ToTarget = Targeting.TargetLocation - Location;
				UE_LOG(LogLootNPop, Log, TEXT("[CrawlEngage] enemy=%d dist=%.0f range=%.0f stop=%.0f along=%.0f height=%.0f normalUp=%.2f blocked=%d phase=%d cd=%.2f hold=%d seek=%d speed=%.0f sep=%.0f moveUp=%.2f detour=%d r=%.0f signal=%d"),
					Ctx.GetEntity(i).Index, FMath::Sqrt(Targeting.DistanceToTargetSq), MoveConfig.AttackRange, StopDistance,
					FVector::DotProduct(ToTarget, Normal), FVector::DotProduct(ToTarget, GravityUp), FVector::DotProduct(Normal, GravityUp),
					static_cast<int32>(AttackFragments[i].bLineOfSightBlocked), static_cast<int32>(AttackFragments[i].Phase), AttackFragments[i].CooldownRemaining,
					static_cast<int32>(CrawlData.bHoldingFirePosition), static_cast<int32>(bSeekLineOfSight), Speed, SeparationPush.Size(), FVector::DotProduct(MoveDir, GravityUp),
					static_cast<int32>(CrawlData.Detour.DetourTime > 0.f), FVector::Dist(Location, GravityOrigin),
					static_cast<int32>(ChunkEntitiesToSignal.Contains(Ctx.GetEntity(i))));
				// 진행 기반 우회(Phase 14b): 모드(0 추격·1 갈래·2 대기)·갈래(1 옆·2 아래·3 위)·옆 부호·진행 최솟값·갇힌 거리·남은 예산.
				const LNPSurfaceCrawl::FChaseDetourState& Chase = CrawlData.ChaseDetour;
				UE_LOG(LogLootNPop, Log, TEXT("[CrawlEngage] enemy=%d chase=%d mode=%d branch=%d side=%d best=%.0f stall=%.1f attempt=%d stuck=%.0f tried=%d attemptLeft=%.1f branchLeft=%.0f wait=%.1f"),
					Ctx.GetEntity(i).Index, static_cast<int32>(bOutOfRangeChase), static_cast<int32>(ChaseMode), static_cast<int32>(Chase.Branch),
					Chase.Side, Chase.Progress.BestDistance, Chase.Progress.StallTime, static_cast<int32>(Chase.bAttempt), Chase.StuckDistance,
					Chase.TriedMask, Chase.bAttempt ? ChaseDetourParams.TotalBudget - Chase.AttemptTime : 0.f,
					Chase.Branch != LNPSurfaceCrawl::EChaseBranch::None ? ChaseDetourParams.BranchBudget - Chase.BranchTravel : 0.f,
					Chase.bWaiting ? Chase.WaitTime : 0.f);
			}
			FLNPExactHitIdentity SurfaceIdentity;
			// 정지 개체는 Step이 query를 하지 않는다. 패널 근처에서만 내려오는 패널에 깔렸는지 제자리 겹침을 본다.
			bool bNearPanel = false;
			if (Velocity.IsNearlyZero() && !EnemyData.DynamicSupportContact.IsValid())
			{
				for (const FLNPDynamicSupportSnapshot& Support : DynamicFrame->Supports)
				{
					bNearPanel |= FVector::DistSquared(Support.CurrentTransform.GetLocation(), Location) < FMath::Square(StationaryCrushCheckRadius);
				}
			}
			const LNPSurfaceCrawl::EStepResult StepResult = bNearPanel
				? LNPSurfaceCrawl::ResolveStationaryOverlap(WorldCollision, CrawlParams, Location, Normal, NewLocation)
				: LNPSurfaceCrawl::Step(WorldCollision, CrawlParams, Location, Normal, Velocity, DeltaTime, NewLocation, NewNormal, &SurfaceIdentity);
			// 압사(사용자 결정 2026-10-06): 움직이는 지형과 다른 면 사이에 끼어 겹침을 풀 수 없다. 체력을 0으로 만들어 기존 사망 경로
			// (ULNPHealthProcessor → Dying 태그·사망 낙하·복제)를 탄다. 지상·비행 압사는 Idea_Backlog 후보다.
			if (StepResult == LNPSurfaceCrawl::EStepResult::Crushed)
			{
				++ChunkStats.Crushes;
				EnemyData.Health = 0.f;
				UE_LOG(LogLootNPop, Log, TEXT("[SurfaceCrawlCrush] enemy=%d at %s"), Ctx.GetEntity(i).Index, *Location.ToString());
			}
			// 새로 붙은 면이 있으면 운반 접촉을 그 면으로 바꾼다(정적 면이면 비운다). 새 면이 없는 프레임(정지·막힘)은 유지한다.
			if (StepResult != LNPSurfaceCrawl::EStepResult::Lost && SurfaceIdentity.IsKnown())
			{
				LNPEnemySurfaceMovement::MakeDynamicSupportContact(SurfaceIdentity, *DynamicFrame, NewLocation, EnemyData.DynamicSupportContact);
			}
			if (StepResult == LNPSurfaceCrawl::EStepResult::Lost)
			{
				++ChunkStats.SurfaceLost;
				EnemyData.DynamicSupportContact.Reset();
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

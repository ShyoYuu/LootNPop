// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyWanderProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
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
#include "LNPMassUtils.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "LootNPop.h"

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
	const auto Snapshot = Context.GetSubsystemChecked<ULNPSurfaceDataSubsystem>().TakeSnapshot();
	const auto Overlay = Context.GetSubsystemChecked<ULNPNavPathSubsystem>().TakeOverlay();
	const auto& Collision = Context.GetSubsystemChecked<ULNPMassWorldCollisionSubsystem>();
	const auto DynamicFrame = Context.GetSubsystemChecked<ULNPDynamicTerrainSubsystem>().GetDynamicSupportFrame();
	struct FReservation
	{
		FMassEntityHandle Entity;
		FVector Start;
		FVector Goal;
		float Radius;
	};
	TMap<FMassEntityHandle, TArray<FReservation>> Reservations;
	TMap<FMassEntityHandle, TArray<FMassEntityHandle>> Members;
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
			const FMassEntityHandle Entity = Ctx.GetEntity(I);
			Members.FindOrAdd(Enemies[I].ParentLootPod).Add(Entity);
			const auto& Segment = Idle[I].WanderSegment;
			const FVector Position = Transforms[I].GetTransform().GetLocation();
			Reservations.FindOrAdd(Enemies[I].ParentLootPod).Add({ Entity, Position,
				Segment.bValid ? Segment.Goal : Position, FMath::Max(Config->CapsuleHalfHeight, Config->CapsuleRadius) });
		}
	});
	for (auto& Pair : Members)
		Pair.Value.Sort([](const FMassEntityHandle& A, const FMassEntityHandle& B) { return A.Index < B.Index; });

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
		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			auto& Enemy = Enemies[I];
			auto& Data = Idle[I];
			auto& Segment = Data.WanderSegment;
			auto& Transform = Transforms[I].GetMutableTransform();
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
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=UnsafeOrRevision Safe=%d"), Ctx.GetEntity(I).Index, Enemy.ParentLootPod.Index, bSafe);
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
				}
				continue;
			}
			Data.bWanderHandled = true;
			if (!Segment.bValid || Now >= Segment.StopUntil)
			{
				bool bInitiallyOverlapping = false;
				for (const auto& R : Reservations.FindChecked(Enemy.ParentLootPod))
					if (R.Entity != Ctx.GetEntity(I))
						bInitiallyOverlapping |= FVector::DistSquared(Position, R.Start)
							< FMath::Square(R.Radius + FMath::Max(Params.CapsuleRadius, Params.CapsuleHalfHeight));
				if (bInitiallyOverlapping)
				{
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Overlap Flight=%d"), Ctx.GetEntity(I).Index, Enemy.ParentLootPod.Index, Config->IsFlying());
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
				Segment.StopUntil = Now + 3.0;
				Segment.Generation = Snapshot->Generation;
				Segment.bValid = true;
				const auto& Group = Members.FindChecked(Enemy.ParentLootPod);
				const double Sector = 2.0 * PI / Group.Num();
				const double SectorStart = Sector * Group.IndexOfByKey(Ctx.GetEntity(I));
				const FVector Out = (Enemy.ParentPodLocation - Params.GravityOrigin).GetSafeNormal();
				const FVector Axis = FMath::Abs(Out.X) < 0.9 ? FVector::ForwardVector : FVector::RightVector;
				const FVector Tangent = FVector::CrossProduct(Out, Axis).GetSafeNormal();
				const FVector Other = FVector::CrossProduct(Out, Tangent).GetSafeNormal();
				for (int32 Attempt = 0; Attempt < 4; ++Attempt)
				{
					const double Angle = SectorStart + Sector * FMath::FRandRange(0.15f, 0.85f);
					const float Distance = FMath::FRandRange(Config->MovementConfig.WanderMinDistance, Config->MovementConfig.WanderMaxDistance);
					FVector Goal = Enemy.ParentPodLocation + (Tangent * FMath::Cos(Angle) + Other * FMath::Sin(Angle)) * Distance;
					TArray<int32> Nodes;
					if (Config->IsFlying())
					{
						Goal -= Out * FMath::FRandRange(Config->FlightConfig.IdleAltitudeMin, FMath::Max(Config->FlightConfig.IdleAltitudeMin, Config->FlightConfig.IdleAltitudeMax));
						FLNPWorldHit Hit;
						if (Collision.SweepSphereWorld(Position, Goal, FMath::Max(Params.CapsuleRadius, Params.CapsuleHalfHeight) + Config->FlightConfig.Clearance,
							FLNPWorldQueryParams(ELNPWorldQueryClass::FlightSteering), Hit))
							continue;
					}
					else
					{
						Goal = Params.GravityOrigin + (Goal - Params.GravityOrigin).GetSafeNormal() * (Position - Params.GravityOrigin).Size();
						FLNPSurfaceHandle Surface = Enemy.SurfaceHandle;
						if (!LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Goal, Surface, Goal))
							continue;
						FLNPNavEndpointQuery Query;
						Query.StartPosition = Position;
						Query.StartSurface = &Enemy.SurfaceHandle;
						Query.GoalPosition = Goal;
						Query.GoalSurface = &Surface;
						Query.ApproachRadius = 0.0;
						const auto Ends = LNPNavGraph::ResolveEndpoints(Snapshot->Nav, Query, Overlay.Get());
						if (Ends.Status != ELNPNavEndpointStatus::Reachable
							|| !LNPNavGraph::CollectDirectWalkNodes(Snapshot->Nav, Ends.StartNode, Ends.GoalNode, Nodes, Overlay.Get()))
							continue;
						// 실제 보간 곡선 전체를 안전한 같은 Layer에 투영하고 정적 blocker를 확인한다.
						FVector Previous = Position;
						bool bClear = true;
						const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Distance(Position, Goal) / FMath::Max(Params.CapsuleRadius * 0.5f, 1.f)));
						for (int32 Step = 1; Step <= Steps && bClear; ++Step)
						{
							FVector Sample = FMath::Lerp(Position, Goal, static_cast<double>(Step) / Steps);
							Surface = Enemy.SurfaceHandle;
							bClear = LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Sample, Surface, Sample)
								&& Surface.OctantSlot == Enemy.SurfaceHandle.OctantSlot && Surface.LocalLayerId == Enemy.SurfaceHandle.LocalLayerId;
							FLNPWorldHit Hit;
							const FVector Up = (Params.GravityOrigin - Previous).GetSafeNormal();
							bClear &= !Collision.SweepCapsuleWorld(Previous + Up * 2.f, Sample + Up * 2.f,
								FQuat::FindBetweenNormals(FVector::UpVector, Up), Params.CapsuleRadius, Params.CapsuleHalfHeight,
								FLNPWorldQueryParams(ELNPWorldQueryClass::GroundRiskFallback), Hit);
							Previous = Sample;
						}
						if (!bClear)
							continue;
					}
					// 기존 위치와 예약된 전체 선분을 함께 검사한다. 교차 구간은 다음 추첨까지 기다린다.
					bool bOverlap = false;
					for (const auto& R : Reservations.FindChecked(Enemy.ParentLootPod))
					{
						if (R.Entity == Ctx.GetEntity(I))
							continue;
						FVector A, B;
						FMath::SegmentDistToSegmentSafe(Position, Goal, R.Start, R.Goal, A, B);
						bOverlap |= FVector::DistSquared(A, B) < FMath::Square(R.Radius + FMath::Max(Params.CapsuleHalfHeight, Params.CapsuleRadius) + 10.f);
					}
					if (bOverlap)
						continue;
					Segment.TileCount = 0;
					bool bTileOverflow = false;
					for (const int32 Node : Nodes)
					{
						const uint32 Tile = LNPNavGraph::GetTileKey(Snapshot->Nav, Node);
						bool bFound = false;
						for (uint8 T = 0; T < Segment.TileCount; ++T)
							bFound |= Segment.Tiles[T].Key == Tile;
						if (bFound)
							continue;
						if (Segment.TileCount == UE_ARRAY_COUNT(Segment.Tiles))
						{
							bTileOverflow = true;
							break;
						}
						Segment.Tiles[Segment.TileCount++] = { Tile, LNPNavOverlay::GetTileRevision(Overlay.Get(), Tile) };
					}
					if (bTileOverflow)
					{
						Segment.TileCount = 0;
						continue;
					}
					Segment.Goal = Goal;
					const float Speed = 0.3f * (Config->IsFlying() ? Config->FlightConfig.FlightSpeed : Config->MovementConfig.MoveSpeed);
					Segment.ArrivalTime = Now + FVector::Distance(Position, Goal) / FMath::Max(Speed, 1.f);
					Segment.StopUntil = Segment.ArrivalTime + 3.0;
					break;
				}
				for (auto& R : Reservations.FindChecked(Enemy.ParentLootPod))
					if (R.Entity == Ctx.GetEntity(I))
						R.Goal = Segment.Goal;
				const auto Quantized = FLNPReplicatedWanderData::Encode(Segment, Config->IsFlying(), Enemy.SurfaceHandle,
					LNP::Replication::EncodeSphereLocalYaw(Transform));
				if (!Quantized.IsEnabled())
				{
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Codec"), Ctx.GetEntity(I).Index, Enemy.ParentLootPod.Index);
					Segment = FLNPEnemyWanderSegment();
					Data.bWanderHandled = false;
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
					continue;
				}
				Quantized.ApplyTo(Segment, Now);
			}
			FVector Next = Segment.Evaluate(Now);
			if (!Config->IsFlying() && !LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Next, Enemy.SurfaceHandle, Next))
			{
				if (LNPLoadBaseline::IsActivityDiagnostics())
					UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=Support"), Ctx.GetEntity(I).Index, Enemy.ParentLootPod.Index);
				Segment = FLNPEnemyWanderSegment();
				Data.bWanderHandled = false;
				Enemy.bActivityWakeRequested = true;
				Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
				continue;
			}
			// 동적 지형은 시작 때의 sweep만으로 미래 자세를 보장할 수 없다. 해당 slot만 정확성을 우선한다.
			bool bDynamicRisk = false;
			for (const auto& Support : DynamicFrame->Supports)
			{
				if (Support.Id.Slot == INDEX_NONE)
					bDynamicRisk = true;
				else if (Snapshot->Slots.IsValidIndex(Support.Id.Slot))
				{
					const FVector Local = Snapshot->Slots[Support.Id.Slot].WorldToSlotRotation.RotateVector(Next - Params.GravityOrigin);
					bDynamicRisk |= Local.X >= 0.0 && Local.Y >= 0.0 && Local.Z >= 0.0;
				}
			}
			if (bDynamicRisk)
			{
				const FVector Up = (Params.GravityOrigin - Position).GetSafeNormal();
				FLNPWorldHit Hit;
				const bool bBlocked = Config->IsFlying()
					? Collision.SweepSphereWorld(Position, Next, FMath::Max(Params.CapsuleRadius, Params.CapsuleHalfHeight) + Config->FlightConfig.Clearance,
						FLNPWorldQueryParams(ELNPWorldQueryClass::FlightSteering), Hit)
					: Collision.SweepCapsuleWorld(Position + Up * 2.f, Next + Up * 2.f, FQuat::FindBetweenNormals(FVector::UpVector, Up),
						Params.CapsuleRadius, Params.CapsuleHalfHeight, FLNPWorldQueryParams(ELNPWorldQueryClass::DynamicSupportContact), Hit);
				if (bBlocked)
				{
					if (LNPLoadBaseline::IsActivityDiagnostics())
						UE_LOG(LogLootNPop, Display, TEXT("[WanderDiagnostic] Entity=%d Pod=%d Reason=DynamicBlock"), Ctx.GetEntity(I).Index, Enemy.ParentLootPod.Index);
					Segment = FLNPEnemyWanderSegment();
					Data.bWanderHandled = false;
					Enemy.bActivityWakeRequested = true;
					Enemy.ActivityBand = ELNPEnemyActivityBand::Active;
					Moves[I].Center = Position;
					continue;
				}
			}
			Transform.SetLocation(Next);
			const FVector Up = (Params.GravityOrigin - Next).GetSafeNormal();
			FVector Forward = FVector::VectorPlaneProject(Segment.Goal - Segment.Start, Up).GetSafeNormal();
			if (Forward.IsNearlyZero())
				Forward = FVector::VectorPlaneProject(Transform.GetRotation().GetForwardVector(), Up).GetSafeNormal();
			Transform.SetRotation(Segment.Start.Equals(Segment.Goal)
				? LNP::Replication::DecodeSphereRotation(Next, Segment.LocalYaw)
				: FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat());
			Moves[I].Center = Segment.Goal;
			Moves[I].DistanceToGoal = FVector::Distance(Next, Segment.Goal);
			Moves[I].DesiredSpeed = FMassInt16Real(Now < Segment.ArrivalTime ? 0.3f * (Config->IsFlying() ? Config->FlightConfig.FlightSpeed : Config->MovementConfig.MoveSpeed) : 0.f);
		}
	});
}

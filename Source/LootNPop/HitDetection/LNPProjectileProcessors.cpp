// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileProcessors.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "GAS/LNPPoiseTypes.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPProjectileImpactContext.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Character/LNPPlayerCharacter.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "GAS/LNPDamageFormula.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassCommandBuffer.h"
#include "AbilitySystemComponent.h"
#if WITH_EDITOR
#include "MassDebugDrawHelpers.h"
#endif

namespace
{

	/** 클라이언트 예측 전용: ULNPGhostProjectileSubsystem::Ghosts(TMap)는 RPC 콜백(게임 스레드)에서도 갱신되므로,
	 *  Mass Execute()(워커 스레드에서 돌 수 있음)에서 직접 건드리면 데이터 레이스다. Command Buffer flush로 위탁한다. */
	struct FLNPGhostSweepCommand : public FMassBatchedCommand
	{
		FLNPGhostSweepCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

		void Add() { bHasWork = true; }

		virtual void Run(FMassEntityManager& EntityManager) override
		{
			UWorld* World = EntityManager.GetWorld();
			if (ULNPGhostProjectileSubsystem* GhostSub = World ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr)
				GhostSub->SweepExpiredGhosts();
		}

		virtual void Reset() override { FMassBatchedCommand::Reset(); }
		virtual SIZE_T GetAllocatedSize()     const override { return 0; }
		virtual int32  GetNumOperationsStat() const override { return 1; }
	};

	/** 로컬 코스메틱 판정에 의한 Ghost 파괴 — 서버 확정 큐의 VFX 중복 재생을 막기 위해 키를 기록한다. */
	struct FLNPGhostDestroyCommand : public FMassBatchedCommand
	{
		FLNPGhostDestroyCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

		void Add(const FLNPGhostKey& InKey)
		{
			Entries.Add(InKey);
			bHasWork = true;
		}

		virtual void Run(FMassEntityManager& EntityManager) override
		{
			UWorld* World = EntityManager.GetWorld();
			ULNPGhostProjectileSubsystem* GhostSub = World ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr;
			if (!GhostSub)
				return;

			for (const FLNPGhostKey& Key : Entries)
				GhostSub->DestroyGhostFromLocalImpact(Key);
		}

		virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
		virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
		virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

	private:
		TArray<FLNPGhostKey> Entries;
	};
}

// ============================================================
// ULNPProjectileMovementProcessor
// ============================================================

ULNPProjectileMovementProcessor::ULNPProjectileMovementProcessor()
	: ProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPProjectileMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.RegisterWithProcessor(*this);
}

void ULNPProjectileMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FTransformFragment>     Transforms  = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();

		// 무기 상수라 Chunk마다 한 번만 읽는다. 0이면 등속 직선이므로 타입 분기가 없다.
		const float GravityAccel = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>().GravityAccel;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPProjectileFragment& Proj      = Projectiles[i];
			FTransform&             Transform = Transforms[i].GetMutableTransform();
			FVector                 Pos       = Transform.GetLocation();

			Proj.PreviousPos = Pos;
			LNPProjectileMotion::Step(Pos, Proj.Velocity, GravityAccel, DeltaTime);
			Transform.SetLocation(Pos);
			Proj.LifetimeRemaining -= DeltaTime;
		}
	});
}

// ============================================================
// ULNPProjectileHitDetectionProcessor
// ============================================================

ULNPProjectileHitDetectionProcessor::ULNPProjectileHitDetectionProcessor()
	: ProjectileQuery(*this), EnemyQuery(*this), PlayerQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPProjectileHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::None);
	ProjectileQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EnemyQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);   // Actor 없는 적의 넉백
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPSurfaceCacheSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	const bool bIsServer = World && World->GetNetMode() < NM_Client;

	const ULNPSurfaceCacheSubsystem& SurfaceCache = Context.GetSubsystemChecked<ULNPSurfaceCacheSubsystem>();
	ULNPProjectileVisualSubsystem&   VisualSub    = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	/**
	 * 캐릭터에 닿지 않은 탄의 **종말 판정** — 지면 착탄과 수명 만료를 한 자리에서 가른다.
	 *
	 * 표면 충돌은 예전에 ULNPProjectileMovementProcessor(PrePhysics)가 직접 파괴까지 했다.
	 * 그러면 그 페이즈 끝에서 FLNPProjectileDeadTag가 flush되어 **이 Processor의 쿼리에서 아예
	 * 빠지고**, 스플래시에 도달할 길이 구조적으로 없었다. 그래서 판정 소유권을 여기로 모았다 —
	 * 이동 Processor는 이제 전진과 수명 감소만 한다.
	 *
	 * 수명 만료도 폭발로 취급한다. 임팩트 VFX는 예전부터 두 경우 모두 재생하고 있었으므로,
	 * 스플래시만 같이 붙여야 연출과 판정의 대칭이 맞는다.
	 */
	auto IsTerminated = [&SurfaceCache](const FVector& Pos, const float LifetimeRemaining) -> bool
	{
		if (LifetimeRemaining <= 0.f)
			return true;

		return LNPProjectileMotion::IsUnderSurface(SurfaceCache, Pos);
	};

	// 클라이언트: 로컬 예측 공격자의 Ghost Projectile에 한해 Physics/Actor 기반 예측 판정 (코스메틱 HitStop만, GE 미적용).
	// 서버 판정(Mass 엔티티 쿼리 + GE 적용)과 완전히 분리된 경로다.
	if (!bIsServer)
	{
		if (!World)
			return;

		Context.Defer().PushCommand<FLNPGhostSweepCommand>();

		// 클라이언트 예측 전용 타겟 캡슐 수집 — Enemy MassReplication(Phase 6) 이후 EnemyQuery/PlayerQuery가
		// 클라이언트에도 유효한 엔티티를 반환하므로, 게임 스레드 전용인 TActorIterator 없이 서버 Pass 1/2와 동일하게 조회한다.
		struct FClientCapsuleTarget
		{
			AActor* Actor;            // Enemy 엔티티는 클라이언트에서 Actor 미링크(nullptr)일 수 있음 — 판정에는 불필요
			FVector Location;
			FVector UpDir;
			float   CapsuleHalfHeight;
			float   CapsuleRadius;
			bool    bIsEnemy;
			int32   PlayerID;         // Player 타겟의 PlayerId — 관전용 Ghost가 발사자 본인 캡슐에 자폭하지 않도록 제외용
		};
		TArray<FClientCapsuleTarget> ClientTargets;

		EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
			if (!Shared.Config)
				return;

			const float HalfH  = Shared.Config->CapsuleHalfHeight;
			const float Radius = Shared.Config->CapsuleRadius;

			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FVector Loc   = Transforms[i].GetTransform().GetLocation();
				const FVector UpDir = (-Loc).GetSafeNormal();
				AActor*       Actor = ActorFrags[i].GetMutable();
				ClientTargets.Add({ Actor, LNPHitDetection::ResolveEnemyCapsuleCenter(Loc, UpDir, HalfH, Actor),
					UpDir, HalfH, Radius, true, INDEX_NONE });
			}
		});

		PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				AActor* PlayerActor = ActorFrags[i].GetMutable();
				const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(PlayerActor);
				if (!PlayerPawn)
					continue;

				float HalfH, Radius;
				LNPHitDetection::GetCapsuleSize(PlayerPawn->GetCapsule(), HalfH, Radius);
				const APlayerState* TargetPS = PlayerPawn->GetPlayerState<APlayerState>();
				const FVector Loc = Transforms[i].GetTransform().GetLocation();
				ClientTargets.Add({ PlayerActor, Loc, (-Loc).GetSafeNormal(), HalfH, Radius, false,
					TargetPS ? TargetPS->GetPlayerId() : INDEX_NONE });
			}
		});

		const bool bFriendlyFireClient = GetDefault<ULNPSettings>()->bFriendlyFire;
		UMassActorSubsystem* ActorSub  = World->GetSubsystem<UMassActorSubsystem>();

		ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const FLNPProjectileSharedFragment&                 Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
			const TConstArrayView<FTransformFragment>           Transforms  = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FLNPProjectileFragment>                  Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();
			const TConstArrayView<FLNPProjectileVisualFragment> Visuals     = Ctx.GetFragmentView<FLNPProjectileVisualFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				// 공격자 예측 Ghost(bIsLocalInstigator)와 관전용 Ghost 모두 로컬 코스메틱 충돌을 수행한다.
				// 관전용도 로컬에서 즉시 소멸·VFX를 재생해야 "캐릭터를 관통한 뒤 뒤늦게 터지는" 잔상이 없다.
				// 서버 확정 큐가 도착하면 DestroyGhostFromLocalImpact가 기록한 키로 VFX 중복 재생을 걸러낸다.
				FLNPProjectileFragment& Proj = Projectiles[i];

				const FVector CurrentPos = Transforms[i].GetTransform().GetLocation();
				AActor* InstigatorActor = (ActorSub && Proj.Instigator.IsSet() && EntityManager.IsEntityActive(Proj.Instigator)) ? ActorSub->GetActorFromHandle(Proj.Instigator) : nullptr;

				bool bHit = false;
				for (const FClientCapsuleTarget& Target : ClientTargets)
				{
					// 발사자 자신 제외 — 로컬 예측 Ghost는 Instigator 핸들로, 관전용 Ghost는 PlayerID로 걸러낸다.
					if (Target.Actor && InstigatorActor && Target.Actor == InstigatorActor)
						continue;
					if (!Target.bIsEnemy && Target.PlayerID != INDEX_NONE && Target.PlayerID == Proj.InstigatorPlayerID)
						continue;

					if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player && !Target.bIsEnemy && !bFriendlyFireClient)
						continue;
					if (Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy && Target.bIsEnemy)
						continue;

					FVector HitPoint;
					if (!LNPHitDetection::SegmentHitsCapsule(Proj.PreviousPos, CurrentPos, Target.Location, Target.UpDir,
						Target.CapsuleHalfHeight, Target.CapsuleRadius + Shared.HitRadius, HitPoint))
						continue;

					// 원거리는 공격자 HitStop을 재생하지 않는다 (근접 전용 — 물리적 충돌감이 없어 어색함).
					// Ghost 즉시 소멸(트레일 관통 방지) + 예측 위치 임팩트 VFX. 서버 확정 결과가 최종.
					Ctx.Defer().PushCommand<FLNPGhostDestroyCommand>(
						FLNPGhostKey{ Proj.InstigatorPlayerID, Proj.PredictionKeyID, Proj.SpawnIndex });
					VisualSub.EnqueueImpact(Shared.VFXData, HitPoint, (HitPoint - Target.Location).GetSafeNormal());

					bHit = true;
					break;  // 이 Projectile은 예측 판정 종료 — 서버 확정 결과가 최종
				}

				// 지면 착탄·수명 만료 — Ghost는 로컬 코스메틱이므로 소멸·VFX만 하고 스플래시는 서버 몫이다.
				if (bHit || !IsTerminated(CurrentPos, Proj.LifetimeRemaining))
					continue;

				const FMassEntityHandle ProjEnt = Ctx.GetEntity(i);
				if (Visuals[i].bInitialized)
					VisualSub.EnqueueTrailRelease(ProjEnt);
				VisualSub.EnqueueImpact(Shared.VFXData, CurrentPos, -CurrentPos.GetSafeNormal());
				Ctx.Defer().AddTag<FLNPProjectileDeadTag>(ProjEnt);
			}
		});
		return;
	}

	const double Now = World ? World->GetTimeSeconds() : 0.0; // 패링 창 RTT 역보정용 (섹션 5.1)

	// ── Pass 1: Enemy 캡슐 데이터 수집 ────────────────────────────────────────
	struct FCollectedEnemy
	{
		FVector            CapsuleCenter;    // 판정 캡슐 중심 (LNPHitDetection::ResolveEnemyCapsuleCenter)
		FVector            UpDir;            // (-Location).GetSafeNormal() — 구형 세계 UP
		float              CapsuleHalfHeight;
		float              CapsuleRadius;
		FLNPEnemyFragment* Fragment;
		FMassEntityHandle  Handle;
		AActor*            Actor;
		FVector            RawLocation;      // Lag Compensation 되감기 기준 원점
		const FLNPPositionHistoryFragment* History;
		FLNPPoiseFragment* Poise;         // 아키타입에 없으면 null (Optional 요구)
		const ULNPEnemyConfig* Config;    // 청크 공용 Config (피격 반응 시간 조회용)
		FVector*           Velocity;      // Actor 없는 적의 넉백이 쓰는 물리 속도
	};
	TArray<FCollectedEnemy> Enemies;

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (nullptr == Shared.Config)
			return;

		const float HalfH  = Shared.Config->CapsuleHalfHeight;
		const float Radius = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>                       EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>                      ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment>  Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();
		TArrayView<FLNPPoiseFragment>                       PoiseFrags = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();
		TArrayView<FLNPEnemyVelocityFragment>               VelFrags   = Ctx.GetMutableFragmentView<FLNPEnemyVelocityFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Loc   = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir = (-Loc).GetSafeNormal();
			AActor*       Actor = ActorFrags[i].GetMutable();
			Enemies.Add({ LNPHitDetection::ResolveEnemyCapsuleCenter(Loc, UpDir, HalfH, Actor), UpDir, HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), Actor, Loc, &Histories[i], PoiseFrags.IsValidIndex(i) ? &PoiseFrags[i] : nullptr, Shared.Config, &VelFrags[i].Velocity });
		}
	});

	// ── Pass 2: Player 캡슐 데이터 수집 ───────────────────────────────────────
	struct FCollectedPlayer
	{
		FVector                Location;       // 액터 위치 (충돌 판정 중심으로 사용)
		FVector                UpDir;          // (-Location).GetSafeNormal()
		FVector                ForwardVector;  // 패링 각도 계산용
		float                  CapsuleHalfHeight;
		float                  CapsuleRadius;
		FMassEntityHandle      Handle;
		AActor*                Actor;
		FLNPParryStateFragment ParryState;
		FVector                RawLocation;    // Lag Compensation 되감기 기준 원점
		const FLNPPositionHistoryFragment* History;
		FLNPPoiseFragment* Poise;         // 아키타입에 없으면 null (Optional 요구)
	};
	TArray<FCollectedPlayer> Players;

	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment>                     ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPParryStateFragment>      ParryFrags = Ctx.GetFragmentView<FLNPParryStateFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment> Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();
		TArrayView<FLNPPoiseFragment>                      PoiseFrags = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			AActor* PlayerActor = ActorFrags[i].GetMutable();
			const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(PlayerActor);
			if (PlayerPawn == nullptr)
				continue;

			float HalfH, Radius;
			LNPHitDetection::GetCapsuleSize(PlayerPawn->GetCapsule(), HalfH, Radius);
			const FTransform& T   = Transforms[i].GetTransform();
			const FVector     Loc = T.GetLocation();
			Players.Add({
				Loc,
				(-Loc).GetSafeNormal(),
				T.GetRotation().GetForwardVector(),
				HalfH,
				Radius,
				Ctx.GetEntity(i),
				PlayerActor,
				ParryFrags[i],
				Loc,
				&Histories[i],
				PoiseFrags.IsValidIndex(i) ? &PoiseFrags[i] : nullptr
			});
		}
	});

	const bool bFriendlyFire = GetDefault<ULNPSettings>()->bFriendlyFire;
	const float PoiseGuardMultiplier = GetDefault<ULNPSettings>()->PoiseGuardMultiplier;

	// ExcludeEnemy/ExcludePlayer = 직접 피격 대상 (중복 피해 방지)
	auto ApplySplash = [&](FMassExecutionContext& Ctx,
		const FLNPProjectileSharedFragment& Shared,
		const FLNPProjectileFragment&       Proj,
		FVector                             HitPoint,
		const FCollectedEnemy*              ExcludeEnemy,
		const FCollectedPlayer*             ExcludePlayer,
		bool                                bFF)
	{
		if (Shared.ExplosionRadius <= 0.f || !Shared.DamageEffectClass)
			return;

		const float ExpRadSq = FMath::Square(Shared.ExplosionRadius);

		/**
		 * 폭심에서 멀수록 약해진다. **데미지는 선형(`1-t`), 넉백은 뒤집힌 제곱(`1-t²`)** 이다.
		 *
		 * ⚠️ **`1-t²`와 `(1-t)²`는 전혀 다른 곡선이다.** 둘 다 0에서 1, 1에서 0이지만
		 *    **어디서 꺾이는지가 반대**다:
		 *
		 * ```
		 *   t        0     0.25    0.5    0.75    1.0
		 *   1-t²    1.00   0.94   0.75   0.44    0     폭심 근처는 평평, 가장자리에서 급락
		 *   (1-t)²  1.00   0.56   0.25   0.06    0     폭심 근처에서 이미 급락
		 * ```
		 *
		 *    넉백이 원하는 것은 앞쪽이다 — **"폭발에 휘말리면 확실히 날아가고, 반경을 겨우 벗어난
		 *    쪽만 안 날아간다."** 뒤쪽을 쓰면 폭심에서 몇 미터만 떨어져도 밀림이 사라져
		 *    반경을 크게 잡은 의미가 없어진다(실측 체감: "넉백이 너무 약하다").
		 *
		 * 데미지가 선형인 이유는 다르다 — 가장자리에서도 피해가 어느 정도 남아야
		 * 광범위 무기가 제 역할을 한다.
		 *
		 * ⚠️ 예전에는 감쇠가 **아예 없어서** 반경 안이면 어디서나 직격과 같은 값이 들어갔다.
		 *    기본 반경이 5cm이던 시절에는 드러날 수 없던 문제이고, 반경을 키우는 순간 표면화된다.
		 *
		 * 경직도는 감쇠시키지 않는다 — 경직은 "몇 번 맞았는가"의 눈금이라 거리로 희석하면
		 * 누적 규칙(→ GameDesign_Poise.md)과 축이 어긋난다.
		 */
		auto SplashFalloff = [&Shared](const float DistSq, float& OutDamage, float& OutKnockback)
		{
			const float T = FMath::Clamp(FMath::Sqrt(DistSq) / Shared.ExplosionRadius, 0.f, 1.f);
			OutDamage    = Shared.Damage * (1.f - T);
			OutKnockback = Shared.SplashKnockbackStrength * (1.f - T * T);
		};

		if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player)
		{
			for (FCollectedEnemy& SE : Enemies)
			{
				if (&SE == ExcludeEnemy) continue;
				const float DistSq = FVector::DistSquared(SE.CapsuleCenter, HitPoint);
				if (DistSq > ExpRadSq) continue;
				LNPPoise::Accumulate(SE.Poise, Shared.PoiseDamage, Now);

				float SplashDamage, SplashKnockback;
				SplashFalloff(DistSq, SplashDamage, SplashKnockback);

				// 폭발에서는 폭심이 곧 공격자다. `HitFromDirection`의 규약은 **"피격자 → 공격자"**
				// (공격이 날아온 쪽)이므로 폭심 쪽을 가리켜야 한다 — 소비처가 부호를 뒤집어
				// 밀어내는 방향을 만든다.
				//
				// ⚠️ 예전에는 이 부호가 반대(`피격자 - 폭심`)였다. 그러면 넉백이 **폭심 쪽으로
				//    빨아들이고** HitReact 방향 판정도 앞뒤가 뒤집힌다. 직격·근접은 처음부터
				//    올바른 규약을 쓰고 있었고 스플래시 두 곳만 어긋나 있었다.
				const FVector SplashDir = (HitPoint - SE.CapsuleCenter).GetSafeNormal();

				// ⚠️ 예전에는 여기서 Actor 없는 적을 통째로 건너뛰었다 — **순수 엔티티는 폭발
				//    반경 안에 서 있어도 피해를 받지 않았다.** 직격 분기와 같은 형태로 맞춘다.
				if (SE.Actor && Shared.DamageEffectClass)
				{
					Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(
						SE.Actor, Shared.DamageEffectClass,
						SplashDamage, SplashDir,
						SE.CapsuleCenter + (HitPoint - SE.CapsuleCenter).GetSafeNormal() * SE.CapsuleRadius,
						SplashKnockback);
				}
				else if (!SE.Actor)
				{
					SE.Fragment->Health = FMath::Max(0.f,
						SE.Fragment->Health - LNPDamage::ApplyDefense(SplashDamage, SE.Fragment->Defense));
					if (SE.Velocity)
						LNPHitDetection::ApplyEntityKnockback(*SE.Velocity, SplashDir, SE.UpDir, SplashKnockback);
					if (SE.Config)
						SE.Fragment->FlinchTimeRemaining = SE.Config->PureEntityFlinchTime;
				}
			}
		}

		if (Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy || bFF)
		{
			for (FCollectedPlayer& SP : Players)
			{
				if (&SP == ExcludePlayer) continue;
				if (!SP.Actor) continue;
				const float DistSq = FVector::DistSquared(SP.Location, HitPoint);
				if (DistSq > ExpRadSq) continue;
				LNPPoise::Accumulate(SP.Poise, Shared.PoiseDamage, Now);

				float SplashDamage, SplashKnockback;
				SplashFalloff(DistSq, SplashDamage, SplashKnockback);

				// 적 쪽과 같은 규약 — "피격자 → 공격자(폭심)". 부호 뒤집기는 소비처가 한다.
				Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(
					SP.Actor, Shared.DamageEffectClass,
					SplashDamage, (HitPoint - SP.Location).GetSafeNormal(),
					SP.Location + (HitPoint - SP.Location).GetSafeNormal() * SP.CapsuleRadius,
					SplashKnockback);
			}
		}
	};

	// ── Pass 3: 선분 vs 캡슐 충돌 판정 (Lag Compensation 포함) ─────────────────
	// 되감기 **핑 항**의 상한. 보간 지연 항은 여기에 묶지 않는다 (LNPPositionHistoryFragment.h).
	constexpr float MaxPingRewindSeconds = LNPHitDetection::MaxPingRewindSeconds;

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&                 Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		const TConstArrayView<FTransformFragment>           Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPProjectileFragment>                  Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();
		const TConstArrayView<FLNPProjectileVisualFragment> Visuals     = Ctx.GetFragmentView<FLNPProjectileVisualFragment>();

		const float HitRadius   = Shared.HitRadius;
		const float ParryRadius = Shared.ParryRadius;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPProjectileFragment& Proj       = Projectiles[i];
			const FVector           CurrentPos = Transforms[i].GetTransform().GetLocation();
			const FMassEntityHandle ProjEnt    = Ctx.GetEntity(i);

			// 공격자(발사자) RTT/2만큼 과거 시점의 피격 대상 위치로 판정한다 (섹션 5.0).
			// 발사(또는 패링 반사) 시점에 1회 캐싱된 값을 재사용 — "공격자가 조준해서 쏜 순간의 지연"만 보정하고
			// 이후에는 대상의 현재 위치와 비교한다. 매 프레임 재계산 시 느린/유도 발사체가 비행 내내
			// 대상의 과거 잔상을 쫓아가 맞는 문제와, 매 프레임 Actor→PlayerState→Ping 조회 비용을 함께 제거.
			// 대상별 가산항(순수 엔티티의 보간 지연)은 아래 RewoundEnemyCenter가 더한다.
			const float RewindSeconds = FMath::Clamp(Proj.CachedRewindSeconds, 0.f, MaxPingRewindSeconds);

			// 되감긴 캡슐 중심 = 현재 캡슐 중심 + (과거 원점 - 현재 원점). 되감기량이 0이면 원본 그대로 반환.
			auto RewoundCenterAt = [&](const FVector& CapsuleCenter, const FVector& RawLoc,
				const FLNPPositionHistoryFragment* History, const float TotalRewind) -> FVector
			{
				// Count == 0이면 GetInterpolatedLocation이 ZeroVector를 돌려준다 — 구 내벽 월드에서
				// 그것은 "원점"이 아니라 **행성 중심**이라, 그대로 빼면 캡슐이 25,000cm 아래로 튄다.
				// 스폰 직후 첫 기록(50ms) 전 대상이 여기 걸린다.
				if (TotalRewind <= 0.f || !History || History->Count == 0)
					return CapsuleCenter;
				return CapsuleCenter + (History->GetInterpolatedLocation(Now - TotalRewind) - RawLoc);
			};

			// 플레이어는 보간을 타지 않는다 — 위치가 Mass 버블이 아니라 Actor 복제로 오고,
			// 시뮬레이티드 프록시는 Mover ForwardPredict로 현재까지 전방 예측된다. 표시 지연은 RTT/2뿐.
			auto RewoundCenter = [&](const FVector& CapsuleCenter, const FVector& RawLoc, const FLNPPositionHistoryFragment* History) -> FVector
			{
				return RewoundCenterAt(CapsuleCenter, RawLoc, History, RewindSeconds);
			};

			// 순수 엔티티 적은 RTT/2에 **클라이언트 보간 지연**만큼 더 과거다 (LNPPositionHistoryFragment.h).
			// 거리 기준점은 발사(반사) 시점의 공격자 위치다 — 복제 LOD는 뷰어(= 공격자) 기준으로 정해지고,
			// 명중 시점에 공격자 엔티티를 조회하면 워커 스레드에서 임의 엔티티 접근이 된다.
			// LOD 대역이 1,000/5,000cm로 성겨 비행 중 공격자 이동이 대역을 뒤집는 일은 드물다.
			auto RewoundEnemyCenter = [&](const FCollectedEnemy& Enemy) -> FVector
			{
				float TotalRewind = RewindSeconds;
				if (Proj.bInstigatorIsRemoteClient && Enemy.Config && Enemy.Config->CombatMode == ELNPEnemyCombatMode::PureEntity)
				{
					// 총량에 상한을 걸지 않는다 — 합의 최댓값은 0.2(핑 상한) + 0.3(Low 주기) = 0.5초로
					// 구조적으로 묶여 있고, 히스토리 버퍼가 정확히 그만큼 덮는다(static_assert).
					const float DistSqFromViewer = static_cast<float>(FVector::DistSquared(Enemy.RawLocation, Proj.InstigatorViewLocation));
					TotalRewind = RewindSeconds + LNPHitDetection::GetInterpolationLagSeconds(DistSqFromViewer);
				}
				return RewoundCenterAt(Enemy.CapsuleCenter, Enemy.RawLocation, Enemy.History, TotalRewind);
			};

			// 피격 시 공통 후처리: 트레일 해제 · 임팩트 VFX(GameplayCue) · Dead 태그 · 스플래시
			bool bHit = false;
			auto FinishHit = [&](FVector HitPoint, FVector ImpactNormal,
				const FCollectedEnemy* ExclEnemy, const FCollectedPlayer* ExclPlayer)
			{
				if (Visuals[i].bInitialized)
					VisualSub.EnqueueTrailRelease(ProjEnt);

				// 캐릭터 피격 임팩트 VFX는 GameplayCue.LNP.Projectile.Impact로 일원화한다 (섹션 5.2).
				// Ghost 재조정에 필요한 토큰(PredictionKeyID/SpawnIndex)과 InstigatorPlayerID를 커스텀 컨텍스트로 전달.
				// 이 Processor는 워커 Thread에서 실행되므로 ASC를 직접 건드리지 않고 BatchedCommand로 위탁한다.
				//
				// ⚠️ **피격자가 null일 수 있다** — 순수 엔티티에는 Actor가 없다. 예전에는 커맨드가
				//    피격자 ASC를 못 찾아 통째로 빠졌고, 그래서 호스트 화면에서 ISM 적을 쏘면 착탄
				//    이펙트가 안 뜨고 서버 확정 Ghost 정리도 나가지 않았다. 이제는 공격자 핸들을 함께
				//    실어 보내 커맨드가 전송 ASC를 고른다.
				AActor* VictimActor = ExclEnemy ? ExclEnemy->Actor : (ExclPlayer ? ExclPlayer->Actor : nullptr);
				Ctx.Defer().PushCommand<FLNPImpactCueCommand>(
					VictimActor, Proj.Instigator, Shared.VFXData, HitPoint, ImpactNormal,
					Proj.PredictionKeyID, Proj.SpawnIndex, Proj.InstigatorPlayerID);

				Ctx.Defer().AddTag<FLNPProjectileDeadTag>(ProjEnt);
				ApplySplash(Ctx, Shared, Proj, HitPoint, ExclEnemy, ExclPlayer, bFriendlyFire);
				bHit = true;
			};

			// Enemy 판정 — Player 발사체만 Enemy에게 피해를 줌 (비 Player 발사체는 캡슐에 닿아도 파괴만 됨)
			for (FCollectedEnemy& Enemy : Enemies)
			{
				if (Enemy.Handle == Proj.Instigator)
					continue;

#if !UE_BUILD_SHIPPING
				// ── 계측 ─────────────────────────────────────────────────
				// 보정을 켠 채로 평소처럼 쏘면서 개선율을 잴 수 있게, CVar 값과 **무관하게** 두 판정을 만든다.
				// 둘 중 하나라도 맞은 경우에만 찍으므로(빗나간 프레임은 침묵) 볼륨이 발당 한 줄 수준이다.
				if (LNPHitDetection::IsMeasuringInterpolationLag()
					&& Proj.bInstigatorIsRemoteClient
					&& Enemy.Config && Enemy.Config->CombatMode == ELNPEnemyCombatMode::PureEntity)
				{
					const float DistSq = static_cast<float>(FVector::DistSquared(Enemy.RawLocation, Proj.InstigatorViewLocation));
					const float RewindWith = RewindSeconds + LNPHitDetection::GetInterpolationLagSeconds(DistSq, /*bIgnoreCVar*/ true);

					const FVector CenterWith    = RewoundCenterAt(Enemy.CapsuleCenter, Enemy.RawLocation, Enemy.History, RewindWith);
					const FVector CenterWithout = RewoundCenterAt(Enemy.CapsuleCenter, Enemy.RawLocation, Enemy.History, RewindSeconds);

					FVector Ignored;
					const bool bWith = LNPHitDetection::SegmentHitsCapsule(Proj.PreviousPos, CurrentPos,
						CenterWith, Enemy.UpDir, Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius + HitRadius, Ignored);
					const bool bWithout = LNPHitDetection::SegmentHitsCapsule(Proj.PreviousPos, CurrentPos,
						CenterWithout, Enemy.UpDir, Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius + HitRadius, Ignored);

					if (bWith || bWithout)
					{
						const TCHAR* Verdict = bWith && bWithout ? TEXT("BOTH")
							: (bWith ? TEXT("RESCUED") : TEXT("LOST"));
						UE_LOG(LogLootNPop, Log,
							TEXT("[RewindLag] ranged %-7s dist=%6.0fcm lod=%-6s rewind=%.3f/%.3f shift=%5.1fcm"),
							Verdict, FMath::Sqrt(DistSq), LNPHitDetection::GetReplicationLODName(DistSq),
							RewindWith, RewindSeconds, (CenterWith - CenterWithout).Size());
					}
				}
#endif

				FVector HitPoint;
				if (!LNPHitDetection::SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					RewoundEnemyCenter(Enemy), Enemy.UpDir,
					Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius + HitRadius,
					HitPoint))
					continue;

				if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player)
				{
					const FVector HitFromDir = (-Proj.Velocity).GetSafeNormal(); // 피격자 → 공격자

					// Actor 승격 여부와 무관하게 같은 눈금으로 쌓는다.
					LNPPoise::Accumulate(Enemy.Poise, Shared.PoiseDamage, Now);

					// 피격 반응 — 배회 중이면 이 방향으로 돌아선다(ULNPEnemyMovementProcessor의 None 분기).
					// Actor·Entity 두 경로 **모두**에서 같은 값을 기록해야 LOD에 따라 반응이 갈리지 않는다.
					if (Enemy.Config)
					{
						Enemy.Fragment->HitReactTimer     = Enemy.Config->TargetingConfig.HitReactLookTime;
						Enemy.Fragment->HitReactDirection = HitFromDir;
						// 플린치는 갱신만 한다 — 이미 움찔하는 중이면 전이를 새로 만들지 않는다.
						Enemy.Fragment->FlinchTimeRemaining = Enemy.Config->PureEntityFlinchTime;
					}

					if (Enemy.Actor && Shared.DamageEffectClass)
					{
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.Actor, Shared.DamageEffectClass, Shared.Damage, HitFromDir, HitPoint, Shared.KnockbackStrength);
					}
					else
					{
						const float HpBefore = Enemy.Fragment->Health;
						Enemy.Fragment->Health = FMath::Max(0.f, HpBefore - LNPDamage::ApplyDefense(Shared.Damage, Enemy.Fragment->Defense));
						UE_LOG(LogLootNPop, Log, TEXT("[HitDetection][Entity] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, Enemy.Fragment->Health, Shared.Damage);

						// Actor 넉백(Mover)의 엔티티판. 이동 프로세서의 공중 분기가 이 속도를 적분한다.
						if (Enemy.Velocity)
							LNPHitDetection::ApplyEntityKnockback(*Enemy.Velocity, HitFromDir, Enemy.UpDir, Shared.KnockbackStrength);
					}
				}

				FinishHit(HitPoint, (HitPoint - Enemy.CapsuleCenter).GetSafeNormal(), &Enemy, nullptr);
				break;
			}

			if (bHit)
				continue;

			// Player 판정 (패링/가드/피격 분기)
			for (FCollectedPlayer& Player : Players)
			{
				if (Player.Handle == Proj.Instigator)
					continue;

				const FLNPParryStateFragment& PS          = Player.ParryState;
				const FVector                 IncomingDir = (Proj.PreviousPos - CurrentPos).GetSafeNormal();
				const float                   Dot         = FVector::DotProduct(Player.ForwardVector, IncomingDir);
				const bool bShouldProcess = Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy || bFriendlyFire;

				// 1단계: 패링 체크 (ParryRadius — 피격보다 큰 반경)
				if (bShouldProcess && PS.bIsParrying && (PS.ParryWindowExpiryTime < 0.0 || Now <= PS.ParryWindowExpiryTime) && Dot >= PS.ParryAngleCos)
				{
					FVector HitPoint;
					if (LNPHitDetection::SegmentHitsCapsule(
						Proj.PreviousPos, CurrentPos,
						RewoundCenter(Player.Location, Player.RawLocation, Player.History), Player.UpDir,
						Player.CapsuleHalfHeight, Player.CapsuleRadius + ParryRadius,
						HitPoint))
					{
						// 투사체 반사: 속도 반전 + 진영 전환 + 식별자 재발급 (섹션 5.2 반사 개정 — 소멸+재스폰 방송).
						// 이후 임팩트 큐·Ghost 대조는 전부 새 식별자 기준이 된다.
						const int32 OldInstigatorPlayerID = Proj.InstigatorPlayerID;
						const int32 OldKeyOrSalvo         = Proj.PredictionKeyID;
						const uint8 OldSpawnIndex         = Proj.SpawnIndex;

						// 반사 주체(방어자)를 새 공격자로 귀속 — Lag Compensation 기준도 방어자 RTT/2로 갱신.
						int32 DefenderPlayerID       = INDEX_NONE;
						float DefenderHalfRTT        = 0.f;
						bool  bDefenderIsRemoteClient = false;
						if (const APawn* DefenderPawn = Cast<APawn>(Player.Actor))
						{
							if (const APlayerState* DefenderPS = DefenderPawn->GetPlayerState())
							{
								DefenderPlayerID = DefenderPS->GetPlayerId();
								DefenderHalfRTT  = FMath::Clamp(DefenderPS->GetPingInMilliseconds() * 0.0005f, 0.f, MaxPingRewindSeconds);
								bDefenderIsRemoteClient = !DefenderPawn->IsLocallyControlled();
							}
						}

						Proj.Velocity            = -Proj.Velocity;
						Proj.InstigatorTeam      = ELNPInstigatorTeam::Player;
						Proj.Instigator          = Player.Handle;
						Proj.InstigatorPlayerID  = DefenderPlayerID;
						Proj.PredictionKeyID     = ULNPGhostProjectileSubsystem::IssueServerSalvoID();
						Proj.SpawnIndex          = 0;
						Proj.CachedRewindSeconds = DefenderHalfRTT;
						Proj.bInstigatorIsRemoteClient = bDefenderIsRemoteClient;
						Proj.InstigatorViewLocation    = Player.Location; // 반사 주체가 새 뷰어다

						FLNPProjectileParryCommand::FEntry ParryEntry;
						ParryEntry.Victim                = Player.Actor;
						ParryEntry.SharedData            = Shared;
						ParryEntry.SpawnPos              = CurrentPos;
						ParryEntry.NewVelocity           = Proj.Velocity;
						ParryEntry.LifetimeRemaining     = Proj.LifetimeRemaining;
						ParryEntry.NewTeam               = Proj.InstigatorTeam;
						ParryEntry.OldInstigatorPlayerID = OldInstigatorPlayerID;
						ParryEntry.OldKeyOrSalvo         = OldKeyOrSalvo;
						ParryEntry.OldSpawnIndex         = OldSpawnIndex;
						ParryEntry.NewInstigatorPlayerID = DefenderPlayerID;
						ParryEntry.NewKeyOrSalvo         = Proj.PredictionKeyID;
						ParryEntry.ImpactPoint           = HitPoint;
						ParryEntry.ImpactNormal          = IncomingDir;
						Ctx.Defer().PushCommand<FLNPProjectileParryCommand>(ParryEntry);
						break;  // 반사된 투사체는 파괴하지 않고 계속 비행 (FinishHit 호출 없음)
					}
				}

				// 2단계: 피격 체크 (HitRadius — 정상 반경)
				FVector HitPoint;
				if (!LNPHitDetection::SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					RewoundCenter(Player.Location, Player.RawLocation, Player.History), Player.UpDir,
					Player.CapsuleHalfHeight, Player.CapsuleRadius + HitRadius,
					HitPoint))
					continue;

				if (bShouldProcess)
				{
					// 막아내도 경직은 쌓인다 (가드 브레이크). 피격은 전량.
					const bool bBlocked = PS.bIsGuarding && Dot >= PS.GuardAngleCos;
					LNPPoise::Accumulate(Player.Poise, Shared.PoiseDamage, Now, bBlocked ? PoiseGuardMultiplier : 1.f);

					if (bBlocked)
						Ctx.Defer().PushCommand<FLNPGuardBlockCommand>(Player.Actor, HitPoint, IncomingDir);
					else if (Shared.DamageEffectClass)
					{
						const FVector HitFromDir = (-Proj.Velocity).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, Shared.DamageEffectClass, Shared.Damage, HitFromDir, HitPoint, Shared.KnockbackStrength);
					}
				}

				FinishHit(HitPoint, (HitPoint - Player.Location).GetSafeNormal(), nullptr, &Player);
				break;
			}

			// 캐릭터 어디에도 닿지 않았다 — 지면 착탄·수명 만료를 여기서 가른다.
			// 패링으로 반사된 탄은 bHit가 서지 않아 이 검사를 그대로 통과하고 계속 비행한다.
			if (bHit || !IsTerminated(CurrentPos, Proj.LifetimeRemaining))
				continue;

			// 지면 폭발의 임팩트는 로컬 VFX로 남긴다 — 캐릭터 피격과 달리 Ghost 대조 토큰이 필요 없고,
			// GameplayCue로 올리면 게스트가 자기 Ghost의 착탄 VFX와 겹쳐 두 번 보게 된다.
			if (Visuals[i].bInitialized)
				VisualSub.EnqueueTrailRelease(ProjEnt);
			VisualSub.EnqueueImpact(Shared.VFXData, CurrentPos, -CurrentPos.GetSafeNormal());
			Ctx.Defer().AddTag<FLNPProjectileDeadTag>(ProjEnt);

			// 제외 대상 없음 — 직격이 없었으니 반경 안의 모두가 스플래시를 받는다.
			ApplySplash(Ctx, Shared, Proj, CurrentPos, nullptr, nullptr, bFriendlyFire);
		}
	});
}

// ============================================================
// ULNPProjectileVisualizationProcessor
// ============================================================

ULNPProjectileVisualizationProcessor::ULNPProjectileVisualizationProcessor()
	: ProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;

	ExecutionOrder.ExecuteAfter.Add(ULNPProjectileHitDetectionProcessor::StaticClass()->GetFName());
}

void ULNPProjectileVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPProjectileVisualSubsystem& VisualSub = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	// Movement/HitDetection Processor가 큐에 넣은 작업 처리 (둘 다 Worker Thread에서 실행 가능)
	VisualSub.FlushTrailReleases();
	VisualSub.FlushPendingImpacts();

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&           Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		const TConstArrayView<FTransformFragment>     Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();
		TArrayView<FLNPProjectileVisualFragment>      Visuals     = Ctx.GetMutableFragmentView<FLNPProjectileVisualFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector                 CurrentPos = Transforms[i].GetTransform().GetLocation();
			FLNPProjectileVisualFragment& Visual     = Visuals[i];
			const FMassEntityHandle       Entity     = Ctx.GetEntity(i);

			const ELNPInstigatorTeam Team = Projectiles[i].InstigatorTeam;

			if (false == Visual.bInitialized)
			{
				VisualSub.SpawnSpawnEffects(Shared.VFXData, Projectiles[i].SpawnLocation);
				VisualSub.AllocateTrails(Entity, Shared.VFXData, Projectiles[i].SpawnLocation, Team);
				Visual.AppliedTeam  = Team;
				Visual.bInitialized = true;
			}
			else
			{
				VisualSub.UpdateTrails(Entity, CurrentPos);

				// 패링으로 소유권이 넘어간 프레임에만 색을 다시 주입한다 — 매 프레임 Niagara 파라미터를
				// 건드리면 발사체가 많을 때(샷건 19발) 헛비용이 된다.
				if (Visual.AppliedTeam != Team)
				{
					VisualSub.SetTrailTeam(Entity, Team);
					Visual.AppliedTeam = Team;
				}
			}
		}
	});
}

// ============================================================
// ULNPProjectileDestructionProcessor
// ============================================================

ULNPProjectileDestructionProcessor::ULNPProjectileDestructionProcessor()
	: DeadProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPProjectileDestructionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DeadProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	DeadProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::All);
	DeadProjectileQuery.RegisterWithProcessor(*this);
}

void ULNPProjectileDestructionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> ToDestroy;
	DeadProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			ToDestroy.Add(Ctx.GetEntity(i));
	});
	if (ToDestroy.Num() > 0)
		Context.Defer().DestroyEntities(MoveTemp(ToDestroy));
}

#if WITH_EDITOR
// ============================================================
// ULNPProjectileDebugDrawProcessor
// ============================================================

ULNPProjectileDebugDrawProcessor::ULNPProjectileDebugDrawProcessor()
	: ProjectileQuery(*this), PlayerQuery(*this), EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPProjectileVisualizationProcessor::StaticClass()->GetFName());
}

void ULNPProjectileDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::None);
	ProjectileQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);
}

void ULNPProjectileDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	auto Batcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

	// Player 위치 수집 + Player 캡슐 드로우 (항상 표시)
	TArray<FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Location  = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir     = (-Location).GetSafeNormal();
			const FVector TopSphere = Location + UpDir * 54.f;  // HalfHeight(96) - Radius(42)
			const FVector BotSphere = Location - UpDir * 54.f;
			Batcher.DrawSphere(TopSphere, 42.f, FLinearColor(FColor::Green));
			Batcher.DrawSphere(BotSphere, 42.f, FLinearColor(FColor::Green));
			PlayerLocations.Add(Location);
		}
	});

	const float ProjectileProximityDistSq = GetDefault<ULNPSettings>()->DebugDrawProjectileDistSq;
	const float MeleeProximityDistSq      = GetDefault<ULNPSettings>()->DebugDrawProximityDistSq;

	auto IsNearAnyPlayer = [&](const FVector& Pos, const float& ProximityDistSq) -> bool
	{
		for (const FVector& PL : PlayerLocations)
		{
			if (FVector::DistSquared(Pos, PL) < ProximityDistSq)
				return true;
		}
		return false;
	};

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>     Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();
		const FLNPProjectileSharedFragment&           Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Pos = Transforms[i].GetTransform().GetLocation();
			if (!IsNearAnyPlayer(Pos, ProjectileProximityDistSq))
				continue;

			const FVector VelDir     = Projectiles[i].Velocity.GetSafeNormal();
			const bool    bPlayer    = Projectiles[i].InstigatorTeam == ELNPInstigatorTeam::Player;
			const FColor  Color      = bPlayer ? FColor::Cyan   : FColor::Red;
			const FColor  ParryColor = bPlayer ? FColor::Silver : FColor::Orange;

			//Batcher.DrawSphere(Pos, Shared.HitRadius,   FLinearColor(Color));
			//Batcher.DrawSphere(Pos, Shared.ParryRadius, FLinearColor(ParryColor));
			if (!VelDir.IsNearlyZero())
			{
				const FTransform ArrowTf(FQuat::FindBetweenNormals(FVector::ForwardVector, VelDir), Pos);
				//Batcher.DrawArrow(ArrowTf, 30.f, FColor::White);
			}
		}
	});

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FMassActorFragment> ActorFrags = Ctx.GetFragmentView<FMassActorFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const FLNPEnemySharedFragment&            Shared     = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (!Shared.Config)
			return;

		const float HalfHeight = Shared.Config->CapsuleHalfHeight;
		const float Radius     = Shared.Config->CapsuleRadius;
		const float CylHalfLen = FMath::Max(0.f, HalfHeight - Radius);

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Location = Transforms[i].GetTransform().GetLocation();
			if (!IsNearAnyPlayer(Location, MeleeProximityDistSq))
				continue;

			const FVector UpDir  = (-Location).GetSafeNormal();
			const FVector Center = LNPHitDetection::ResolveEnemyCapsuleCenter(Location, UpDir, HalfHeight, ActorFrags[i].Get());

			const FVector TopSphere = Center + UpDir * CylHalfLen;
			const FVector BotSphere = Center - UpDir * CylHalfLen;
			Batcher.DrawSphere(TopSphere, Radius, FLinearColor(FColor::Red));
			Batcher.DrawSphere(BotSphere, Radius, FLinearColor(FColor::Red));
		}
	});
}
#else
ULNPProjectileDebugDrawProcessor::ULNPProjectileDebugDrawProcessor()
	: ProjectileQuery(*this), PlayerQuery(*this), EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPProjectileDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPProjectileDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}
#endif

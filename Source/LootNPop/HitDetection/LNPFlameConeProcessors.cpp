// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPFlameConeProcessors.h"
#include "HitDetection/LNPFlameConeMassTypes.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "GAS/LNPPoiseTypes.h"
#include "GAS/LNPBurnTypes.h"
#include "GAS/LNPDamageFormula.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "GAS/Effects/LNPGameplayEffect_Burn.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "Config/LNPSettings.h"
#include "Character/LNPPlayerCharacter.h"
#include "LootNPop.h"
#include "LNPMassUtils.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"

namespace
{
	/**
	 * 원뿔 형상 + 사선 판정 — 적·플레이어 대상이 같은 식을 쓴다.
	 * 맞으면 대표점(캡슐 축 위에서 원뿔 축에 가장 가까운 점)과 그 점의 축 방향 거리를 돌려준다.
	 */
	bool FlameConeHitsCapsule(const FLNPFlameConeFragment& C, const FVector& Center, const FVector& UpDir,
		const float HalfH, const float CapsuleR, const ULNPMassWorldCollisionSubsystem* WorldCollision,
		FVector& OutPoint, float& OutT)
	{
		// 1차: 원뿔 전체를 감싸는 구 밖이면 버린다.
		const float BoundR = FMath::Max(C.NearRadius, C.FarRadius) + C.Range + HalfH;
		if (FVector::DistSquared(Center, C.Origin) > FMath::Square(BoundR))
			return false;

		// 캡슐 축 선분에서 원뿔 축에 가장 가까운 점을 대표점으로 쓴다 — 키 큰 대상의 머리·발만 들어와도 맞는다.
		const float CylHalfLen = FMath::Max(0.f, HalfH - CapsuleR);
		FVector OnConeAxis;
		FMath::SegmentDistToSegment(C.Origin, C.Origin + C.Axis * C.Range,
			Center - UpDir * CylHalfLen, Center + UpDir * CylHalfLen, OnConeAxis, OutPoint);

		const FVector ToPoint = OutPoint - C.Origin;
		OutT = FVector::DotProduct(ToPoint, C.Axis);
		if (OutT < -CapsuleR || OutT > C.Range + CapsuleR)
			return false;

		const float Alpha   = C.ConeRange > 0.f ? FMath::Clamp(OutT / C.ConeRange, 0.f, 1.f) : 0.f;
		const float Allowed = FMath::Lerp(C.NearRadius, C.FarRadius, Alpha) + CapsuleR;
		if ((ToPoint - C.Axis * OutT).SizeSquared() > FMath::Square(Allowed))
			return false;

		// 축 트레이스는 정면 벽만 자른다. 축 옆 기둥 뒤에 숨은 대상은 총구→대표점 사선으로 거른다.
		FLNPWorldHit WallHit;
		return !(WorldCollision && LNPProjectileMotion::TraceWorld(*WorldCollision, C.Origin, OutPoint, WallHit));
	}

	/**
	 * 공격자 RTT/2만큼 과거의 캡슐 중심 — 근접과 같은 규약(보간 지연 가산 없음, TechDesign_HitDetection.md §5).
	 * History가 비어 있으면 GetInterpolatedLocation이 행성 중심을 돌려주므로 되감지 않는다.
	 */
	FVector FlameConeRewoundCenter(const FLNPFlameConeFragment& C, const FVector& Center, const FVector& RawLoc,
		const FLNPPositionHistoryFragment& History, const double Now, const FLNPSurfaceDataSnapshot* Surface)
	{
		const float RewindSeconds = FMath::Clamp(C.CachedRewindSeconds, 0.f, LNPHitDetection::MaxPingRewindSeconds);
		return (RewindSeconds <= 0.f || History.Count == 0)
			? Center
			: Center + (History.GetInterpolatedLocation(Now - RewindSeconds, Surface) - RawLoc);
	}
}

/**
 * 플레이어 화상 GE 적용 (아군 사격). 게임 스레드에서 피격자 ASC에 ULNPGameplayEffect_Burn을 건다 —
 * 같은 GE가 이미 있으면 엔진 스택 규칙이 중첩 +1·지속 갱신을 한다. 스펙의 지속·주기는 새 인스턴스일 때만 쓰인다.
 */
struct FLNPApplyPlayerBurnGECommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor> Victim;
		float DamagePerStack;
		float Duration;
		float TickInterval;
	};

	FLNPApplyPlayerBurnGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, const float InDamagePerStack, const float InDuration, const float InTickInterval)
	{
		Entries.Add({ InVictim, InDamagePerStack, InDuration, InTickInterval });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			// 같은 배치의 원뿔 피해가 방금 이 대상을 죽였을 수 있다 — 사망 처리가 지운 화상을 다시 걸지 않는다.
			// 사망 태그(쿼리 제외)는 지연 커맨드라 같은 프레임의 이 커맨드를 막지 못한다.
			const ALNPPlayerCharacter* VictimPawn = Cast<ALNPPlayerCharacter>(Entry.Victim.Get());
			if (VictimPawn && VictimPawn->IsDead())
				continue;

			UAbilitySystemComponent* ASC = LNPHitDetection::GetASC(Entry.Victim.Get());
			if (!IsValid(ASC))
				continue;

			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ULNPGameplayEffect_Burn::StaticClass(), 1.f, ASC->MakeEffectContext());
			if (!Spec.IsValid())
				continue;

			Spec.Data->SetSetByCallerMagnitude(TAG_GE_Data_Damage, Entry.DamagePerStack);
			Spec.Data->SetDuration(Entry.Duration, /*bLockDuration*/ true);
			Spec.Data->Period = Entry.TickInterval;
			const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			if (LNPBurn::IsLogEnabled())
			{
				UE_LOG(LogLootNPop, Log, TEXT("[Burn][Player] apply victim=%s stacks=%d perStack=%.1f"),
					*GetNameSafe(Entry.Victim.Get()), ASC->GetCurrentStackCount(Handle), Entry.DamagePerStack);
			}
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

ULNPFlameConeHitDetectionProcessor::ULNPFlameConeHitDetectionProcessor()
	: ConeQuery(*this), EnemyQuery(*this), PlayerQuery(*this)
{
	// 원뿔 엔티티는 서버에서만 생긴다 — 클라이언트에는 판정할 것도 파괴할 것도 없다.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPFlameConeHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ConeQuery.AddRequirement<FLNPFlameConeFragment>(EMassFragmentAccess::ReadOnly);
	ConeQuery.RegisterWithProcessor(*this);

	// 근접 판정(ULNPWeaponTraceHitDetectionProcessor)의 적 쿼리와 같은 구성 — 넉백 속도만 쓰지 않는다.
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EnemyQuery.AddRequirement<FLNPBurnFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);

	// 아군 사격 대상 — 근접 판정의 플레이어 쿼리와 같은 구성.
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	// 시체는 리스폰 지연 동안 남아 있다 — 거르지 않으면 사망 때 지운 화상이 다시 걸린다.
	PlayerQuery.AddTagRequirement<FLNPPlayerDeadTag>(EMassFragmentPresence::None);
	PlayerQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPFlameConeHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	struct FCone
	{
		FLNPFlameConeFragment Data;
		FMassEntityHandle     Handle;
	};
	TArray<FCone> Cones;

	ConeQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FLNPFlameConeFragment> Frags = Ctx.GetFragmentView<FLNPFlameConeFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			Cones.Add({ Frags[i], Ctx.GetEntity(i) });
	});

	if (Cones.IsEmpty())
		return;

	// 판정 결과와 무관하게 이번 실행에서 모두 파괴한다 — 원뿔 엔티티의 유일한 파괴 지점.
	{
		TArray<FMassEntityHandle> ToDestroy;
		ToDestroy.Reserve(Cones.Num());
		for (const FCone& Cone : Cones)
			ToDestroy.Add(Cone.Handle);
		Context.Defer().PushCommand<FMassCommandDestroyEntities>(TConstArrayView<FMassEntityHandle>(ToDestroy));
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	const ULNPMassWorldCollisionSubsystem* WorldCollision = Context.GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	const auto* RewindSurfaces = World->GetSubsystem<ULNPSurfaceDataSubsystem>();
	const auto RewindSurface = RewindSurfaces ? RewindSurfaces->TakeSnapshot() : nullptr;
	const double Now = World->GetTimeSeconds();

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		const ULNPEnemyConfig* Config = Shared.Config;
		if (!Config)
			return;

		const float HalfH     = Config->GetHitCapsuleHalfHeight();
		const float CapsuleR  = Config->GetHitCapsuleRadius();
		const float HitOffset = Config->HitCenterUpOffset;

		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>                       EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>                      ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment>  Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();
		TArrayView<FLNPPoiseFragment>                       PoiseFrags = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();
		TArrayView<FLNPBurnFragment>                        BurnFrags  = Ctx.GetMutableFragmentView<FLNPBurnFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector RawLoc = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir  = LNPMass::GetEntityUp(Transforms[i].GetTransform());
			AActor*       Actor  = ActorFrags[i].GetMutable();
			const FVector Center = LNPHitDetection::ResolveEnemyCapsuleCenter(RawLoc, UpDir, HitOffset, Actor);
			const FLNPPositionHistoryFragment& History = Histories[i];
			FLNPEnemyFragment& Enemy = EnemyFrags[i];

			for (const FCone& Cone : Cones)
			{
				const FLNPFlameConeFragment& C = Cone.Data;
				if (Ctx.GetEntity(i) == C.InstigatorEntity)
					continue;

				const FVector JudgedCenter = FlameConeRewoundCenter(C, Center, RawLoc, History, Now, RewindSurface.Get());

				FVector OnCapsuleAxis;
				float   T;
				if (!FlameConeHitsCapsule(C, JudgedCenter, UpDir, HalfH, CapsuleR, WorldCollision, OnCapsuleAxis, T))
					continue;

				const FVector HitFromDir = (C.Origin - Center).GetSafeNormal(); // 피격자 → 공격자

				LNPPoise::Accumulate(PoiseFrags.IsValidIndex(i) ? &PoiseFrags[i] : nullptr, C.PoiseDamage, Now);
				Enemy.bActivityWakeRequested = true;
				// 배회 중이면 불이 날아온 쪽으로 돌아선다. 플린치는 걸지 않는다 — 0.1초마다 갱신되면 움찔이 끊기지 않는다.
				Enemy.HitReactTimer     = Config->TargetingConfig.HitReactLookTime;
				Enemy.HitReactDirection = HitFromDir;

				const TSubclassOf<UGameplayEffect> EffectClass(C.DamageEffectClass);
				if (Actor && EffectClass)
				{
					// 틱 피해라 HitReact 몽타주를 억제한다 — 근접·원거리와 달리 0.1초마다 들어온다.
					Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Actor, EffectClass, C.Damage, HitFromDir,
						OnCapsuleAxis, /*KnockbackStrength*/ 0.f, /*bSuppressHitReact*/ true);
				}
				else
				{
					const float HpBefore = Enemy.Health;
					Enemy.Health = FMath::Max(0.f, HpBefore - LNPDamage::ApplyDefense(C.Damage, Enemy.Defense));
					UE_LOG(LogLootNPop, Log, TEXT("[FlameCone][Entity] HP: %.1f -> %.1f (damage=%.1f t=%.0f)"),
						HpBefore, Enemy.Health, C.Damage, T);
				}

				// 화상은 Actor·순수 엔티티 구분 없이 프래그먼트에 건다 — 정산은 ULNPBurnProcessor.
				FLNPBurnFragment* Burn = BurnFrags.IsValidIndex(i) ? &BurnFrags[i] : nullptr;
				const uint8 StacksBefore = Burn ? Burn->Stacks : 0;
				if (LNPBurn::TryApply(Burn, C.BurnChance, C.MaxBurnStacks, C.BurnDuration, C.BurnTickInterval,
						C.BurnDamagePerStack, C.DamageEffectClass)
					&& LNPBurn::IsLogEnabled())
				{
					UE_LOG(LogLootNPop, Log, TEXT("[Burn] entity=%d %s stacks=%d->%d remaining=%.2f perStack=%.1f"),
						Ctx.GetEntity(i).Index, StacksBefore == 0 ? TEXT("apply") : TEXT("refresh"),
						StacksBefore, Burn->Stacks, Burn->TimeRemaining, Burn->DamagePerStack);
				}
			}
		}
	});

	// ── 아군 사격: 플레이어 대상 ──────────────────────────────────────────────
	// 꺼져 있으면 플레이어 쿼리 자체를 돌리지 않는다. 적은 화염방사기를 쓰지 않으므로 이 분기는 플레이어→플레이어뿐이다.
	if (!GetDefault<ULNPSettings>()->bFriendlyFire)
		return;

	const float PoiseGuardMultiplier = GetDefault<ULNPSettings>()->PoiseGuardMultiplier;

	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment>                     ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPParryStateFragment>      ParryFrags = Ctx.GetFragmentView<FLNPParryStateFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment> Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();
		TArrayView<FLNPPoiseFragment>                      PoiseFrags = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			AActor* Actor = ActorFrags[i].GetMutable();
			if (!Actor)
				continue;

			float HalfH, CapsuleR;
			LNPHitDetection::GetCapsuleSize(Actor->FindComponentByClass<UCapsuleComponent>(), HalfH, CapsuleR);

			// 플레이어는 항상 Actor가 있어 엔티티 위치가 곧 캡슐 중심이다 (근접 판정과 같은 규약).
			const FTransform&  Xf     = Transforms[i].GetTransform();
			const FVector      Center = Xf.GetLocation();
			const FVector      UpDir  = (-Center).GetSafeNormal();
			const FVector      Fwd    = Xf.GetRotation().GetForwardVector();
			FLNPPoiseFragment* Poise  = PoiseFrags.IsValidIndex(i) ? &PoiseFrags[i] : nullptr;

			for (const FCone& Cone : Cones)
			{
				const FLNPFlameConeFragment& C = Cone.Data;
				if (Ctx.GetEntity(i) == C.InstigatorEntity)
					continue;

				const FVector JudgedCenter = FlameConeRewoundCenter(C, Center, Center, Histories[i], Now, RewindSurface.Get());

				FVector OnCapsuleAxis;
				float   T;
				if (!FlameConeHitsCapsule(C, JudgedCenter, UpDir, HalfH, CapsuleR, WorldCollision, OnCapsuleAxis, T))
					continue;

				const FVector HitFromDir = (C.Origin - Center).GetSafeNormal(); // 피격자 → 공격자

				// 화염은 패링할 수 없다(bInParryRange=false). 가드는 틱 피해만 막고 큐는 내지 않는다 — 초당 10회라 소음이 된다.
				const ELNPPlayerDefenseResult Result = LNPGuardParry::Judge(ParryFrags[i],
					FVector::DotProduct(Fwd, HitFromDir), Now, /*bInParryRange*/ false, /*bInHitRange*/ true);

				if (Result == ELNPPlayerDefenseResult::Guarded)
				{
					// 막아내도 경직은 쌓인다 (가드 브레이크).
					LNPPoise::Accumulate(Poise, C.PoiseDamage, Now, PoiseGuardMultiplier);
				}
				else
				{
					LNPPoise::Accumulate(Poise, C.PoiseDamage, Now);
					const TSubclassOf<UGameplayEffect> EffectClass(C.DamageEffectClass);
					if (EffectClass)
					{
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Actor, EffectClass, C.Damage, HitFromDir,
							OnCapsuleAxis, /*KnockbackStrength*/ 0.f, /*bSuppressHitReact*/ true);
					}
				}

				// 화상은 가드로 막지 못한다 — 가드 여부와 무관하게 확률 판정한다 (GameDesign_Ability.md 화상 절).
				if (C.MaxBurnStacks > 0 && C.BurnDuration > 0.f && C.BurnTickInterval > 0.f && FMath::FRand() < C.BurnChance)
					Ctx.Defer().PushCommand<FLNPApplyPlayerBurnGECommand>(Actor, C.BurnDamagePerStack, C.BurnDuration, C.BurnTickInterval);
			}
		}
	});
}

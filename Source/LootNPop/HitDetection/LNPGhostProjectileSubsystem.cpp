// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	/** Dead Reckoning 외삽 상한. Lag Compensation 되감기 클램프(섹션 5.0)와 동일한 200ms. */
	constexpr float MaxExtrapolationSeconds = 0.2f;

	/** RegisterGhost 만료 여유 — 발사체 수명 자연 소멸(MovementProcessor)이 항상 먼저 오도록 넉넉히 잡는다. */
	constexpr float GhostExpiryMarginSeconds = 1.0f;

	/** 로컬 임팩트 기록 보존 시간 — 서버 확정 큐가 이 안에 도착하지 않으면 기록을 버린다. */
	constexpr double RecentLocalImpactRetentionSeconds = 2.0;
}

int32 ULNPGhostProjectileSubsystem::IssueServerSalvoID()
{
	// 65536부터 시작 — uint16 예측 키(<= 65535)와 키 공간이 겹치지 않는다.
	static FThreadSafeCounter Counter(65535);
	return Counter.Increment();
}

void ULNPGhostProjectileSubsystem::DestroyEntity(FMassEntityHandle Entity)
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem)
		return;

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
		return;

	// 트레일 Niagara Component는 EnqueueTrailRelease로 명시적으로 해제해야 한다 (자동 정리 없음, LNPProjectileVisualSubsystem 참조).
	if (const FLNPProjectileVisualFragment* Visual = EntityManager.GetFragmentDataPtr<FLNPProjectileVisualFragment>(Entity))
	{
		if (Visual->bInitialized)
		{
			if (ULNPProjectileVisualSubsystem* VisualSub = World->GetSubsystem<ULNPProjectileVisualSubsystem>())
				VisualSub->EnqueueTrailRelease(Entity);
		}
	}

	EntityManager.Defer().AddTag<FLNPProjectileDeadTag>(Entity);
}

void ULNPGhostProjectileSubsystem::RegisterGhost(FMassEntityHandle Entity, const FLNPGhostKey& Key, float LifetimeSeconds)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Ghosts.Add(Key, { Entity, Now + LifetimeSeconds + GhostExpiryMarginSeconds });
}

bool ULNPGhostProjectileSubsystem::DestroyGhost(const FLNPGhostKey& Key)
{
	FGhostEntry Entry;
	if (!Ghosts.RemoveAndCopyValue(Key, Entry))
		return false;

	DestroyEntity(Entry.Entity);
	return true;
}

bool ULNPGhostProjectileSubsystem::DestroyGhostFromLocalImpact(const FLNPGhostKey& Key)
{
	if (!DestroyGhost(Key))
		return false;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RecentLocalImpacts.Add(Key, Now);
	return true;
}

bool ULNPGhostProjectileSubsystem::ConsumeRecentLocalImpact(const FLNPGhostKey& Key)
{
	return RecentLocalImpacts.Remove(Key) > 0;
}

void ULNPGhostProjectileSubsystem::DestroyAllGhostsForKey(int32 InstigatorPlayerID, int32 KeyOrSalvo)
{
	for (auto It = Ghosts.CreateIterator(); It; ++It)
	{
		if (It->Key.InstigatorPlayerID != InstigatorPlayerID || It->Key.KeyOrSalvo != KeyOrSalvo)
			continue;

		DestroyEntity(It->Value.Entity);
		It.RemoveCurrent();
	}
}

void ULNPGhostProjectileSubsystem::SweepExpiredGhosts()
{
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	const FMassEntityManager* EntityManager = MassSubsystem ? &MassSubsystem->GetEntityManager() : nullptr;

	for (auto It = Ghosts.CreateIterator(); It; ++It)
	{
		// 엔티티가 이미 소멸(지형 충돌·수명 등 임팩트 큐가 없는 경로)한 stale 엔트리는 만료와 무관하게 즉시 GC.
		// ProjectileLifetime이 매우 커도(구 내벽 충돌까지 유지하는 의도적 설계) 맵이 세션 내내 누적되지 않는다.
		if (EntityManager && !EntityManager->IsEntityValid(It->Value.Entity))
		{
			It.RemoveCurrent();
			continue;
		}

		// per-entry 만료: 발사체 수명 + 여유가 지난 항목만 정리한다 (브랜치 A/B·자연 소멸 모두 미도달 시 안전망).
		if (Now < It->Value.ExpiryTime)
			continue;

		DestroyEntity(It->Value.Entity);
		It.RemoveCurrent();
	}

	for (auto It = RecentLocalImpacts.CreateIterator(); It; ++It)
	{
		if (Now - It->Value >= RecentLocalImpactRetentionSeconds)
			It.RemoveCurrent();
	}
}

void ULNPGhostProjectileSubsystem::SpawnSpectatorGhosts(const FLNPProjectileSharedFragment& SharedData, FVector SpawnPos,
	TConstArrayView<FVector> Velocities, float LifetimeSeconds, ELNPInstigatorTeam InstigatorTeam,
	int32 InstigatorPlayerID, int32 KeyOrSalvo, float UpstreamDelaySeconds)
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem)
		return;

	// Dead Reckoning: 방송이 도착한 시점엔 이미 (발신 전 지연 + 서버→나 RTT/2)만큼 시간이 흘렀다.
	// 그만큼 속도 방향으로 외삽해 스폰하면 서버 실제 발사체와의 시작 괴리가 줄어든다.
	float LocalHalfRTT = 0.f;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APlayerState* PS = PC->PlayerState)
			LocalHalfRTT = PS->GetPingInMilliseconds() * 0.0005f;
	}
	const float ExtrapolateSeconds = FMath::Clamp(UpstreamDelaySeconds + LocalHalfRTT, 0.f, MaxExtrapolationSeconds);
	const float RemainingLifetime  = LifetimeSeconds - ExtrapolateSeconds;
	if (RemainingLifetime <= 0.f)
		return; // 외삽 시점에 이미 수명이 다한 발사체 — 스폰 생략

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FConstSharedStruct SharedStruct = EntityManager.GetOrCreateConstSharedFragment(SharedData);
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(SharedStruct);

	// 관전 전용(순수 시각) 엔티티 — 서버 확정 GameplayCue.LNP.Projectile.Impact가 도착하면
	// 동일 FLNPGhostKey로 정리된다. 로컬 코스메틱 충돌·수명·지형 판정으로도 자연 소멸한다.
	for (int32 i = 0; i < Velocities.Num(); ++i)
	{
		const uint8 SpawnIndex = static_cast<uint8>(i);
		const FMassEntityHandle Entity = EntityManager.ReserveEntity();
		const FVector ExtrapolatedPos  = SpawnPos + Velocities[i] * ExtrapolateSeconds;

		FLNPProjectileFragment FragData;
		FragData.PreviousPos        = ExtrapolatedPos;
		FragData.SpawnLocation      = ExtrapolatedPos;
		FragData.Velocity           = Velocities[i];
		FragData.LifetimeRemaining  = RemainingLifetime;
		FragData.InstigatorTeam     = InstigatorTeam;
		FragData.bIsLocalInstigator = false;
		FragData.InstigatorPlayerID = InstigatorPlayerID;
		FragData.PredictionKeyID    = KeyOrSalvo;
		FragData.SpawnIndex         = SpawnIndex;

		FLNPProjectileVisualFragment VisualFrag;
		FTransformFragment TransFrag;
		TransFrag.GetMutableTransform().SetLocation(ExtrapolatedPos);

		FMassArchetypeSharedFragmentValues SharedValuesCopy = SharedValues;
		EntityManager.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
			FMassArchetypeSharedFragmentValues,
			FLNPProjectileFragment,
			FLNPProjectileVisualFragment,
			FTransformFragment>>(
			Entity,
			MoveTemp(SharedValuesCopy),
			FragData,
			VisualFrag,
			TransFrag);

		RegisterGhost(Entity, { InstigatorPlayerID, KeyOrSalvo, SpawnIndex }, RemainingLifetime);
	}
}

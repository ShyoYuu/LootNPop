// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "LootNPop.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	/** Dead Reckoning 외삽 상한. 되감기의 **핑 항** 상한(LNPHitDetection::MaxPingRewindSeconds)과 같은 200ms다 —
	 *  둘 다 "왕복 지연을 얼마까지 갚아줄 것인가"라는 같은 질문의 반대 방향(외삽/되감기)이라 값을 맞춘다.
	 *  ⚠️ 되감기 **총량**과는 무관하다. 되감기에는 클라 보간 지연 항이 따로 더해져 최대 0.5초까지 간다. */
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

	// IsEntityActive가 아니라 IsEntityValid다 — 스폰 방송과 서버 착탄 큐가 같은 프레임에 도착하면 Ghost는
	// 아직 예약(Reserved)만 되고 생성 커맨드가 flush되기 전이라 Active가 아니다. 거기서 조기 반환하면
	// 맵에서는 빠졌는데 태그는 안 붙어, Ghost가 캐릭터를 관통해 계속 비행한다. 커맨드 버퍼는 생성을
	// 태그 추가보다 먼저 실행하므로 예약 상태에서 걸어도 안전하다.
	// 트레일 해제는 ULNPProjectileDestructionProcessor가 Dead 태그를 보고 한다.
	if (!EntityManager.IsEntityValid(Entity))
		return;

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
	int32 DestroyedCount = 0;
	for (auto It = Ghosts.CreateIterator(); It; ++It)
	{
		if (It->Key.InstigatorPlayerID != InstigatorPlayerID || It->Key.KeyOrSalvo != KeyOrSalvo)
			continue;

		DestroyEntity(It->Value.Entity);
		It.RemoveCurrent();
		++DestroyedCount;
	}

	// 서버 거부로 예측 Ghost가 VFX 없이 사라지는 유일한 경로라 드물게 한 줄 남긴다.
	UE_LOG(LogLootNPop, Log, TEXT("Ghost: prediction key %d rejected by server. Destroyed %d predicted ghosts."), KeyOrSalvo, DestroyedCount);
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
	const ULNPMassWorldCollisionSubsystem* WorldCollision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

	FConstSharedStruct SharedStruct = EntityManager.GetOrCreateConstSharedFragment(SharedData);
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(SharedStruct);

	// 관전 전용(순수 시각) 엔티티 — 서버 확정 GameplayCue.LNP.Projectile.Impact가 도착하면
	// 동일 FLNPGhostKey로 정리된다. 로컬 코스메틱 충돌·수명·지형 판정으로도 자연 소멸한다.
	for (int32 i = 0; i < Velocities.Num(); ++i)
	{
		const uint8 SpawnIndex = static_cast<uint8>(i);

		// 외삽도 실제 비행과 같은 적분을 쓴다. 포물선 탄을 직선으로 외삽하면 시작부터 어긋난다.
		// 속도 Verlet은 상수 중력에서 스텝 분할에 불변이므로 이 구간(최대 200ms)은 1스텝으로 정확하다.
		// **속도까지 갱신해 넣어야** 이후 비행이 서버 발사체와 겹친다.
		FVector ExtrapolatedPos = SpawnPos;
		FVector ExtrapolatedVel = Velocities[i];
		LNPProjectileMotion::Step(ExtrapolatedPos, ExtrapolatedVel, SharedData.GravityAccel, ExtrapolateSeconds);

		// 외삽 구간은 비행 프레임이 아니라 월드 판정을 받지 않는다. 그 사이에 지면·프랍이 있으면 Ghost가 그 너머에서
		// 태어나 지각 아래·프랍 뒤를 날게 된다. 실탄과 같은 함수로 검사해 이미 착탄한 탄은 그리지 않는다 —
		// 착탄 VFX는 로컬 임팩트 기록이 없으므로 서버 확정 큐가 서버 위치에 재생한다.
		FLNPWorldHit ExtrapolationHit;
		if (WorldCollision && LNPProjectileMotion::TraceWorld(*WorldCollision, SpawnPos, ExtrapolatedPos, ExtrapolationHit))
			continue;

		const FMassEntityHandle Entity = EntityManager.ReserveEntity();

		FLNPProjectileFragment FragData;
		FragData.PreviousPos        = ExtrapolatedPos;
		FragData.SpawnLocation      = ExtrapolatedPos;
		FragData.Velocity           = ExtrapolatedVel;
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

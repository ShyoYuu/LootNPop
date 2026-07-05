// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPGhostProjectileSubsystem.generated.h"

/**
 * Ghost Projectile(로컬 시각 전용 발사체 엔티티)의 전역 고유 식별자.
 * PredictionKey는 클라이언트별 로컬 카운터라 단독으로는 전역 고유가 아니므로 공격자
 * InstigatorPlayerID를 함께 묶는다. 예측 키가 없는 발사(리슨 호스트·NPC·패링 반사)는
 * KeyOrSalvo에 서버 발급 SalvoID(>= 65536 — uint16 예측 키 범위와 겹치지 않음)가 들어간다.
 */
struct FLNPGhostKey
{
	int32 InstigatorPlayerID = INDEX_NONE;
	int32 KeyOrSalvo         = 0;
	uint8 SpawnIndex         = 0;

	bool operator==(const FLNPGhostKey& Other) const
	{
		return InstigatorPlayerID == Other.InstigatorPlayerID
			&& KeyOrSalvo == Other.KeyOrSalvo
			&& SpawnIndex == Other.SpawnIndex;
	}

	friend uint32 GetTypeHash(const FLNPGhostKey& Key)
	{
		return HashCombine(HashCombine(::GetTypeHash(Key.InstigatorPlayerID), ::GetTypeHash(Key.KeyOrSalvo)), ::GetTypeHash(Key.SpawnIndex));
	}
};

/**
 * 클라이언트가 로컬로 스폰한 Projectile Mass 엔티티(Ghost)의 수명을 관리한다.
 * 서버 확정 결과(GameplayCue.LNP.Projectile.Impact)와 클라이언트 로컬 판정(HitDetectionProcessor
 * 클라 분기) 중 먼저 도착하는 쪽이 Ghost를 소멸시키고, 나머지 한쪽은 못 찾아 no-op 처리된다.
 * 게임 스레드 전용 — Mass 워커 스레드에서 호출하지 않는다 (IssueServerSalvoID만 예외적으로 스레드 세이프).
 */
UCLASS()
class LOOTNPOP_API ULNPGhostProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** 예측 키가 없는 발사(호스트·NPC·패링 반사)에 부여할 전역 고유 ID를 발급한다. 스레드 세이프 — Mass 워커 스레드 호출 허용. */
	static int32 IssueServerSalvoID();

	/** Ghost 엔티티를 등록한다. LifetimeSeconds + 여유(1s)가 지나면 SweepExpiredGhosts가 안전망으로 정리한다. */
	void RegisterGhost(FMassEntityHandle Entity, const FLNPGhostKey& Key, float LifetimeSeconds);

	/** 등록된 Ghost를 찾아 파괴(FLNPProjectileDeadTag 부여)하고 맵에서 제거한다. 찾았으면 true. */
	bool DestroyGhost(const FLNPGhostKey& Key);

	/** 클라이언트 로컬 코스메틱 판정 경유 파괴 — 서버 확정 큐 도착 시 임팩트 VFX를
	 *  중복 재생하지 않도록 키를 잠시 기록해 둔다 (ConsumeRecentLocalImpact로 소거). */
	bool DestroyGhostFromLocalImpact(const FLNPGhostKey& Key);

	/** 서버 확정 큐 처리 시 호출 — 로컬 판정이 이미 임팩트를 재생했으면 true를 반환하고 기록을 소거한다. */
	bool ConsumeRecentLocalImpact(const FLNPGhostKey& Key);

	/** 어빌리티 활성화가 서버에서 거부됐을 때, 해당 발사 전체(모든 SpawnIndex)의 Ghost를 롤백한다. */
	void DestroyAllGhostsForKey(int32 InstigatorPlayerID, int32 KeyOrSalvo);

	/** per-entry 만료 시각이 지난 Ghost(브랜치 A/B 둘 다 미도달)와 오래된 로컬 임팩트 기록을 정리한다. */
	void SweepExpiredGhosts();

	/**
	 * 관전용 Ghost 엔티티 스폰 공용 경로 — 발사 방송·패링 반사 방송이 공유한다.
	 * Dead Reckoning: UpstreamDelaySeconds(발신 시점 이전에 이미 흐른 지연) + 수신자 자신의 RTT/2만큼
	 * 속도 방향으로 외삽(클램프 200ms)해 서버 실제 위치와의 시작 괴리를 줄인다.
	 */
	void SpawnSpectatorGhosts(const FLNPProjectileSharedFragment& SharedData, FVector SpawnPos,
		TConstArrayView<FVector> Velocities, float LifetimeSeconds, ELNPInstigatorTeam InstigatorTeam,
		int32 InstigatorPlayerID, int32 KeyOrSalvo, float UpstreamDelaySeconds);

private:
	struct FGhostEntry
	{
		FMassEntityHandle Entity;
		double            ExpiryTime = 0.0;
	};

	void DestroyEntity(FMassEntityHandle Entity);

	TMap<FLNPGhostKey, FGhostEntry> Ghosts;

	/** 로컬 코스메틱 판정이 임팩트를 재생한 키 → 기록 시각. 서버 확정 큐의 VFX 중복 재생 방지용. */
	TMap<FLNPGhostKey, double> RecentLocalImpacts;
};

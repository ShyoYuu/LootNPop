// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationProcessor.h"
#include "LNPMassReplicator.generated.h"

/**
 * 통합 서버 Replicator — 월드의 모든 복제 대상 Mass 엔티티 타입을 단일 스트림으로 처리한다.
 * 타입 분기는 FLNPEnemyFragment의 Optional 요구로 청크 단위 판별:
 * - Enemy 청크: Add 시 타입 태그 + 매 갱신 위치/Yaw 반영 (서버 AI가 매 틱 이동시키므로)
 * - 그 외(Player·LootPod) 청크: Add 시 위치 1회만, 이후 갱신 없음 (구 SpawnOnly 패턴)
 * 통합 배경: LNPMassReplication.h 및 EngineAnalysis_MassReplication.md §7.1 참조.
 */
// ⚠️ Abstract로 두지 않는다 — DA_PlayerEntityConfig가 이 클래스를 ReplicatorClass로 직접 참조하며,
//    플레이어는 타입이 하나뿐이라 별도 서브클래스 없이 이 클래스가 그대로 고유 CDO 역할을 한다.
UCLASS()
class LOOTNPOP_API ULNPMassReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};

/**
 * **타입마다 리플리케이터 클래스 1개** — 엔진이 의도한 타입 분리 축이다.
 * `UMassReplicationProcessor`는 공유 프래그먼트마다 전용 쿼리를 만들고 그 요구사항을
 * `CachedReplicator->AddRequirements`로 채우므로, 분리 단위가 곧 리플리케이터다
 * (엔진 주석: *"derive from this per entity type"*, `MassReplicationProcessor.h:23`).
 *
 * 아래 두 클래스는 요구사항이 베이스와 같아서 **동작이 하나도 다르지 않다.**
 * 그래도 나누는 이유는 `FMassReplicationSharedFragment`의 중복 제거 CRC에 들어가는 UPROPERTY가
 * 사실상 `CachedReplicator`(= ReplicatorClass의 CDO) 하나뿐이라, **클래스가 같으면
 * 타입별 컬 거리가 통째로 뭉개지기 때문**이다. 근거와 실측은 `LNP::Replication::ConfigureParams` 주석.
 *
 * ⚠️ 복제 대상 Mass 타입을 새로 추가하면 **반드시 여기에 전용 서브클래스를 하나 더 만든다.**
 * 빠뜨리면 컬 거리가 조용히 다른 타입 값으로 대체된다 — 크래시도 경고도 없다.
 * (`ConfigureParams`의 checkf가 베이스 클래스 직접 사용만 막아 준다.)
 *
 * 버블 **핸들러**(`FLNPMassClientBubbleHandler`)는 여전히 하나이므로 §7.1 불변식은 유지된다 —
 * 제약은 버블 클래스 수가 아니라 핸들 발급자 수다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyReplicator : public ULNPMassReplicator
{
	GENERATED_BODY()
};

UCLASS()
class LOOTNPOP_API ULNPLootPodReplicator : public ULNPMassReplicator
{
	GENERATED_BODY()
};

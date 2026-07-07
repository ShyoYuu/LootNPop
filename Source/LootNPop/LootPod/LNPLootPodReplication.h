// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Replication/LNPSpawnOnlyReplication.h"
#include "MassClientBubbleInfoBase.h"
#include "LNPLootPodReplication.generated.h"

/**
 * LootPod 개체별 복제 데이터 — 존재 + 스폰 시점 위치/Yaw 1회 (Phase 7).
 *
 * LootPod은 정적이므로 위치는 스폰(Add) 시 1회만 전송한다 (§5.4).
 * PodState·게이지는 Actor 복제(ALNPLootPod)가 담당하므로 스키마에서 제외한다.
 * 복제된 Pod Actor가 근접 시 도착하면 UMassAgentComponent NetID 퍼펫 핸드셰이크로
 * bubble이 스폰한 이 엔티티와 자동 연결된다 (Enemy와 동일 — 스포너 경로라 NetID 캐싱 문제 없음).
 * (공용 구현·설계 배경: LNPSpawnOnlyReplication.h)
 */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedLootPodAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()

	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

	/** FReplicatedAgentBase 파생 구조체가 반드시 제공해야 하는 접근자 (TMassClientBubbleTransformHandler 요구사항) */
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }

private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;
};

/** Fast Array 복제 항목. 스폰(Add) 이후에는 갱신하지 않는다 — Dirty 마킹 없음. */
USTRUCT()
struct LOOTNPOP_API FLNPLootPodFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FLNPLootPodFastArrayItem() = default;
	FLNPLootPodFastArrayItem(const FLNPReplicatedLootPodAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** FMassFastArrayItemBase 파생 구조체가 반드시 제공해야 하는 typedef */
	typedef FLNPReplicatedLootPodAgent FReplicatedAgentType;

	UPROPERTY()
	FLNPReplicatedLootPodAgent Agent;
};

/** LootPod 타입 전용 Client Bubble 핸들러 — 스폰 1회 복제 공용 구현을 그대로 사용한다. */
using FLNPLootPodClientBubbleHandler = TLNPSpawnOnlyBubbleHandler<FLNPLootPodFastArrayItem>;

/** 클라이언트당 하나씩 존재하며 LootPod Fast Array 복제를 담당한다. */
USTRUCT()
struct FLNPLootPodClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FLNPLootPodClientBubbleSerializer()
	{
		Bubble.Initialize(LootPods, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPLootPodFastArrayItem, FLNPLootPodClientBubbleSerializer>(LootPods, DeltaParams, *this);
	}

	FLNPLootPodClientBubbleHandler Bubble;

protected:
	/** 서버에서는 프리 리스트로 유지된다 (인덱스가 핸들로 쓰이므로). 서버·클라 간 배열 순서는 보장되지 않으며 FMassNetworkID로만 식별 가능. */
	UPROPERTY(Transient)
	TArray<FLNPLootPodFastArrayItem> LootPods;
};

template<>
struct TStructOpsTypeTraits<FLNPLootPodClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FLNPLootPodClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/** 클라이언트 접속마다 하나씩 자동 스폰되는 복제 전용 Actor (Owner = 해당 PlayerController). */
UCLASS()
class LOOTNPOP_API ALNPLootPodClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()

public:
	ALNPLootPodClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FLNPLootPodClientBubbleSerializer& GetLootPodSerializer() { return LootPodSerializer; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FLNPLootPodClientBubbleSerializer LootPodSerializer;
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Replication/LNPSpawnOnlyReplication.h"
#include "MassClientBubbleInfoBase.h"
#include "LNPPlayerReplication.generated.h"

/**
 * Player 개체별 복제 데이터 — 존재 + 스폰 시점 위치/Yaw 1회.
 *
 * 위치를 매 프레임 복제하지 않는 이유 (Phase 6.5 설계):
 * 플레이어 위치는 이미 Mover Actor 복제가 고빈도·예측·보간으로 전달하므로 Mass 채널로 또 보내면 순수 중복이다.
 * 이 bubble의 역할은 "클라이언트 월드에 엔티티를 존재하게 만들고, NetID 퍼펫 핸드셰이크로 복제된 폰과
 * 자동 연결시키는 것"뿐이다. 연결 후 엔티티 Transform은 클라이언트 로컬에서 보간된 폰 액터를 따라간다.
 * (공용 구현·설계 배경: LNPSpawnOnlyReplication.h)
 */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedPlayerAgent : public FReplicatedAgentBase
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
struct LOOTNPOP_API FLNPPlayerFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FLNPPlayerFastArrayItem() = default;
	FLNPPlayerFastArrayItem(const FLNPReplicatedPlayerAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** FMassFastArrayItemBase 파생 구조체가 반드시 제공해야 하는 typedef */
	typedef FLNPReplicatedPlayerAgent FReplicatedAgentType;

	UPROPERTY()
	FLNPReplicatedPlayerAgent Agent;
};

/** Player 타입 전용 Client Bubble 핸들러 — 스폰 1회 복제 공용 구현을 그대로 사용한다. */
using FLNPPlayerClientBubbleHandler = TLNPSpawnOnlyBubbleHandler<FLNPPlayerFastArrayItem>;

/** 클라이언트당 하나씩 존재하며 Player Fast Array 복제를 담당한다. */
USTRUCT()
struct FLNPPlayerClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FLNPPlayerClientBubbleSerializer()
	{
		Bubble.Initialize(Players, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPPlayerFastArrayItem, FLNPPlayerClientBubbleSerializer>(Players, DeltaParams, *this);
	}

	FLNPPlayerClientBubbleHandler Bubble;

protected:
	/** 서버에서는 프리 리스트로 유지된다 (인덱스가 핸들로 쓰이므로). 서버·클라 간 배열 순서는 보장되지 않으며 FMassNetworkID로만 식별 가능. */
	UPROPERTY(Transient)
	TArray<FLNPPlayerFastArrayItem> Players;
};

template<>
struct TStructOpsTypeTraits<FLNPPlayerClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FLNPPlayerClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/** 클라이언트 접속마다 하나씩 자동 스폰되는 복제 전용 Actor (Owner = 해당 PlayerController). */
UCLASS()
class LOOTNPOP_API ALNPPlayerClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()

public:
	ALNPPlayerClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FLNPPlayerClientBubbleSerializer& GetPlayerSerializer() { return PlayerSerializer; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FLNPPlayerClientBubbleSerializer PlayerSerializer;
};

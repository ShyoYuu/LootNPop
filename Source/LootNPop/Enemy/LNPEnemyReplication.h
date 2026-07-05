// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassEntityView.h"
#include "GameplayTagContainer.h"
#include "LNPEnemyReplication.generated.h"

/** Enemy 개체별 복제 데이터 — 위치/Yaw(Low LOD 시각화용) + 타입 태그(스폰 시 1회 고정). */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedEnemyAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()

	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

	/** FReplicatedAgentBase 파생 구조체가 반드시 제공해야 하는 접근자 (TMassClientBubbleTransformHandler 요구사항) */
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }

	FGameplayTag GetEnemyTypeTag() const { return EnemyTypeTag; }
	void SetEnemyTypeTag(FGameplayTag InTag) { EnemyTypeTag = InTag; }

private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;

	UPROPERTY(Transient)
	FGameplayTag EnemyTypeTag;
};

/** Fast Array 복제 항목. FLNPReplicatedEnemyAgent 멤버가 바뀌면 반드시 Dirty 표시할 것. */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FLNPEnemyFastArrayItem() = default;
	FLNPEnemyFastArrayItem(const FLNPReplicatedEnemyAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** FMassFastArrayItemBase 파생 구조체가 반드시 제공해야 하는 typedef */
	typedef FLNPReplicatedEnemyAgent FReplicatedAgentType;

	UPROPERTY()
	FLNPReplicatedEnemyAgent Agent;
};

/** Enemy 타입 전용 Client Bubble 핸들러. 위치/Yaw 복제 + FLNPEnemyFragment::EnemyTypeTag 스폰 시 반영을 담당한다. */
class FLNPEnemyClientBubbleHandler : public TClientBubbleHandlerBase<FLNPEnemyFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FLNPEnemyFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FLNPEnemyFastArrayItem> FMassClientBubbleTransformHandler;

	FLNPEnemyClientBubbleHandler()
		: TransformHandler(*this)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE
	const FMassClientBubbleTransformHandler& GetTransformHandler() const { return TransformHandler; }
	FMassClientBubbleTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

	FMassClientBubbleTransformHandler TransformHandler;

#if UE_REPLICATION_COMPILE_CLIENT_CODE
	TArrayView<struct FLNPEnemyFragment> EnemyFragmentList;
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE
};

/** 클라이언트당 하나씩 존재하며 Enemy Fast Array 복제를 담당한다. */
USTRUCT()
struct FLNPEnemyClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FLNPEnemyClientBubbleSerializer()
	{
		Bubble.Initialize(Enemies, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPEnemyFastArrayItem, FLNPEnemyClientBubbleSerializer>(Enemies, DeltaParams, *this);
	}

	FLNPEnemyClientBubbleHandler Bubble;

protected:
	/** 서버에서는 프리 리스트로 유지된다 (인덱스가 핸들로 쓰이므로). 서버·클라 간 배열 순서는 보장되지 않으며 FMassNetworkID로만 식별 가능. */
	UPROPERTY(Transient)
	TArray<FLNPEnemyFastArrayItem> Enemies;
};

template<>
struct TStructOpsTypeTraits<FLNPEnemyClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FLNPEnemyClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/** 클라이언트 접속마다 하나씩 자동 스폰되는 복제 전용 Actor (Owner = 해당 PlayerController). */
UCLASS()
class LOOTNPOP_API ALNPEnemyClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()

public:
	ALNPEnemyClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FLNPEnemyClientBubbleSerializer& GetEnemySerializer() { return EnemySerializer; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FLNPEnemyClientBubbleSerializer EnemySerializer;
};

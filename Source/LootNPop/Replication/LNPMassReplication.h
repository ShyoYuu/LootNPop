// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassEntityView.h"
#include "GameplayTagContainer.h"
#include "LNPMassReplication.generated.h"

/**
 * 통합 Mass 복제 — 월드의 모든 복제 대상 Mass 엔티티 타입(Enemy·Player·LootPod)이
 * 단일 버블/리플리케이터를 공유한다.
 *
 * 통합 이유: 엔진의 파괴 처리 경로(CalculateClientReplication의 클라 장부 AgentsData 순회)는
 * 타입 필터가 없어, 버블 클래스가 2개 이상이면 타 타입 엔티티의 파괴 엔트리를 자기 버블 핸들로
 * 제거 시도한다 — RemoveAgentChecked checkf 크래시 또는 silent corruption.
 * 버블이 하나면 파괴 루프의 실행자와 핸들 소유자가 항상 일치해 원천 차단된다.
 * 이종 아키타입 스폰은 엔진이 TemplateID 그룹핑으로 자동 처리한다.
 * (상세: EngineAnalysis_MassReplication.md §7.1)
 *
 * 타입별 페이로드 차이:
 * - Enemy: 위치/Yaw 매 갱신 + EnemyTypeTag 스폰 1회 (Low LOD 시각화용)
 * - Player·LootPod: 스폰 1회(존재 + 초기 위치)만 — 지속 위치는 Actor 복제 채널(Mover, LootPod Actor)이
 *   담당하거나 아예 불필요(정적)하다. 스폰 위치를 1회 싣는 이유: 퍼펫 링크 시 엔진
 *   (UMassAgentComponent::SetEntityHandleInternal)이 엔티티 Transform으로 Actor 위치를 초기화하므로,
 *   비워두면 원점으로 튄다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedAgent : public FReplicatedAgentBase
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

	/** Enemy 엔티티만 세팅한다 — Player·LootPod은 빈 태그로 남는다 (직렬화 비용 미미). */
	UPROPERTY(Transient)
	FGameplayTag EnemyTypeTag;
};

/** Fast Array 복제 항목. FLNPReplicatedAgent 멤버가 바뀌면 반드시 Dirty 표시할 것. */
USTRUCT()
struct LOOTNPOP_API FLNPMassFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FLNPMassFastArrayItem() = default;
	FLNPMassFastArrayItem(const FLNPReplicatedAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** FMassFastArrayItemBase 파생 구조체가 반드시 제공해야 하는 typedef */
	typedef FLNPReplicatedAgent FReplicatedAgentType;

	UPROPERTY()
	FLNPReplicatedAgent Agent;
};

/**
 * 통합 Client Bubble 핸들러.
 * 클라 수신 시 TemplateID별 배치 스폰(엔진 헬퍼) + 위치/Yaw 반영 + Enemy 아키타입만 타입 태그 기록.
 */
class FLNPMassClientBubbleHandler : public TClientBubbleHandlerBase<FLNPMassFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FLNPMassFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FLNPMassFastArrayItem> FMassClientBubbleTransformHandler;

	FLNPMassClientBubbleHandler()
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
};

/** 클라이언트당 하나씩 존재하며 통합 Fast Array 복제를 담당한다. */
USTRUCT()
struct FLNPMassClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FLNPMassClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPMassFastArrayItem, FLNPMassClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

	FLNPMassClientBubbleHandler Bubble;

protected:
	/** 서버에서는 프리 리스트로 유지된다 (인덱스가 핸들로 쓰이므로). 서버·클라 간 배열 순서는 보장되지 않으며 FMassNetworkID로만 식별 가능. */
	UPROPERTY(Transient)
	TArray<FLNPMassFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FLNPMassClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FLNPMassClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/** 클라이언트 접속마다 하나씩 자동 스폰되는 복제 전용 Actor (Owner = 해당 PlayerController). */
UCLASS()
class LOOTNPOP_API ALNPMassClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()

public:
	ALNPMassClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FLNPMassClientBubbleSerializer& GetAgentSerializer() { return AgentSerializer; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FLNPMassClientBubbleSerializer AgentSerializer;
};

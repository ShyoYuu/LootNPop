// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassEntityView.h"
#include "GameplayTagContainer.h"
#include "LNPMassReplication.generated.h"

struct FMassReplicationParameters;

namespace LNP::Replication
{
	/**
	 * 구 내벽(Dyson Sphere) 월드의 접평면 기저 — 월드 원점 방향이 Up이다.
	 * (LNPPawnGravityComponent::GetUpDirection의 RadialOutward, GravityOrigin=ZeroVector와 동일 부호)
	 *
	 * 엔진 기본 경로(TMassClientBubbleTransformHandler::SetEntityData)는 클라이언트에서 자세를
	 * FQuat(FVector::UpVector, Yaw) — 즉 월드 Z축 Yaw로만 복원하므로, 구면 위에서는 적도 부근 엔티티가
	 * 통째로 누워버린다. 그래서 Yaw를 "월드 Yaw"가 아니라 "이 기저 기준 로컬 Yaw"로 인코딩하고,
	 * 기저 자체는 이미 복제 중인 위치에서 양쪽이 재구성한다 (추가 대역폭 0).
	 */
	inline FQuat MakeSphereTangentBasis(const FVector& Position)
	{
		return FRotationMatrix::MakeFromZ(-Position.GetSafeNormal()).ToQuat();
	}

	/** 서버: 월드 자세 → 접평면 기준 로컬 Yaw (라디안). */
	inline float EncodeSphereLocalYaw(const FTransform& Transform)
	{
		const FQuat LocalRotation = MakeSphereTangentBasis(Transform.GetLocation()).Inverse() * Transform.GetRotation();
		return static_cast<float>(FMath::DegreesToRadians(LocalRotation.Rotator().Yaw));
	}

	/** 클라이언트: 복제된 위치 + 로컬 Yaw → 월드 자세. */
	inline FQuat DecodeSphereRotation(const FVector& Position, const float LocalYaw)
	{
		return MakeSphereTangentBasis(Position) * FQuat(FVector::UpVector, LocalYaw);
	}

	/**
	 * 통합 복제 스트림용 파라미터 설정 — 모든 LNP 트레잇이 이 함수 하나로 Params를 채운다.
	 * CullDistance는 EMassLOD::Off 경계(= 이 거리를 넘으면 클라이언트 버블에서 제거)이며,
	 * 반드시 해당 EntityConfig의 시각화 VisibleLODDistance[Off]와 같은 값으로 맞춰야 한다.
	 * 더 크면 렌더링되지도 않을 엔티티에 대역폭을 쓰고, 더 작으면 서버엔 보이는데 클라엔 안 보인다.
	 */
	LOOTNPOP_API void ConfigureParams(FMassReplicationParameters& Params, float CullDistance);
}

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
 * 클라 수신 시 TemplateID별 배치 스폰(엔진 헬퍼) + 위치/자세 반영 + Enemy 아키타입만 타입 태그 기록.
 *
 * 자세 복원은 엔진의 TMassClientBubbleTransformHandler를 쓰지 않고 직접 처리한다 —
 * 엔진 구현은 월드 Z축 Yaw만 복원해 구 내벽 월드에서 엔티티가 눕는다 (LNP::Replication 주석 참조).
 */
class FLNPMassClientBubbleHandler : public TClientBubbleHandlerBase<FLNPMassFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FLNPMassFastArrayItem> Super;

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;

	/** 복제 페이로드(위치 + 접평면 로컬 Yaw)를 Transform Fragment에 반영한다. 스케일은 보존한다. */
	static void ApplyReplicatedTransform(FTransformFragment& TransformFragment, const FReplicatedAgentPositionYawData& PositionYaw);

	/** 스폰 쿼리 순회 동안만 유효한 Transform Fragment 뷰 (엔진 핸들러의 TransformList 대체). */
	TArrayView<FTransformFragment> SpawnTransformList;
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE
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

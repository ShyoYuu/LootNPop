// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "Templates/SubclassOf.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassEntityView.h"
#include "MassProcessor.h"
#include "GameplayTagContainer.h"
#include "LNPMassReplication.generated.h"

struct FMassReplicationParameters;
class UMassReplicatorBase;

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
	 *
	 * ⚠️ **퍼펫 타입은 여기에 더해 "Actor 릴러번시 < CullDistance"를 지켜야 한다.**
	 * `UMassAgentComponent`로 링크되는 Actor(적·LootPod·플레이어 폰)는 엔진이
	 * "Actor가 사는 동안 엔티티는 유효하다"를 전제로 상태를 검사한다. 릴러번시가 컬 거리보다 크면
	 * 그 사이 거리대에서 **엔티티만 버블에서 빠지고 Actor는 남아** assert가 난다
	 * (2026-09-03 실측: 적이 컬 12,000 / 릴러번시 기본 15,000이라 120~150m 구간에서 발생).
	 * 히스테리시스(`BufferHysteresisOnDistancePercentage` 10%)를 감안해 여유를 두고 잡을 것.
	 *
	 * | 타입 | 버블 컬 | Actor 릴러번시 |
	 * |:---|---:|---:|
	 * | Enemy | 12,000 | 8,000 (`ALNPEnemyCharacter` 생성자) |
	 * | LootPod | 60,000 | 20,000 (`ALNPLootPod` 생성자) |
	 * | 플레이어 폰 | 1,000,000 (DA) | 15,000 (엔진 기본) |
	 *
	 * ⚠️ **ReplicatorClass는 타입마다 서로 다른 클래스여야 한다. 이것이 CullDistance를 유효하게 만든다.**
	 *
	 * 이것은 우회가 아니라 **엔진이 의도한 타입 분리 축 그 자체다.**
	 * `UMassReplicationProcessor::PrepareExecution`은 공유 프래그먼트마다 전용 쿼리를 만들어
	 * 자기 청크로 한정하고 그 요구사항을 `CachedReplicator->AddRequirements`로 채운다 —
	 * 즉 분리 단위가 곧 리플리케이터다. 엔진 주석도 *"derive from this per entity type"*
	 * (`MassReplicationProcessor.h:23`)라고 적어 두었다.
	 * (다만 정석에서 나누는 동기는 *쿼리 요구사항 차이*이고, 여기서 나누는 동기는 *해시 분리*다.
	 *  요구사항이 같은 타입들이므로 결과물은 동작이 같은 빈 서브클래스가 된다 — 형태는 정석, 동기는 우회.)
	 *
	 * 엔진 `UMassReplicationTrait::BuildTemplate`(`MassReplicationTrait.cpp:44`)은
	 * `FMassReplicationSharedFragment`를 **자기 자신의 리플렉션 CRC**로 중복 제거하는데,
	 * 그 구조체의 UPROPERTY는 `BubbleInfos`(빌드 시점엔 빈 배열)와
	 * `CachedReplicator`(= `ReplicatorClass`의 **CDO**) 둘뿐이다.
	 * **LOD 거리를 들고 있는 `LODCalculator`·`LODCollector`는 UPROPERTY가 아니라 CRC에 들어가지 않는다.**
	 * 따라서 리플리케이터 클래스가 같으면 CullDistance가 달라도 CRC가 같아지고,
	 * `FindOrAdd`가 **먼저 만들어진 공유 프래그먼트 하나**를 전 타입에 돌려준다
	 * → 먼저 빌드된 타입의 컬 거리가 모두에게 적용된다.
	 *
	 * 2026-09-02 실측: 전 타입이 `ULNPMassReplicator` 하나를 쓰던 시절, LootPod의 60,000cm가
	 * 적(12,000cm)에도 적용돼 **반지름 250m 월드 전체(478개)가 모든 클라이언트 버블에 들어왔고,
	 * 교전이 전혀 없는 대기 상태에서 송신량이 1.1MB/s였다** (그중 98%가 이 버블).
	 *
	 * 클래스를 나누면 CDO 포인터가 달라 CRC가 갈린다 —
	 * `GetStructInstanceCrc32`는 `SerializeItem`으로 태그드 프로퍼티를 훑고 오브젝트는 포인터로 해싱한다.
	 *
	 * ⚠️ **§7.1의 진짜 불변식은 "버블 클래스 1개"가 아니라 "핸들 발급자 1개"다.**
	 * `AgentHandleManager`는 `TClientBubbleHandlerBase`의 **인스턴스 멤버**이므로, 버블이 하나여도
	 * 그 안에 핸들러를 여럿 두면 발급자도 여럿이 되어 똑같이 깨진다. 이 프로젝트는
	 * `FLNPMassClientBubbleSerializer`가 `FLNPMassClientBubbleHandler`를 **하나만** 들고 있으므로 안전하다 —
	 * 리플리케이터를 나눠도 파괴 루프가 만지는 핸들은 항상 그 하나가 발급한 것이라 유효하고,
	 * 처리 후 `AgentData.Invalidate()`가 걸려 2회차 이후는 건너뛴다(멱등).
	 *
	 * 대가는 CPU다: 파괴 루프가 순회하는 **클라이언트 장부는 청크 필터가 걸리지 않은 공용 자료구조**라,
	 * 리플리케이터를 N개로 나누면 넷 틱마다 장부 전체를 N번 순회한다 — **O(N × 장부 크기).**
	 * 현재 N=3, 장부 약 135개라 무시할 수준이지만, 타입이나 엔티티가 크게 늘면 다시 재야 한다.
	 */
	LOOTNPOP_API void ConfigureParams(FMassReplicationParameters& Params,
		TSubclassOf<UMassReplicatorBase> ReplicatorClass, float CullDistance);
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
 * 클라이언트 전용 보간 상태 — 복제 수신 사이의 빈 프레임을 메운다.
 *
 * 수신 간격은 복제 LOD가 정하는데(엔진 기본 High 0.1 / Medium 0.2 / Low 0.3초),
 * 수신값을 Transform에 즉시 대입하면 화면 갱신 주기가 곧 수신 주기가 되어
 * 초당 3~10칸씩 순간이동하는 것으로 보인다. 그래서 수신값은 여기(목표)에 적재하고
 * Transform에는 ULNPMassSmoothingProcessor가 매 프레임 보간값을 쓴다.
 *
 * 출발점을 "직전 수신값"이 아니라 "지금 화면에 그려진 값"으로 잡으므로
 * 패킷이 늦거나 중간값이 걸러져도 끊기지 않고 이어진다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedMovementFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 마지막으로 수신한 스냅샷 (보간의 도착점). */
	FVector TargetPosition = FVector::ZeroVector;
	FQuat TargetRotation = FQuat::Identity;

	/** 그 수신 시점에 화면에 그려져 있던 자세 (보간의 출발점). */
	FVector SourcePosition = FVector::ZeroVector;
	FQuat SourceRotation = FQuat::Identity;

	/** 마지막 수신 이후 흐른 시간(초). 다음 수신 때 그대로 실측 간격이 된다. */
	float TimeSinceUpdate = 0.f;

	/** 이번 구간을 메우는 데 쓸 시간(초) = 직전 두 수신의 실측 간격. 0이면 즉시 스냅. */
	float BlendDuration = 0.f;
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

	// PreReplicatedRemove는 오버라이드하지 않는다 — 베이스(TClientBubbleHandlerBase)가 이미
	// ResetEntityIfValid → SpawnerSubsystem->DestroyEntities로 클라이언트 엔티티를 파괴한다.
	// 이 경로가 안 도는 것처럼 보였던 2026-08-20 버그의 원인은 여기가 아니라 서버 쪽이었다
	// (파괴 옵저버 누락 → ULNPMassSpawnSubsystem::RestoreServerOnlyMassObservers 참조).

	/** 복제 페이로드(위치 + 접평면 로컬 Yaw)를 Transform Fragment에 즉시 반영한다. 스케일은 보존한다. */
	static void ApplyReplicatedTransform(FTransformFragment& TransformFragment, const FReplicatedAgentPositionYawData& PositionYaw);

	/**
	 * 수신 페이로드를 보간 목표로 적재한다 (Transform은 건드리지 않는다 — 프로세서가 매 프레임 채운다).
	 * 보간 Fragment가 없는 아키타입(Player·LootPod)은 즉시 반영으로 폴백한다.
	 */
	static void PushSmoothingTarget(const FMassEntityView& EntityView, const FReplicatedAgentPositionYawData& PositionYaw);

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

/**
 * 클라이언트 전용 — FLNPReplicatedMovementFragment의 보간값을 매 프레임 Transform에 쓴다.
 *
 * SyncWorldToMass 그룹(PrePhysics의 첫 그룹)에 두는 이유: 이 프레임의 LOD 판정·Representation·
 * UMassUpdateISMProcessor가 모두 뒤에서 돌기 때문에, 여기서 채운 값이 그대로 그려진다.
 */
UCLASS()
class LOOTNPOP_API ULNPMassSmoothingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPMassSmoothingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery SmoothingQuery;
};

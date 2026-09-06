// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "LNPTargetQuerySubsystem.generated.h"

/** 질의 종류. 게이트와 점수 함수가 여기서 갈린다. */
enum class ELNPTargetQueryKind : uint8
{
	/** 광선이 캡슐 반지름 안을 지날 것 · 최근접 깊이가 이긴다. (원거리 조준점) */
	Ray,

	/** 접평면 거리·각도 게이트 · 각도·거리 가중합이 큰 쪽이 이긴다. (근접 공격 보정) */
	Cone,

	/**
	 * 지목된 엔티티 하나를 계속 따라간다. 살아 있고 Origin에서 MaxDistance 이내면 위치를 돌려준다.
	 * 사망·소멸·거리 이탈이 모두 "결과 없음"으로 나오므로, 소비처는 그것만 보고 해제하면 된다. (락온)
	 */
	Track,
};

namespace LNPTargetQuery
{
	/**
	 * 구형 월드에서 거리·각도는 반드시 접평면 성분으로만 잰다.
	 * 반지름 방향 성분이 섞이면 같은 높이에 있지 않은 대상의 거리가 실제보다 멀게 나오고,
	 * 워프 지점도 지면에서 떠버린다. (TechDesign_EnemyNPC.md 5.1과 같은 규약)
	 */
	inline bool ProjectToTangent(const FVector& UpDir, const FVector& Delta, FVector& OutDir, float& OutDist)
	{
		const FVector Tangent = Delta - UpDir * FVector::DotProduct(Delta, UpDir);
		OutDist = Tangent.Size();
		if (OutDist <= KINDA_SMALL_NUMBER)
		{
			OutDir = FVector::ZeroVector;
			return false;
		}
		OutDir = Tangent / OutDist;
		return true;
	}
}

/**
 * 질의 슬롯 핸들. 소비처가 등록 시 받아 두고, 파라미터 쓰기·결과 읽기에 그대로 쓴다.
 */
struct FLNPTargetQueryHandle
{
	int32 SlotIndex = INDEX_NONE;

	bool IsValid() const { return INDEX_NONE != SlotIndex; }
};

/**
 * 질의 파라미터. 게임 스레드가 매 프레임 갱신한다.
 *
 * MaxDistance가 0 이하면 이번 프레임 그 슬롯은 평가하지 않는다 — 소비처가 등록을 해제하지 않고도
 * 질의를 쉬게 하는 수단이다(발사체 무기를 들지 않은 프레임 등).
 */
struct FLNPTargetQueryParams
{
	ELNPTargetQueryKind Kind = ELNPTargetQueryKind::Ray;

	FVector Origin    = FVector::ZeroVector;

	/** Ray: 광선 방향. Cone: 접평면에 투영된 기준 전방. */
	FVector Direction = FVector::ForwardVector;

	/** Cone 전용 — 접평면의 법선. */
	FVector UpDir = FVector::UpVector;

	/** Ray: 광선 길이. Cone: 탐색 반경(접평면 거리). */
	float MaxDistance = 0.f;

	/** Cone 전용 — 기준 전방으로부터의 허용 각도. */
	float MaxAngleDeg = 0.f;

	/** Cone 전용 — 점수 가중치. 둘의 합이 1이 되게 두는 것이 관례다. */
	float AngleWeight    = 0.f;
	float DistanceWeight = 0.f;

	/** Track 전용 — 따라갈 대상. */
	FMassEntityHandle TrackedEntity;
};

/** 질의 결과. 프로세서가 쓰고 게임 스레드가 읽는다. */
struct FLNPTargetQueryResult
{
	FMassEntityHandle Entity;

	/** Ray: 광선 위에서 캡슐 중심에 가장 가까운 점(**몸통 깊이**). Cone: 대상의 위치. */
	FVector Location = FVector::ZeroVector;

	/** Ray: Origin에서 Direction 축을 따라 잰 거리. Cone: 접평면 거리. */
	float Distance = 0.f;

	bool bHit = false;
};
/**
 * 적 엔티티에 대한 기하 질의 창구.
 *
 * **왜 필요한가:** 락온·근접 보정·조준점이 모두 물리 API(`SphereOverlapActors`, 라인 트레이스)로 적을
 * 찾고 있었는데, `CombatMode::PureEntity`는 Actor도 콜리전 바디도 없어 그 경로에 아예 잡히지 않는다.
 * 판정은 전부 수학인데 조준·보조만 물리에 의존하던 것을 여기로 모은다.
 *
 * **상시 질의(standing query)다** — 요청/응답형이 아니다. 소비처는 슬롯을 등록해 두고 파라미터만
 * 갱신하며, 필요할 때 항상 준비된 최신 결과를 읽는다. 락온·근접 보정은 "버튼을 누른 순간" 1회
 * 호출되므로, 요청/응답형이면 지연이 하필 가장 체감되는 자리에 몰린다.
 * 결과는 최대 1프레임 늦지만 그 지연은 아무도 체감하지 않는 자리에 놓인다.
 *
 * 상세 설계는 `.context/TechDesign_TargetQuery.md`.
 */
UCLASS()
class LOOTNPOP_API ULNPTargetQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 소비처(게임 스레드) ──────────────────────────────────────────────────

	/** 슬롯을 하나 확보한다. 해제할 때까지 유효하다. */
	FLNPTargetQueryHandle RegisterQuery();

	/** 슬롯을 반납한다. 핸들은 무효화된다. */
	void UnregisterQuery(FLNPTargetQueryHandle& InOutHandle);

	/** 이번 프레임의 광선 파라미터를 쓴다. (원거리 조준점) */
	void SetRayQuery(const FLNPTargetQueryHandle& Handle, const FVector& Origin, const FVector& Direction, float MaxDistance);

	/** 이번 프레임의 원뿔 파라미터를 쓴다. 거리·각도는 UpDir 접평면에서 잰다. (근접 공격 보정) */
	void SetConeQuery(const FLNPTargetQueryHandle& Handle, const FVector& Origin, const FVector& Direction,
		const FVector& UpDir, float Radius, float MaxAngleDeg, float AngleWeight, float DistanceWeight);

	/** 지목한 엔티티를 계속 따라간다. 결과가 사라지면 사망·소멸·거리 이탈 중 하나다. (락온) */
	void SetTrackQuery(const FLNPTargetQueryHandle& Handle, const FVector& Origin, FMassEntityHandle TrackedEntity, float MaxDistance);

	/** 가장 최근에 평가된 결과를 읽는다. 슬롯이 유효하지 않으면 false. */
	bool GetResult(const FLNPTargetQueryHandle& Handle, FLNPTargetQueryResult& OutResult) const;

	// ── 프로세서 ─────────────────────────────────────────────────────────────

	/** 평가 대상 슬롯과 파라미터를 복사해 간다. MaxDistance가 0 이하인 슬롯은 제외된다. */
	void SnapshotQueries(TArray<int32>& OutSlotIndices, TArray<FLNPTargetQueryParams>& OutParams) const;

	/** SnapshotQueries가 돌려준 슬롯 순서 그대로 결과를 되돌려 놓는다. */
	void SubmitResults(const TArray<int32>& SlotIndices, const TArray<FLNPTargetQueryResult>& Results);

private:
	struct FSlot
	{
		FLNPTargetQueryParams    Params;
		FLNPTargetQueryResult Result;
		bool                  bInUse = false;
	};

	TArray<FSlot> Slots;

	/**
	 * 게임 스레드(파라미터 쓰기·결과 읽기)와 Mass 워커 스레드(스냅샷·결과 쓰기)가 같은 슬롯을 만진다.
	 * 슬롯 수가 로컬 플레이어당 소수이고 각 방향이 프레임당 한 번뿐이라 경합이 사실상 없으므로,
	 * 이중 버퍼 대신 짧은 임계 구역으로 둔다 — 잠금 구간은 작은 POD 복사뿐이다.
	 */
	mutable FCriticalSection SlotsLock;
};

/**
 * Mass에 이 Subsystem의 Thread 모델을 알린다.
 *
 * 이 선언이 없으면 기본값(GameThreadOnly = true)이 적용되어 ULNPTargetQueryProcessor가
 * 게임 Thread로 승격된다. public 메서드가 모두 SlotsLock으로 보호되므로 워커 Thread 접근은 안전하다.
 *
 * ThreadSafeWrite는 false로 둔다 — 쓰기 자체는 Lock으로 안전하지만, true로 두면 Mass가 RW를 RO처럼
 * 취급해 이 Subsystem을 쓰는 Processor들을 병렬로 돌린다. 결과 기록은 슬롯당 "최근접 하나"를 고르는
 * 축약이라 병렬로 뒤섞이면 어느 것이 이겼는지가 프레임마다 달라진다.
 */
template<>
struct TMassExternalSubsystemTraits<ULNPTargetQuerySubsystem> final
{
	enum
	{
		GameThreadOnly  = false,
		ThreadSafeWrite = false,
	};
};

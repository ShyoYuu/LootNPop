// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "LNPEnemySpatialGrid.generated.h"

/**
 * 적 엔티티의 **브로드페이즈 격자** — "이 지점 근처에 어떤 적이 있는가"를 O(1) 색인으로 좁힌다.
 *
 * 상세 설계는 `.context/TechDesign_TargetQuery.md` §6. 요지는 셋이다.
 *
 * **① 등장방형 + 축소 행.** 평면 XY 격자는 구형 월드에서 깨진다(적도에서 구면이 얇은 띠로 접히고
 * 반대편 반구가 이웃으로 잡힌다 — `DiscardedApproaches.md` Case 05). 그래서 방향 벡터를 위도·경도로
 * 색인하되, **위도 행마다 경도 칸 수를 `cos(위도)`에 비례해 줄인다.** 줄이지 않으면 개체가 몰리는
 * 극에서 하필 가속 구조가 선형 스캔으로 퇴화한다(반지름 25,000·질의 반경 300에서 위도 89.9도면
 * 훑어야 하는 칸이 그 위도의 전부인 314칸). 축소 행이면 셀 면적이 균일해져 **질의는 위도와 무관하게
 * 항상 3x3 규모**고, 극점 캡이 자연히 1칸이 되어 경계 예외 처리가 없다.
 *
 * **② 매 프레임 재구축.** 설계 문서는 "셀이 배회 반경보다 크면 격자가 거의 정지하므로 증분 갱신"과
 * "회피에는 수백 cm 셀이 필요"가 충돌한다고 적었는데, **그 충돌은 증분 갱신을 전제할 때만 존재한다.**
 * 재구축은 카운팅 정렬 2패스 = 개체당 상수라 셀 크기와 비용이 분리되고, 엔티티당 셀 프래그먼트도
 * 파괴 시 제거 처리도 통째로 없어진다. 3,000기면 프레임당 6,000회 쓰기다.
 *
 * **③ 페이로드를 격자가 직접 들고 있다.** Mass 워커에서는 **다른 엔티티의 프래그먼트에 임의 접근할 수
 * 없으므로**, 이웃의 위치를 프래그먼트가 아니라 여기서 읽어야 한다(가상 칼날이 2패스인 이유와 같다).
 *
 * ⚠️ **광선 질의에는 쓰지 말 것.** 구 내벽을 가로지르는 긴 광선은 방향 공간에서 최대 180도에 가까운
 * 호를 쓸고, 고도를 버린 격자에서는 그 호에 걸린 반대편 벽의 적까지 전부 후보가 된다 —
 * 선형 스캔보다 나쁠 수 있다. 이 격자가 맞는 것은 **국소 반경 질의**다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemySpatialGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 빌더 (ULNPEnemySpatialGridProcessor 전용) ────────────────────────────

	/** 이번 프레임 등록을 시작한다. 이전 내용은 버려지고 조회는 FinishRebuild까지 결과가 없다. */
	void BeginRebuild(int32 ExpectedCount);

	/** 개체 하나를 등록한다. 셀은 위치의 **방향 성분**으로만 정해진다(고도는 버린다). */
	void AddEntity(FMassEntityHandle Handle, const FVector& Position);

	/** 카운팅 정렬을 마쳐 조회 가능 상태로 만든다. */
	void FinishRebuild();

	// ── 소비처 ───────────────────────────────────────────────────────────────

	/**
	 * Center에서 Radius(cm) 안에 **있을 수 있는** 개체를 훑는다. 브로드페이즈이므로
	 * **과다포함이다** — 정확한 거리 판정은 호출자가 한다.
	 *
	 * Center 자신도 등록돼 있으면 함께 나오므로, 호출자가 핸들로 걸러야 한다.
	 */
	void ForEachNeighbor(const FVector& Center, float Radius,
		TFunctionRef<void(FMassEntityHandle, const FVector&)> Visitor) const;

private:
	/** 해상도가 바뀌었거나 아직 없으면 행 오프셋 테이블을 다시 만든다. */
	void EnsureLayout();

	/** 방향 → 위도 행. 극에서 클램프된다. */
	int32 RowFromDirection(const FVector& Dir) const;

	/** 행 안에서의 경도 칸. 0/360도 경계에서 wrap된다. */
	int32 ColFromLongitude(int32 Row, float LonDegrees) const;

	int32 LatRes = 0;

	/** 행 r의 경도 칸 수. 길이 = LatRes. */
	TArray<int32> LonCounts;

	/** 행 r의 첫 셀 인덱스. 길이 = LatRes + 1이고 마지막 원소가 전체 셀 수다. */
	TArray<int32> RowOffsets;

	/** 셀별 개체 수 → 접두합을 거쳐 셀별 시작 인덱스가 된다. 길이 = 셀 수 + 1. */
	TArray<int32> CellStarts;

	/** 등록 순서 그대로의 페이로드. 셀 정렬 결과는 Ordered가 가리키는 인덱스로 참조한다. */
	TArray<FVector>           Positions;
	TArray<FMassEntityHandle> Handles;
	TArray<int32>             CellOf;

	/** 셀 순으로 정렬된 페이로드 인덱스. */
	TArray<int32> Ordered;

	bool bBuilt = false;
};

/**
 * Mass에 이 Subsystem의 Thread 모델을 알린다.
 *
 * `ThreadSafeWrite`를 false로 두는 것이 핵심이다 — true면 Mass가 RW를 RO처럼 취급해 빌더와
 * 소비처를 **병렬로** 돌리고, 그러면 아직 채워지지 않은 격자를 읽게 된다.
 * false면 두 프로세서가 직렬화되어 별도의 잠금이 필요 없다
 * (`ULNPTargetQuerySubsystem`이 같은 이유로 같은 선언을 하고 있다).
 */
template<>
struct TMassExternalSubsystemTraits<ULNPEnemySpatialGridSubsystem> final
{
	enum
	{
		GameThreadOnly  = false,
		ThreadSafeWrite = false,
	};
};

/**
 * 매 프레임 격자를 다시 짓는다. **서버 전용** — 유일한 소비처인 분리력이 서버 이동 시뮬레이션의 일부다.
 *
 * ⚠️ 순서는 그룹이 아니라 **이름으로** 건다. 그룹 간 순서는 엔진의 고정 목록이 아니라 프로세서들이
 *    선언한 간선에서 유도되고, 이 프로젝트는 그 간선을 만들던 엔진 이동 프로세서를 쓰지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemySpatialGridProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemySpatialGridProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EnemyQuery;
};

/**
 * 겹쳐 선 적들을 서로 밀어낸다. 격자로 좁힌 이웃과의 **접평면** 거리로 분리력을 만들어
 * `FLNPEnemySeparationFragment`에 남기고, 소비는 `ULNPEnemyMovementProcessor`가 한다 —
 * **Transform의 주인은 하나로 유지한다**(표면 스냅·경사 체크가 그쪽에 있다).
 */
UCLASS()
class LOOTNPOP_API ULNPEnemySeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemySeparationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery SeparationQuery;
};

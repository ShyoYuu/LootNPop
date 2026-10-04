// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"

/** 이동·투사체가 독립된 데이터로 사용하는 축소 행 격자의 저장·재구축 코어. */
class LOOTNPOP_API FLNPEnemySpatialGrid
{
public:
	void BeginRebuild(int32 ExpectedCount);
	void AddEntity(FMassEntityHandle Handle, const FVector& Position);
	void FinishRebuild();
	void ForEachNeighbor(const FVector& Center, float Radius,
		TFunctionRef<void(FMassEntityHandle, const FVector&)> Visitor) const;

	/** 등록 인덱스를 오름차순으로 반환한다. false면 원점/넓은 조회이므로 호출자가 전수 검사한다. */
	bool GatherSphereCandidates(const FVector& Center, double Radius, TArray<int32>& OutIndices) const;
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

	TArray<int32> Unindexed; // 방향이 없는 캡슐은 보수적 조회에 항상 포함한다.
	bool bBuilt = false;
};

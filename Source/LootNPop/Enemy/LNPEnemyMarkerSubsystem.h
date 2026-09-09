// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "LNPEnemyMarkerSubsystem.generated.h"

/**
 * HP 바 표시 후보를 고를 때 쓰는 파라미터. 게임 스레드(HUD)가 매 프레임 쓴다.
 *
 * MaxDistance가 0 이하면 이번 프레임은 수집하지 않는다 — HUD가 등록을 해제하지 않고도
 * 수집을 쉬게 하는 수단이다(마커 위젯이 WBP에 없을 때 등).
 */
struct FLNPEnemyMarkerParams
{
	FVector Origin    = FVector::ZeroVector;   // 카메라 위치
	FVector Direction = FVector::ForwardVector; // 카메라 전방

	float MaxDistance = 0.f;
	/**
	 * 카메라 축 기준 허용 반각. "화면 밖 제외"를 대신한다 —
	 * 뷰포트 투영은 게임 스레드 전용이라 Mass 워커에서 부를 수 없다(락온 후보 탐색과 같은 관례).
	 */
	float MaxAngleDeg = 0.f;

	/** 거리순으로 자를 상한. 0 이하면 수집하지 않는다. */
	int32 MaxCount = 0;

	/** 이 시간(초) 안에 HP가 변한 적은 상한과 무관하게 포함시킨다. */
	float RecentDamageWindow = 0.f;
};

/** 표시 후보 하나. 프로세서가 쓰고 게임 스레드가 읽는다. */
struct FLNPEnemyMarkerEntry
{
	FMassEntityHandle Entity;

	/** 캡슐 중심. 머리 위 오프셋은 표시 측이 더한다. */
	FVector Location = FVector::ZeroVector;

	/** 0~1 HP 비율. */
	float Ratio = 1.f;

	/** 카메라까지의 직선 거리 (원근 스케일에 쓴다). */
	float Distance = 0.f;
};

/**
 * 적 HP 바 표시 후보를 매 프레임 모으는 창구.
 *
 * ULNPTargetQuerySubsystem과 성격이 같지만 **최선 1개가 아니라 상위 N개**를 돌려주므로 별도로 둔다.
 * 잠금 관례는 그쪽과 같다 — 잠금은 작은 POD 복사 구간에만 걸고, 적 전수 순회는 잠금 밖에서 돈다.
 *
 * 슬롯이 하나뿐이다: 이 프로젝트는 로컬 분할화면을 쓰지 않으므로 월드당 로컬 플레이어가 하나다.
 * 상세 설계는 `.context/TechDesign_HUD.md` §11.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 소비처(게임 스레드) ──────────────────────────────────────────────────

	/** 이번 프레임의 수집 파라미터를 쓴다. */
	void SetParams(const FLNPEnemyMarkerParams& InParams);

	/** 이번 프레임은 수집하지 않는다. */
	void ClearParams();

	/** 가장 최근에 수집된 후보를 읽는다. 거리순으로 정렬돼 있다. */
	void GetEntries(TArray<FLNPEnemyMarkerEntry>& OutEntries) const;

	// ── 프로세서 ─────────────────────────────────────────────────────────────

	/** 평가할 파라미터를 복사해 간다. 수집하지 않아야 하면 false. */
	bool SnapshotParams(FLNPEnemyMarkerParams& OutParams) const;

	/** 수집 결과를 되돌려 놓는다. */
	void SubmitEntries(TArray<FLNPEnemyMarkerEntry>&& InEntries);

private:
	FLNPEnemyMarkerParams Params;
	TArray<FLNPEnemyMarkerEntry> Entries;

	/** 게임 스레드(파라미터 쓰기·결과 읽기)와 Mass 워커(스냅샷·결과 쓰기)가 같은 자료를 만진다. */
	mutable FCriticalSection DataLock;
};

/**
 * ULNPTargetQuerySubsystem과 같은 이유로 워커 접근을 허용한다 — public 메서드가 전부 DataLock으로 보호된다.
 * ThreadSafeWrite는 false로 둔다: true면 Mass가 RW를 RO처럼 취급해 프로세서를 병렬로 돌리는데,
 * 결과 기록이 "상위 N개"라는 축약이라 병렬로 뒤섞이면 프레임마다 선택이 달라진다.
 */
template<>
struct TMassExternalSubsystemTraits<ULNPEnemyMarkerSubsystem> final
{
	enum
	{
		GameThreadOnly  = false,
		ThreadSafeWrite = false,
	};
};

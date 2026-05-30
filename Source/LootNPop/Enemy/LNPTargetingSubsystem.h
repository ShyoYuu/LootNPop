// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "LNPTargetingSubsystem.generated.h"

/** 슬롯 경쟁 중인 Enemy의 정보 */
struct FLNPPendingTargetEntry
{
	FMassEntityHandle EnemyHandle;
	FMassEntityHandle PlayerHandle;
	float Score;
	bool bIsMelee;

	bool operator<(const FLNPPendingTargetEntry& Other) const
	{
		return Score > Other.Score; // 내림차순 정렬
	}
};

/** 단일 Player의 슬롯 상태 */
USTRUCT()
struct FLNPPlayerSlotData
{
	GENERATED_BODY()

	/** 현재 슬롯을 점유하는 Entity (Confirmed 상태) */
	TSet<FMassEntityHandle> OccupiedMelee;
	TSet<FMassEntityHandle> OccupiedRanged;
};

/**
 * Player별 Enemy 어그로를 관리하는 전역 Subsystem.
 * Scoring 규칙에 따라 어떤 Enemy가 공격할지 결정한다.
 */
UCLASS()
class LOOTNPOP_API ULNPTargetingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 특정 Player 타겟에 대한 Enemy의 어그로 Score를 등록한다 */
	void RegisterEnemyInterest(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle, float Score, bool bIsMelee);

	/** Enemy의 슬롯 확정 여부를 반환한다 */
	bool IsSlotConfirmed(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle) const;

	/** 재균형 수행: 고점수 Enemy을 승격하고 저점수 Enemy을 강등한다 */
	void RebalanceSlots();

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Targeting")
	int32 MaxMeleeSlotsPerPlayer = 10;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Targeting")
	int32 MaxRangedSlotsPerPlayer = 20;

protected:
	/** Player Entity -> 슬롯 데이터 맵 */
	TMap<FMassEntityHandle, FLNPPlayerSlotData> PlayerSlots;

	/** 이번 프레임에 등록된 모든 Enemy의 어그로 목록 */
	TArray<FLNPPendingTargetEntry> PendingEntries;

	/** 병렬 Mass 처리 중 Thread-Safe를 위한 Lock */
	mutable FCriticalSection DataLock;
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavPathScheduler.h"
#include "SurfaceNavigation/LNPNavOverlay.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPNavPathSubsystem.generated.h"

class ULNPSurfaceDataSubsystem;
class UMassEntitySubsystem;

/** 경로 요청을 낸 사유 비트(Phase 10 단위 1 진단). `LNPEnemyPathProcessor`의 재요청 조건과 1:1이다. */
namespace ELNPNavRequestReason
{
	enum Type : uint32
	{
		/** 이 개체가 아직 요청한 적이 없다(휴면에서 깨면 다시 이 상태다). */
		NoPrior = 1 << 0,
		TargetChanged = 1 << 1,
		GoalTileChanged = 1 << 2,
		/** 목표가 마지막 요청 지점에서 RepathGoalDrift를 넘게 움직였다. */
		GoalDrift = 1 << 3,
		/** 따라가던 경로가 지나는 Tile의 overlay revision이 바뀌었다. */
		PathInvalid = 1 << 4,
		/** 경로 없이 끝난 요청 뒤 overlay revision이 바뀌었다. */
		NoPathOverlayChanged = 1 << 5,
		StaleResult = 1 << 6,
		/** 직선 보행 가능 구간에서 요청을 버린 뒤 다시 요청한다. */
		AfterDirect = 1 << 7,
		/** 이동·도착·타겟 상태·배회 대역 등으로 요청을 버린 뒤 다시 요청한다. */
		AfterCancel = 1 << 8,
	};
}

/**
 * 서버 전용 지상 경로 요청 창구(`phases/Phase07b_PathExecution.md` §3.4). 게시된 Nav snapshot 위에서 FLNPNavPathScheduler를 매 프레임 돌린다.
 * 게스트는 경로를 계산하지 않는다. 모든 호출은 게임 스레드에서 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPNavPathSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** owner의 이전 요청은 Cancelled가 된다. snapshot 게시 전이면 게시될 때까지 대기한다. */
	uint32 Submit(const FLNPNavPathRequest& Request);
	/** 부하 측정 구간의 요청을 메모리에 모으고 종료 후 CSV로 저장한다. */
	void BeginRequestCapture();
	bool EndRequestCapture(const FString& Filename);
	/**
	 * 캡처 중이면 요청을 낸 사유를 `<캡처>_Reasons.csv`에 남긴다(Phase 10 단위 1). 요청 CSV 스키마는 재생기가 검사하므로 따로 둔다.
	 * Reasons는 ELNPNavRequestReason 비트 묶음이며 여러 사유가 함께 참일 수 있다.
	 */
	void RecordRequestReason(const FMassEntityHandle Owner, const uint32 Serial, const uint32 Reasons, const uint32 StartTile,
		const int32 GoalNode, const uint32 GoalTile)
	{
		if (bCaptureRequests)
		{
			AppendRequestReason(Owner, Serial, Reasons, StartTile, GoalNode, GoalTile);
		}
	}
	/** 캡처 중이면 활동 대역 깨움을 `<캡처>_Wakes.csv`에 남긴다. 요청 몰림과 시각을 대조하는 용도다. */
	void RecordWake(const FMassEntityHandle Owner, const uint8 Band, const bool bFromDormant)
	{
		if (bCaptureRequests)
		{
			AppendWake(Owner, Band, bFromDormant);
		}
	}
	void Cancel(const FMassEntityHandle Owner) { Scheduler.Cancel(Owner); }
	ELNPNavPathStatus GetResult(const FMassEntityHandle Owner, const uint32 Serial, FLNPNavPathResult& OutResult) const
	{
		return Scheduler.GetResult(Owner, Serial, OutResult);
	}

	const FLNPNavPathScheduler& GetScheduler() const { return Scheduler; }
	void AddPodBlocker(int32 PodID, const FVector& Location, const FLNPSurfaceHandle& Surface);
	void RemovePodBlocker(int32 PodID);
	/** 스폰 중 모은 Pod blocker를 다음 Mass phase 경계에서 한 번 게시하도록 요청한다. */
	void CommitPodBlockers();
	TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> TakeOverlay() const { return Overlay; }
	/** 마지막 tick의 scheduler 게임 스레드 시간(병렬 확장 대기 포함). */
	double GetLastTickSeconds() const { return LastTickSeconds; }
	void SetLastConsumerSeconds(double Seconds) { LastConsumerSeconds = Seconds; }
	double GetLastConsumerSeconds() const { return LastConsumerSeconds; }
	void RecordFollowerFrame(const bool bFollowingPath, const int32 WaypointsAdvanced)
	{
		++FollowerFrames;
		FollowingPathFrames += bFollowingPath ? 1 : 0;
		FollowerWaypointsAdvanced += WaypointsAdvanced;
	}
	uint64 GetFollowerFrames() const { return FollowerFrames; }
	uint64 GetFollowingPathFrames() const { return FollowingPathFrames; }
	uint64 GetFollowerWaypointsAdvanced() const { return FollowerWaypointsAdvanced; }

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return Scheduler.HasWork(); }
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	friend class FLNPConditionalPatchPublicationTest;
	friend class FLNPStatefulTerrainTest;
	void OnMassPhaseStart(float DeltaSeconds);
	void AppendRequestReason(FMassEntityHandle Owner, uint32 Serial, uint32 Reasons, uint32 StartTile, int32 GoalNode, uint32 GoalTile);
	void AppendWake(FMassEntityHandle Owner, uint8 Band, bool bFromDormant);
	void PublishOverlay();
	FDelegateHandle PhaseStartHandle;
	bool bOverlayDirty = true;
	FLNPNavPathScheduler Scheduler;
	TMap<int32, FLNPNavPodBlocker> PodBlockers;
	TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay;
	uint64 OverlayGeneration = 0;
	double LastTickSeconds = 0.0;
	double LastConsumerSeconds = 0.0;
	bool bCaptureRequests = false;
	double RequestCaptureStart = 0.0;
	double TimingCaptureWallStart = 0.0;
	FString RequestCapture;
	FString ReasonCapture;
	FString WakeCapture;
	uint64 FollowerFrames = 0;
	uint64 FollowingPathFrames = 0;
	uint64 FollowerWaypointsAdvanced = 0;

	UPROPERTY(Transient)
	TObjectPtr<ULNPSurfaceDataSubsystem> SurfaceData;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
};

// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavPathScheduler.h"
#include "SurfaceNavigation/LNPNavOverlay.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPNavPathSubsystem.generated.h"

class ULNPSurfaceDataSubsystem;
class UMassEntitySubsystem;

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
	void Cancel(const FMassEntityHandle Owner) { Scheduler.Cancel(Owner); }
	ELNPNavPathStatus GetResult(const FMassEntityHandle Owner, const uint32 Serial, FLNPNavPathResult& OutResult) const
	{
		return Scheduler.GetResult(Owner, Serial, OutResult);
	}

	const FLNPNavPathScheduler& GetScheduler() const { return Scheduler; }
	void AddPodBlocker(int32 PodID, const FVector& Location, const FLNPSurfaceHandle& Surface);
	void RemovePodBlocker(int32 PodID);
	/** 스폰 중 모은 Pod blocker를 overlay revision 하나로 게시한다. */
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
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return Scheduler.HasWork(); }
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	FLNPNavPathScheduler Scheduler;
	TMap<int32, FLNPNavPodBlocker> PodBlockers;
	TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay;
	uint64 OverlayGeneration = 0;
	double LastTickSeconds = 0.0;
	double LastConsumerSeconds = 0.0;
	bool bCaptureRequests = false;
	double RequestCaptureStart = 0.0;
	FString RequestCapture;
	uint64 FollowerFrames = 0;
	uint64 FollowingPathFrames = 0;
	uint64 FollowerWaypointsAdvanced = 0;

	UPROPERTY(Transient)
	TObjectPtr<ULNPSurfaceDataSubsystem> SurfaceData;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
};

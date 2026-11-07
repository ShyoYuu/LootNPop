// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "Subsystems/WorldSubsystem.h"
#include "Async/Future.h"
#include "LNPGraphicsBenchmark.generated.h"

class ACameraActor;
class UMassEntityConfigAsset;
class FJsonObject;

namespace LNPGraphicsBenchmark
{
	/** 월드 시작 전에 선택한다. 일반 실행에서는 false다. */
	bool IsRequested();
	FString GetSpawnConfigPath();
	/** 관찰 경로의 카메라 위치를 활동 거리 판정에 사용한다. 게임 스레드 전용. */
	bool GetObserverLocation(const UWorld* World, FVector& OutLocation);
}

/** 기존 스폰·배회를 관찰하는 단일 플레이어 그래픽스 측정. 전투 부하를 만들지 않는다. */
UCLASS()
class LOOTNPOP_API ULNPGraphicsBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	void RecordSpawn(const UMassEntityConfigAsset& Config, int32 Slot, int32 Requested, int32 Accepted, bool bEnemy);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	friend class FLNPGraphicsPathTest;
	static bool ReadViewpoints(const FString& Json, TArray<FTransform>& OutViews, float& OutFOV, float& OutAspect);
	static FTransform EvaluateView(TConstArrayView<FTransform> Views, double Phase);
	bool Prepare();
	void UpdateView(double Phase);
	void SamplePopulation(double Elapsed, bool bWriteRows);
	void SaveManifest(const FString& Status, const FString& Reason = FString());
	void Finish(bool bSuccess, const FString& Reason = FString());
	void SetRunCVar(const TCHAR* Name, const TCHAR* Value);
	void RestoreCVars();

	enum class EStage : uint8 { Waiting, Warmup, Arming, Capture, Flushing, Done };
	EStage Stage = EStage::Waiting;
	double StageStart = 0.0;
	double NextPopulationSample = 0.0;
	float WarmupSeconds = 60.f;
	float DurationSeconds = 60.f;
	FString Profile;
	FString OutputDirectory;
	FString PathJson;
	TArray<FTransform> Views;
	float FieldOfView = 80.f;
	float ReferenceAspect = 4.f / 3.f;
	bool bOwnsCsv = false;
	bool bQuit = false;
	int32 MeasuredFrames = 0;
	TSharedFuture<FString> CsvWrite;
	TSharedPtr<FJsonObject> Manifest;
	FMassEntityQuery PopulationQuery;
	TArray<FString> PopulationRows;
	int32 LastAlive = 0;
	int32 LastActive = 0;
	int32 LastFrustumCandidates = 0;
	int32 LastActors = 0;
	int32 LastInstances = 0;

	struct FSpawnCounts { int32 Requested = 0; int32 Accepted = 0; bool bEnemy = false; };
	TMap<FString, TArray<FSpawnCounts>> InitialSpawns;
	struct FCVarBackup { FString Name; FString Value; uint32 Priority; };
	TArray<FCVarBackup> SavedCVars;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};

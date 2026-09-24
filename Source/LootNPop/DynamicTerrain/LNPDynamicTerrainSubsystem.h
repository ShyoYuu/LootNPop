// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPDynamicTerrainSubsystem.generated.h"

class ALNPMovingPanel;
class APawn;
class ULNPDynamicTerrainSubsystem;

/** 모든 패널 틱 뒤에 DynamicSupport snapshot을 게시하는 틱. 패널 Actor 틱이 prerequisite다. */
USTRUCT()
struct FLNPDynamicSupportPublishTickFunction : public FTickFunction
{
	GENERATED_BODY()

	ULNPDynamicTerrainSubsystem* Owner = nullptr;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override { return TEXT("ULNPDynamicTerrainSubsystem::PublishTick"); }
};

template<>
struct TStructOpsTypeTraits<FLNPDynamicSupportPublishTickFunction> : public TStructOpsTypeTraitsBase2<FLNPDynamicSupportPublishTickFunction>
{
	enum { WithCopy = false };
};

/** PureEntity가 읽을 동적 지지면 한 개의 프레임 상태(DynamicTerrain.md §3). UObject 참조가 없는 POD다. */
struct FLNPDynamicSupportSnapshot
{
	FLNPPlacementId Id;
	FTransform PreviousTransform;
	FTransform CurrentTransform;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	bool bWalkable = false;
};

/** 한 프레임에 게시하는 불변 동적 지지면 목록. */
struct FLNPDynamicSupportFrame
{
	TArray<FLNPDynamicSupportSnapshot> Supports;
	uint64 FrameNumber = 0;

	const FLNPDynamicSupportSnapshot* Find(const FLNPPlacementId& Id) const
	{
		return Supports.FindByPredicate([&Id](const FLNPDynamicSupportSnapshot& Support) { return Support.Id == Id; });
	}
};

/**
 * 동적 지형(DynamicTerrain.md).
 *
 * - 서버: 로드된 slot Level과 persistent level의 Placement Marker를 모아 요소 Actor를 스폰한다.
 *   seed 기반 장치 배치도 같은 스폰 함수(SpawnPlacedActor)를 거친다.
 * - 서버·클라: 패널은 자기 Actor 틱(TG_PrePhysics)에서 자세를 갱신하고, 이 서브시스템이 모든 패널 틱 뒤
 *   같은 그룹에서 DynamicSupport snapshot을 게시한다(FLNPDynamicSupportPublishTickFunction).
 *   worker exact query는 StartPhysics 이후 페이즈에만 있으므로 TG_PrePhysics의 transform 쓰기와 겹치지 않는다(Gate 0).
 *   PrePhysics 페이즈에 exact 소비자를 추가하려면 이 배치를 다시 검토해야 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPDynamicTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 서버 전용 공통 스폰. deferred 스폰으로 Initialize를 FinishSpawning 전에 불러
	 * 거기서 채운 복제 프로퍼티가 초기 스폰 번치에 실리게 한다.
	 */
	static AActor* SpawnPlacedActor(UWorld& World, UClass* ActorClass, const FTransform& Transform,
		TFunctionRef<void(AActor&)> Initialize);

	/** 서버 전용. 옥탄트 레벨이 가시화된 뒤, 플레이어 스폰(Complete) 전에 한 번 호출한다. */
	void SpawnFromMarkers();

	void RegisterPanel(ALNPMovingPanel* Panel);
	void UnregisterPanel(ALNPMovingPanel* Panel);

	/** 모든 스레드. 최신 게시 snapshot. */
	TSharedRef<const FLNPDynamicSupportFrame, ESPMode::ThreadSafe> GetDynamicSupportFrame() const { return Frame; }

	/** server time 점프로 자세를 snap한 횟수(진단). */
	void NotePoseSnap() { ++PoseSnapCount; }
	int32 GetPoseSnapCount() const { return PoseSnapCount; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	friend FLNPDynamicSupportPublishTickFunction;

	void Publish();
	void OnWorldPreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);

	/** 진단(LNP.DynamicTerrain.LogRiders). 패널 근처 Mover 폰의 base와 패널 로컬 위치를 남긴다. */
	void LogRiders(double ServerTime) const;

	/** 진단(LNP.DynamicTerrain.LogDepartures). 패널 base를 떠난 폰의 관성을 이탈 프레임과 0.3초 뒤에 남긴다. */
	void TrackDepartures(double ServerTime);

	struct FRiderTrack
	{
		TWeakObjectPtr<ALNPMovingPanel> Panel;
		FVector PanelVelocity = FVector::ZeroVector;
		double DepartedTime = -1.0;
		double OnBaseSince = -1.0;
		bool bFollowUpLogged = true;
	};
	TMap<TWeakObjectPtr<APawn>, FRiderTrack> RiderTracks;

	TArray<TWeakObjectPtr<ALNPMovingPanel>> Panels;
	TSharedRef<const FLNPDynamicSupportFrame, ESPMode::ThreadSafe> Frame = MakeShared<FLNPDynamicSupportFrame, ESPMode::ThreadSafe>();
	FLNPDynamicSupportPublishTickFunction PublishTick;
	FDelegateHandle PreActorTickHandle;
	int32 PoseSnapCount = 0;
};

/** worker는 GetDynamicSupportFrame만 호출한다. 게시는 Mass 실행 구간 밖(프레임 선두)이다. */
template<>
struct TMassExternalSubsystemTraits<ULNPDynamicTerrainSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};

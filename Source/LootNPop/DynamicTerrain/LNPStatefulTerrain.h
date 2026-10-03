// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "DynamicTerrain/LNPConditionalPatchTypes.h"
#include "GameFramework/Actor.h"
#include "LNPStatefulTerrain.generated.h"

class UStaticMeshComponent;
struct FLNPDynamicSupportSnapshot;

UENUM(BlueprintType)
enum class ELNPTerrainState : uint8 { Standing, Transitioning, Bridge, Intact, Destroyed };

/** 초기 번치·late join이 마커나 LVI 경로 없이 같은 형상을 만든다. */
USTRUCT()
struct FLNPStatefulTerrainConfig
{
	GENERATED_BODY()
	UPROPERTY() FLNPPlacementId Id;
	UPROPERTY() FTransform Origin;
	UPROPERTY() FTransform Standing;
	UPROPERTY() float TransitionSeconds = 2.f;
	UPROPERTY() ELNPConditionalPatchCondition Condition = ELNPConditionalPatchCondition::Intact;
	UPROPERTY() TArray<FLNPConditionalPatchSource> Sources;
};

/** 시작 시각은 GameState의 server world epoch에 속한다. 상태 이벤트는 revision당 한 번만 적용한다. */
USTRUCT()
struct FLNPStatefulTerrainState
{
	GENERATED_BODY()
	UPROPERTY() ELNPTerrainState Value = ELNPTerrainState::Intact;
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY() double StartServerTime = 0.0;
};

/** 한 마커의 안정 형상과 파괴/다리 상태. 물리 잔해는 만들지 않는다. */
UCLASS()
class LOOTNPOP_API ALNPStatefulTerrain : public AActor, public ILNPPlacedElement
{
	GENERATED_BODY()
public:
	ALNPStatefulTerrain();
	virtual void InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id) override;
	/** 서버 권위 요청. collision·identity·patch는 다음 요소 틱에서 함께 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Terrain") bool StartTransition();
	UFUNCTION(BlueprintCallable, Category = "LNP|Terrain") bool BreakTerrain();
	ELNPTerrainState GetTerrainState() const { return State.Value; }
	uint32 GetStateRevision() const { return State.Revision; }
	bool GetDynamicSupport(FLNPDynamicSupportSnapshot& OutSupport) const;
	uint64 GetLastTickFrame() const { return LastTickFrame; }
	static FTransform EvaluatePose(const FLNPStatefulTerrainConfig& Config,
		const FLNPStatefulTerrainState& State, double ServerTime);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	friend class FLNPStatefulTerrainTest;
	void Apply(double ServerTime);
	void ChangeState(ELNPTerrainState Value, double ServerTime);
	double ServerTime() const;
	UPROPERTY(Replicated) FLNPStatefulTerrainConfig Config;
	UPROPERTY(Replicated) FLNPStatefulTerrainState State;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
	FTransform PreviousTransform;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	uint32 AppliedRevision = 0;
	uint64 BoundGeneration = 0;
	uint64 LastTickFrame = 0;
	bool bRegistered = false;
	bool bPatchQueued = false;
};

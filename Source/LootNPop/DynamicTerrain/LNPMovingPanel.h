// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "LNPMovingPanel.generated.h"

class UStaticMeshComponent;

/**
 * 패널 경로와 시작 시각. 스폰 때 한 번 복제되고 이후 바뀌지 않는다(D-027).
 * 자세는 이 값과 server world time만의 함수라 late join도 수신 즉시 같은 자세를 계산한다.
 */
USTRUCT()
struct FLNPPanelPath
{
	GENERATED_BODY()

	/** 마커 월드 transform. 스폰 번치의 Actor 위치는 복제 시점의 현재 위치라 원점으로 쓸 수 없다. */
	UPROPERTY()
	FVector OriginLocation = FVector::ZeroVector;

	UPROPERTY()
	FQuat OriginRotation = FQuat::Identity;

	/** 마커 스플라인의 위치 곡선(마커 로컬). tangent까지 계산된 값을 그대로 싣는다. */
	UPROPERTY()
	FInterpCurveVector Curve;

	UPROPERTY()
	float Speed = 0.f;

	UPROPERTY()
	float EndHoldSeconds = 0.f;

	/** 서버가 경로를 바꾸면 올린다. 현재는 스폰 때 1로 고정이다. */
	UPROPERTY()
	uint16 Revision = 0;

	/**
	 * 경로 시작 시각(server world time, 초). server world time이 곧 server epoch다 —
	 * 매치(월드)마다 0에서 시작하므로 다른 매치의 시각과 섞이지 않는다.
	 */
	UPROPERTY()
	double StartServerTime = 0.0;

	bool IsValid() const { return Curve.Points.Num() >= 2 && Speed > 0.f && Revision > 0; }
};

/**
 * 움직이는 패널(DynamicTerrain.md §3). 서버가 Placement Marker 위치에 스폰하는 복제 Actor다.
 *
 * - 결정론적 kinematic 이동: 자기 Actor 틱(TG_PrePhysics)에서 UpdatePose를 부른다. ReplicatedMovement는 쓰지 않는다.
 *   Mover는 NPP 시뮬레이션(OnWorldPreActorTick) 뒤, base의 Actor 틱을 prerequisite로 건 base 추종 틱에서
 *   이동량을 옮긴다(UMoverComponent::UpdateBasedMovementScheduling). 패널을 시뮬레이션 전에 옮기거나
 *   Actor 틱 없이 옮기면 이동량 일부를 놓친다(2026-09-24 측정: 절반만 추종).
 * - 회전하지 않고 마커 회전을 유지한 채 평행 이동만 한다.
 * - Mover는 루트 메시를 movement base로 직렬화하므로 NetGUID가 필요하다 — 복제 Actor인 이유다.
 */
UCLASS()
class LOOTNPOP_API ALNPMovingPanel : public AActor, public ILNPPlacedElement
{
	GENERATED_BODY()

public:
	ALNPMovingPanel();

	// ILNPPlacedElement
	virtual void InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id) override;

	/** server world time의 자세로 옮긴다. 게임 스레드, worker exact query 구간(StartPhysics~) 밖에서만 호출한다. */
	void UpdatePose(double ServerTime);

	const FLNPPlacementId& GetPlacementId() const { return PlacementId; }
	const FTransform& GetPreviousTransform() const { return PreviousTransform; }
	FVector GetLinearVelocity() const;
	UStaticMeshComponent* GetPanelMesh() const { return PanelMesh; }

	/** 경로 전체에서 collision geometry가 원점에서 가장 멀어지는 거리의 상한(cm). world collision envelope 입력이다. */
	float GetSweptMaxRadius() const { return SweptMaxRadius; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_Path();

	UPROPERTY(VisibleAnywhere, Category = "LNP|Panel")
	TObjectPtr<UStaticMeshComponent> PanelMesh;

	UPROPERTY(Replicated)
	FLNPPlacementId PlacementId;

	UPROPERTY(ReplicatedUsing = OnRep_Path)
	FLNPPanelPath Path;

private:
	/** BeginPlay와 경로 수신이 모두 끝나면 한 번 활성화한다. 초기 번치 순서에 기대지 않기 위해서다. */
	void TryActivate();
	void BuildArcTable();

	/** 경로 거리 → 곡선 key. */
	float DistanceToKey(double Distance) const;

	/** 시각 → (경로 거리, 진행 방향 부호). 멈춤 구간의 부호는 0이다. */
	void EvaluateDistance(double ServerTime, double& OutDistance, float& OutDirection) const;

	/** 등속 이동용 거리 표. 같은 곡선에서 서버·클라가 같은 표를 만든다. */
	TArray<double> ArcDistances;
	TArray<float> ArcKeys;
	double PathLength = 0.0;

	float SweptMaxRadius = 0.f;
	FTransform PreviousTransform = FTransform::Identity;
	FVector CurrentVelocity = FVector::ZeroVector;
	double LastServerTime = -1.0;
	bool bActive = false;
};

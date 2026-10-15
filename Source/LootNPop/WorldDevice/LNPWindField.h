// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "LNPWindField.generated.h"

class UNiagaraComponent;

/**
 * 윈드필드 — 행글라이더를 설정 높이까지 띄워 올리는 상승 기류 구역.
 *
 * 구역은 **구면 원판**이다: 배치 지점 중심, 지표 거리 Radius 이내, 지표부터 배치 지점 위 RiseHeight 고도까지.
 * 목표 고도는 행성 중심 거리로 환산하므로 필드 전역이 같은 고도 셸에 머문다. 액터는 Up 정렬로 배치한다고 가정한다.
 *
 * 이동 시뮬레이션은 이 액터를 직접 보지 않고 ULNPWindFieldSubsystem::FindWindLift로 기하 판정만 묻는다.
 * 활공 상태 자체는 모드 이름(SyncState)이라 윈드필드가 더하는 예측 상태는 없다 — 복제되는 가변 상태는 on/off 하나뿐이고,
 * 토글 순간 서버·클라가 RTT만큼 다르게 보면 보정이 한 번 날 수 있다(수용).
 */
UCLASS()
class LOOTNPOP_API ALNPWindField : public AActor, public ILNPPlacedElement
{
	GENERATED_BODY()

public:
	ALNPWindField();

	/** 서버 전용. 꺼지면 필드 안 활공은 일반 활공(하강)으로 돌아가고 VFX가 멈춘다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LNP|WindField")
	void SetWindFieldEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "LNP|WindField")
	bool IsWindFieldEnabled() const { return bEnabled; }

	float GetRadius() const { return Radius; }

	/** 목표 고도 셸의 행성 중심(월드 원점) 거리 (cm). */
	float GetTargetCenterDistance() const;

	/** 구역 반경 — 배치 지점에서의 지표 거리 (cm). 상한 없음. */
	UPROPERTY(EditAnywhere, Replicated, Category = "LNP|WindField", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 1500.f;

	/** 배치 지점에서 Up으로 이만큼 위가 상승 목표 고도 (cm). */
	UPROPERTY(EditAnywhere, Replicated, Category = "LNP|WindField", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RiseHeight = 2000.f;

	// --- ILNPPlacedElement ---
	virtual void InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_Enabled();

	/** VFX 활성과 User 파라미터(Radius·Lifetime·SpawnRate)를 현재 값에 맞춘다. */
	void RefreshEffect();

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Enabled, Category = "LNP|WindField")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, Category = "LNP|WindField")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 상승 바람 VFX. 로컬 +Z = Up이다(로컬 스페이스 시뮬레이션 — 월드 스페이스는 원점에서 먼 좌표에서 컬링된다). */
	UPROPERTY(VisibleAnywhere, Category = "LNP|WindField")
	TObjectPtr<UNiagaraComponent> EffectComponent;
};

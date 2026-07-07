// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LNPLootPod.generated.h"

/**
 * 루팅 대상 Actor (Phase 7 §5.4 이중 복제).
 *
 * MassEntity(FLNPLootPodFragment)가 게이지·근접 판정 로직을 담당하고, 이 Actor는
 * SmartObject 연동과 비주얼(Niagara 기둥), 그리고 PodState·게이지 퍼센트의 근접 클라이언트
 * 복제를 담당한다. 엔티티 존재·초기 위치는 MassReplication bubble이 전 클라이언트에 전달한다.
 */
UCLASS()
class LOOTNPOP_API ALNPLootPod : public AActor
{
	GENERATED_BODY()

public:
	ALNPLootPod();

	/** MassEntity에서 보상 및 데이터 조회를 위한 고유 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|LootPod")
	int32 PodID = 0;

	/** 루팅 프로세스를 시작하고 Mass 상태와 비주얼을 업데이트한다 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Interaction")
	void StartLooting();

	/** 현재 상태에 따라 Niagara VFX를 업데이트한다 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Visuals")
	void UpdateVisuals(ELNPLootPodState NewState);

	/** LootPod의 현재 로컬 상태를 반환한다 */
	ELNPLootPodState GetCurrentState() const { return CurrentState; }

	/** 루팅 게이지 진행률 (0~1). 서버가 쓰고 클라이언트로 복제된다. */
	UFUNCTION(BlueprintPure, Category = "LNP|LootPod")
	float GetGaugePercent() const { return CurrentGaugePercent; }

	/**
	 * 서버 전용: 게이지 진행률(0~1)을 복제 프로퍼티에 반영한다.
	 * 매 프레임 변하는 값이므로 2% 이상 변화(또는 0/1 도달) 시에만 실제로 기록해 복제 트래픽을 줄인다 (§5.4).
	 */
	void SetGaugePercent(float NewPercent);

	/** 상호작용자가 이 LootPod과 상호작용할 수 있는 유효 거리 및 각도 내에 있는지 확인한다 */
	UFUNCTION(BlueprintNativeEvent, Category = "LNP|Interaction")
	bool CanInteract(const APawn* Interactor) const;

	UFUNCTION()
	UMassAgentComponent* GetMassAgentComponent() const { return MassAgentComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PodState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USmartObjectComponent> SmartObjectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UNiagaraComponent> LootPillarVFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USphereComponent> LootingZoneSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMassAgentComponent> MassAgentComponent;

	/** LootPod의 현재 상태 — 서버가 쓰고(Mass 프로세서 전환 커맨드) 클라이언트가 OnRep으로 비주얼을 갱신한다. */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_PodState, Category = "LNP|LootPod")
	ELNPLootPodState CurrentState = ELNPLootPodState::Idle;

	/** 루팅 게이지 진행률 (0~1) — 서버가 2% 임계값으로 갱신, 클라이언트 UI/VFX용. */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "LNP|LootPod")
	float CurrentGaugePercent = 0.0f;

	/** LootPod의 Forward 벡터로부터 상호작용이 허용되는 최대 각도 (도) */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float MaxInteractionAngle = 60.0f;

	// --- 비주얼 설정 ---
	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor IdleColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor LootingColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor PoppedColor = FLinearColor::Yellow;

	/** 색상에 사용할 Niagara 파라미터 이름 */
	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FName ColorParameterName = TEXT("User.Color");
};

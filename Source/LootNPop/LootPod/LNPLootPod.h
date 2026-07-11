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

	/** 상호작용 프롬프트(키 아이콘) 표시 여부를 갱신한다 — 로컬 플레이어의 ULNPInteractionComponent가 호출한다 */
	void SetInteractionPromptVisible(bool bVisible);

	/** 진단용: 상호작용 판정 세부 값(거리/각도/상태)을 문자열로 반환한다 — LootPod 개발 중 테스트 로그 */
	FString GetInteractDiagnosticString(const APawn* Interactor) const;

	UFUNCTION()
	UMassAgentComponent* GetMassAgentComponent() const { return MassAgentComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PodState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USceneComponent> SceneComponent;

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

	/** 상호작용 프롬프트 위젯 (스크린 스페이스, 기본 숨김) — WidgetClass 교체로 아트 적용 가능 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UWidgetComponent> InteractionPromptWidget;

	/** 루팅 존 홀로그램 링 — 바닥 평면 메시 + 반경 방향 게이지 머티리얼(M_LootZoneRing).
	 *  메시·머티리얼은 BP에서 지정하고, 스케일은 BeginPlay에서 루팅 존 반경과 자동 동기화된다.
	 *  Looting 상태(감쇠 중 포함)에서만 표시. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> ZoneRingComponent;

	/** 존 링 머티리얼의 GaugePercent 파라미터 구동용 동적 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> ZoneRingMID;

	/** LootPod의 현재 상태 — 서버가 쓰고(Mass 프로세서 전환 커맨드) 클라이언트가 OnRep으로 비주얼을 갱신한다. */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_PodState, Category = "LNP|LootPod")
	ELNPLootPodState CurrentState = ELNPLootPodState::Idle;

	/** 루팅 게이지 진행률 (0~1) — 서버가 2% 임계값으로 갱신, 클라이언트 UI/VFX용. */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "LNP|LootPod")
	float CurrentGaugePercent = 0.0f;

	/** LootPod의 Forward 벡터로부터 상호작용이 허용되는 최대 각도 (도) */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float MaxInteractionAngle = 60.0f;

	/** 상호작용(루팅 State 진입) 허용 최대 거리 (cm) — "단말기를 직접 조작한다"는 컨셉의 초근접 반경.
	 *  루팅 존 반경(LootingZoneSphere, 게이지 기여·존 사수 범위)과는 별개다.
	 *  원점 기준 판정이므로 Pod 메시 반경 + 캐릭터 캡슐 반경보다 커야 한다 (메시에 붙었을 때 실거리 ~213cm 실측). */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float InteractionRadius = 250.0f;

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

	/** 게이지 진행 로그의 스팸 방지용 — 마지막으로 출력한 10% 단위 구간 */
	int32 LastLoggedGaugeDecile = 0;
};

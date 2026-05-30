// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LNPLootPod.generated.h"

UCLASS()
class LOOTNPOP_API ALNPLootPod : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
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

	/** 상호작용자가 이 LootPod과 상호작용할 수 있는 유효 거리 및 각도 내에 있는지 확인한다 */
	UFUNCTION(BlueprintNativeEvent, Category = "LNP|Interaction")
	bool CanInteract(const APawn* Interactor) const;

	UFUNCTION()
	UMassAgentComponent* GetMassAgentComponent() const { return MassAgentComponent; }

protected:
	virtual void BeginPlay() override;

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

	/** LootPod의 현재 로컬 상태, 비주얼과 동기화 유지 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|LootPod")
	ELNPLootPodState CurrentState = ELNPLootPodState::Idle;

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

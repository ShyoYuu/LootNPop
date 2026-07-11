// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPInteractionComponent.generated.h"

class ALNPLootPod;

/**
 * ALNPLootPod Actor를 탐색하고 상호작용하는 Component.
 */
UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 주변에서 최선의 상호작용 타겟을 지속적으로 탐색한다 */
	void UpdateInteractionCandidate();

	/** 후보 중 프롬프트(키 아이콘)를 표시할 최선 타겟을 결정한다 — 로컬 플레이어 전용 */
	void UpdateInteractionPrompt();

	/** 현재 후보와 실제 상호작용을 시작한다 */
	void PerformInteraction();

	/** 현재 강조된 모든 상호작용 타겟을 반환한다 (UI용) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	/** 첫 번째 유효한 상호작용 타겟을 반환한다 (하위 호환 또는 단순 로직용) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetFirstInteractionCandidate() const;

protected:
	/**
	 * 원격 클라이언트의 루팅 시작을 서버에 전달한다 (Phase 7).
	 * FLNPPlayerLootingTag는 서버 월드의 플레이어 엔티티에 붙어야 서버 전용 LootPod 프로세서가
	 * 감지한다 — 로컬 엔티티에만 붙이면 리슨 호스트를 제외한 모든 클라이언트의 루팅이 서버에
	 * 반영되지 않는다 (Phase 3 Guard/Parry RPC와 동일 유형의 공백).
	 */
	UFUNCTION(Server, Reliable)
	void Server_StartLooting(ALNPLootPod* Pod);

	/** 서버 전용: 유효성 검증 후 플레이어 엔티티에 루팅 태그/프래그먼트를 부여한다. */
	void StartLootingOnServer(ALNPLootPod* Pod);
	/** 상호작용 가능한 오브젝트 탐색 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float InteractionRadius = 500.0f;

	/** 캐릭터가 현재 바라보거나 근처에 있는 LootPod/Actor (잠재적 타겟) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> InteractionCandidates;

	/** 현재 프롬프트를 표시 중인 타겟 (로컬 플레이어 전용) */
	UPROPERTY(Transient)
	TWeakObjectPtr<ALNPLootPod> CurrentPromptTarget;

	// --- LootPod 개발용 테스트 로그 상태 (변화 시에만 출력해 스팸 방지) ---
	/** 쿼리 범위 내에서 발견된 가장 가까운 Pod — CanInteract 실패 사유 진단용 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ALNPLootPod> NearestNearbyPod;
	int32 LastLoggedPodCount = -1;
	int32 LastLoggedInteractableCount = -1;
};


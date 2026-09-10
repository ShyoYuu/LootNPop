// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPTrajectoryGuideComponent.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * ADS 중 유탄의 예상 궤도를 그리는 컴포넌트. ALNPPlayerCharacter에 붙인다.
 *
 * **로컬 전용성은 스폰 위치에서 나온다** — Niagara를 로컬 클라이언트에서만 스폰하므로
 * 복제되지 않고, 따라서 SetOnlyOwnerSee 같은 가시성 플래그가 필요 없다.
 *
 * 궤적은 실탄과 **같은 함수**로 만든다 (LNPFireGeometry / LNPProjectileMotion). 가이드가
 * 자기 나름대로 총구·조준선·적분을 다시 짜면 그 순간부터 가리키는 곳과 터지는 곳이 갈린다.
 *
 * 상태를 명령형으로 세우지 않고 **매 Tick 게이트를 다시 평가**한다. 무기 교체·ADS 해제·사망
 * 어느 쪽으로 빠져나가도 저절로 풀린다 — 가드가 눌린 입력을 남겨 겪었던 문제
 * (→ TechDesign_CharacterMovement.md §2.6)를 되풀이하지 않기 위해서다.
 */
UCLASS(ClassGroup=(LNP), meta=(BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPTrajectoryGuideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPTrajectoryGuideComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/**
	 * 궤도 가이드 Niagara. User 파라미터 **`Points`(Position 배열) 하나만** 노출하면 된다.
	 *
	 * 궤적을 항상 LNPProjectileMotion::ArcPointCount개로 재표집해 넘기므로 Niagara는 그 수만큼
	 * 정점을 버스트하고 ExecIndex로 읽기만 하면 된다 — 배열 길이 조회도, 여분 정점 숨기기도 없다.
	 * 마지막 원소가 착탄 예상 지점이라 착탄 표식도 같은 배열에서 읽는다.
	 *
	 * ⚠️ 노출하지 않은 파라미터는 조용히 무시되므로 이름이 틀리면 아무 일도 일어나지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Trajectory")
	TObjectPtr<UNiagaraSystem> GuideSystem;

	/**
	 * 착탄 예상 지점에 스플래시 반경만큼 깔리는 바닥 장판 (Deferred Decal 머티리얼).
	 *
	 * 카메라를 향하는 스프라이트로 그리면 반구처럼 서 있어 전투 시야를 가린다. 데칼은 지형에
	 * 투영되므로 경사면에서도 바닥에만 붙고, 반투명이라 그 위의 적이 그대로 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Trajectory")
	TObjectPtr<UMaterialInterface> BlastRadiusMaterial;

	/** 데칼 투영 상자의 깊이 (cm). 경사·굴곡을 넘어 바닥까지 닿을 만큼은 되어야 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Trajectory", meta = (ClampMin = "1"))
	float BlastRadiusDecalDepth = 400.f;

private:
	void ShowGuide(float ExplosionRadius);
	void ShowBlastRadius(float ExplosionRadius);
	void HideGuide();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> GuideComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> BlastRadiusDecal;

	/** 매 프레임 재사용하는 궤적 버퍼 — 틱마다 할당하지 않는다. */
	TArray<FVector> ArcPoints;
};

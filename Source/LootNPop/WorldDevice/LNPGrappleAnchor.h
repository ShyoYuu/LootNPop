// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LNPInteractable.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "LNPGrappleAnchor.generated.h"

class UNiagaraComponent;
class UWidgetComponent;

/**
 * 그래플 훅 앵커 — 바라보고 F를 누르면 그 지점으로 고속 이동하는 월드 장치.
 *
 * **가변 멤버가 없다** (런처와 같다). 쿨다운도 점유도 없고, 여러 명이 같은 앵커를 동시에 써도 된다.
 *
 * 판정이 세 곳에 나뉘어 있고 그 경계가 이 장치의 핵심이다:
 *   · `GetInteractionSearchRadius` — 광역 컷. 근접 대상(250cm)과 달리 수십 m라 인터페이스 승격의 직접적 계기가 됐다.
 *   · `CanInteract` — **거리만** 본다. 서버 재검증도 이 함수를 쓰므로 소유 클라만 아는 값(카메라)을 넣으면 안 된다.
 *   · `WantsInteractionPrompt` — 카메라 조준 + 가림(LOS). **로컬 전용**이라 서버는 절대 부르지 않는다.
 * 발동 후 시뮬레이션 쪽 재검증(각도)은 `ULNPCharacterMoverComponent`가 InputCmd의 ControlRotation으로 따로 한다 —
 * 서버의 원격 폰은 `GetControlRotation()`이 최신이 아니라서 여기서 각도를 볼 수 없다.
 */
UCLASS()
class LOOTNPOP_API ALNPGrappleAnchor : public AActor, public ILNPInteractable, public ILNPPlacedElement
{
	GENERATED_BODY()

public:
	ALNPGrappleAnchor();

	/**
	 * 앵커 식별자. 그래플 의도가 InputCmd에 실릴 때 **좌표가 아니라 이 정수**가 간다 —
	 * 목적지의 권위를 서버에 남기고(서버가 자기 앵커의 좌표를 읽는다), 리시뮬레이션 결정성 문제도 없앤다.
	 * 배치 서브시스템이 스폰 순서대로 부여하며, 스폰 번치에 실려야 하므로 Deferred 스폰으로 대입한다.
	 */
	UPROPERTY(EditAnywhere, Replicated, Category = "LNP|Anchor")
	int32 AnchorID = INDEX_NONE;

	/** 캐릭터가 도달할 지점 (월드). 액터 원점을 그대로 쓴다 — 배치가 이 점을 지면 위로 띄운다. */
	FVector GetGrapplePoint() const { return GetActorLocation(); }

	/** 그래플 가능 최대 거리 (cm). 설정 에셋이 없으면 0 — 모든 판정이 자동으로 실패한다. */
	float GetMaxGrappleDistance() const;

	/** 월드에서 ID로 앵커를 찾는다. 인터랙터블 레지스트리를 순회하므로 서버·클라 어디서나 같은 결과를 준다. */
	static ALNPGrappleAnchor* FindByID(const UWorld* World, int32 InAnchorID);

	// --- ILNPInteractable ---
	virtual bool CanInteract_Implementation(const APawn* Interactor) const override;
	virtual float GetInteractionSearchRadius() const override { return GetMaxGrappleDistance(); }
	virtual bool WantsInteractionPrompt(const APawn* Interactor) const override;
	virtual void SetInteractionPromptVisible(bool bVisible) override;
	/** 근접 대상(Pod·Dice·런처)에게 항상 진다 — 앵커 프롬프트가 루팅을 가리는 것을 막는다. */
	virtual int32 GetInteractionPriority() const override { return -10; }

	// --- ILNPPlacedElement ---
	/** 마커 배치 앵커도 시드 배치와 같은 카운터에서 AnchorID를 받는다(ULNPWorldDeviceSpawnSubsystem::AllocateAnchorID). */
	virtual void InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Anchor")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * 앵커의 시각 표현. **메시가 아니라 나이아가라다** — 앵커는 도달 지점일 뿐 부딪히는 물체가 아니고,
	 * 25m 밖에서 식별되는 것이 유일한 요구사항이라 빛나는 스프라이트가 3D 모델보다 잘 맞는다.
	 * 콜리전이 아예 없으므로 비행 스윕이 도착 직전에 자기 자신에 막힐 일도 없다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Anchor")
	TObjectPtr<UNiagaraComponent> EffectComponent;

	/** 상호작용 프롬프트 위젯 — 로컬 판정으로만 표시되므로 복제와 무관, 기본 숨김 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Anchor")
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

};

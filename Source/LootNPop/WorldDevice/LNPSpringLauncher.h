// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LNPInteractable.h"
#include "LNPSpringLauncher.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

/**
 * 스프링 런처 — 정렬 모션 후 캐릭터를 크게 사출하는 월드 장치.
 *
 * **가변 멤버가 없다.** 쿨다운도 점유도 없어 두 명이 동시에 밟아도 각자 날아가고, 액터는 스폰 이후
 * 아무것도 바뀌지 않는다. 그래서 복제 프로퍼티가 0개이며 초기 스폰 번치 뒤 트래픽도 0이다.
 * 복제를 켜 두는 이유는 상태 동기화가 아니라 **NetGUID** 때문이다 — 발동 요청이 이 액터를 참조한다
 * (동적 스폰된 비복제 액터는 NetGUID가 없어 참조가 null로 도착한다).
 *
 * 발사 자체는 ULNPAbility_SpringLaunch가 수행한다. 이 액터는 "어디서 어느 방향으로 얼마나"만 들고 있다.
 */
UCLASS()
class LOOTNPOP_API ALNPSpringLauncher : public AActor, public ILNPInteractable
{
	GENERATED_BODY()

public:
	ALNPSpringLauncher();

	/** 정렬 목적지이자 발사 기준축. 캐릭터는 이 지점으로 끌려와 이 축을 향해 발사된다. */
	const USceneComponent* GetLaunchSlot() const { return LaunchSlot; }

	/** 발사 속도 벡터 (월드, cm/s). 서버·클라가 같은 액터 트랜스폼에서 파생하므로 값이 갈리지 않는다. */
	FVector GetLaunchVelocity() const;

	/** 정렬이 끝난 캐릭터가 바라볼 방향 — 발사 방향의 접평면 성분. 영벡터면 회전 정렬을 건너뛴다. */
	FVector GetFacingDirection() const;

	// --- ILNPInteractable ---
	virtual bool CanInteract_Implementation(const APawn* Interactor) const override;
	virtual float GetInteractionSearchRadius() const override;
	virtual void SetInteractionPromptVisible(bool bVisible) override;
	/** 각도 제한이 없어 근접 3종 중 수용 범위가 가장 넓다 — 그만큼 셋 중 가장 낮다. */
	virtual int32 GetInteractionPriority() const override { return 10; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Launcher")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Launcher")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 정렬 목적지 + 발사 기준축. 메시에 맞춰 BP에서 위치를 잡는다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Launcher")
	TObjectPtr<USceneComponent> LaunchSlot;

	/** 상호작용 프롬프트 위젯 — 로컬 판정으로만 표시되므로 복제와 무관, 기본 숨김 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Launcher")
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

};

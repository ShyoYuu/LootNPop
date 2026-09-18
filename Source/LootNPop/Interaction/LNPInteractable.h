// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LNPInteractable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class ULNPInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * F 상호작용 대상 Actor가 구현하는 인터페이스.
 *
 * 도입 근거는 Cast 분기 개수가 아니라 **탐색 반경의 충돌**이다. 승격 전 ULNPInteractionComponent는
 * 타입을 알기 전에 단일 반경(500cm)으로 후보를 잘랐는데, 그래플 앵커는 2,500cm가 필요하다.
 * 컷을 타입 분기 뒤로 옮기면 레지스트리 전체에 Cast를 시도한 뒤에 거리를 재게 되어 컷의 존재 이유가 사라진다.
 * GetInteractionSearchRadius()가 이 문제를 코드가 아니라 타입 계약으로 해소한다.
 *
 * **구현은 C++ 전용이다.** 순수 가상 함수가 있어 BP만으로는 구현할 수 없고, 소비자는
 * Cast<ILNPInteractable>로 얻은 포인터를 직접 호출한다. 단 CanInteract만 BlueprintNativeEvent라
 * ILNPInteractable::Execute_CanInteract(Actor, ...) 형태로 불러야 한다 (LootPod이 갖고 있던
 * BP 오버라이드 가능성을 유지하기 위함).
 */
class LOOTNPOP_API ILNPInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 정밀 판정 — 거리·각도·조준·상태. **서버 재검증도 이 함수를 쓴다.**
	 * 따라서 프롬프트를 띄울지보다 넓게 통과시켜야 한다: 클라 판정 시점과 서버 수신 시점 사이의
	 * 정당한 상태 전이를 기각하지 않기 위함이다 (LootPod은 Looting 상태를 여기서 허용한다).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LNP|Interaction")
	bool CanInteract(const APawn* Interactor) const;

	/**
	 * 광역 후보 컷 반경 (cm). ULNPInteractionComponent가 폰 위치 기준으로 이 거리 안의 대상만 정밀 판정에 넘긴다.
	 * CanInteract 내부의 정밀 판정 반경보다 크거나 같아야 한다.
	 */
	virtual float GetInteractionSearchRadius() const = 0;

	/** 상호작용 프롬프트(키 글리프) 표시 토글 — 로컬 플레이어의 ULNPInteractionComponent만 호출한다. */
	virtual void SetInteractionPromptVisible(bool bVisible) = 0;

	/**
	 * 프롬프트를 띄울 대상인가 — **로컬 조준·타게팅 판정이며 서버는 절대 쓰지 않는다.**
	 * CanInteract와 나누는 기준은 "권한이냐 조준이냐"다:
	 *   · CanInteract = 이 상호작용이 **허용되는가** (서버가 재검증한다 → 넓게 통과시킨다)
	 *   · 여기 = 지금 이걸 **겨냥하고 있는가** (카메라·캐릭터 facing·가림 — 소유 클라만 아는 값)
	 * 조준 판정을 CanInteract에 넣으면 누른 뒤 RTT 동안 시선이 움직인 정당한 입력을 서버가 기각한다.
	 */
	virtual bool WantsInteractionPrompt(const APawn* Interactor) const { return true; }

	/**
	 * 프롬프트 중재 우선순위. 높을수록 우선하며 동률은 최근접이 이긴다.
	 * 근접 대상(Pod·Dice·런처)은 기본값 0을 쓰고, 원거리 대상(그래플 앵커)만 음수를 반환해
	 * "Pod 앞에 서 있는데 먼 훅이 프롬프트를 훔치는" 상황을 결정적으로 막는다.
	 */
	virtual int32 GetInteractionPriority() const { return 0; }
};

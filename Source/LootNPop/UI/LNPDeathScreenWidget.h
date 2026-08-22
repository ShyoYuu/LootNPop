// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPDeathScreenWidget.generated.h"

class UTextBlock;

/**
 * 사망~리스폰 사이에만 화면을 덮는 반투명 오버레이 + 카운트다운.
 *
 * Blueprint 서브클래스(WBP_LNPDeathScreen) 구성:
 *  1. 루트를 화면 전체를 채우는 반투명 Border/Image로 (예: 검정 알파 0.6)
 *  2. 그 안에 TextBlock 하나를 `CountdownText`라는 이름으로 배치 — **Is Variable을 켜야 바인딩된다**
 *     (이 프로젝트의 위젯 BP는 이 플래그가 기본 off인 경우가 잦다)
 *  3. "YOU DIED" 같은 고정 문구는 BP에서 직접 넣는다 — C++이 관여할 이유가 없다
 *
 * 남은 시간은 **각 클라이언트가 로컬로 센다.** 서버 타이머와 시계를 맞출 필요가 없고
 * (오차 ≈ 편도 지연), 이 방식은 버프 잔여 시간 표시와 같은 패턴이다.
 */
UCLASS()
class LOOTNPOP_API ULNPDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 오버레이를 띄우고 Seconds부터 카운트다운을 시작한다. */
	void ShowCountdown(float Seconds);

	/** 오버레이를 숨기고 타이머를 정리한다. 중복 호출 안전. */
	void HideCountdown();

protected:
	virtual void NativeDestruct() override;

private:
	/** 남은 초를 올림해 표시한다. 0 이하가 되면 타이머를 멈춘다(숨기는 건 리스폰 빙의가 한다). */
	void RefreshCountdown();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountdownText;

	/** 카운트다운이 끝나는 로컬 월드 시각. */
	double RespawnWorldTime = 0.0;

	FTimerHandle CountdownTimer;
};

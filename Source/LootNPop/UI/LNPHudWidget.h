// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Mass/EntityHandle.h"
#include "LNPHudWidget.generated.h"

class ULNPHudViewModel;
class ULNPRadialCooldownWidget;
class ULNPScreenMarkerWidget;
class ULNPLockOnComponent;
class UAbilitySystemComponent;
class ULNPCharacterMoverComponent;

/**
 * 플레이어 HUD 위젯 C++ 기반 클래스.
 *
 * Blueprint 서브클래스(WBP_LNPHud) 설정 절차:
 *  1. 에디터 View Model 패널 → "HUD_ViewModel" 추가, 클래스 ULNPHudViewModel, 생성 모드 Manual
 *  2. HpBar  ProgressBar.Percent ← HUD_ViewModel.HealthPercent 바인딩
 *  3. AimDot Widget.Visibility   ← HUD_ViewModel.bIsFreeAiming 바인딩
 *  4. 팔레트 LNP UI → LNP Screen Marker 2개를 LockOnMarkerWidget / EnemyHpBarWidget 이름으로 배치
 *     (Is Variable 켜기, Canvas 앵커 (0,0)-(1,1)·오프셋 0)
 */
UCLASS()
class LOOTNPOP_API ULNPHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 폰의 ASC를 전달해 ViewModel을 생성·초기화하고 MVVM View에 주입한다.
	 * Mover는 대시 쿨다운 표시용이며 null이어도 무방하다.
	 */
	void InitViewModel(UAbilitySystemComponent* InASC, ULNPCharacterMoverComponent* InMover);

	/** ViewModel 구독을 해제한다. 빙의 해제 또는 위젯 소멸 시 호출. */
	void DeinitViewModel();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** 적 HP 바를 표시할 최대 거리 (cm). */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0.0"))
	float HpBarMaxDistance = 4000.f;

	/** 카메라 축 기준 허용 반각. 화면 밖 적을 대략 걸러낸다 (수집은 Mass 워커에서 돌아 뷰포트를 못 쓴다). */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float HpBarMaxAngleDeg = 70.f;

	/** 동시에 표시할 HP 바 수. */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0"))
	int32 HpBarMaxCount = 20;

	/**
	 * 이탈 여유 순위. 진입은 상위 HpBarMaxCount, 이탈은 그보다 이만큼 아래까지 버틴다 —
	 * 20위와 21위가 순위를 오갈 때 HP 바가 깜빡이는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0"))
	int32 HpBarExitMargin = 4;

	/** 이 시간(초) 안에 HP가 변한 적은 상한과 무관하게 표시한다. 저격한 먼 적이 잡몹에 밀리지 않게 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0.0"))
	float HpBarRecentDamageSeconds = 3.f;

	/** 표시 진입·이탈에 걸리는 페이드 시간(초). */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0.0"))
	float HpBarFadeSeconds = 0.2f;

	/** 캡슐 중심에서 이만큼 머리 쪽으로 올린 지점에 그린다 (cm). */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar")
	float HpBarHeightOffset = 110.f;

	/** 원근 스케일의 기준 거리 — 이 거리에서 스케일 1이 되고 멀수록 작아진다 (cm). */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "1.0"))
	float HpBarScaleDistance = 1500.f;

	/** 원근 스케일의 하한. 너무 작아져 읽을 수 없게 되는 것을 막는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|HpBar", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HpBarMinScale = 0.5f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULNPHudViewModel> HudViewModel;

	/**
	 * 대시 쿨다운 파이. WBP에 없으면 표시만 조용히 생략된다.
	 * ⚠️ WBP에서 Is Variable을 켜야 바인딩된다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPRadialCooldownWidget> DashCooldownWidget;

	/** 락온 마커. 순수 엔티티에는 UWidgetComponent를 달 수 없어 스크린 스페이스로 그린다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPScreenMarkerWidget> LockOnMarkerWidget;

	/** 적 HP 바. 락온 마커와 같은 위젯 클래스이고 스타일만 다르다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPScreenMarkerWidget> EnemyHpBarWidget;

	/** 대시 쿨다운은 "값"이 아니라 "시작됐다"는 이벤트라 ViewModel을 거치지 않는다 (근거는 TechDesign_HUD.md). */
	void HandleDashExecuted();

	void UpdateLockOnMarker(const APlayerController& PC, const FVector& CameraLocation, const FVector& CameraForward, float ViewportScale);
	void UpdateEnemyHpBars(float DeltaTime, const APlayerController& PC, const FVector& CameraLocation, const FVector& CameraForward, float ViewportScale);

	/** HP 바 하나의 표시 상태. 대상이 사라져도 페이드아웃이 끝날 때까지 마지막 좌표로 남는다. */
	struct FHpBarState
	{
		FVector2f LocalPos = FVector2f::ZeroVector;
		float Alpha = 0.f;
		float Ratio = 1.f;
		float Scale = 1.f;
		bool  bSeen = false;
	};
	TMap<FMassEntityHandle, FHpBarState> HpBarStates;

	TWeakObjectPtr<ULNPCharacterMoverComponent> BoundMover;
	FDelegateHandle DashExecutedHandle;

	/** 락온 컴포넌트는 폰이 바뀔 때만 다시 찾는다. */
	TWeakObjectPtr<const APawn> CachedPawn;
	TWeakObjectPtr<ULNPLockOnComponent> CachedLockOn;
};

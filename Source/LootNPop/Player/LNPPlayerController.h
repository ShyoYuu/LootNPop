// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LNPPlayerController.generated.h"

class ULNPDeathScreenWidget;
class ULNPHudWidget;
class ULNPMenuRootWidget;
class ULNPUILayoutWidget;
class UInputMappingContext;
class UInputAction;
class FNavigationConfig;

UCLASS()
class LOOTNPOP_API ALNPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void SetupInputComponent() override;

	/**
	 * 인게임 메뉴를 연다. 로컬 전용.
	 * @param TabId  진입할 탭. None이면 마지막으로 보던 탭(기획 §2의 기억 규칙)을 쓴다.
	 */
	void OpenMenu(FName TabId);

	/** 인게임 메뉴를 닫는다. 열려 있지 않으면 아무 일도 하지 않는다. */
	void CloseMenu();

	/** 사망 오버레이 + 리스폰 카운트다운을 띄운다. 로컬 전용 — 폰이 사망 방송을 받고 호출한다. */
	void ShowDeathScreen(float RespawnDelay);

	/** 사망 오버레이를 걷는다. 리스폰 빙의(OnPossess/AcknowledgePossession)가 호출한다. */
	void HideDeathScreen();

	/** Blueprint에서 Override하여 로딩 스크린 Widget을 표시한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LNP|UI")
	void ShowLoadingScreen();

	/** Blueprint에서 Override하여 로딩 스크린 Widget을 숨긴다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LNP|UI")
	void HideLoadingScreen();

protected:
	UFUNCTION()
	void OnLocalBakingComplete();

	/** 이 클라이언트가 로컬 초기화를 완료했음을 서버에 알린다 */
	UFUNCTION(Server, Reliable)
	void ServerNotifyClientReady();

private:
	bool bLoadingComplete = false;

	/** BP 서브클래스에서 지정할 HUD 위젯 클래스. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|HUD")
	TSubclassOf<ULNPHudWidget> HudWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULNPHudWidget> HudWidget;

	/** BP 서브클래스에서 지정할 사망 오버레이 위젯 클래스. 미지정이면 오버레이가 생략된다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|HUD")
	TSubclassOf<ULNPDeathScreenWidget> DeathScreenWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULNPDeathScreenWidget> DeathScreenWidget;

	/** 뷰포트에 상주하는 UI 레이아웃 위젯 클래스 (메뉴 스택 보유). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TSubclassOf<ULNPUILayoutWidget> UILayoutWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULNPUILayoutWidget> UILayoutWidget;

	/** 메뉴 스택에 push할 메뉴 루트 위젯 클래스. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TSubclassOf<ULNPMenuRootWidget> MenuWidgetClass;

	/** 컨트롤러 상시 매핑 컨텍스트 — 폰의 DefaultMappingContext와 분리해 빙의와 무관한 컨트롤러 수명으로 관리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	/** 메뉴 열기 (키보드 I / 게임패드 Special_Left) — 마지막으로 보던 탭으로 진입한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Input")
	TObjectPtr<UInputAction> OpenMenuAction;

	/** 환경설정 탭 직행 (키보드 F1·Esc / 게임패드 Special_Right) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Input")
	TObjectPtr<UInputAction> OpenSettingsAction;

	/** 다음 진입 시 복원할 탭. 환경설정은 기억 대상이 아니다 (기획 §2). */
	FName LastViewedTabId;

	/** OpenMenuAction 핸들러 — 마지막 탭으로 진입. */
	void HandleOpenMenuInput();

	/** OpenSettingsAction 핸들러 — 항상 환경설정 탭으로 진입. */
	void HandleOpenSettingsInput();

	/** 메뉴가 닫힐 때 UILayoutWidget이 통지 — 입력·일시정지·입력 모드를 되돌린다. */
	void HandleMenuClosed();

	/** 폰의 게임플레이 입력 매핑을 켜고 끈다. */
	void SetPawnGameplayInputEnabled(bool bEnabled);

	/**
	 * 메뉴가 열려 있는 동안만 WASD를 Slate 방향 네비게이션 키로 추가한다 (화살표·D-Pad는 엔진 기본).
	 * ⚠️ 네비게이션 설정은 FSlateApplication 전역이고 PIE는 에디터와 Slate를 공유하므로,
	 * 상시 등록하면 에디터 패널까지 WASD로 이동하게 된다. 반드시 메뉴 수명에만 걸고 원복한다.
	 */
	void SetMenuNavigationEnabled(bool bEnabled);

	/** 메뉴를 열기 전의 네비게이션 설정. 닫을 때 되돌린다. */
	TSharedPtr<FNavigationConfig> PreviousNavigationConfig;

public:
	/** 서버: ServerPhase == Complete 확인. 클라이언트: bLoadingComplete 확인. */
	bool IsLoadingComplete() const;
};

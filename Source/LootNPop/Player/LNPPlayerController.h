// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LNPPlayerController.generated.h"

class ULNPHudWidget;
class ULNPInventoryWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class LOOTNPOP_API ALNPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void SetupInputComponent() override;

	/** 인벤토리 패널 표시/숨김 전환 — 열면 마우스 커서 + GameAndUI 입력 모드. 로컬 전용. */
	void ToggleInventory();

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

	/** BP 서브클래스에서 지정할 인벤토리 패널 위젯 클래스. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Inventory")
	TSubclassOf<ULNPInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULNPInventoryWidget> InventoryWidget;

	/** 컨트롤러 상시 매핑 컨텍스트 — 폰의 DefaultMappingContext와 분리해 빙의와 무관한 컨트롤러 수명으로 관리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	/** 인벤토리 토글 입력 액션 (기본 I키 매핑) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	bool bInventoryOpen = false;

	/** PlayerState의 InventoryComponent로 인벤토리 ViewModel을 초기화한다 (빙의 경로 공용). */
	void InitInventoryViewModel();

public:
	/** 서버: ServerPhase == Complete 확인. 클라이언트: bLoadingComplete 확인. */
	bool IsLoadingComplete() const;
};

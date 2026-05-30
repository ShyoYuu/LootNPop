// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LNPPlayerController.generated.h"

UCLASS()
class LOOTNPOP_API ALNPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

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

public:
	/** 서버: ServerPhase == Complete 확인. 클라이언트: bLoadingComplete 확인. */
	bool IsLoadingComplete() const;
};

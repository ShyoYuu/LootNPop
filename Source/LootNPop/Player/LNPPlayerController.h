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

	/** Override in Blueprint to show a loading screen widget */
	UFUNCTION(BlueprintImplementableEvent, Category = "LNP|UI")
	void ShowLoadingScreen();

	/** Override in Blueprint to hide the loading screen widget */
	UFUNCTION(BlueprintImplementableEvent, Category = "LNP|UI")
	void HideLoadingScreen();

protected:
	UFUNCTION()
	void OnLocalBakingComplete();

	/** Notifies the server that this client has finished local initialization */
	UFUNCTION(Server, Reliable)
	void ServerNotifyClientReady();
};

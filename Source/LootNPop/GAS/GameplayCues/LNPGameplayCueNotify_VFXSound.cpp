// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_VFXSound.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

bool ULNPGameplayCueNotify_VFXSound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (VFX)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(MyTarget, VFX, Parameters.Location, Parameters.Normal.Rotation());

	if (Sound)
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Parameters.Location);

	if (CameraShake)
	{
		const APawn* TargetPawn = Cast<APawn>(MyTarget);
		APlayerController* PC = TargetPawn ? Cast<APlayerController>(TargetPawn->GetController()) : nullptr;
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
			PC->PlayerCameraManager->StartCameraShake(CameraShake);
	}

	return true;
}

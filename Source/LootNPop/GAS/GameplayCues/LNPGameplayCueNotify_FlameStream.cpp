// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_FlameStream.h"
#include "Character/LNPCharacterBase.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

ALNPGameplayCueNotify_FlameStream::ALNPGameplayCueNotify_FlameStream()
{
	bAutoDestroyOnRemove = false;
}

bool ALNPGameplayCueNotify_FlameStream::IsPredictingOwner(const AActor* Target)
{
	const APawn* Pawn = Cast<APawn>(Target);
	return Pawn && Pawn->IsLocallyControlled() && !Pawn->HasAuthority();
}

bool ALNPGameplayCueNotify_FlameStream::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (IsPredictingOwner(MyTarget))
		return false;

	StartStream(MyTarget);
	return true;
}

bool ALNPGameplayCueNotify_FlameStream::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 늦게 릴러번트해진 관전자는 OnActive 없이 WhileActive만 받는다.
	if (IsPredictingOwner(MyTarget))
		return false;

	StartStream(MyTarget);
	return true;
}

bool ALNPGameplayCueNotify_FlameStream::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 게스트 소유자: 로컬 펄스가 유지 중이면 그쪽 타이머가 끝낸다. 아니면 늦게 온 GE 이벤트가 새로 만든
	// 빈 인스턴스이므로(GameplayCueSet은 Removed에도 인스턴스를 만든다) 여기서 회수해야 새지 않는다.
	if (IsPredictingOwner(MyTarget) && GetWorldTimerManager().IsTimerActive(LocalHoldTimer))
		return false;

	StopStream();
	GameplayCueFinishedCallback();
	return true;
}

bool ALNPGameplayCueNotify_FlameStream::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (!IsPredictingOwner(MyTarget))
		return false;

	StartStream(MyTarget);
	GetWorldTimerManager().SetTimer(LocalHoldTimer, this, &ALNPGameplayCueNotify_FlameStream::OnLocalHoldExpired,
		FMath::Max(Parameters.RawMagnitude, 0.05f), false);
	return true;
}

void ALNPGameplayCueNotify_FlameStream::OnLocalHoldExpired()
{
	StopStream();
	GameplayCueFinishedCallback();
}

void ALNPGameplayCueNotify_FlameStream::StartStream(AActor* Target)
{
	if (StreamComponent && StreamComponent->IsActive())
		return;

	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Target);
	USkeletalMeshComponent* WeaponMesh = Character ? Character->GetWeaponMesh() : nullptr;
	if (!WeaponMesh || !StreamVFX)
		return;

	StreamComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(StreamVFX, WeaponMesh, AttachSocket,
		FVector::ZeroVector, RelativeRotation, EAttachLocation::SnapToTarget, /*bAutoDestroy=*/false);
}

void ALNPGameplayCueNotify_FlameStream::StopStream()
{
	GetWorldTimerManager().ClearTimer(LocalHoldTimer);

	// 이미 뿜은 불길은 수명대로 사라지게 두고, 컴포넌트는 그 뒤 스스로 파괴된다 — 큐 액터는 곧바로 회수돼도 된다.
	if (StreamComponent)
	{
		StreamComponent->SetAutoDestroy(true);
		StreamComponent->Deactivate();
	}
	StreamComponent = nullptr;
}

bool ALNPGameplayCueNotify_FlameStream::Recycle()
{
	StopStream();
	return Super::Recycle();
}

void ALNPGameplayCueNotify_FlameStream::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStream();
	Super::EndPlay(EndPlayReason);
}

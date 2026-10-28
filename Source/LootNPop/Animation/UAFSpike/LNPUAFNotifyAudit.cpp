// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFNotifyAudit.h"
#include "Animation/UAFSpike/LNPUAFSpikePlayerCharacter.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

static TAutoConsoleVariable<int32> CVarUAFNotifyAudit(
	TEXT("LNP.UAFSpike.DebugNotifies"), 0,
	TEXT("Audit notify callbacks on UAF spike players only. 0: off, 1: Begin/End and entity cleanup, 2: include functional Tick callbacks."));

static ALNPUAFSpikePlayerCharacter* GetAuditCharacter(USkeletalMeshComponent* MeshComp)
{
	return CVarUAFNotifyAudit.GetValueOnGameThread() > 0 && MeshComp
		? Cast<ALNPUAFSpikePlayerCharacter>(MeshComp->GetOwner()) : nullptr;
}

void LNPUAFNotifyAudit::Trace(const TCHAR* Phase, const UAnimNotifyState* Notify, USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	ALNPUAFSpikePlayerCharacter* Character = GetAuditCharacter(MeshComp);
	if (!Character || (FCString::Strcmp(Phase, TEXT("Tick")) == 0 && CVarUAFNotifyAudit.GetValueOnGameThread() < 2))
		return;

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const FGameplayAbilityActorInfo* ActorInfo = ASC ? ASC->AbilityActorInfo.Get() : nullptr;
	const FAnimNotifyEvent* Event = EventReference.GetNotify();
	UE_LOG(LogLootNPop, Log, TEXT("[UAFNotify] phase=%s frame=%llu time=%.6f notify=%s event=%p mode=%d mesh=%s anim=%s instance=%s gasMesh=%s gasAnim=%s move=%d attack=%d combo=%d"),
		Phase, GFrameCounter, Character->GetWorld()->GetTimeSeconds(), *GetPathNameSafe(Notify), Event,
		Event ? static_cast<int32>(Event->MontageTickType.GetValue()) : -1,
		*GetPathNameSafe(MeshComp), *GetPathNameSafe(Animation), *GetPathNameSafe(MeshComp->GetAnimInstance()),
		*GetPathNameSafe(ActorInfo ? ActorInfo->SkeletalMeshComponent.Get() : nullptr),
		*GetPathNameSafe(ActorInfo ? ActorInfo->GetAnimInstance() : nullptr),
		ASC ? ASC->GetTagCount(TAG_Block_MovementInput) : 0,
		ASC ? ASC->GetTagCount(TAG_Block_AttackInput) : 0,
		ASC ? ASC->GetTagCount(TAG_State_ComboWindow) : 0);
}

void LNPUAFNotifyAudit::TraceMeleeEntity(USkeletalMeshComponent* MeshComp, FMassEntityHandle Entity, bool bDestroy)
{
	if (!GetAuditCharacter(MeshComp))
		return;
	const FString MeshPath = MeshComp->GetPathName();
	UE_LOG(LogLootNPop, Log, TEXT("[UAFMeleeEntity] phase=%s frame=%llu mesh=%s entity=%d:%d"),
		bDestroy ? TEXT("DestroyQueued") : TEXT("BuildQueued"), GFrameCounter, *MeshPath, Entity.Index, Entity.SerialNumber);
	if (!bDestroy)
		return;

	// Mass의 지연 파괴가 반영된 뒤 읽기만 한다. TTL이나 정리 경로를 바꾸지 않는다.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakWorld = TWeakObjectPtr<UWorld>(MeshComp->GetWorld()), MeshPath, Entity](float)
		{
			UWorld* World = WeakWorld.Get();
			UMassEntitySubsystem* Subsystem = World && !World->bIsTearingDown ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
			if (Subsystem)
			{
				const FMassEntityManager& Manager = Subsystem->GetEntityManager();
				const bool bValid = Manager.IsEntityValid(Entity);
				UE_LOG(LogLootNPop, Log, TEXT("[UAFMeleeEntity] phase=AfterDestroy frame=%llu mesh=%s entity=%d:%d valid=%d active=%d"),
					GFrameCounter, *MeshPath, Entity.Index, Entity.SerialNumber,
					bValid, bValid && Manager.IsEntityActive(Entity));
			}
			else
				UE_LOG(LogLootNPop, Log, TEXT("[UAFMeleeEntity] phase=AfterDestroyUnavailable mesh=%s entity=%d:%d"),
					*MeshPath, Entity.Index, Entity.SerialNumber);
			return false;
		}), 0.1f);
}

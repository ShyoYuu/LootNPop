// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_JustDodgeBuff.h"
#include "Character/LNPCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

#if !UE_BUILD_SHIPPING
#include "Character/LNPPlayerCharacter.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "MassAgentComponent.h"
#include "MassEntityUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "TimerManager.h"
#endif

bool ALNPGameplayCueNotify_JustDodgeBuff::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MyTarget);
	if (!Character || Character->IsRagdollActive())
		return false;

	FGameplayCueParameters AttachedParameters = Parameters;
	AttachedParameters.TargetAttachComponent = Character->GetVisualMesh();
	return Super::WhileActive_Implementation(MyTarget, AttachedParameters);
}

#if !UE_BUILD_SHIPPING
namespace
{
	FTimerHandle GLNPJustDodgeSampleTimer;

	// 임시 확인 명령. 기본은 실시간 재생이며 pause를 지정할 때만 에디터 정지 샘플을 만든다.
	void RunJustDodgeTest(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->WorldType != EWorldType::PIE || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[DodgeTest] Run in a standalone/listen-host PIE world."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->IsLocalController())
			return;

		const FString Mode = Args.IsEmpty() ? TEXT("") : Args[0].ToLower();
		if (!Mode.IsEmpty() && Mode != TEXT("pause") && Mode != TEXT("resume"))
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[DodgeTest] Usage: LNP.Debug.JustDodgeTest [pause|resume]"));
			return;
		}

		World->GetTimerManager().ClearTimer(GLNPJustDodgeSampleTimer);
		PC->SetPause(false);
		World->bDebugPauseExecution = false;
		if (Mode == TEXT("resume"))
		{
			UE_LOG(LogLootNPop, Log, TEXT("[DodgeTest] Resumed gamePaused=%d editorPaused=%d"),
				PC->IsPaused() ? 1 : 0, World->bDebugPauseExecution ? 1 : 0);
			return;
		}

		const bool bPauseSample = Mode == TEXT("pause");
		ALNPPlayerCharacter* Player = Cast<ALNPPlayerCharacter>(PC->GetPawn());
		if (!Player || Player->IsDead())
			return;

		const UMassAgentComponent* Agent = Player->FindComponentByClass<UMassAgentComponent>();
		const FMassEntityHandle Entity = Agent ? Agent->GetEntityHandle() : FMassEntityHandle();
		FMassEntityManager& Manager = UE::Mass::Utils::GetEntityManagerChecked(*World);
		FLNPParryStateFragment* State = Manager.IsEntityActive(Entity)
			? Manager.GetFragmentDataPtr<FLNPParryStateFragment>(Entity) : nullptr;
		if (!State)
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[DodgeTest] Player combat entity is not ready."));
			return;
		}

		// 재입력할 수 있게 소비 플래그·방향만 잠깐 빌린다. 판정 창을 열거나 대시를 실행하지 않는다.
		{
			TGuardValue<bool> Consumed(State->bJustDodgeConsumed, false);
			TGuardValue<FVector> Direction(State->DodgeDirection, Player->GetActorForwardVector());
			FLNPJustDodgeCommand::ApplyEntry(Manager, { Entity, Player, TEXT("Debug") });
		}

		World->GetTimerManager().SetTimer(GLNPJustDodgeSampleTimer, FTimerDelegate::CreateWeakLambda(PC, [PC, bPauseSample]()
		{
			if (const ALNPPlayerCharacter* Sample = Cast<ALNPPlayerCharacter>(PC->GetPawn()))
			{
				// 풀 컴포넌트는 캐릭터가 아니라 WorldSettings가 소유한다.
				TArray<UNiagaraComponent*> Components;
				Sample->GetWorld()->GetWorldSettings()->GetComponents(Components);
				UE_LOG(LogLootNPop, Log, TEXT("[DodgeTest] Snapshot visual=%s poolComponents=%d"),
					*GetPathNameSafe(Sample->GetVisualMesh()), Components.Num());
				for (UNiagaraComponent* FX : Components)
				{
					if (!FX || (GetNameSafe(FX->GetAsset()) != TEXT("NS_JustDodge_Aura")
						&& GetNameSafe(FX->GetAsset()) != TEXT("NS_JustDodge_FootAura")))
						continue;
					FString Counts;
					if (const auto Controller = FX->GetSystemInstanceController())
					{
						Controller->WaitForConcurrentTickAndFinalize();
						if (const FNiagaraSystemInstance* Instance = Controller->GetSystemInstance_Unsafe())
							for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
								Counts += FString::Printf(TEXT("%d "), Emitter->GetNumParticles());
					}
					UE_LOG(LogLootNPop, Log, TEXT("[DodgeTest] Aura parent=%s bone=%s location=%s scale=%s active=%d particles=[%s]"),
						*GetPathNameSafe(FX->GetAttachParent()), *FX->GetAttachSocketName().ToString(), *FX->GetComponentLocation().ToString(),
						*FX->GetComponentScale().ToString(), FX->IsActive() ? 1 : 0, *Counts);
				}
			}
			if (bPauseSample)
				PC->GetWorld()->bDebugPauseExecution = true;
			UE_LOG(LogLootNPop, Log, TEXT("[DodgeTest] Sample captured editorPaused=%d; use 'LNP.Debug.JustDodgeTest resume' or editor Resume."),
				PC->GetWorld()->bDebugPauseExecution ? 1 : 0);
		}), 0.3f, false);
		UE_LOG(LogLootNPop, Log, TEXT("[DodgeTest] Triggered player=%s samplePause=%d."), *GetNameSafe(Player), bPauseSample ? 1 : 0);
	}

	FAutoConsoleCommandWithWorldAndArgs GLNPJustDodgeTest(
		TEXT("LNP.Debug.JustDodgeTest"),
		TEXT("PIE host only: play the local player's Just Dodge buff and success cue. Optional args: pause (editor-pause after 0.3s), resume (clear game/editor pause)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunJustDodgeTest), ECVF_Cheat);
}
#endif

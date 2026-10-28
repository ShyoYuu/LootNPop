// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/UAFNotifyDispatcherComponent.h"

void FLNPUAFNotifyCleanup::OnEndExecution(float InDeltaTime)
{
	// 등록 시 기본 dispatcher가 먼저 존재하므로, 그쪽이 이번 프레임 목록을 갱신한 뒤 복사한다.
	const FUAFNotifyDispatcherComponent* Dispatcher = GetModuleInstance().TryGetComponent<FUAFNotifyDispatcherComponent>();
	if (Dispatcher)
	{
		ActiveNotifyReferences = Dispatcher->ActiveAnimNotifyEventReference;
		DispatchMesh = Dispatcher->SkeletalMeshComponent;
	}
}

FLNPUAFNotifyCleanup::~FLNPUAFNotifyCleanup()
{
	if (ActiveNotifyReferences.IsEmpty())
		return;

	// UAF의 모듈 해제는 게임 스레드에서 tick을 등록 해제한 뒤 컴포넌트를 해제한다.
	check(IsInGameThread());
	TArray<FAnimNotifyEventReference> Remaining = MoveTemp(ActiveNotifyReferences);
	USkeletalMeshComponent* Mesh = DispatchMesh.Get();
	if (!Mesh || !Mesh->GetWorld() || Mesh->GetWorld()->bIsTearingDown)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[UAFNotifyCleanup] Module release could not end %d states: dispatch mesh or world is no longer available."), Remaining.Num());
		return;
	}

	const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.UAFSpike.DebugNotifies"));
	if (Debug && Debug->GetInt() > 0)
		UE_LOG(LogLootNPop, Log, TEXT("[UAFNotifyCleanup] phase=ModuleRelease mesh=%s remaining=%d"), *Mesh->GetPathName(), Remaining.Num());

	for (const FAnimNotifyEventReference& Reference : Remaining)
	{
		const FAnimNotifyEvent* Event = Reference.GetNotify();
		if (Event && Event->NotifyStateClass)
		{
			UAnimNotifyState* Notify = Event->NotifyStateClass;
			Notify->NotifyEnd(Mesh, Cast<UAnimSequenceBase>(Notify->GetOuter()), Reference);
		}
	}
}

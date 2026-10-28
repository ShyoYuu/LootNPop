// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Component/AnimNextComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Misc/ScopeLock.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/UAFNotifyDispatcherComponent.h"
#include "Module/UAFWeakSystemReference.h"

static void DispatchNotifyEnds(USkeletalMeshComponent* Mesh, const TArray<FAnimNotifyEventReference>& References, const TCHAR* Phase)
{
	const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.UAFSpike.DebugNotifies"));
	if (Debug && Debug->GetInt() > 0)
		UE_LOG(LogLootNPop, Log, TEXT("[UAFNotifyCleanup] phase=%s mesh=%s remaining=%d"), Phase, *Mesh->GetPathName(), References.Num());

	for (const FAnimNotifyEventReference& Reference : References)
	{
		const FAnimNotifyEvent* Event = Reference.GetNotify();
		if (Event && Event->NotifyStateClass)
		{
			UAnimNotifyState* Notify = Event->NotifyStateClass;
			Notify->NotifyEnd(Mesh, Cast<UAnimSequenceBase>(Notify->GetOuter()), Reference);
		}
	}
}

void FLNPUAFNotifyCleanup::OnEndExecution(float InDeltaTime)
{
	FScopeLock Lock(&Mutex);
	if (bOwnerShuttingDown)
		return;

	// 등록 시 기본 dispatcher가 먼저 존재하므로, 그쪽이 이번 프레임 목록을 갱신한 뒤 복사한다.
	const FUAFNotifyDispatcherComponent* Dispatcher = GetModuleInstance().TryGetComponent<FUAFNotifyDispatcherComponent>();
	if (Dispatcher)
	{
		ActiveNotifyReferences = Dispatcher->ActiveAnimNotifyEventReference;
		DispatchMesh = Dispatcher->SkeletalMeshComponent;
	}
}

void FLNPUAFNotifyCleanup::CopyForOwnerShutdown(USkeletalMeshComponent* Mesh, TArray<FAnimNotifyEventReference>& OutReferences) const
{
	FScopeLock Lock(&Mutex);
	if (bOwnerShuttingDown || DispatchMesh.Get() != Mesh)
		return;

	// 실제 콜백 전에 종료 소유권을 넘겨 재진입과 이후 모듈 소멸자의 중복 End를 막는다.
	bOwnerShuttingDown = true;
	OutReferences = ActiveNotifyReferences;
}

void FLNPUAFNotifyCleanup::EndBeforeOwnerShutdown(UUAFComponent* Component, USkeletalMeshComponent* Mesh, const TCHAR* Phase)
{
	check(IsInGameThread());
	if (!Component || !Mesh || !Mesh->GetWorld())
		return;

	TArray<FAnimNotifyEventReference> Remaining;
	const FUAFWeakSystemReference Reference = Component->GetSystemReference();
	if (Reference.IsValid())
	{
		Reference.ReadComponent<FLNPUAFNotifyCleanup>(
			[Mesh, &Remaining](TConstStructView<FLNPUAFNotifyCleanup> Cleanup)
			{
				Cleanup.Get().CopyForOwnerShutdown(Mesh, Remaining);
			});
	}

	// 다음 프레임 실행을 중지한다. 실제 모듈 해제는 지연되므로 그 소멸자에 End를 맡기지 않는다.
	if (Component->IsRegistered())
		Component->UnregisterComponent();
	if (!Remaining.IsEmpty())
	{
		// 월드 EndPlay는 이미 tearing down 상태여도 메시/PlayerState/서브시스템이 남은 이 시점에 정리한다.
		// 게임플레이 콜백은 UAF의 읽기 잠금과 사본 잠금 밖에서 실행한다.
		DispatchNotifyEnds(Mesh, Remaining, Phase);
	}
}

FLNPUAFNotifyCleanup::~FLNPUAFNotifyCleanup()
{
	// UAF의 모듈 해제는 게임 스레드에서 tick을 등록 해제한 뒤 컴포넌트를 해제한다.
	TArray<FAnimNotifyEventReference> Remaining;
	{
		FScopeLock Lock(&Mutex);
		if (bOwnerShuttingDown || ActiveNotifyReferences.IsEmpty())
			return;
		check(IsInGameThread());
		Remaining = MoveTemp(ActiveNotifyReferences);
	}
	USkeletalMeshComponent* Mesh = DispatchMesh.Get();
	if (!Mesh || !Mesh->GetWorld() || Mesh->GetWorld()->bIsTearingDown)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[UAFNotifyCleanup] Module release could not end %d states: dispatch mesh or world is no longer available."), Remaining.Num());
		return;
	}

	DispatchNotifyEnds(Mesh, Remaining, TEXT("ModuleRelease"));
}

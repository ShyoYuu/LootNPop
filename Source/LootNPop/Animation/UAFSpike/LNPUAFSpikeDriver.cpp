// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeDriver.h"

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"
#include "Animation/UAFSpike/LNPUAFNotifyCleanup.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleTickFunction.h"
#include "Module/UAFWeakSystemReference.h"

static TAutoConsoleVariable<int32> CVarDestroyBeforeFirstEvent(
	TEXT("LNP.UAFSpike.DestroyBeforeFirstEvent"), 0,
	TEXT("Test only. A UAF spike enemy destroys itself on an input tick, before that frame's UAF PrePhysics event runs.\n")
	TEXT("1: first tick of the frame after spawn. The system already ran once in its spawn frame, so this does not hit the AsShared assert.\n")
	TEXT("2: first tick in the spawn frame. The system has never run; with LNP.UAFSpike.DisableTicksOnRelease 0 this reproduces the AsShared assert (crashes the editor)."));

static TAutoConsoleVariable<bool> CVarDisableTicksOnRelease(
	TEXT("LNP.UAFSpike.DisableTicksOnRelease"), true,
	TEXT("Disables the UAF system's event tick functions before a spike pawn releases its system.\n")
	TEXT("UAF unregisters them only in its deferred delete before the next world tick, so an event still pending this frame runs on the released system.\n")
	TEXT("0: engine behavior, test only (a never-run system then hits the AsShared assert)."));

// 입력 공급이 선행 조건으로 거는 이벤트이자, 해제 전 시스템 틱 함수를 찾는 사용자 이벤트.
static const FName SystemEventName(TEXT("PrePhysics"));

ULNPUAFSpikeInputComponent::ULNPUAFSpikeInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULNPUAFSpikeInputComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlayFrame = GFrameCounter;
	AddTickPrerequisiteActor(GetOwner());
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPInputHandlerComponent>());
	// 적에는 ControlRotation 컴포넌트가 없다(null이면 엔진이 무시한다).
	AddTickPrerequisiteComponent(GetOwner()->FindComponentByClass<ULNPControlRotationComponent>());
	if (auto* UAF = GetOwner()->FindComponentByClass<UUAFComponent>())
	{
		UAF->AddComponentPrerequisite(this, SystemEventName);
		// 해제 진단용: 첫 사용자 이벤트 시작 직전에 한 번 기록한다.
		UAF->QueueTask(NAME_None, [Ran = SystemEventRan](const UE::UAF::FModuleTaskContext&) { Ran->store(true); });
	}
}

void ULNPUAFSpikeInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	const auto* Character = CastChecked<ALNPCharacterBase>(Owner);

	// 재현 시험: 월드 틱 도중, 이번 프레임 PrePhysics 이벤트(이 틱이 선행 조건) 직전에 적 Actor를 파괴한다.
	// 시스템 틱 함수는 OnRegister에서 활성 상태로 등록되므로 월드 틱 중 스폰되면 스폰 프레임에 이미 첫 실행을 한다.
	// 그래서 1(다음 프레임)은 실행 이력이 있는 시스템, 2(스폰 프레임)는 한 번도 돌지 않은 시스템을 놓는다.
	const bool bSpawnFrameTick = GFrameCounter == BeginPlayFrame;
	const int32 DestroyMode = CVarDestroyBeforeFirstEvent.GetValueOnGameThread();
	if (!bTestDestroyChecked && Owner->IsA<ALNPEnemyCharacter>()
		&& ((DestroyMode == 1 && !bSpawnFrameTick) || (DestroyMode == 2 && bSpawnFrameTick)))
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[UAFSpikeDriver] %s test destroy mode=%d frame=%llu spawnFrame=%llu"),
			*Owner->GetName(), DestroyMode, GFrameCounter, BeginPlayFrame);
		Owner->Destroy();
		return;
	}
	bTestDestroyChecked |= !bSpawnFrameTick;

	// Mass 표현이 적 Actor를 끄면(ISKM 전환 중 유지) 기존 ABP는 원본 메시 틱과 함께 멈춘다. UAF 시스템도 같은 시점에 멈춘다.
	// 시스템 활성은 다음 월드 틱 직전에 반영된다. 시스템은 UAF 컴포넌트 BeginPlay에서 이미 활성이다.
	const bool bActorEnabled = Owner->IsActorTickEnabled();
	if (auto* UAF = Owner->FindComponentByClass<UUAFComponent>(); UAF && UAF->IsActive() != bActorEnabled)
	{
		UAF->SetActive(bActorEnabled);
		UE_LOG(LogLootNPop, Log, TEXT("[UAFSpikeDriver] %s system=%s frame=%llu"),
			*Owner->GetName(), bActorEnabled ? TEXT("enabled") : TEXT("disabled"), GFrameCounter);
	}
	if (!bActorEnabled)
		return;

	if (auto* Source = Cast<ULNPUAFSpikeAnimInstance>(Character->GetAnimInstance()))
		Source->UpdateFromDriver(DeltaTime);

	// 애니 평가가 꺼진 표시 메시는 랙돌 물리 블렌드 뒤 본 버퍼 교체가 요청되지 않아(엔진은 애니 평가 경로에서만 요청)
	// 렌더가 마지막 UAF 포즈에 멈춘다. 랙돌 동안은 ABP 없이 엔진 기본 평가(참조 포즈)로 되돌려 표준 블렌드 경로를 쓴다.
	// SetEnableAnimation은 프레임 끝에 반영되므로 다음 틱까지 같은 요청이 반복될 수 있다(값이 같으면 무시된다).
	const bool bRagdoll = Character->IsRagdollActive();
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() != TEXT("VisualMesh") || Mesh->bEnableAnimation == bRagdoll)
			continue;
		if (bRagdoll)
			Mesh->SetAnimInstanceClass(nullptr);
		Mesh->SetEnableAnimation(bRagdoll);
	}
}

bool LNPUAFSpikeDriver::InitializeSource(USkeletalMeshComponent& Source, bool& bReportedError)
{
	const UClass* SourceClass = Source.AnimClass.Get();
	if (Source.bEnableAnimation || !SourceClass || !SourceClass->IsChildOf(ULNPUAFSpikeAnimInstance::StaticClass()))
	{
		if (!bReportedError)
		{
			UE_LOG(LogLootNPop, Error, TEXT("[UAFSpikeDriver] %s requires AnimSourceMesh animation disabled and a UAF spike AnimClass."), *GetNameSafe(Source.GetOwner()));
			bReportedError = true;
		}
		return false;
	}

	// 일반 평가가 꺼져 있으면 InitAnim은 인스턴스를 만들지 않는다. UAF 그래프 생성 전에 공급층을 준비한다.
	if (!Source.GetAnimInstance())
		Source.AnimScriptInstance = NewObject<ULNPUAFSpikeAnimInstance>(&Source, Source.AnimClass);
	ULNPUAFSpikeAnimInstance* Instance = Cast<ULNPUAFSpikeAnimInstance>(Source.GetAnimInstance());
	if (!Instance)
		return false;
	Instance->InitializeExplicitDriver();
	return true;
}

ULNPPhysicalHitReactAnimInstance* LNPUAFSpikeDriver::ResolveHitReactInstance(ULNPPhysicalHitReactAnimInstance* Display, const USkeletalMeshComponent& Source)
{
	if (Display)
		return Display;
	return Cast<ULNPUAFSpikeAnimInstance>(Source.GetAnimInstance());
}

// UAF는 시스템을 놓을 때 공유 포인터만 즉시 해제하고, 이벤트 틱 함수는 다음 월드 틱 직전의 지연 삭제에서 등록 해제한다.
// 그 사이 이번 프레임에 아직 돌지 않은 이벤트가 해제된 시스템 위에서 돌고, 첫 실행이면 그래프 할당(GetReference)의 AsShared 단언으로 죽는다.
// 틱 작업은 실행 직전에 활성 여부를 다시 보므로, 놓기 전에 모든 이벤트 틱을 끄면 이번 프레임 실행도 막힌다.
static void StopSystemBeforeRelease(AActor& Owner, const TCHAR* Phase)
{
	auto* UAF = Owner.FindComponentByClass<UUAFComponent>();
	if (!UAF || !UAF->GetSystemReference().IsValid())
		return;

	const bool bDisable = CVarDisableTicksOnRelease.GetValueOnGameThread();
	if (bDisable)
	{
		if (const auto* EventTick = static_cast<const UE::UAF::FModuleEventTickFunction*>(UAF->FindTickFunction(SystemEventName)))
		{
			for (UE::UAF::FModuleEventTickFunction& TickFunction : EventTick->ModuleInstance->GetTickFunctions())
				TickFunction.SetTickFunctionEnable(false);
		}
	}

	// 진단: 한 번도 돌지 않은 시스템을 놓는 경로를 남기고, 놓은 뒤 이벤트가 돌면 오류로 남긴다(틱을 껐다면 돌지 않아야 한다).
	const auto* Input = Owner.FindComponentByClass<ULNPUAFSpikeInputComponent>();
	const UWorld* World = Owner.GetWorld();
	if (Input && !Input->HasSystemEventRun())
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[UAFSpikeDriver] %s releases a UAF system before its first event phase=%s net=%s frame=%llu spawnFrame=%llu tickGroup=%s ticks=%s"),
			*Owner.GetName(), Phase, World ? *ToString(World->GetNetMode()) : TEXT("None"), GFrameCounter, Input->GetBeginPlayFrame(),
			World ? *StaticEnum<ETickingGroup>()->GetNameStringByValue(World->TickGroup) : TEXT("None"), bDisable ? TEXT("disabled") : TEXT("left"));
	}
	UAF->QueueTask(NAME_None, [Name = Owner.GetName(), ReleaseFrame = GFrameCounter](const UE::UAF::FModuleTaskContext&)
	{
		UE_LOG(LogLootNPop, Error, TEXT("[UAFSpikeDriver] %s UAF event ran after its system was released releaseFrame=%llu frame=%llu"),
			*Name, ReleaseFrame, GFrameCounter);
	});
}

void LNPUAFSpikeDriver::ShutdownNotifies(AActor& Owner, USkeletalMeshComponent* Source, const TCHAR* Phase)
{
	StopSystemBeforeRelease(Owner, Phase);
	FLNPUAFNotifyCleanup::EndBeforeOwnerShutdown(Owner.FindComponentByClass<UUAFComponent>(), Source, Phase);
}

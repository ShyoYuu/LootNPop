// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include <atomic>
#include "LNPUAFSpikeDriver.generated.h"

class ULNPPhysicalHitReactAnimInstance;
class USkeletalMeshComponent;

/**
 * Actor·입력·ControlRotation 뒤, UAF 앞에서 시험 입력을 한 번 공급한다.
 * UAF 시험 폰(플레이어·적)의 표지이기도 하다 — ANS 감사가 이 컴포넌트 유무로 대상을 고른다.
 */
UCLASS()
class ULNPUAFSpikeInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPUAFSpikeInputComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	uint64 GetBeginPlayFrame() const { return BeginPlayFrame; }
	/** UAF 시스템의 첫 사용자 이벤트가 한 번이라도 시작됐는가(워커 스레드가 기록한다). */
	bool HasSystemEventRun() const { return SystemEventRan->load(); }

private:
	/** 재현 시험 CVar용 — 스폰 프레임/다음 프레임의 첫 틱을 고른다. */
	uint64 BeginPlayFrame = 0;
	bool bTestDestroyChecked = false;
	TSharedRef<std::atomic<bool>> SystemEventRan = MakeShared<std::atomic<bool>>(false);
};

/** 플레이어·적 UAF 시험 폰이 공유하는 원본 공급층 수명주기. */
namespace LNPUAFSpikeDriver
{
	/** 일반 평가가 꺼진 원본 메시에 공급층 인스턴스를 만들고 명시적 드라이버로 초기화한다. UAF 그래프 생성 전에 불러야 한다. */
	bool InitializeSource(USkeletalMeshComponent& Source, bool& bReportedError);

	/** 표시 ABP가 남아 있으면 그쪽 RBWC가 맡고(표시 경로 비교용), 없으면 원본 공급층이 물리 CR 입력을 계산한다. */
	ULNPPhysicalHitReactAnimInstance* ResolveHitReactInstance(ULNPPhysicalHitReactAnimInstance* Display, const USkeletalMeshComponent& Source);

	/** 원본 메시/PlayerState 연결이 해제되기 전에 활성 ANS를 종료하고, 시스템을 놓기 전에 이벤트 틱을 멈춘다. */
	void ShutdownNotifies(AActor& Owner, USkeletalMeshComponent* Source, const TCHAR* Phase);
}

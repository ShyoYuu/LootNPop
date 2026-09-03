// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPNetBudgetDebug.generated.h"

/**
 * 대역폭 예산 조사용 임시 계측 (I-009). 조사 종료 시 이 파일과 Build 참조를 통째로 제거한다.
 *
 * `LNP.Net.Budget <초>` 로 켜면 그 주기마다 원격 연결별로 한 줄을 남긴다.
 *   - 송신량/상한 (UNetConnection::OutBytesPerSecond / CurrentNetSpeed) — 포화 여부
 *   - Mass 버블 구성 (전체 / 적 / 그 외) — 엔티티당 단가를 역산하는 분모
 *   - 서버가 실제로 스폰한 Actor 수 (적 / Pod / 주사위) — Actor 승격 비용의 분모
 *
 * 클래스·프로퍼티 단위 바이트 분해는 이 로그가 아니라 NetTrace가 한다:
 *   실행 인자 `-trace=net,frame -NetTrace=2` (또는 런타임 `NetTrace.SetTraceVerbosity 2`)
 *   → Unreal Insights의 Networking 탭.
 * 이 로그는 그 트레이스를 읽을 때 필요한 **분모와 시간축**을 제공하는 역할이다.
 */
UCLASS()
class LOOTNPOP_API ULNPNetBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** 다음 로그까지 남은 시간(초). */
	float TimeUntilNextLog = 0.f;

	/** Iris 활성 여부 1회 보고. 넷 드라이버가 생기기 전에는 판정할 수 없어 첫 틱이 아니라 첫 '유효' 틱에 찍는다. */
	bool bReportedReplicationMode = false;
};

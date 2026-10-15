// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPWindFieldSubsystem.generated.h"

class ALNPWindField;

/**
 * 윈드필드 레지스트리. 활공 시뮬레이션이 매 스텝 "이 위치가 상승 기류 안인가"를 묻는 곳이다.
 *
 * 물리 오버랩이 아니라 **기하 판정**(구면 원판 각도 + 고도)으로 답한다 — 재시뮬레이션에서 결정적이고,
 * 오버랩 이벤트처럼 컴포넌트 갱신 시점에 묶이지 않는다. 행성 중심은 월드 원점이다.
 */
UCLASS()
class LOOTNPOP_API ULNPWindFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterWindField(ALNPWindField* Field);
	void UnregisterWindField(ALNPWindField* Field);

	/**
	 * Location이 켜진 윈드필드 안이면 true와 목표 고도 셸의 행성 중심 거리(cm)를 돌려준다.
	 * 여러 필드가 겹치면 목표가 가장 높은 것을 고른다.
	 */
	bool FindWindLift(const FVector& Location, float& OutTargetCenterDistance) const;

	const TArray<TWeakObjectPtr<ALNPWindField>>& GetWindFields() const { return WindFields; }

private:
	TArray<TWeakObjectPtr<ALNPWindField>> WindFields;
};

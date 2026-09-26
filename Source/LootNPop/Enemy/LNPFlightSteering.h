// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;

/**
 * 완전 비행 개체의 3D local planner(D-019, SurfaceSupportNavigation Phase03c §3.3).
 *
 * - Mass fragment를 받지 않는 순수 함수다. Mass 비행 이동 프로세서, 자동화, 이후의 ActorPromoted 비행 엘리트가 같은 함수를 부른다.
 * - 모든 스레드에서 호출할 수 있다. Mass worker의 동기 exact query만 쓴다(D-025). query 분류는 FlightSteering이다.
 * - 위치는 몸 중심이고, 지형과의 여유는 lookahead 구(몸 반지름 + Clearance)가 보장한다.
 */
namespace LNPFlightSteering
{
	struct FParams
	{
		/** 몸을 감싸는 구 반지름(cm). */
		float BodyRadius = 60.f;

		/** 몸과 지형 사이에 남길 여유(cm). */
		float Clearance = 50.f;

		/** 전방 lookahead 길이(초). 한 프레임보다 짧으면 한 프레임으로 올린다. */
		float LookaheadTime = 1.f;
	};

	enum class EStepResult : uint8
	{
		/** 원하는 속도가 0이라 query 없이 제자리에 떠 있다. */
		Hover,
		/** lookahead가 비었다. 원하는 속도대로 옮겼다. */
		Clear,
		/** lookahead가 막혔다. 여유를 남긴 지점까지만 옮겼다. 회피는 구현 단위 2에서 이 결과를 받는다. */
		Blocked,
	};

	/**
	 * 목표점으로 향하는 원하는 속도. 남은 거리가 한 프레임 이동보다 짧으면 딱 도착하도록 줄여 넘어서지 않게 한다.
	 * 목표가 이미 겹쳐 있으면 0이다.
	 */
	LOOTNPOP_API FVector ComputeArrivalVelocity(const FVector& Location, const FVector& Goal, float MaxSpeed, float DeltaTime);

	/**
	 * 한 프레임 비행. DesiredVelocity 방향으로 lookahead sphere sweep 1회.
	 * - clear면 DesiredVelocity × DeltaTime만큼 옮긴다. lookahead가 한 프레임 이동보다 길어 그 구간은 안전하다.
	 * - 막히면 lookahead 구가 닿는 지점(= 여유를 남긴 지점)까지만 옮긴다.
	 * - 시작부터 겹쳐 있으면(여유 안으로 들어와 있으면) 겹침 법선으로 풀기만 하고 전진하지 않는다.
	 * - Unknown hit도 막힘이다(D-037).
	 */
	LOOTNPOP_API EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& DesiredVelocity, float DeltaTime, FVector& OutLocation);
}

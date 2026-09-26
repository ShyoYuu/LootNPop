// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;

/**
 * 완전 비행 개체의 3D local planner(D-019, SurfaceSupportNavigation Phase03c §3.3).
 *
 * - Mass fragment를 받지 않는 순수 함수다. Mass 비행 이동 프로세서, 자동화, 이후의 ActorPromoted 비행 엘리트가 같은 함수를 부른다.
 *   개체별 기억은 FSteeringState 하나에 모여 있고, 호출자(fragment·컴포넌트)가 들고 있는다.
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

		/** 막혔을 때 고른 회피 방향을 유지하는 시간(초). 이 동안은 그 방향으로 lookahead 1회만 한다. */
		float AvoidHoldTime = 0.25f;

		/** 회피 후보 원뿔 반각(도). 교착이 이어지면 WideConeDeg로 넓힌다. */
		float ConeDeg = 45.f;
		float WideConeDeg = 90.f;

		/**
		 * 교착 복구 단계(초, 목표까지 거리가 ProgressEpsilon 이상 줄지 않은 시간).
		 * 넓힘 → 원뿔을 넓히고 후퇴 방향을 후보에 넣는다. 반전 → 위·아래 선호를 뒤집는다. 포기 → Stuck을 돌려준다.
		 */
		float StuckWidenTime = 1.5f;
		float StuckReverseTime = 3.f;
		float StuckGiveUpTime = 5.f;

		/** 이만큼 가까워져야 진행으로 본다(cm). */
		float ProgressEpsilon = 50.f;

		/** 목표가 이만큼 옮겨 가면 진행 기준을 새로 잡는다(cm). 움직이는 타겟을 쫓을 때 교착으로 오판하지 않게 한다. */
		float GoalMovedReset = 300.f;
	};

	/** 개체별 steering 기억. 0으로 초기화된 값이 "기억 없음"이다. */
	struct FSteeringState
	{
		/** 유지 중인 회피 방향(단위 벡터). 0이면 없다. */
		FVector AvoidDirection = FVector::ZeroVector;
		float AvoidTimeRemaining = 0.f;

		/** 진행 측정 기준. */
		FVector ProgressGoal = FVector::ZeroVector;
		float BestGoalDistance = 0.f;
		float NoProgressTime = 0.f;

		/** 회피 후보의 위·아래 선호(+1 = 구 중심 쪽, -1 = 지각 쪽). 반전 단계에서 한 번 뒤집는다. */
		float VerticalPreference = 1.f;
		bool bVerticalReversed = false;

		void Reset() { *this = FSteeringState(); }
	};

	enum class EStepResult : uint8
	{
		/** 원하는 속도가 0이라 query 없이 제자리에 떠 있다. */
		Hover,
		/** lookahead가 비었다. 원하는 속도대로 옮겼다. */
		Clear,
		/** lookahead가 막혔다. 여유를 남긴 지점까지만 옮겼다. */
		Blocked,
		/** 교착 복구를 다 해도 진행이 없다. 호출자가 다른 목표를 골라야 한다. 이번 프레임은 추가 속도만 적용했다. */
		Stuck,
	};

	/**
	 * 목표점으로 향하는 원하는 속도. 남은 거리가 한 프레임 이동보다 짧으면 딱 도착하도록 줄여 넘어서지 않게 한다.
	 * 목표가 이미 겹쳐 있으면 0이다.
	 */
	LOOTNPOP_API FVector ComputeArrivalVelocity(const FVector& Location, const FVector& Goal, float MaxSpeed, float DeltaTime);

	/**
	 * 한 프레임 직선 비행. DesiredVelocity 방향으로 lookahead sphere sweep 1회.
	 * - clear면 DesiredVelocity × DeltaTime만큼 옮긴다. lookahead가 한 프레임 이동보다 길어 그 구간은 안전하다.
	 * - 막히면 lookahead 구가 닿는 지점(= 여유를 남긴 지점)까지만 옮긴다.
	 * - 시작부터 겹쳐 있으면(여유 안으로 들어와 있으면) 겹침 법선으로 풀기만 하고 전진하지 않는다.
	 * - Unknown hit도 막힘이다(D-037).
	 */
	LOOTNPOP_API EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& DesiredVelocity, float DeltaTime, FVector& OutLocation);

	/**
	 * 한 프레임 조향 비행(Phase03c §3.3). 목표점으로 가되, 막히면 후보 heading을 평가해 회피 방향을 고르고 유지한다.
	 * - ExtraVelocity(분리력 등)는 방향 선택과 무관하게 더한다. 이동은 언제나 Step의 sweep을 거친다.
	 * - query: 트인 프레임 1회, 막힌 프레임 1 + 후보 수(9~10회).
	 * - Up은 위·아래 후보의 기준(내부형 구에서는 구 중심 방향)이다.
	 */
	LOOTNPOP_API EStepResult Steer(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Up, const FVector& Goal, float MaxSpeed, const FVector& ExtraVelocity,
		float DeltaTime, FSteeringState& State, FVector& OutLocation);
}

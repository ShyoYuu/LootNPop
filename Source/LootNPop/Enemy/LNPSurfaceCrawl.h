// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;

/**
 * 벽보행 개체의 wall-walker 한 프레임(D-066, SurfaceSupportNavigation Phase 14 단위 2).
 *
 * - Mass fragment를 받지 않는 순수 함수다. Mass 벽보행 이동 프로세서와 자동화가 같은 함수를 부른다.
 * - 모든 스레드에서 호출할 수 있다. Mass worker의 동기 exact query만 쓴다(D-025). query 분류는 SurfaceCrawl이다.
 * - 몸은 구이고 위치는 구 중심이다. 표면 법선(Normal)은 표면 바깥 = 몸의 Up이며 구 중심 방향과 무관하다.
 * - 붙을 수 있는 표면은 hit identity가 Known이고 Support나 Blocker 역할이 있는 면이다. Unknown에는 붙지 않는다(D-037).
 * - 표면 법선은 구의 **접촉 법선**(접점 → 구 중심)이다. 평면에서는 면 법선과 같고, 볼록 모서리에서는 구가 모서리를 축으로
 *   굴러 돌며 법선이 연속으로 돈다 — 모서리 감싸기가 별도 분기 없이 probe 하나로 일어난다.
 */
namespace LNPSurfaceCrawl
{
	struct FParams
	{
		/** 몸 구 반지름(cm). */
		float BodyRadius = 40.f;

		/** 붙어 있을 때 몸과 표면 사이에 남기는 틈(cm). 0이면 매 sweep이 시작 겹침이 된다. */
		float SurfaceGap = 1.f;

		/** 접평면 sweep을 법선 쪽으로 띄우는 높이이자 넘을 수 있는 턱(cm). 이보다 높은 면은 오목 모서리로 갈아탄다. */
		float StepUp = 15.f;

		/** 붙은 채 내려갈 수 있는 깊이(cm). 이보다 깊으면 볼록 모서리 감싸기를 시도하고, 그래도 없으면 표면을 잃는다. */
		float StepDown = 20.f;
	};

	enum class EStepResult : uint8
	{
		/** 표면에 붙어 있다(이동했거나 제자리). */
		Attached,
		/** 앞이 Unknown이거나 지지 hit가 Unknown이라 움직이지 않았다. 표면은 그대로다. */
		Blocked,
		/** 이동 끝에 붙을 표면이 없다. 호출자가 공중으로 넘긴다. */
		Lost,
	};

	/**
	 * 목표까지의 도착 판정 거리²(cm²). 지금 표면의 접평면 거리에, 법선 방향 거리 중 몸 반지름을 넘는 부분을 더한다.
	 * 접평면 거리만 쓰면 벽에 붙은 개체가 바닥 목표와 같은 높이에 오는 순간 도착으로 오판한다.
	 * 반지름까지는 무시해 경사·굴곡 위 목표의 캡슐 중심 높이 차이로 도착이 막히지 않게 한다.
	 */
	LOOTNPOP_API float GoalDistanceSq(const FVector& Location, const FVector& Normal, const FVector& Goal, float BodyRadius);

	/**
	 * 목표로 기어갈 방향(Normal 접평면의 단위 벡터, 목표가 겹쳐 있으면 0). 길찾기 없이 목표 방향을 접평면에 투영한다.
	 * 목표가 표면 앞쪽(법선 방향)으로 반지름보다 멀면, 그 거리만큼 내리막(구 중심 중력 아래의 접평면 성분)을 더한다 —
	 * 벽에 붙은 개체가 앞 바닥의 목표와 같은 높이에 오면 투영이 0이 되어 벽 구석에 멈추는 것을 막고, 벽을 타고 내려가게 한다.
	 */
	LOOTNPOP_API FVector SteerDirection(const FVector& Location, const FVector& Normal, const FVector& Goal,
		const FVector& GravityUp, float BodyRadius);

	/** 막힘 우회 상태. 개체마다 하나(FLNPEnemyCrawlFragment)이며 UpdateDetour·ResetDetour만 바꾼다. */
	struct FDetourState
	{
		/** 막힘 판정 기준점. 여기서 StallRadius 밖으로 벗어나면 진전으로 보고 다시 잡는다. 0이면 아직 없다. */
		FVector StallAnchor = FVector::ZeroVector;
		/** 기준점 근처에 머문 시간(초). */
		float StallTime = 0.f;
		/** 남은 우회 시간(초). 0보다 크면 우회 중이다. */
		float DetourTime = 0.f;
		/** 우회 기준 면의 법선(시작 위치를 떠날 때 붙어 있던 면)과 시작 위치. 시작 위치에서 StallRadius 밖으로 나간 뒤 다른 면으로 넘어가면 우회를 끝낸다. */
		FVector DetourNormal = FVector::ZeroVector;
		FVector DetourOrigin = FVector::ZeroVector;
		/** 옆 방향 부호(+1/-1). 0이면 아직 정하지 않았다. 우회 중 다시 막히면 뒤집는다. */
		int8 Side = 0;
	};

	struct FDetourParams
	{
		/** 이 반경(cm) 안에 StallTimeout 동안 머물면 막힘이다. 몸 반지름을 쓴다. */
		float StallRadius = 40.f;
		float StallTimeout = 1.f;
		/** 한 번 우회하는 최대 시간(초). */
		float Duration = 2.f;
		/**
		 * 기준 면 법선과의 내적이 이보다 작으면 다른 면에 올라선 것으로 보고 일반 조향으로 돌아간다.
		 * 모서리를 감싸 도는 도중(법선이 반쯤 돈 상태)에 끝나면 일반 조향이 다시 원래 면으로 끌어내린다 — 거의 직각까지 기다린다.
		 */
		float NewSurfaceDot = 0.1f;
	};

	/**
	 * 길찾기 없는 단순 우회(Phase 14 단위 2e). 목표로 기어가는 중에만 매 프레임 부르고, 그 밖에는 ResetDetour를 부른다.
	 * - 막힘: 위치가 StallRadius 안에 StallTimeout 동안 머물면(벽 ↔ 천장 떨림, 투영 0 정지, Unknown 막힘) 우회를 시작한다.
	 *   목표 거리가 아니라 자기 이동량으로 재므로 달아나는 플레이어를 쫓는 평지 추격에는 끼어들지 않는다.
	 * - 방향: 벽·바닥에서는 목표 방향에 수직인 옆(Side), 천장(법선이 중력 아래쪽)에서는 목표 반대쪽에 옆을 섞은 방향이다.
	 *   천장 여부는 기준 면으로 정한다 — 가장자리를 감싸 도는 동안 같은 방식을 유지해야 옆면까지 넘어간다.
	 *   천장 위에 있는 목표는 판 가장자리를 돌아야 닿는다 — 가장자리에서 옆면으로 감싸 돌면 일반 조향이 윗면으로 올린다.
	 * - 끝: 시간이 다 되거나, 시작 위치를 벗어난 뒤 다른 면으로 넘어가면 일반 조향으로 돌아간다. 시작 위치 근처의 면 전환은
	 *   무시한다 — 막힘이 벽 ↔ 천장 떨림이면 시작 직후 법선이 매 프레임 바뀌어 우회가 한 프레임 만에 끝난다(자동화 실측).
	 *   우회 중 다시 막히면 Side를 뒤집고 다시 시작한다.
	 * query는 쓰지 않는다. 반환값은 SteerDirection 대신 쓸 단위 방향이다(목표와 겹쳐 있으면 0).
	 */
	LOOTNPOP_API FVector UpdateDetour(FDetourState& State, const FDetourParams& Params, const FVector& Location, const FVector& Normal,
		const FVector& SteerDir, const FVector& ToGoal, const FVector& GravityUp, const FVector& FallbackSide, int8 InitialSide, float DeltaTime);

	/** 목표로 기어가지 않는 프레임(정지·공격·배회·공중)에 막힘 계측과 우회를 비운다. Side는 유지한다. */
	LOOTNPOP_API void ResetDetour(FDetourState& State);

	/** 붙을 수 있는 표면인가. */
	LOOTNPOP_API bool IsCrawlable(const FLNPExactHitIdentity& Identity);

	/**
	 * 한 프레임 이동. Velocity는 월드 속도이며 Normal의 접평면 성분만 쓴다.
	 * - 접평면 sweep이 붙을 수 있는 면에 막히면 그 면의 접촉 법선으로 갈아탄다(오목 모서리). 남은 이동은 버린다.
	 * - 이동 뒤 -Normal probe가 붙을 수 있는 면에 닿으면 그 접촉 법선으로 붙는다(평면·완만한 굴곡·볼록 모서리).
	 * - probe가 비면 이동 반대쪽 아래로 감싸기 sweep을 한 번 더 한다(한 프레임 이동이 커서 모서리를 넘어선 경우).
	 * - 속도가 0이면 query 없이 제자리다.
	 * query: 정지 0회, 이동 2회(접평면 sweep + probe). 오목 모서리 +1(내려앉기), 감싸기 +1, 띄운 시작점이 천장에 겹치면 +1.
	 * OutNormal은 Attached일 때만 바뀐다. 그 밖에는 입력 Normal 그대로다.
	 */
	LOOTNPOP_API EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, const FVector& Velocity, float DeltaTime,
		FVector& OutLocation, FVector& OutNormal);

	/**
	 * 제자리에서 표면을 찾아 붙는다(스폰·착지 직후처럼 법선을 모를 때). SearchDirection 쪽으로 StepDown + Reach만큼 probe한다.
	 * 찾으면 위치·법선을 붙은 자리로 바꾸고 true. query 1회.
	 */
	LOOTNPOP_API bool Attach(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& SearchDirection, float Reach, FVector& OutLocation, FVector& OutNormal);
}

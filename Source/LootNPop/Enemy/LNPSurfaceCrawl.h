// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;
struct FLNPDynamicSupportFrame;
struct FLNPEnemyDynamicSupportContact;

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
		/**
		 * 앞이 Unknown이거나 지지 hit가 Unknown이라 움직이지 않았다. 표면은 그대로다.
		 * 낮은 틈에서 이동 결과가 법선 쪽 면에 겹쳐도 전진하지 않고 Blocked다 — 이때만 겹침 해소로 위치가 조금 바뀔 수 있다.
		 */
		Blocked,
		/** 이동 끝에 붙을 표면이 없다. 호출자가 공중으로 넘긴다. */
		Lost,
		/**
		 * 겹침을 풀 수 없고 겹친 면 중에 움직이는 지형(Dynamic 수명주기, 패널)이 있다 — 패널과 다른 면 사이에 끼었다.
		 * 위치는 그대로다. 호출자가 압사 처리한다(체력 0, 사망 경로). Phase 14 단위 6e, 사용자 결정 2026-10-06.
		 */
		Crushed,
	};

	/**
	 * 목표까지의 도착 판정 거리²(cm²). 지금 표면의 접평면 거리에, 법선 방향 거리 중 몸 반지름을 넘는 부분을 더한다.
	 * 접평면 거리만 쓰면 벽에 붙은 개체가 바닥 목표와 같은 높이에 오는 순간 도착으로 오판한다.
	 * 반지름까지는 무시해 경사·굴곡 위 목표의 캡슐 중심 높이 차이로 도착이 막히지 않게 한다.
	 */
	LOOTNPOP_API float GoalDistanceSq(const FVector& Location, const FVector& Normal, const FVector& Goal, float BodyRadius);

	/**
	 * 목표로 기어갈 방향(Normal 접평면의 단위 벡터, 목표가 겹쳐 있으면 0). 길찾기 없이 목표 방향을 접평면에 투영한다.
	 * 목표가 표면 앞쪽(법선 방향)으로 ReachDistance보다 멀면, 그 거리만큼 내리막(구 중심 중력 아래의 접평면 성분)을 더한다 —
	 * 벽에 붙은 개체가 앞 바닥의 목표와 같은 높이에 오면 투영이 0이 되어 벽 구석에 멈추는 것을 막고, 벽을 타고 내려가게 한다.
	 * ReachDistance는 "이 면 위에서 목표에 그만큼 다가가면 충분하다"는 거리다. 목표에 닿아야 하는 배회는 몸 반지름,
	 * 원거리 교전 추격은 사거리를 준다 — 이 면에서 사거리 안에 들 수 있는데 내리막을 더하면 목표의 수선 발보다
	 * 법선 거리만큼 아래의 한 점(접평면 오르막과 내리막이 상쇄되는 자리, 목표까지 √2배)에 모여 사거리 밖에서 맴돈다
	 * (PIE 로그 2026-10-10: 법선 거리 752, 높이 차 약 750, 거리 약 1060).
	 * bSeekLineOfSight(사거리 안에서 사선이 막힘)면 내리막 대신 오르막을 더한다. 내리막은 목표에 **닿으려는** 규칙이라
	 * 건너편 절벽 위의 타겟을 보려는 개체를 틈 아래로 끌어내린다. 면 앞쪽의 타겟은 그 면의 위쪽 가장자리에서 트인다.
	 */
	LOOTNPOP_API FVector SteerDirection(const FVector& Location, const FVector& Normal, const FVector& Goal,
		const FVector& GravityUp, float ReachDistance, bool bSeekLineOfSight = false);

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

	/** 교전 추격 진행 계측(Phase 14b 단위 1). 개체마다 하나이며 UpdateChaseProgress·ResetChaseProgress만 바꾼다. */
	struct FChaseProgressState
	{
		/** 계측을 시작한 뒤 타겟까지 가장 가까웠던 직선거리(cm). 0이면 아직 재지 않았다. */
		float BestDistance = 0.f;
		/** BestDistance가 마지막으로 줄어든 뒤 지난 시간(초). */
		float StallTime = 0.f;
		/** 계측을 시작한 때의 타겟 위치. 타겟이 여기서 RetargetDistance 넘게 움직이면 다시 잰다. */
		FVector TargetAnchor = FVector::ZeroVector;
	};

	struct FChaseProgressParams
	{
		/** 이만큼(cm) 이상 가까워져야 진전으로 본다. 몸 반지름 × 2를 쓴다 — 평형점 주변의 분리력·우회 흔들림을 진전으로 세지 않는다. */
		float ProgressMargin = 150.f;
		/** 진전 없이 이 시간(초)이 지나면 갇힘이다. */
		float StallWindow = 3.f;
		/** 타겟이 계측 시작 자리에서 이만큼(cm) 넘게 움직이면 기록을 버리고 다시 잰다 — 달아나는 타겟 추격은 갇힘이 아니다. */
		float RetargetDistance = 1000.f;
	};

	/**
	 * 교전 추격(사거리 밖, 사선 탐색·자리 지키기 아님) 중 표면에 붙어 있는 프레임마다 부른다. 위치가 아니라 타겟까지 직선거리의
	 * 최솟값이 줄고 있는지로 잰다 — 평형점 맴돌기는 계속 조금씩 움직여 위치 기준 막힘(UpdateDetour)이 잘 서지 않는다(PIE 2026-10-10).
	 * StallWindow 동안 BestDistance − ProgressMargin보다 가까워지지 않으면 true를 반환하고 계측을 새로 시작한다.
	 * 갇힌 순간의 거리는 호출자가 이 프레임의 DistanceToTarget으로 남긴다. query는 쓰지 않는다.
	 */
	LOOTNPOP_API bool UpdateChaseProgress(FChaseProgressState& State, const FChaseProgressParams& Params, float DistanceToTarget,
		const FVector& TargetLocation, float DeltaTime);

	/** 계측하지 않는 프레임(사거리 안·비교전)에 비운다. */
	LOOTNPOP_API void ResetChaseProgress(FChaseProgressState& State);

	/** 고정 방향 우회의 갈래(Phase 14b 단위 2). 벽은 넷, 바닥·천장은 좌우 둘이다. */
	enum class EChaseBranch : uint8
	{
		None,
		/** 등고선(벽) 또는 타겟 방향에 수직인 옆(바닥·천장). 부호는 FChaseDetourState::Side. */
		Lateral,
		/** 경사선 아래. 다른 면(대개 바닥)에 닿으면 끝난다. */
		Down,
		/** 경사선 위. 다른 면(대개 윗면)에 올라서면 끝난다. */
		Up,
	};

	enum class EChaseDetourMode : uint8
	{
		/** 일반 추격. 반환 방향은 SteerDirection과 같고, 호출자는 이때만 국소 장애물 우회(UpdateDetour)를 덧붙인다. */
		Chase,
		/** 갈래를 따라가는 중. */
		Branch,
		/** 포기 후 대기. 반환 방향은 내리막 없는 조향(가장자리로 올라가 선다)이며 도착하면 0이다. */
		Wait,
	};

	/** 진행 기반 고정 방향 우회 상태. 개체마다 하나이며 UpdateChaseDetour·InterruptChaseDetour·ResetChaseDetour만 바꾼다. */
	struct FChaseDetourState
	{
		/** 일반 추격 구간의 진행 계측. 갈래를 따라가는 동안은 재지 않는다. */
		FChaseProgressState Progress;

		/** 갇힘으로 시작한 시도가 진행 중인가. 성공(StuckDistance − ProgressMargin보다 가까워짐)·포기·타겟 큰 이동으로 끝난다. */
		bool bAttempt = false;
		/** 시도 시작 뒤 지난 시간(초). 갈래와 그 사이 일반 추격을 모두 센다. */
		float AttemptTime = 0.f;
		/** 처음 갇힌 순간 타겟까지 직선거리(cm). 등고선 갈래의 이탈 조건(Bug2의 leave)과 시도 성공 판정이 쓴다. */
		float StuckDistance = 0.f;
		FVector AttemptTargetAnchor = FVector::ZeroVector;
		/** 이번 시도에서 끝난 갈래(비트: 옆+ 1, 옆− 2, 아래 4, 위 8). 다시 시도하지 않는다. */
		uint8 TriedMask = 0;
		/** 시도의 첫 옆 부호. 갈래 순서를 시도 시작 때 고정한다 — 첫 옆이 실패한 자리에서 다시 재면 반대쪽이 치우친 쪽이 되어 아래가 밀린다. */
		int8 AttemptSide = 0;

		EChaseBranch Branch = EChaseBranch::None;
		/** 옆 갈래 부호(+1/−1). 벽에서는 Cross(Normal, GravityUp) 기준이며, 면이 바뀌어도 회전 방향이 유지된다. */
		int8 Side = 0;
		/** 마지막으로 쓴 갈래 방향(월드). 바닥·천장의 옆 갈래는 이 방향을 지금 면에 투영해 직진한다. */
		FVector BranchDir = FVector::ZeroVector;
		/** 갈래를 시작한 면의 법선과 위치. 위·아래 갈래는 시작 위치를 벗어난 뒤 다른 면으로 넘어가면 끝난다. */
		FVector BranchNormal = FVector::ZeroVector;
		FVector BranchOrigin = FVector::ZeroVector;
		FVector LastLocation = FVector::ZeroVector;
		/** 이번 갈래에서 이동한 거리(cm). */
		float BranchTravel = 0.f;

		/** 포기 후 대기 중인가. 타겟이 크게 움직이거나 GiveUpCooldown이 지나면 실패 기억을 비우고 다시 추격한다. */
		bool bWaiting = false;
		float WaitTime = 0.f;
		FVector WaitTargetAnchor = FVector::ZeroVector;
		/**
		 * 포기한 시도의 첫 옆 반대쪽(0이면 없음). 타겟이 그대로인 채 대기가 끝나 다시 시도하면 첫 옆으로 쓴다 — 실패한 첫 옆이 열린 쪽이면
		 * 닫힌 쪽 옆은 열린 쪽 끝에서 출발해 예산을 되돌아오는 데 쓰고 실패한다. 재시도마다 같은 순서를 되풀이하지 않게 한다(PIE 2026-10-12).
		 */
		int8 RetrySide = 0;
	};

	struct FChaseDetourParams
	{
		FChaseProgressParams Progress;
		/** 한 갈래의 이동 거리 예산(cm). */
		float BranchBudget = 4000.f;
		/**
		 * 갈래 중 타겟까지 직선거리가 이보다 멀어지면 그 갈래를 실패로 기억하고 다음 갈래로 넘어간다(cm, 0이면 제한 없음).
		 * 타겟 추적 유지 거리보다 작게 준다 — 깊은 틈의 아래 갈래가 타겟을 놓치면 비교전 프레임이 시도 기억을 지워
		 * 같은 갈래 순서를 끝없이 되풀이한다(PIE 2026-10-12: 거리 약 6,000에서 놓침, 홀수 개체 3기가 옆 → 아래를 반복).
		 */
		float MaxTargetDistance = 0.f;
		/** 시도 전체의 시간 예산(초). 넘으면 포기한다. 갈래별 시간 예산을 두면 네 갈래 포기까지 30초가 넘는다. */
		float TotalBudget = 20.f;
		/** 대기 뒤 다시 추격하기까지의 시간(초). */
		float GiveUpCooldown = 10.f;
		/** 위·아래 갈래가 시작 면 법선과의 내적이 이보다 작은 면에 오르면 끝난다(UpdateDetour와 같은 규칙). */
		float NewSurfaceDot = 0.1f;
		/** 몸 반지름(cm). 갈래 시작 위치 이탈 판정과 대기 도착 판정에 쓴다. */
		float BodyRadius = 40.f;
		/** 일반 추격 방향과 내적이 이보다 큰 갈래는 일반 추격이 이미 시도한 것이므로 뺀다. */
		float ExcludeDot = 0.7f;
		/** 타겟 방향과 옆 축의 내적이 이보다 작으면 치우침이 없는 것으로 보고 첫 옆 갈래를 엔티티 홀짝으로 정한다. */
		float BiasDot = 0.2f;
	};

	/**
	 * 진행 기반 고정 방향 우회(Phase 14b 단위 2, Bug 알고리즘의 표면판). 교전 추격(사거리 밖, 사선 탐색·자리 지키기 아님) 중
	 * 표면에 붙어 있는 프레임마다 부른다. 반환값은 이번 프레임 이동 방향(단위 벡터 또는 0)이다.
	 * - 일반 추격(SteerDirection, ReachDistance 그대로)을 하며 UpdateChaseProgress로 잰다. 갇히면 시도를 시작하고 갈래를 고른다.
	 * - 갈래 순서: 타겟이 치우친 쪽 옆 → 아래 → 반대쪽 옆 → 위. 바닥·천장은 옆 둘만이다. 갇힌 자리에서 일반 추격 방향이 뚜렷하면 그와 같은 갈래는 시도 내내 뺀다
	 *   (평형점에서는 오르막·내리막이 상쇄되어 방향이 뚜렷하지 않으므로 빼지 않는다).
	 * - 갈래는 섞지 않고 한 방향으로 민다. 옆은 StuckDistance − ProgressMargin보다 가까워질 때까지, 위·아래는 다른 면으로 넘어갈 때까지다.
	 *   한 갈래가 BranchBudget을 다 쓰면 실패로 기억하고 지금 자리에서 다음 갈래를 시작한다. 위·아래가 끝나면 일반 추격으로 돌아가 다시 잰다.
	 * - 시도가 TotalBudget을 넘거나 후보가 없으면 포기하고 대기한다(Wait).
	 * - 국소 장애물(돌출부 밑·판 밑면) 떨림은 위치 기준 우회(UpdateDetour)가 맡는다. 호출자는 OutMode가 Chase이고 타겟이 면 앞쪽으로
	 *   ReachDistance 안일 때(내리막이 켜지지 않음)만 그것을 덧붙인다. 내리막이 켜진 구간의 막힘은 평형점이며, 2초 옆걸음은 평형선 위를
	 *   오가게 할 뿐 아니라 갇힌 순간의 자리를 흔들어 갈래 순서를 우연에 맡긴다.
	 * query는 쓰지 않는다. FallbackSide는 치우침이 없을 때 첫 옆 부호(엔티티 인덱스 홀짝 — 무리 절반씩 나뉜다).
	 */
	LOOTNPOP_API FVector UpdateChaseDetour(FChaseDetourState& State, const FChaseDetourParams& Params, const FVector& Location,
		const FVector& Normal, const FVector& Goal, const FVector& GravityUp, float ReachDistance, int8 FallbackSide, float DeltaTime,
		EChaseDetourMode& OutMode);

	/** 넉백·표면 상실(공중) 프레임에 부른다. 지금 갈래를 끝내고(실패로 기억하지 않는다) 진행 계측을 비운다. 시도와 대기는 유지한다. */
	LOOTNPOP_API void InterruptChaseDetour(FChaseDetourState& State);

	/** 사거리 안·비교전 등 교전 추격이 아닌 프레임에 모두 비운다. */
	LOOTNPOP_API void ResetChaseDetour(FChaseDetourState& State);

	/** Home 접근 계측(Phase 14 단위 4d). 개체마다 하나(FLNPEnemyCrawlFragment)이며 UpdateHomeApproach·ResetHomeApproach만 바꾼다. */
	struct FHomeApproachState
	{
		/** 계측을 시작한 뒤 Home까지 가장 가까웠던 직선거리(cm). 0이면 아직 재지 않았다. */
		float BestDistance = 0.f;
		/** BestDistance가 마지막으로 줄어든 뒤 지난 시간(초). */
		float StallTime = 0.f;
	};

	struct FHomeApproachParams
	{
		/** Home까지 직선거리가 이보다 멀 때만 잰다. 배회 목표가 나올 수 있는 거리(배회 최대 거리 + 몸 반지름)를 쓴다. */
		float FarDistance = 2500.f;
		/** 이만큼(cm) 이상 가까워져야 진전으로 본다. 몸 반지름을 쓴다 — 제자리 떨림을 진전으로 세지 않는다. */
		float ProgressMargin = 80.f;
		/** 진전 없이 이 시간(초)이 지나면 재귀속한다. */
		float Timeout = 10.f;
	};

	/**
	 * 비교전 중 표면에 붙어 있는 프레임마다 부른다. Home 쪽으로 기어가다 Timeout 동안 가까워지지 않으면 true를 반환하고
	 * 상태를 비운다 — 호출자가 직선거리로 가장 가까운 활성 Pod로 재귀속한다(D-066). Nav 도달성은 쓰지 않는다.
	 * FarDistance 안에 들어오면 계측을 비우고 false다. query는 쓰지 않는다.
	 */
	LOOTNPOP_API bool UpdateHomeApproach(FHomeApproachState& State, const FHomeApproachParams& Params, float DistanceToHome, float DeltaTime);

	/** 교전·공중 등 계측하지 않는 프레임에 비운다. 재귀속 뒤에도 비운다. */
	LOOTNPOP_API void ResetHomeApproach(FHomeApproachState& State);

	/**
	 * 직선거리로 가장 가까운 후보의 인덱스. 같은 거리면 앞 인덱스다. 후보가 없으면 INDEX_NONE.
	 * Excluded[i]가 true인 후보는 건너뛴다(이미 다가가지 못한 Pod). 비어 있으면 모두 후보다.
	 */
	LOOTNPOP_API int32 SelectNearestPod(const FVector& From, TConstArrayView<FVector> PodPositions, TConstArrayView<bool> Excluded = {});

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
	 * OutSurfaceIdentity: 이번 이동으로 새로 붙은 면의 hit identity. 붙을 면을 새로 찾지 않은 프레임(정지·겹침 해소·막힘·상실)에는
	 * 바꾸지 않는다 — 호출자가 Unknown으로 초기화해 두면 "새 면 없음"으로 읽는다. 동적 지지면 운반 접촉에 쓴다.
	 * 시작 겹침을 풀지 못하면(Depenetrate 실패) 제자리에 둔다. 마지막으로 민 자리는 다른 면 안일 수 있다.
	 */
	LOOTNPOP_API EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, const FVector& Velocity, float DeltaTime,
		FVector& OutLocation, FVector& OutNormal, FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/**
	 * 제자리에서 표면을 찾아 붙는다(스폰·착지 직후처럼 법선을 모를 때). SearchDirection 쪽으로 StepDown + Reach만큼 probe한다.
	 * 찾으면 위치·법선을 붙은 자리로 바꾸고 true. query 1회.
	 */
	LOOTNPOP_API bool Attach(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& SearchDirection, float Reach, FVector& OutLocation, FVector& OutNormal,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/**
	 * 움직이는 지지면(패널, D-013) 위의 벽보행 개체를 이번 프레임 패널 자세로 옮긴다. 지상 적과 같은 로컬 접촉
	 * (LNPEnemySurfaceMovement::ApplyDynamicSupportDelta)을 쓰고, 패널이 돈 만큼 표면 법선도 같이 돌린다.
	 * 패널이 snapshot에서 사라지거나 비보행이 되면 false — 호출자가 접촉을 비우고 마지막 패널 속도로 공중에 넘긴다.
	 * query 없음.
	 */
	LOOTNPOP_API bool CarryOnDynamicSupport(const FLNPDynamicSupportFrame& DynamicFrame, FLNPEnemyDynamicSupportContact& InOutContact,
		FTransform& InOutTransform, FVector& InOutNormal);

	/**
	 * 몸 구가 지형과 겹쳐 있으면 밀어낸다. 한 면에서 밀어낸 자리가 다른 면(프랍 사이 주머니·구석)에 들어갈 수 있으므로
	 * 밀어낸 뒤 다시 검사하며 최대 8회 반복한다(Phase 14 단위 6b 실측). 직전에 민 방향과 마주 보는 면에 걸리면 두 방향의
	 * 이등분(주머니 출구 쪽)으로 민다. 민 자리의 겹침이 줄지 않으면 법선이 뒤집혀 보고된 것으로 보고 반대로 민다
	 * (Chaos trimesh 뒷면 법선, 단위 6e 실측). 겹치지 않으면 OutLocation = Location.
	 * 끝내 빠져나오면 true. query: 겹치지 않으면 1회, 반복마다 1~2회.
	 * ProbeDirection: 겹침 검사 sweep 방향. Chaos trimesh는 멀어지는 방향의 얕은 시작 겹침을 보고하지 않으므로(단위 6e 실측)
	 * 의심되는 면 쪽을 넘긴다.
	 * SupportNormal: 붙어 있는 면의 법선(0이면 제한 없음). 이 면 안쪽(법선 반대쪽)으로 미는 해소는 실패로 본다 — 위에서 덮치는 패널을
	 * 피해 지지면 쪽으로 밀면 매 틱 조금씩 파고들다 면을 넘는 순간 법선이 뒤집혀 반대편으로 "빠져나온다"(단위 6e 자동화 실측).
	 */
	LOOTNPOP_API bool Depenetrate(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, FVector& OutLocation, const FVector& ProbeDirection = FVector::UpVector,
		bool* bOutTouchedDynamic = nullptr, const FVector& SupportNormal = FVector::ZeroVector);

	/**
	 * 정지한 개체의 제자리 겹침 해소. Step은 이동량이 없으면 query를 하지 않으므로, 움직이는 지형이 정지 개체를 덮칠 수 있는
	 * 곳(호출자가 패널 근처로 거른다)에서만 부른다. 겹치지 않으면 Attached(제자리), 풀면 Attached(새 위치), 풀 수 없으면
	 * Dynamic 겹침이 있을 때 Crushed, 아니면 Blocked(제자리). query: 겹치지 않으면 1회.
	 */
	LOOTNPOP_API EStepResult ResolveStationaryOverlap(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, FVector& OutLocation);

	/**
	 * 구간 배회(D-068, Phase 14 단위 4e) 한 구간을 생성할 때 한 번 검증한다. 구간은 Start에서 Goal까지 법선이 고정된 평면 위의 직선이고,
	 * 서버·클라이언트 모두 보간 중 query 없이 이 직선을 그대로 쓴다. 그래서 직선 전체가 같은 표면(법선 동일) 위에 떠 있어야 한다.
	 * - 같은 높이 구 sweep: 턱·벽·천장에 닿으면 거부한다.
	 * - 몸 반지름 간격의 하향 probe(마지막은 Goal): 붙을 표면(IsCrawlable)이 있고, 그 자리의 법선이 Normal과
	 *   MinNormalDot 이상이며, 붙을 자리와 직선 위 점의 차이가 MaxGapError 이하여야 한다. 모서리·절벽·홈을 건너는 구간을 거른다.
	 * query: 1 + ceil(길이 / 몸 반지름)회. 실패하면 첫 실패에서 멈춘다.
	 */
	LOOTNPOP_API bool ValidateWanderSegment(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Start, const FVector& Normal, const FVector& Goal, float MaxGapError = 2.f, float MinNormalDot = 0.99f);
}

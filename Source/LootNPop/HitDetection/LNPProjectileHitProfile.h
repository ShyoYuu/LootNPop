// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

/** 프로세서 인스턴스별 진단 누계. 상세 계측을 끄면 시계·카운터 갱신·로그를 수행하지 않는다. */
struct FLNPProjectileHitProfile
{
	enum EStage { CollectEnemies, CollectPlayers, World, Targets, Filter, Rewind, History, Capsule, GridBuild, GridQuery, StageCount };
	uint64 Cycles[StageCount] = {};
	uint64 Calls[StageCount] = {};
	uint64 Frames = 0;
	uint64 Projectiles = 0;
	uint64 Samples = 0;
	uint64 Candidates = 0;
	uint64 SelfExcluded = 0;
	uint64 TeamExcluded = 0;
	uint64 EnemyTeamPairs = 0;
	uint64 RemoteSamples = 0;
	uint64 RewindApplied = 0;
	uint64 HistoryCacheRequests = 0;
	uint64 HistoryCacheHits = 0;
	uint64 HistoryCacheValidationQueries = 0;
	uint64 HistoryCacheValidationErrors = 0;
	uint64 SphereTests = 0;
	uint64 SphereRejected = 0;
	uint64 GridQueries = 0;
	uint64 GridFallbacks = 0;
	uint64 GridCandidates = 0;
	uint64 GridFullCandidates = 0;
	uint64 GridValidationQueries = 0;
	uint64 GridValidationHits = 0;
	uint64 GridValidationErrors = 0;
	uint64 ClockCycles = 0;
	uint64 Ordinal = 0;
	double WindowStart = 0.0;
	int32 Stride = 0;
	bool bSample = false;
};

/** 타겟별 trace 이벤트 대신 선택된 투사체의 연산 시간을 누적한다. 하위 범위는 부모에 포함된다. */
struct FLNPProjectileHitProfileScope
{
	FLNPProjectileHitProfileScope(FLNPProjectileHitProfile& InProfile,
		FLNPProjectileHitProfile::EStage InStage, bool bEnabled)
		: Profile(bEnabled ? &InProfile : nullptr), Stage(InStage), Start(bEnabled ? FPlatformTime::Cycles64() : 0)
	{
	}
	~FLNPProjectileHitProfileScope()
	{
		if (Profile)
		{
			Profile->Cycles[Stage] += FPlatformTime::Cycles64() - Start;
			++Profile->Calls[Stage];
		}
	}
	FLNPProjectileHitProfile* Profile;
	FLNPProjectileHitProfile::EStage Stage;
	uint64 Start;
};

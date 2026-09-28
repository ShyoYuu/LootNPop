// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEntityAttackShared.h"
#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPEnemyFlightMovementProcessor.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LootNPop.h"

#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "MassCommands.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/MiscTrace.h"

namespace
{
	/** 링 안쪽 반지름(cm). 링 중심에 선 플레이어 바로 옆에 적을 두지 않는다. */
	constexpr float RingInnerRadius = 1500.f;

	/**
	 * Meadow_00 큰 섬(Phase03b §3.1, Phase03b_Log 구현 단위 1). slot 로컬 방향 (1,1,1)이고 윗면은 월드 구와 동심이다.
	 * slot 4(pitch 180)는 -Z 반구라 PlayerStart 극에 가장 가까운 네 slot 중 하나다. 옥탄트를 고치면 이 값도 고친다.
	 */
	constexpr int32 IslandSlot = 4;
	constexpr float IslandTopRadius = 25800.f;
	constexpr float IslandRadius = 3000.f;

	/** 섬 윗면 배치는 가장자리에서 이만큼 안쪽까지만. */
	constexpr float IslandPlacementEdgeMargin = 200.f;

	/**
	 * 층 판정 반지름(cm). 섬 윗면(25,800) 위 캡슐 중심은 이보다 작고, 지각(최소 28,235) 위는 크다.
	 * 섬 밑면(가장 깊은 곳 27,300)과 지각 사이에서 시작하는 probe는 섬 아래 지각만 찾는다.
	 */
	constexpr float IslandLayerRadius = 27000.f;
	constexpr float CrustProbeStartRadius = 27700.f;
	constexpr float CrustProbeEndRadius = 32000.f;
	constexpr float IslandProbeStartRadius = 25000.f;

	/** 배치 probe 구 반지름(cm). 캡슐 반지름과 같다. */
	constexpr float PlacementProbeRadius = 35.f;

	/** 접지 상태로 한 프레임에 이보다 크게 반지름이 변하면 층 순간이동이다. 정상 보행 단차(최대 60cm)보다 훨씬 크다. */
	constexpr float LayerJumpThreshold = 500.f;

	/** 20마리 중 섬 윗면에 두는 인덱스. 4는 근접, 15는 원거리다(GetEnemyKind). 10%. */
	bool IsIslandTopIndex(const int32 Index)
	{
		const int32 Slot = Index % 20;
		return Slot == 4 || Slot == 15;
	}

	FVector GetIslandDirection()
	{
		return ULNPOctantSpawnSubsystem::OctantRotations[IslandSlot].RotateVector(FVector(1.0, 1.0, 1.0).GetSafeNormal());
	}

	/** 링 중심 방향. 섬 중심에서 PlayerStart 극(-Z) 쪽으로 섬 반지름만큼 옮긴 섬 가장자리 아래다. */
	FVector GetRingCenterDirection()
	{
		const FVector IslandDir = GetIslandDirection();
		const FVector Axis = FVector::CrossProduct(IslandDir, FVector::DownVector).GetSafeNormal();
		return FQuat(Axis, IslandRadius / IslandTopRadius).RotateVector(IslandDir);
	}

	/** 비행 전용 실행의 플레이어 자리가 섬 가장자리에서 들어온 거리(cm). */
	constexpr float IslandPlayerEdgeInset = 500.f;

	/**
	 * 비행 전용 실행의 플레이어 자리 방향. 링 중심 쪽 섬 가장자리에서 IslandPlayerEdgeInset만큼 안쪽 섬 윗면이다.
	 * Home(링 중심)에서 직선 약 2,500cm라 비행 적의 시야·세력권 안이고, 드론은 섬 밑에서 가장자리를 돌아 올라와야 교전할 수 있다.
	 * 섬 윗면 가운데(Home에서 약 6,000cm)에 두면 시야 밖이라 거의 교전하지 않았다(Phase03c 로그).
	 */
	FVector GetIslandPlayerDirection()
	{
		const FVector IslandDir = GetIslandDirection();
		const FVector Axis = FVector::CrossProduct(IslandDir, FVector::DownVector).GetSafeNormal();
		return FQuat(Axis, (IslandRadius - IslandPlayerEdgeInset) / IslandTopRadius).RotateVector(IslandDir);
	}

	/** Direction 방향으로 StartRadius에서 EndRadius까지 지지면을 찾아 발밑 위치를 돌려준다. */
	bool ProbeFoot(const ULNPMassWorldCollisionSubsystem& Collision, const FVector& Direction, const float StartRadius,
		const float EndRadius, FVector& OutFoot)
	{
		FLNPSupportProbeQuery Query;
		Query.Position = Direction * StartRadius;
		Query.Up = -Direction;
		Query.MaxStepUp = 0.f;
		Query.MaxDrop = EndRadius - StartRadius;
		Query.Radius = PlacementProbeRadius;

		FLNPSupportProbeResult Result;
		if (!Collision.ProbeSupport(Query, FLNPWorldQueryParams(ELNPWorldQueryClass::DebugValidation), Result))
			return false;
		OutFoot = Result.Hit.Location + Direction * PlacementProbeRadius;
		return true;
	}

	/** 적 한 마리당 링 넓이(cm²). 적 수가 늘어도 밀도가 같도록 바깥 반지름을 키운다. 간격 약 390cm. */
	constexpr float RingAreaPerEnemy = 150000.f;

	constexpr float EnemyMinSpacing = 200.f;
	constexpr int32 EnemyPlacementRetries = 30;

	/** 발사 높이·조준 범위. 링 위에서 링 중심 부근을 향해 쏜다. */
	constexpr float MuzzleHeight = 120.f;
	constexpr float AimHeight = 100.f;
	constexpr float AimScatterRadius = 1500.f;
	constexpr float MinPitchDegrees = -2.f;
	constexpr float MaxPitchDegrees = 6.f;

	/** 한 프레임에 주입하는 발사체 상한. 시작 직후 500발이 한 프레임에 몰리지 않게 한다. */
	constexpr int32 MaxInjectPerFrame = 100;

	/** 리슨 서버가 보고 뒤 종료를 미루는 시간. 게스트가 capture를 끝낼 때까지 연결을 유지한다. */
	constexpr double ListenServerQuitDelaySeconds = 20.0;

	const TCHAR* EnemyEntityConfigPaths[] =
	{
		TEXT("/Game/Enemy/DA_EnemyEntityConfig_ActorPromoted_Melee01.DA_EnemyEntityConfig_ActorPromoted_Melee01"),
		TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Melee01.DA_EnemyEntityConfig_PureEntity_Melee01"),
		TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Ranged01.DA_EnemyEntityConfig_PureEntity_Ranged01"),
	};
	static_assert(UE_ARRAY_COUNT(EnemyEntityConfigPaths) == static_cast<int32>(LNPLoadBaseline::EEnemyKind::Count));

	const TCHAR* FlyerEntityConfigPath = TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Flyer01.DA_EnemyEntityConfig_PureEntity_Flyer01");

	int32 GFlightPenetrationCheck = 0;
	FAutoConsoleVariableRef CVarFlightPenetrationCheck(
		TEXT("LNP.SurfaceNav.LoadBaseline.FlightPenetrationCheck"),
		GFlightPenetrationCheck,
		TEXT("Load baseline only. 1 = every capture frame, test each living flying enemy capsule for overlap with exact terrain (DebugValidation class). Adds one query per flyer per frame, so do not use the run for frame timing."),
		ECVF_Default);

	/** 주입 발사체의 무기 값 원본. PureEntity 원거리 적이 쏘는 것과 같은 공유 프래그먼트가 된다. */
	const TCHAR* ProjectileSourceConfigPath = TEXT("/Game/Enemy/DA_Enemy_PureEntity_Ranged01.DA_Enemy_PureEntity_Ranged01");

	float GetRingOuterRadius(const int32 Count)
	{
		return FMath::Sqrt(FMath::Square(RingInnerRadius) + Count * RingAreaPerEnemy / PI);
	}

	void MakeTangentBasis(const FVector& Normal, FVector& OutT1, FVector& OutT2)
	{
		const FVector Arbitrary = FMath::Abs(Normal.X) < 0.9f ? FVector::ForwardVector : FVector::RightVector;
		OutT1 = FVector::CrossProduct(Normal, Arbitrary).GetSafeNormal();
		OutT2 = FVector::CrossProduct(Normal, OutT1).GetSafeNormal();
	}

	/** 링 위 표면 방향. 거리는 표면을 따라 잰 근사값이다. */
	FVector MakeRingDirection(const FVector& CenterDir, const FVector& T1, const FVector& T2, const float Angle,
		const float Distance, const float SphereRadius)
	{
		const FVector Tangent = T1 * FMath::Cos(Angle) + T2 * FMath::Sin(Angle);
		return (CenterDir + Tangent * (Distance / SphereRadius)).GetSafeNormal();
	}

	struct FPercentiles
	{
		double P50 = 0.0;
		double P95 = 0.0;
		double Max = 0.0;
	};

	template<typename T>
	FPercentiles ComputePercentiles(TArray<T> Samples, const double Scale)
	{
		FPercentiles Result;
		if (Samples.IsEmpty())
		{
			return Result;
		}
		Samples.Sort();
		auto At = [&Samples, Scale](const double P)
		{
			const int32 Index = FMath::Clamp(FMath::CeilToInt32(P * Samples.Num()) - 1, 0, Samples.Num() - 1);
			return static_cast<double>(Samples[Index]) * Scale;
		};
		Result.P50 = At(0.50);
		Result.P95 = At(0.95);
		Result.Max = static_cast<double>(Samples.Last()) * Scale;
		return Result;
	}

	template<typename T>
	double Average(const TArray<T>& Samples)
	{
		if (Samples.IsEmpty())
		{
			return 0.0;
		}
		double Sum = 0.0;
		for (const T Value : Samples)
		{
			Sum += static_cast<double>(Value);
		}
		return Sum / Samples.Num();
	}

	int32 ParseExpectedPlayers()
	{
		int32 Players = 2;
		FParse::Value(FCommandLine::Get(), TEXT("LNPLoadBaselinePlayers="), Players);
		return FMath::Max(1, Players);
	}
}

// --- LNPLoadBaseline ---

int32 LNPLoadBaseline::GetEnemyCount()
{
	static const int32 Count = []()
	{
		int32 Value = 0;
		FParse::Value(FCommandLine::Get(), TEXT("LNPLoadBaseline="), Value);
		return FMath::Max(0, Value);
	}();
	return Count;
}

int32 LNPLoadBaseline::GetFlyerCount()
{
	static const int32 Count = []()
	{
		int32 Value = 0;
		FParse::Value(FCommandLine::Get(), TEXT("LNPLoadBaselineFlyers="), Value);
		return FMath::Max(0, Value);
	}();
	return Count;
}

bool LNPLoadBaseline::IsActive()
{
	return GetEnemyCount() + GetFlyerCount() > 0;
}

int32 LNPLoadBaseline::GetSeed()
{
	static const int32 Seed = []()
	{
		int32 Value = 1;
		FParse::Value(FCommandLine::Get(), TEXT("LNPLoadBaselineSeed="), Value);
		return Value;
	}();
	return Seed;
}

int32 LNPLoadBaseline::GetTargetProjectiles()
{
	static const int32 Target = []()
	{
		int32 Value = DefaultTargetProjectiles;
		FParse::Value(FCommandLine::Get(), TEXT("LNPLoadBaselineProjectiles="), Value);
		return FMath::Max(0, Value);
	}();
	return Target;
}

LNPLoadBaseline::EEnemyKind LNPLoadBaseline::GetEnemyKind(const int32 Index)
{
	if (Index % 10 == 0)
	{
		return EEnemyKind::ActorPromotedMelee;
	}
	return (Index % 2 == 0) ? EEnemyKind::PureEntityMelee : EEnemyKind::PureEntityRanged;
}

UMassEntityConfigAsset* LNPLoadBaseline::LoadEnemyEntityConfig(const EEnemyKind Kind)
{
	return LoadObject<UMassEntityConfigAsset>(nullptr, EnemyEntityConfigPaths[static_cast<int32>(Kind)]);
}

UMassEntityConfigAsset* LNPLoadBaseline::LoadFlyerEntityConfig()
{
	return LoadObject<UMassEntityConfigAsset>(nullptr, FlyerEntityConfigPath);
}

namespace
{
	/** Direction 주변 [StartRadius, EndRadius] 층의 발밑 위치. 바위 같은 Blocker 전용 프랍이 있을 수 있어 나선으로 찍어 첫 지지면을 쓴다(결정론적). */
	bool FindFootNear(const ULNPMassWorldCollisionSubsystem& Collision, const FVector& Direction, const float StartRadius,
		const float EndRadius, FVector& OutFoot)
	{
		constexpr int32 SearchSteps = 24;
		constexpr float SearchStepDistance = 150.f;
		FVector T1, T2;
		MakeTangentBasis(Direction, T1, T2);
		for (int32 Step = 0; Step < SearchSteps; ++Step)
		{
			const FVector Candidate = MakeRingDirection(Direction, T1, T2, Step * 2.4f, Step * SearchStepDistance, StartRadius);
			if (ProbeFoot(Collision, Candidate, StartRadius, EndRadius, OutFoot))
				return true;
		}
		return false;
	}

	bool FindCrustNear(const ULNPMassWorldCollisionSubsystem& Collision, const FVector& Direction, FVector& OutFoot)
	{
		return FindFootNear(Collision, Direction, CrustProbeStartRadius, CrustProbeEndRadius, OutFoot);
	}

	/** 비행 적만 있는 실행은 플레이어를 큰 섬 윗면에 둔다(LNPLoadBaseline.h). */
	bool ArePlayersOnIsland()
	{
		return LNPLoadBaseline::GetEnemyCount() == 0 && LNPLoadBaseline::GetFlyerCount() > 0;
	}
}

bool LNPLoadBaseline::FindRingCenter(const ULNPMassWorldCollisionSubsystem& Collision, FVector& OutCenter)
{
	return FindCrustNear(Collision, GetRingCenterDirection(), OutCenter);
}

void LNPLoadBaseline::BuildEnemyRing(const ULNPMassWorldCollisionSubsystem& Collision, const float SphereRadius, const int32 Seed,
	const int32 Count, TArray<FVector>& OutLocations, FVector& OutCenter)
{
	const FVector CenterDir = GetRingCenterDirection();
	if (!FindRingCenter(Collision, OutCenter))
	{
		OutCenter = CenterDir * SphereRadius;
	}

	FVector T1, T2;
	MakeTangentBasis(CenterDir, T1, T2);

	const FVector IslandDir = GetIslandDirection();
	FVector IslandT1, IslandT2;
	MakeTangentBasis(IslandDir, IslandT1, IslandT2);

	const float OuterRadius = GetRingOuterRadius(Count);
	FRandomStream Rand(Seed);
	OutLocations.Reset(Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bIslandTop = IsIslandTopIndex(Index);
		for (int32 Retry = 0; Retry < EnemyPlacementRetries; ++Retry)
		{
			// 넓이에 균등하게 뿌리도록 반지름의 제곱을 균등 추첨한다.
			const float Angle = Rand.FRandRange(0.f, 2.f * PI);
			FVector Candidate;
			if (bIslandTop)
			{
				const float MaxDistance = IslandRadius - IslandPlacementEdgeMargin;
				const float Distance = MaxDistance * FMath::Sqrt(Rand.FRand());
				const FVector Direction = MakeRingDirection(IslandDir, IslandT1, IslandT2, Angle, Distance, IslandTopRadius);
				if (!ProbeFoot(Collision, Direction, IslandProbeStartRadius, IslandLayerRadius, Candidate))
					continue;
			}
			else
			{
				// 섬 아래 방향에서도 섬 밑면과 지각 사이에서 시작하므로 지각을 찾는다.
				const float Distance = FMath::Sqrt(Rand.FRandRange(FMath::Square(RingInnerRadius), FMath::Square(OuterRadius)));
				const FVector Direction = MakeRingDirection(CenterDir, T1, T2, Angle, Distance, SphereRadius);
				if (!ProbeFoot(Collision, Direction, CrustProbeStartRadius, CrustProbeEndRadius, Candidate))
					continue;
			}

			bool bTooClose = false;
			for (const FVector& Other : OutLocations)
			{
				if (FVector::DistSquared(Candidate, Other) < FMath::Square(EnemyMinSpacing))
				{
					bTooClose = true;
					break;
				}
			}
			if (!bTooClose)
			{
				OutLocations.Add(Candidate);
				break;
			}
		}
	}
}

// --- ULNPLoadBaselineSubsystem ---

bool ULNPLoadBaselineSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return LNPLoadBaseline::IsActive() && (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

ETickableTickType ULNPLoadBaselineSubsystem::GetTickableTickType() const
{
	return LNPLoadBaseline::IsActive() ? Super::GetTickableTickType() : ETickableTickType::Never;
}

TStatId ULNPLoadBaselineSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPLoadBaselineSubsystem, STATGROUP_Tickables);
}

void ULNPLoadBaselineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Collection.InitializeDependency<ULNPMassWorldCollisionSubsystem>();

	// 락 대기는 성공 기준의 한 축이라 probe를 켠다. probe 자체가 질의당 락 한 번을 더한다(보고서에 적는다).
	if (IConsoleVariable* LockProbe = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.SurfaceNav.WorldCollision.LockProbe")))
	{
		LockProbe->Set(1, ECVF_SetByCode);
	}

	ProjectileStream.Initialize(LNPLoadBaseline::GetSeed());
	KnockbackStream.Initialize(LNPLoadBaseline::GetSeed() + 1);

	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Active: enemies=%d flyers=%d seed=%d expectedPlayers=%d projectiles=%d world=%s"),
		LNPLoadBaseline::GetEnemyCount(), LNPLoadBaseline::GetFlyerCount(), LNPLoadBaseline::GetSeed(), ParseExpectedPlayers(),
		LNPLoadBaseline::GetTargetProjectiles(), *GetWorld()->GetName());
}

bool ULNPLoadBaselineSubsystem::IsReadyToMeasure() const
{
	const UWorld* World = GetWorld();
	const ULNPOctantSpawnSubsystem* Octants = World->GetSubsystem<ULNPOctantSpawnSubsystem>();
	if (Octants == nullptr || !Octants->bGenerationComplete)
		return false;

	if (World->GetNetMode() == NM_Client)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		return PC != nullptr && PC->GetPawn() != nullptr;
	}

	const ULNPMassSpawnSubsystem* Spawner = World->GetSubsystem<ULNPMassSpawnSubsystem>();
	if (Spawner == nullptr || !Spawner->HasFinishedSpawning())
		return false;

	const AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
		return false;

	int32 PlayersWithPawn = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState && PlayerState->GetPawn())
		{
			++PlayersWithPawn;
		}
	}
	return PlayersWithPawn >= ParseExpectedPlayers();
}

void ULNPLoadBaselineSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	const double Now = World->GetRealTimeSeconds();

	if (Stage == EStage::Waiting)
	{
		if (!IsReadyToMeasure())
			return;

		Stage = EStage::Warmup;
		StageStartTime = Now;

		const bool bRingCenterFound = LNPLoadBaseline::FindRingCenter(*World->GetSubsystem<ULNPMassWorldCollisionSubsystem>(), RingCenter);
		if (!bRingCenterFound)
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[LoadBaseline] Ring center probe found no crust."));
		}
		RingOuterRadius = GetRingOuterRadius(LNPLoadBaseline::GetEnemyCount());
		if (World->GetNetMode() != NM_Client)
		{
			ProjectileSourceConfig = LoadObject<ULNPEnemyConfig>(nullptr, ProjectileSourceConfigPath);
		}
		if (bRingCenterFound && World->GetNetMode() != NM_Client)
		{
			RespawnPlayersAtRing();
		}

		float HostToCenter = -1.f;
		if (const APlayerController* PC = World->GetFirstPlayerController(); PC && PC->GetPawn())
		{
			HostToCenter = FVector::Dist(PC->GetPawn()->GetActorLocation(), RingCenter);
		}
		UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Ready (NetMode=%d). Warm-up %.0fs. RingOuter=%.0fcm LocalPawnToRingCenter=%.0fcm ProjectileSource=%s"),
			static_cast<int32>(World->GetNetMode()), LNPLoadBaseline::WarmupSeconds, RingOuterRadius, HostToCenter,
			*GetNameSafe(ProjectileSourceConfig));
	}

	if (Stage == EStage::Done)
		return;

	if (World->GetNetMode() != NM_Client)
	{
		TopUpProjectiles();
		DriveEnemies(DeltaTime, Stage == EStage::Capture);
		if (Stage == EStage::Capture)
		{
			CheckFlyerPenetration();
		}
	}

	if (Stage == EStage::Warmup && Now - StageStartTime >= LNPLoadBaseline::WarmupSeconds)
	{
		Stage = EStage::Capture;
		StageStartTime = Now;

		// 분류별 count·평균이 capture 구간만 담도록 비운다(배치 probe·warm-up 제외).
		ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
		Collision->ResetStats();
		Collision->GetTotals(LastQueryCount, LastQueryNs, LastLockNs);
		StartUnknownHits = Collision->GetUnknownHitCount();
		StartEnvelopeEscapes = Collision->GetEnvelopeEscapeCount();
		LNPEnemyFlightStats::Reset();
		NextActorSampleTime = Now;
		CaptureStartPopulation = Population;
		if (World->GetNetMode() != NM_Client)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
				{
					UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Player %s to ring center %.0fcm"),
						*GetNameSafe(Pawn), FVector::Dist(Pawn->GetActorLocation(), RingCenter));
				}
			}
		}
		// Insights에서 capture 구간만 잘라 볼 수 있게 region을 남긴다(-trace 인자가 없으면 비용 없음).
		TRACE_BEGIN_REGION(TEXT("LNPLoadBaselineCapture"));
		UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Capture %.0fs started."), LNPLoadBaseline::CaptureSeconds);
		return;
	}

	if (Stage == EStage::Capture)
	{
		SampleFrame();
		if (Now - StageStartTime >= LNPLoadBaseline::CaptureSeconds)
		{
			TRACE_END_REGION(TEXT("LNPLoadBaselineCapture"));
			Report();
			Stage = EStage::Done;

			if (FParse::Param(FCommandLine::Get(), TEXT("LNPLoadBaselineQuit")))
			{
				const double Delay = World->GetNetMode() == NM_ListenServer ? ListenServerQuitDelaySeconds : 0.0;
				UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Quitting in %.0fs."), Delay);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
				{
					FPlatformMisc::RequestExit(false, TEXT("LNPLoadBaselineQuit"));
					return false;
				}), static_cast<float>(Delay));
			}
		}
	}
}

void ULNPLoadBaselineSubsystem::RespawnPlayersAtRing()
{
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (GameMode == nullptr)
		return;

	const ULNPMassWorldCollisionSubsystem& Collision = *World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

	// 비행 적만 있는 실행은 링 중심 쪽 큰 섬 가장자리 안쪽 윗면, 그 밖에는 링 중심 지각이 기준 자리다.
	const bool bOnIsland = ArePlayersOnIsland();
	const float SeatStartRadius = bOnIsland ? IslandProbeStartRadius : CrustProbeStartRadius;
	const float SeatEndRadius = bOnIsland ? IslandLayerRadius : CrustProbeEndRadius;
	FVector BaseFoot = RingCenter;
	if (bOnIsland && !FindFootNear(Collision, GetIslandPlayerDirection(), SeatStartRadius, SeatEndRadius, BaseFoot))
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[LoadBaseline] Island top probe found no support. Players stay at ring center."));
		BaseFoot = RingCenter;
	}

	const FVector CenterDir = BaseFoot.GetSafeNormal();
	FVector T1, T2;
	MakeTangentBasis(CenterDir, T1, T2);

	// Mover instant effect(FTeleportEffect)는 네트워크로 전달되지 않아(엔진 주석: Chaos Mover만 복제) 게스트 폰이
	// 서버·게스트 어느 쪽에서 넣어도 옮겨지지 않았다(2026-09-26 스모크). 리스폰(ALNPGameMode::DoRespawn)처럼
	// 폰을 치우고 링 중심에 새로 스폰한다. 스폰 위치는 초기 복제로 전달된다.
	// 자리마다 지각을 exact로 찍고 캡슐 중심을 띄워 둔다(Mover가 떨어뜨려 접지시킨다). 링 중심 기준 300cm 간격.
	// 찍지 않은 자리는 경사·프랍에 걸려 스폰이 충돌로 실패했다.
	constexpr float DropHeight = 150.f;
	constexpr float PlayerSpacing = 300.f;
	TArray<APlayerController*> Controllers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
			Controllers.Add(PC);
	}
	for (int32 PlayerIndex = 0; PlayerIndex < Controllers.Num(); ++PlayerIndex)
	{
		APlayerController* PC = Controllers[PlayerIndex];
		if (APawn* OldPawn = PC->GetPawn())
		{
			PC->UnPossess();
			OldPawn->Destroy();
		}
		FVector Foot = BaseFoot;
		if (PlayerIndex > 0)
		{
			const FVector SeatDir = MakeRingDirection(CenterDir, T1, T2, 0.f, PlayerSpacing * PlayerIndex, BaseFoot.Size());
			FindFootNear(Collision, SeatDir, SeatStartRadius, SeatEndRadius, Foot);
		}
		const FVector Up = -Foot.GetSafeNormal();
		GameMode->RestartPlayerAtTransform(PC, FTransform(UKismetMathLibrary::MakeRotFromZ(Up), Foot + Up * DropHeight));
		if (PC->GetPawn() == nullptr)
		{
			UE_LOG(LogLootNPop, Error, TEXT("[LoadBaseline] Failed to respawn %s at %s"), *GetNameSafe(PC), *Foot.ToString());
		}
	}
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Respawned %d players at %s %s"), Controllers.Num(),
		bOnIsland ? TEXT("island top") : TEXT("ring center"), *BaseFoot.ToString());
}

void ULNPLoadBaselineSubsystem::DriveEnemies(const float DeltaTime, const bool bCount)
{
	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	if (!EnemyQuery.IsInitialized())
	{
		EnemyQuery = FMassEntityQuery(EntityManager.AsShared());
		EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		EnemyQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadWrite);
		EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
		EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
		EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
		// 비행 적의 Velocity는 공중 상태가 아니라 넉백 잔여다. 분포·이동 이벤트·합성 넉백은 지상 개념이다.
		EnemyQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::None);
	}

	// 적마다 독립적인 포아송 과정: 이번 프레임에 넉백을 받을 확률.
	const float KnockbackChance = 1.f - FMath::Exp(-DeltaTime / LNPLoadBaseline::KnockbackMeanIntervalSeconds);
	const FVector IslandDir = GetIslandDirection();
	const FVector IslandCenter = IslandDir * IslandTopRadius;
	const float CosUnderIsland = FMath::Cos(IslandRadius / IslandTopRadius);

	Population = FPopulation();

	// 게임 스레드 tickable이라 Mass 페이즈 밖이다. 프래그먼트를 직접 써도 프로세서와 겹치지 않는다.
	FMassExecutionContext ExecContext(EntityManager, DeltaTime);
	EnemyQuery.ForEachEntityChunk(ExecContext, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const TArrayView<FLNPEnemyVelocityFragment> Velocities = Ctx.GetMutableFragmentView<FLNPEnemyVelocityFragment>();
		const TConstArrayView<FMassActorFragment> Actors = Ctx.GetFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(i);
			// Actor가 있는 적은 Mover가 움직인다. exact 이동 경로의 대상이 아니다.
			if (Actors[i].Get() != nullptr)
			{
				EnemyTracks.Remove(Entity);
				continue;
			}

			const FVector Location = Transforms[i].GetTransform().GetLocation();
			const float Radius = Location.Size();
			const FVector Up = -Location.GetSafeNormal();
			FVector& Velocity = Velocities[i].Velocity;
			const bool bAirborne = !Velocity.IsNearlyZero();
			const bool bIslandTop = !bAirborne && Radius < IslandLayerRadius;

			if (bAirborne)
				++Population.Airborne;
			else if (bIslandTop)
				++Population.IslandTop;
			else if (FVector::DotProduct(-Up, IslandDir) > CosUnderIsland)
				++Population.UnderIsland;
			else
				++Population.OpenCrust;

			FEnemyTrack* Track = EnemyTracks.Find(Entity);
			if (Track == nullptr)
			{
				Track = &EnemyTracks.Add(Entity);
			}
			else
			{
				if (!Track->bAirborne && bAirborne)
				{
					if (bCount && !Track->bKnockedByHarness)
						++Events.Falls;
					if (Track->Radius < IslandLayerRadius)
					{
						Track->bLeftIslandTop = true;
						if (bCount)
							++Events.IslandLeaves;
					}
				}
				else if (Track->bAirborne && !bAirborne)
				{
					if (bCount)
					{
						++Events.Landings;
						if (Track->bLeftIslandTop && !bIslandTop)
							++Events.IslandDrops;
					}
					Track->bLeftIslandTop = false;
				}
				else if (!Track->bAirborne && !bAirborne && bCount
					&& FMath::Abs(Radius - Track->Radius) > LayerJumpThreshold)
				{
					++Events.LayerJumps;
				}
			}

			Track->Radius = Radius;
			Track->bAirborne = bAirborne;
			Track->bKnockedByHarness = false;

			if (bAirborne || KnockbackStream.FRand() >= KnockbackChance)
				continue;

			// 섬 위 적은 섬 중심 반대쪽으로, 나머지는 임의 접선 방향으로 민다.
			FVector PushDir = bIslandTop ? FVector::VectorPlaneProject(Location - IslandCenter, Up).GetSafeNormal() : FVector::ZeroVector;
			if (PushDir.IsNearlyZero())
			{
				FVector T1, T2;
				MakeTangentBasis(-Up, T1, T2);
				const float Angle = KnockbackStream.FRandRange(0.f, 2.f * PI);
				PushDir = T1 * FMath::Cos(Angle) + T2 * FMath::Sin(Angle);
			}
			LNPHitDetection::ApplyEntityKnockback(Velocity, -PushDir, Up, LNPLoadBaseline::KnockbackStrength);
			Track->bKnockedByHarness = true;
			if (bCount)
				++Events.Knockbacks;
		}
	});
}

void ULNPLoadBaselineSubsystem::CheckFlyerPenetration()
{
	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	if (!FlyerQuery.IsInitialized())
	{
		FlyerQuery = FMassEntityQuery(EntityManager.AsShared());
		FlyerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		FlyerQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
		FlyerQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
		FlyerQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::All);
		// 시체는 착지 뒤 지면에 닿아 있는 것이 정상이다.
		FlyerQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	}
	if (GFlightPenetrationCheck == 0)
		return;

	const ULNPMassWorldCollisionSubsystem& Collision = *GetWorld()->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	const FLNPWorldQueryParams QueryParams(ELNPWorldQueryClass::DebugValidation);
	int32 Penetrating = 0;

	FMassExecutionContext ExecContext(EntityManager, 0.f);
	FlyerQuery.ForEachEntityChunk(ExecContext, [&](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr)
			return;
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// 몸 캡슐(피격 판정의 단일 정의)을 Up 방향으로 1cm만 sweep해 시작 겹침을 본다. 겹침 query의 대용이다.
			const FVector Location = Transforms[i].GetTransform().GetLocation();
			const FVector Up = (Config->MovementConfig.GravityOrigin - Location).GetSafeNormal();
			FLNPWorldHit Hit;
			if (Collision.SweepCapsuleWorld(Location, Location + Up, FQuat::FindBetweenNormals(FVector::UpVector, Up),
				Config->CapsuleRadius, Config->CapsuleHalfHeight, QueryParams, Hit) && Hit.bStartPenetrating)
			{
				++Penetrating;
			}
		}
	});

	++PenetrationCheckedFrames;
	PenetrationEntityFrames += Penetrating;
	if (Penetrating > 0)
	{
		++PenetrationFrames;
	}
}

void ULNPLoadBaselineSubsystem::TopUpProjectiles()
{
	if (ProjectileSourceConfig == nullptr || ProjectileSourceConfig->WeaponData == nullptr)
		return;

	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	if (!ProjectileQuery.IsInitialized())
	{
		ProjectileQuery = FMassEntityQuery(EntityManager.AsShared());
		ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	}

	const int32 Alive = ProjectileQuery.GetNumMatchingEntities();
	const int32 ToInject = FMath::Min(LNPLoadBaseline::GetTargetProjectiles() - Alive, MaxInjectPerFrame);
	if (ToInject <= 0)
		return;

	const ULNPWeaponData& WeaponDef = *ProjectileSourceConfig->WeaponData;
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(
		LNPEntityAttack::MakeProjectileSharedData(WeaponDef, ProjectileSourceConfig->EntityAttackConfig)));

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float SphereRadius = Settings ? Settings->SphereRadius : 25000.f;
	const ULNPSurfaceDataSubsystem* SurfaceData = GetWorld()->GetSubsystem<ULNPSurfaceDataSubsystem>();
	if (SurfaceData == nullptr)
		return;

	// 발사 위치는 SurfaceData Layer 0 adapter로 찍는다. exact probe로 찍으면 harness가 측정 대상 counter에 query를 더한다.
	// 섬 아래 방향에서는 섬 윗면에서 쏘게 되지만 조준점은 링 중심이라 부하 성격은 같다.
	const FVector CenterDir = RingCenter.GetSafeNormal();
	// 내부형 구라 위쪽은 월드 중심 방향이다.
	const FVector CenterUp = -CenterDir;
	FVector T1, T2;
	MakeTangentBasis(CenterDir, T1, T2);

	for (int32 Index = 0; Index < ToInject; ++Index)
	{
		const float Angle = ProjectileStream.FRandRange(0.f, 2.f * PI);
		const float Distance = FMath::Sqrt(ProjectileStream.FRandRange(FMath::Square(RingInnerRadius), FMath::Square(RingOuterRadius)));
		FVector Foot;
		if (!SurfaceData->GetSurfacePoint(MakeRingDirection(CenterDir, T1, T2, Angle, Distance, SphereRadius), Foot))
			continue;

		const FVector Up = -Foot.GetSafeNormal();
		const FVector Muzzle = Foot + Up * MuzzleHeight;

		const float AimAngle = ProjectileStream.FRandRange(0.f, 2.f * PI);
		const float AimDistance = ProjectileStream.FRandRange(0.f, AimScatterRadius);
		const FVector AimTarget = RingCenter + CenterUp * AimHeight + (T1 * FMath::Cos(AimAngle) + T2 * FMath::Sin(AimAngle)) * AimDistance;
		const float Pitch = FMath::DegreesToRadians(ProjectileStream.FRandRange(MinPitchDegrees, MaxPitchDegrees));
		const FVector Direction = ((AimTarget - Muzzle).GetSafeNormal() + Up * FMath::Tan(Pitch)).GetSafeNormal();

		FLNPProjectileFragment ProjFrag;
		ProjFrag.PreviousPos         = Muzzle;
		ProjFrag.SpawnLocation       = Muzzle;
		ProjFrag.Velocity            = Direction * WeaponDef.ProjectileSpeed;
		ProjFrag.LifetimeRemaining   = WeaponDef.ProjectileLifetime;
		ProjFrag.InstigatorTeam      = ELNPInstigatorTeam::Enemy;
		ProjFrag.bIsLocalInstigator  = false;
		ProjFrag.InstigatorPlayerID  = INDEX_NONE;
		ProjFrag.PredictionKeyID     = ULNPGhostProjectileSubsystem::IssueServerSalvoID();
		ProjFrag.SpawnIndex          = 0;
		ProjFrag.CachedRewindSeconds = 0.f;
		ProjFrag.InstigatorViewLocation = Muzzle;

		FLNPProjectileVisualFragment VisualFrag;
		FTransformFragment TransFrag;
		TransFrag.GetMutableTransform().SetLocation(Muzzle);

		FMassArchetypeSharedFragmentValues SharedValuesCopy = SharedValues;
		EntityManager.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
			FMassArchetypeSharedFragmentValues,
			FLNPProjectileFragment,
			FLNPProjectileVisualFragment,
			FTransformFragment>>(
			EntityManager.ReserveEntity(),
			MoveTemp(SharedValuesCopy),
			ProjFrag,
			VisualFrag,
			TransFrag);

		if (Stage == EStage::Capture)
		{
			++InjectedProjectiles;
		}
	}
}

void ULNPLoadBaselineSubsystem::SampleFrame()
{
	UWorld* World = GetWorld();
	const ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

	uint64 QueryCount, QueryNs, LockNs;
	Collision->GetTotals(QueryCount, QueryNs, LockNs);
	FrameQueryCount.Add(QueryCount - LastQueryCount);
	FrameExactNs.Add(QueryNs - LastQueryNs);
	FrameLockNs.Add(LockNs - LastLockNs);
	LastQueryCount = QueryCount;
	LastQueryNs = QueryNs;
	LastLockNs = LockNs;

	// 틱 사이 벽시계 간격. FApp::GetDeltaTime()은 엔진이 0.1초로 잘라 과부하 구간을 과소 측정한다.
	const double WallNow = FPlatformTime::Seconds();
	if (LastFrameWallTime > 0.0)
	{
		FrameMs100.Add(static_cast<uint64>((WallNow - LastFrameWallTime) * 1e5));
	}
	LastFrameWallTime = WallNow;

	if (ProjectileQuery.IsInitialized())
	{
		ProjectileSamples.Add(ProjectileQuery.GetNumMatchingEntities());
	}

	// Actor 수는 1초에 한 번 센다. 매 프레임 순회는 측정 대상에 비용을 더한다.
	const double Now = World->GetRealTimeSeconds();
	if (Now >= NextActorSampleTime)
	{
		NextActorSampleTime = Now + 1.0;
		int32 Promoted = 0;
		for (TActorIterator<ALNPEnemyCharacter> It(World); It; ++It)
		{
			++Promoted;
		}
		PromotedActorSamples.Add(Promoted);

		// 비행 적 행동 상태 분포. 호버가 배회 대기인지 교전 자리 유지·슬롯 대기인지 가른다(슬롯 기반 타게팅이라 Confirmed 수에 상한이 있다).
		if (FlyerQuery.IsInitialized() && World->GetNetMode() != NM_Client)
		{
			FMassEntityManager& EntityManager = World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
			FMassExecutionContext ExecContext(EntityManager, 0.f);
			FlyerQuery.ForEachEntityChunk(ExecContext, [this](FMassExecutionContext& Ctx)
			{
				for (const FLNPEnemyTargetingFragment& Targeting : Ctx.GetFragmentView<FLNPEnemyTargetingFragment>())
				{
					++FlyerStateSums[static_cast<int32>(Targeting.State)];
				}
			});
			++FlyerStateSampleCount;
		}
	}
}

void ULNPLoadBaselineSubsystem::Report()
{
	UWorld* World = GetWorld();
	const ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

	FMassEntityManager& EntityManager = World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	FMassEntityQuery EnemyCountQuery(EntityManager.AsShared());
	EnemyCountQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	const int32 EnemyEntities = EnemyCountQuery.GetNumMatchingEntities();

	// 이동 프로세서 병렬 실행의 잡 단위는 청크다. 청크 수가 병렬 이득의 상한이라 함께 남긴다(Phase03b §3.6.1).
	int32 EnemyChunks = 0;
	int32 LargestChunk = 0;
	if (EnemyQuery.IsInitialized())
	{
		FMassExecutionContext ExecContext(EntityManager, 0.f);
		EnemyQuery.ForEachEntityChunk(ExecContext, [&EnemyChunks, &LargestChunk](FMassExecutionContext& Ctx)
		{
			++EnemyChunks;
			LargestChunk = FMath::Max(LargestChunk, Ctx.GetNumEntities());
		});
	}

	const FPercentiles Frame = ComputePercentiles(FrameMs100, 0.01);
	const FPercentiles Exact = ComputePercentiles(FrameExactNs, 1e-6);
	const FPercentiles Lock = ComputePercentiles(FrameLockNs, 1e-6);
	const FPercentiles Queries = ComputePercentiles(FrameQueryCount, 1.0);

	int32 FramesOverBudget = 0;
	for (const uint64 Value : FrameMs100)
	{
		if (Value * 0.01 > LNPLoadBaseline::FrameBudgetMs)
		{
			++FramesOverBudget;
		}
	}

	const bool bFramePass = Frame.P95 <= LNPLoadBaseline::FrameBudgetMs;
	const bool bExactPass = Exact.P95 <= LNPLoadBaseline::ExactP95BudgetMs;
	const bool bLockPass = Lock.P95 <= LNPLoadBaseline::LockP95BudgetMs;

	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] ===== %s NetMode=%d enemies=%d flyers=%d projectiles=%d seed=%d ExactGround=%d LateralSweep=%d ParallelMovement=%d CPU=%s ====="),
		*World->GetName(), static_cast<int32>(World->GetNetMode()), LNPLoadBaseline::GetEnemyCount(), LNPLoadBaseline::GetFlyerCount(),
		LNPLoadBaseline::GetTargetProjectiles(), LNPLoadBaseline::GetSeed(),
		LNPEnemyExactMovement::IsEnabled() ? 1 : 0, LNPEnemyExactMovement::IsLateralSweepEnabled() ? 1 : 0,
		LNPEnemyExactMovement::IsParallelMovementEnabled() ? 1 : 0,
		*FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] frames=%d EnemyEntities=%d EnemyChunks=%d LargestChunk=%d PromotedActors avg=%.1f max=%d Projectiles avg=%.0f min=%d Injected=%d"),
		FrameMs100.Num(), EnemyEntities, EnemyChunks, LargestChunk, Average(PromotedActorSamples),
		PromotedActorSamples.IsEmpty() ? 0 : FMath::Max(PromotedActorSamples),
		Average(ProjectileSamples), ProjectileSamples.IsEmpty() ? 0 : FMath::Min(ProjectileSamples), InjectedProjectiles);
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] FrameMs P50=%.2f P95=%.2f Max=%.2f over16.6=%d(%.1f%%)"),
		Frame.P50, Frame.P95, Frame.Max, FramesOverBudget,
		FrameMs100.IsEmpty() ? 0.0 : 100.0 * FramesOverBudget / FrameMs100.Num());
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] ExactPerFrame ms P50=%.3f P95=%.3f Max=%.3f | Queries/frame P50=%.0f P95=%.0f Max=%.0f | LockPerFrame ms P50=%.3f P95=%.3f Max=%.3f (probe on)"),
		Exact.P50, Exact.P95, Exact.Max, Queries.P50, Queries.P95, Queries.Max, Lock.P50, Lock.P95, Lock.Max);
	if (World->GetNetMode() != NM_Client)
	{
		UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Population start IslandTop=%d UnderIsland=%d OpenCrust=%d Airborne=%d | end IslandTop=%d UnderIsland=%d OpenCrust=%d Airborne=%d"),
			CaptureStartPopulation.IslandTop, CaptureStartPopulation.UnderIsland, CaptureStartPopulation.OpenCrust, CaptureStartPopulation.Airborne,
			Population.IslandTop, Population.UnderIsland, Population.OpenCrust, Population.Airborne);
		UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Events Knockbacks=%d Falls=%d Landings=%d IslandLeaves=%d IslandDrops=%d LayerJumps=%d"),
			Events.Knockbacks, Events.Falls, Events.Landings, Events.IslandLeaves, Events.IslandDrops, Events.LayerJumps);

		// 비행 적(Phase03c §3.8). 개체당 프레임 query는 capture 표본 프레임 수로 나눈다.
		const int32 Flyers = FlyerQuery.IsInitialized() ? FlyerQuery.GetNumMatchingEntities() : 0;
		if (Flyers > 0)
		{
			const LNPEnemyFlightStats::FCounts Steer = LNPEnemyFlightStats::Get();
			const uint64 Moving = Steer.Clear + Steer.Blocked + Steer.Stuck;
			const double FlyerFrames = static_cast<double>(Flyers) * FMath::Max(1, FrameQueryCount.Num());
			const uint64 SteeringQueries = Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering);
			const uint64 LineOfSightQueries = Collision->GetQueryCount(ELNPWorldQueryClass::EnemyLineOfSight);
			UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Flyers alive=%d PlayersOnIsland=%d | Steer hover=%llu clear=%llu blocked=%llu(%.2f%% of moving) stuck=%llu recoveryEntries=%llu"),
				Flyers, ArePlayersOnIsland() ? 1 : 0, Steer.Hover, Steer.Clear, Steer.Blocked,
				Moving > 0 ? 100.0 * Steer.Blocked / Moving : 0.0, Steer.Stuck, Steer.RecoveryEntries);
			UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Flyers queries per flyer-frame FlightSteering=%.3f EnemyLineOfSight=%.4f (LoS total=%llu)"),
				SteeringQueries / FlyerFrames, LineOfSightQueries / FlyerFrames, LineOfSightQueries);
			const double StateSamples = FMath::Max(1, FlyerStateSampleCount);
			UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Flyers state avg None=%.1f Alert=%.1f Confirmed=%.1f (samples=%d, 1s)"),
				FlyerStateSums[0] / StateSamples, FlyerStateSums[1] / StateSamples, FlyerStateSums[2] / StateSamples, FlyerStateSampleCount);
			UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Flyers penetration check=%d checkedFrames=%d penetratingFrames=%d penetratingEntityFrames=%d"),
				GFlightPenetrationCheck, PenetrationCheckedFrames, PenetrationFrames, PenetrationEntityFrames);
		}
	}
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] UnknownHits=%llu EnvelopeEscapes=%llu"),
		Collision->GetUnknownHitCount() - StartUnknownHits, Collision->GetEnvelopeEscapeCount() - StartEnvelopeEscapes);
	UE_LOG(LogLootNPop, Display, TEXT("[LoadBaseline] Result frame=%s(P95<=%.1fms) exact=%s(P95<=%.1fms) lock=%s(P95<=%.1fms)"),
		bFramePass ? TEXT("PASS") : TEXT("FAIL"), LNPLoadBaseline::FrameBudgetMs,
		bExactPass ? TEXT("PASS") : TEXT("FAIL"), LNPLoadBaseline::ExactP95BudgetMs,
		bLockPass ? TEXT("PASS") : TEXT("FAIL"), LNPLoadBaseline::LockP95BudgetMs);

	Collision->Report();

	// 부하 발사체는 패널을 겨냥하지 않으므로 Dynamic 분류는 패널을 직접 쏴서 확인한다. 통계 표본화가 끝난 뒤라 측정에 섞이지 않는다.
	GEngine->Exec(World, TEXT("LNP.SurfaceNav.ProbePanels"));
	// face→Layer 표(D-037)는 cooked trimesh의 external face 표에 기댄다. 패키지에서도 FaceIndex가 나오는지 확인한다.
	GEngine->Exec(World, TEXT("LNP.SurfaceNav.ProbeFaceIndex"));
	// Phase 5 registry가 베이크 face 표를 runtime component에 연결할 수 있도록 Level Instance에서도 source key가 유지되는지 확인한다.
	GEngine->Exec(World, TEXT("LNP.SurfaceNav.ProbeSourceKeys"));
	// 새 Support snapshot의 8-slot 역회전 조회와 hit registry generation/binding 수를 함께 확인한다.
	GEngine->Exec(World, TEXT("LNP.SurfaceNav.ProbeSurfaceData"));
}

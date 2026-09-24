// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "Config/LNPSettings.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "LootNPop.h"

#include "Async/ParallelFor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

/**
 * Gate -1 A production 8-slot exact collision oracle(D-036).
 *
 * 안쪽 반지름에서 바깥쪽으로 line·sphere·capsule을 쏘아 production 옥탄트 8 slot의 LNPWorldExact 응답을 검사한다.
 * 실패 항목:
 * - Miss: world collision envelope 밖까지 아무것도 맞지 않음 — 관통(이음매 틈·응답 누락)
 * - Unknown: hit identity registry가 해석하지 못함
 * - StartPenetrating: 시작점이 geometry 안
 * - ShapeOrder: sphere·capsule이 같은 방향 line보다 늦게 맞음 — 형상 sweep이 틈으로 빠짐
 * - ExactDeeper: line hit가 legacy SurfaceCache 표면보다 허용치 이상 바깥 — legacy가 보는 지면을 exact가 놓침
 * - SlotMismatch: 모든 slot이 같은 Level이면, 같은 로컬 방향을 slot 회전으로 돌린 8개 방향의 hit 거리가 같아야 함.
 *   persistent level의 런타임 source(런처·앵커·LootPod proxy)는 seed 배치라 대칭이 아니므로 그 방향은 비교하지 않는다.
 *
 * 정보 항목:
 * - EdgeMiss: 이음매 좌표 평면 위에 **정확히** 놓인 line의 miss. 서로 다른 body의 공유 모서리를 따라가는 측도 0의 경우로,
 *   같은 방향의 sphere가 맞고 평면에서 조금만 떨어져도 맞으면 틈이 아니다. 실제 투사체에서 새면 envelope 안전망이 받는다.
 *
 * 방향은 로컬 Fibonacci 방향을 8개 slot 회전으로 돌린 집합과, 좌표 평면 3개를 따라 평면 위·양옆에 촘촘히 뿌린 집합이다.
 * query는 ParallelFor로 worker에서 실행한다.
 */
namespace
{
	constexpr int32 FibonacciCount = 4096;
	constexpr int32 SeamSamplesPerPlane = 1440;       // 0.25°

	/** 이음매 평면에서 떨어뜨리는 각도. 기준 반지름 25,000cm에서 0.04·0.4·4.4cm다. */
	constexpr double SeamOffsetDegrees[] = { 0.0, 1e-4, -1e-4, 1e-3, -1e-3, 1e-2, -1e-2 };

	/** 이 값 이하인 좌표 성분은 이음매 평면 위로 본다. 평면에서 1e-4°는 약 1.7e-6이다. */
	constexpr double OnPlaneEpsilon = 1e-9;

	constexpr double StartRadiusRatio = 0.5;

	/** 플레이어 캡슐과 비슷한 크기. 형상 sweep이 틈에 빠지는지만 보므로 정확한 값일 필요는 없다. */
	constexpr float SweepSphereRadius = 30.f;
	constexpr float SweepCapsuleRadius = 34.f;
	constexpr float SweepCapsuleHalfHeight = 88.f;

	constexpr double ShapeOrderTolerance = 0.5;
	constexpr double LegacyDeeperTolerance = 200.0;
	constexpr double SlotMismatchTolerance = 1.0;

	constexpr double ReadyTimeoutSeconds = 120.0;
	constexpr int32 MaxLoggedFailures = 20;

	enum EOracleShape : uint8 { Line, Sphere, Capsule, ShapeCount };
	const TCHAR* ShapeNames[] = { TEXT("Line"), TEXT("Sphere"), TEXT("Capsule") };

	struct FOracleShapeResult
	{
		bool bHit = false;
		bool bKnown = false;
		bool bStartPenetrating = false;
		double Distance = 0.0;
		double HitRadius = 0.0;
		int8 Slot = INDEX_NONE;
		bool bInstance = false;
	};

	struct FOracleResult
	{
		FOracleShapeResult Shapes[ShapeCount];
		double LegacyRadius = 0.0;
		bool bLegacyValid = false;
	};

	/** 단위 구 위의 고른 방향. */
	FVector FibonacciDirection(const int32 Index, const int32 Count)
	{
		const double GoldenAngle = UE_DOUBLE_PI * (3.0 - FMath::Sqrt(5.0));
		const double Z = 1.0 - (Index + 0.5) * 2.0 / Count;
		const double Ring = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		const double Theta = GoldenAngle * Index;
		return FVector(Ring * FMath::Cos(Theta), Ring * FMath::Sin(Theta), Z);
	}

	/** 좌표 평면 Axis(0=X=0 평면, 1=Y=0, 2=Z=0) 위의 각도 Degrees 방향을 평면 법선 쪽으로 OffsetDegrees만큼 기울인다. */
	FVector SeamDirection(const int32 Axis, const double Degrees, const double OffsetDegrees)
	{
		const double A = FMath::DegreesToRadians(Degrees);
		const double Tilt = FMath::DegreesToRadians(OffsetDegrees);
		const double InPlaneU = FMath::Cos(A) * FMath::Cos(Tilt);
		const double InPlaneV = FMath::Sin(A) * FMath::Cos(Tilt);
		const double Normal = FMath::Sin(Tilt);
		switch (Axis)
		{
		case 0:  return FVector(Normal, InPlaneU, InPlaneV);
		case 1:  return FVector(InPlaneU, Normal, InPlaneV);
		default: return FVector(InPlaneU, InPlaneV, Normal);
		}
	}

	bool IsOnSeamPlane(const FVector& Direction)
	{
		return FMath::Min3(FMath::Abs(Direction.X), FMath::Abs(Direction.Y), FMath::Abs(Direction.Z)) <= OnPlaneEpsilon;
	}

	bool AllSlotsShareLevel(const ULNPOctantSpawnSubsystem& Octants)
	{
		const TArray<FLNPOctantDefinition>& Definitions = Octants.GetSelectedOctantDefinitions();
		if (Definitions.Num() != UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations))
			return false;
		for (const FLNPOctantDefinition& Definition : Definitions)
		{
			if (Definition.LevelAsset != Definitions[0].LevelAsset)
				return false;
		}
		return true;
	}

	bool IsReady(UWorld& World)
	{
		const ULNPMassWorldCollisionSubsystem* Collision = World.GetSubsystem<ULNPMassWorldCollisionSubsystem>();
		const ULNPSurfaceCacheSubsystem* SurfaceCache = World.GetSubsystem<ULNPSurfaceCacheSubsystem>();
		FVector Probe;
		return Collision && Collision->GetWorldEnvelopeRadius() > 0.f
			&& SurfaceCache && SurfaceCache->GetSurfacePoint(FVector::UpVector, Probe);
	}

	void RunOracle(UWorld& World)
	{
		const ULNPMassWorldCollisionSubsystem& Collision = *World.GetSubsystem<ULNPMassWorldCollisionSubsystem>();
		const ULNPSurfaceCacheSubsystem& SurfaceCache = *World.GetSubsystem<ULNPSurfaceCacheSubsystem>();
		const ULNPOctantSpawnSubsystem* Octants = World.GetSubsystem<ULNPOctantSpawnSubsystem>();

		const double StartRadius = GetDefault<ULNPSettings>()->SphereRadius * StartRadiusRatio;
		const double EndRadius = Collision.GetWorldEnvelopeRadius() + LNPProjectileMotion::WorldEnvelopeMargin;
		const bool bCheckSlotEquivalence = Octants && AllSlotsShareLevel(*Octants);

		// 앞 FibonacciCount*8개는 [로컬 방향][slot] 순서라 slot 비교에 쓴다. 뒤는 이음매 평면 방향이다.
		constexpr int32 SlotCount = UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations);
		TArray<FVector> Directions;
		Directions.Reserve(FibonacciCount * SlotCount + 3 * SeamSamplesPerPlane * UE_ARRAY_COUNT(SeamOffsetDegrees));
		for (int32 Index = 0; Index < FibonacciCount; ++Index)
		{
			const FVector Local = FibonacciDirection(Index, FibonacciCount);
			for (int32 Slot = 0; Slot < SlotCount; ++Slot)
			{
				Directions.Add(ULNPOctantSpawnSubsystem::OctantRotations[Slot].RotateVector(Local).GetSafeNormal());
			}
		}
		const int32 SeamBegin = Directions.Num();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 Sample = 0; Sample < SeamSamplesPerPlane; ++Sample)
			{
				const double Degrees = 360.0 * Sample / SeamSamplesPerPlane;
				for (const double Offset : SeamOffsetDegrees)
				{
					Directions.Add(SeamDirection(Axis, Degrees, Offset).GetSafeNormal());
				}
			}
		}

		TArray<FOracleResult> Results;
		Results.SetNum(Directions.Num());
		const double StartSeconds = FPlatformTime::Seconds();

		ParallelFor(Directions.Num(), [&](const int32 Index)
		{
			const FVector& Direction = Directions[Index];
			const FVector Start = Direction * StartRadius;
			const FVector End = Direction * EndRadius;
			const FLNPWorldQueryParams Params(ELNPWorldQueryClass::DebugValidation);
			FOracleResult& Result = Results[Index];

			for (int32 Shape = 0; Shape < ShapeCount; ++Shape)
			{
				FLNPWorldHit Hit;
				bool bHit = false;
				switch (Shape)
				{
				case Line:
					bHit = Collision.RaycastWorld(Start, End, Params, Hit);
					break;
				case Sphere:
					bHit = Collision.SweepSphereWorld(Start, End, SweepSphereRadius, Params, Hit);
					break;
				default:
					bHit = Collision.SweepCapsuleWorld(Start, End, FRotationMatrix::MakeFromZ(Direction).ToQuat(),
						SweepCapsuleRadius, SweepCapsuleHalfHeight, Params, Hit);
					break;
				}

				FOracleShapeResult& ShapeResult = Result.Shapes[Shape];
				ShapeResult.bHit = bHit;
				ShapeResult.bKnown = Hit.Identity.IsKnown();
				ShapeResult.bStartPenetrating = Hit.bStartPenetrating;
				ShapeResult.Distance = Hit.Distance;
				ShapeResult.HitRadius = Hit.ImpactPoint.Size();
				ShapeResult.Slot = Hit.Identity.Slot;
				ShapeResult.bInstance = Hit.Identity.InstanceIndex != INDEX_NONE;
			}

			FVector SurfacePoint;
			Result.bLegacyValid = SurfaceCache.GetSurfacePoint(Direction, SurfacePoint);
			Result.LegacyRadius = SurfacePoint.Size();
		});

		const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

		int32 Misses[ShapeCount] = {};
		int32 Unknowns[ShapeCount] = {};
		int32 StartPenetrations[ShapeCount] = {};
		int32 EdgeMisses = 0;
		int32 ShapeOrderFailures = 0;
		int32 ExactDeeperFailures = 0;
		int32 SlotMismatches = 0;
		int32 SlotComparisonsSkipped = 0;
		int32 LoggedFailures = 0;
		TArray<double> TerrainLegacyDiffs;

		auto LogFailure = [&LoggedFailures, &Directions](const TCHAR* Reason, const int32 Index, const FString& Detail)
		{
			if (LoggedFailures++ < MaxLoggedFailures)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[ExactOracle] FAIL %s dir=(%.6f, %.6f, %.6f) %s"),
					Reason, Directions[Index].X, Directions[Index].Y, Directions[Index].Z, *Detail);
			}
		};

		for (int32 Index = 0; Index < Results.Num(); ++Index)
		{
			const FOracleResult& Result = Results[Index];
			for (int32 Shape = 0; Shape < ShapeCount; ++Shape)
			{
				const FOracleShapeResult& ShapeResult = Result.Shapes[Shape];
				if (!ShapeResult.bHit)
				{
					if (Shape == Line && IsOnSeamPlane(Directions[Index]) && Result.Shapes[Sphere].bHit)
					{
						++EdgeMisses;
						continue;
					}
					++Misses[Shape];
					LogFailure(TEXT("Miss"), Index, ShapeNames[Shape]);
					continue;
				}
				if (!ShapeResult.bKnown)
				{
					++Unknowns[Shape];
					LogFailure(TEXT("Unknown"), Index, ShapeNames[Shape]);
				}
				if (ShapeResult.bStartPenetrating)
				{
					++StartPenetrations[Shape];
					LogFailure(TEXT("StartPenetrating"), Index, ShapeNames[Shape]);
				}
			}

			const FOracleShapeResult& LineResult = Result.Shapes[Line];
			if (!LineResult.bHit)
				continue;

			for (const int32 Shape : { static_cast<int32>(Sphere), static_cast<int32>(Capsule) })
			{
				const FOracleShapeResult& ShapeResult = Result.Shapes[Shape];
				if (ShapeResult.bHit && ShapeResult.Distance > LineResult.Distance + ShapeOrderTolerance)
				{
					++ShapeOrderFailures;
					LogFailure(TEXT("ShapeOrder"), Index,
						FString::Printf(TEXT("%s=%.1f line=%.1f"), ShapeNames[Shape], ShapeResult.Distance, LineResult.Distance));
				}
			}

			if (Result.bLegacyValid)
			{
				if (LineResult.HitRadius > Result.LegacyRadius + LegacyDeeperTolerance)
				{
					++ExactDeeperFailures;
					LogFailure(TEXT("ExactDeeper"), Index,
						FString::Printf(TEXT("exact=%.1f legacy=%.1f"), LineResult.HitRadius, Result.LegacyRadius));
				}
				if (LineResult.Slot != INDEX_NONE && !LineResult.bInstance)
				{
					TerrainLegacyDiffs.Add(FMath::Abs(LineResult.HitRadius - Result.LegacyRadius));
				}
			}
		}

		if (bCheckSlotEquivalence)
		{
			for (int32 Local = 0; Local < FibonacciCount; ++Local)
			{
				for (int32 Shape = 0; Shape < ShapeCount; ++Shape)
				{
					double MinDistance = TNumericLimits<double>::Max();
					double MaxDistance = TNumericLimits<double>::Lowest();
					bool bComparable = true;
					for (int32 Slot = 0; Slot < SlotCount; ++Slot)
					{
						const FOracleShapeResult& ShapeResult = Results[Local * SlotCount + Slot].Shapes[Shape];
						bComparable &= ShapeResult.bHit && ShapeResult.Slot != INDEX_NONE;
						MinDistance = FMath::Min(MinDistance, ShapeResult.Distance);
						MaxDistance = FMath::Max(MaxDistance, ShapeResult.Distance);
					}
					if (!bComparable)
					{
						++SlotComparisonsSkipped;
						continue;
					}
					if (MaxDistance - MinDistance > SlotMismatchTolerance)
					{
						++SlotMismatches;
						FString Detail = FString::Printf(TEXT("%s spread=%.1f r/slot:"), ShapeNames[Shape], MaxDistance - MinDistance);
						for (int32 Slot = 0; Slot < SlotCount; ++Slot)
						{
							const FOracleShapeResult& ShapeResult = Results[Local * SlotCount + Slot].Shapes[Shape];
							Detail += FString::Printf(TEXT(" %.1f/%d"), ShapeResult.HitRadius, ShapeResult.Slot);
						}
						LogFailure(TEXT("SlotMismatch"), Local * SlotCount, Detail);
					}
				}
			}
		}

		TerrainLegacyDiffs.Sort();
		auto Percentile = [&TerrainLegacyDiffs](const double P)
		{
			return TerrainLegacyDiffs.IsEmpty() ? 0.0 : TerrainLegacyDiffs[FMath::Min(TerrainLegacyDiffs.Num() - 1,
				static_cast<int32>(P * TerrainLegacyDiffs.Num()))];
		};

		int32 TotalFailures = ShapeOrderFailures + ExactDeeperFailures + SlotMismatches;
		for (int32 Shape = 0; Shape < ShapeCount; ++Shape)
		{
			TotalFailures += Misses[Shape] + Unknowns[Shape] + StartPenetrations[Shape];
		}

		UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] World=%s NetMode=%d Directions=%d (seam %d) Start=%.0fcm End=%.0fcm Queries=%d Time=%.1fms"),
			*World.GetName(), static_cast<int32>(World.GetNetMode()), Directions.Num(), Directions.Num() - SeamBegin,
			StartRadius, EndRadius, Directions.Num() * ShapeCount, ElapsedMs);
		for (int32 Shape = 0; Shape < ShapeCount; ++Shape)
		{
			UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] %-7s Miss=%d Unknown=%d StartPenetrating=%d"),
				ShapeNames[Shape], Misses[Shape], Unknowns[Shape], StartPenetrations[Shape]);
		}
		UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] ShapeOrder=%d ExactDeeper=%d SlotMismatch=%d%s (skipped %d runtime-source comparisons)"),
			ShapeOrderFailures, ExactDeeperFailures, SlotMismatches,
			bCheckSlotEquivalence ? TEXT("") : TEXT(" (not checked: slots use different levels)"), SlotComparisonsSkipped);
		UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] Info: EdgeMiss=%d (line exactly on a seam plane, sphere hits)"), EdgeMisses);
		UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] Info: slot terrain |exact-legacy| radius cm: n=%d P50=%.1f P95=%.1f max=%.1f"),
			TerrainLegacyDiffs.Num(), Percentile(0.5), Percentile(0.95), TerrainLegacyDiffs.IsEmpty() ? 0.0 : TerrainLegacyDiffs.Last());
		UE_LOG(LogLootNPop, Display, TEXT("[ExactOracle] Result=%s Failures=%d"), TotalFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), TotalFailures);
	}

	/**
	 * LNP.SurfaceNav.ExactOracle
	 * 옥탄트 생성·registry 게시·SurfaceCache 베이크가 끝나지 않았으면 끝날 때까지 기다렸다 실행한다(-ExecCmds 시작 실행용).
	 */
	FAutoConsoleCommandWithWorld GLNPExactOracle(
		TEXT("LNP.SurfaceNav.ExactOracle"),
		TEXT("Run the production 8-slot LNPWorldExact oracle: radial line/sphere/capsule queries over every slot and the seam planes. ")
		TEXT("Checks misses, unknown hits, start penetration, sweep order, exact-vs-legacy surface depth and slot equivalence. ")
		TEXT("Waits for world generation and SurfaceCache bake if they are not finished yet."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (World == nullptr)
				return;

			const double Deadline = FPlatformTime::Seconds() + ReadyTimeoutSeconds;
			TWeakObjectPtr<UWorld> WeakWorld = World;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Deadline](float) mutable
			{
				// -game 클라이언트는 시작 맵에서 명령을 받은 뒤 서버 맵으로 travel한다. 그러면 현재 게임 월드로 옮겨 간다.
				if (!WeakWorld.IsValid() && GEngine)
				{
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						if (Context.WorldType == EWorldType::Game && Context.World())
						{
							WeakWorld = Context.World();
							break;
						}
					}
				}
				UWorld* TickWorld = WeakWorld.Get();
				if (TickWorld == nullptr)
					return FPlatformTime::Seconds() <= Deadline;
				if (IsReady(*TickWorld))
				{
					RunOracle(*TickWorld);
					return false;
				}
				if (FPlatformTime::Seconds() > Deadline)
				{
					UE_LOG(LogLootNPop, Error, TEXT("[ExactOracle] Timed out waiting for world generation, hit identity registry and SurfaceCache bake."));
					return false;
				}
				return true;
			}), 0.5f);
		}));
}

#endif // !UE_BUILD_SHIPPING

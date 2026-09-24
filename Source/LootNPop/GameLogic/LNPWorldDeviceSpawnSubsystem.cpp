// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPWorldDeviceSpawnSubsystem.h"
#include "Config/LNPSettings.h"
#include "DataAsset/LNPWorldDeviceConfig.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "GameMode/LNPGameState.h"
#include "WorldDevice/LNPGrappleAnchor.h"
#include "WorldDevice/LNPSpringLauncher.h"
#include "LootNPop.h"

#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

namespace
{
	/** 중심 → 바깥 방향 트레이스. bTraceComplex=false는 표면 캐시·PCG 프랍 배치와 같은 조합이다 —
	 *  세 시스템이 같은 면을 기준으로 삼아야 적 접지·발사체 지면 판정과 장치 위치가 어긋나지 않는다. */
	bool TraceSurface(const UWorld& World, const FVector& Dir, float SphereRadius, FHitResult& OutHit)
	{
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPWorldDevicePlacement), /*bTraceComplex=*/false);
		return World.LineTraceSingleByChannel(OutHit, FVector::ZeroVector, Dir * (SphereRadius * 2.0f),
			ECC_WorldStatic, Params);
	}
}

void ULNPWorldDeviceSpawnSubsystem::SpawnDevices()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	if (Config == nullptr)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[WorldDevice] No device config asset — placement skipped (LNPSettings.WorldDeviceConfig)"));
		return;
	}

	UClass* AnchorClass = Config->GrappleAnchorClass.LoadSynchronous();
	UClass* LauncherClass = Config->SpringLauncherClass.LoadSynchronous();
	if (AnchorClass == nullptr && LauncherClass == nullptr)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[WorldDevice] No device classes configured — placement skipped"));
		return;
	}

	// 시드를 OctantGenSeed에서 파생한다. 서버 단독 실행이어도 **같은 시드 = 같은 배치**가 성립해
	// 버그 재현이 가능해진다. 상수 XOR은 옥탄트 선택과 같은 시퀀스를 쓰지 않게 하려는 것뿐이다.
	const ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	const int32 Seed = (GS && GS->OctantGenSeed != 0) ? (GS->OctantGenSeed ^ 0x5EED) : FMath::Rand();
	FRandomStream Rand(Seed);

	const double StartTime = FPlatformTime::Seconds();
	FLNPDevicePlacementStats Stats;
	TArray<FVector> Occupied;
	int32 NumAnchors = 0;
	int32 NumLaunchers = 0;

	// 앵커를 먼저 배치한다 — 최소 거리 기각이 선착순이라 순서가 결과를 바꾸는데, 순서를 고정해야
	// 같은 시드가 같은 월드를 만든다.
	for (int32 i = 0; AnchorClass != nullptr && i < Config->GrappleAnchorCount; ++i)
	{
		FVector SurfacePoint;
		float Yaw = 0.f;
		if (!TryFindPlacement(Rand, /*bIsAnchor=*/true, Occupied, SurfacePoint, Yaw, Stats))
		{
			continue;
		}
		Occupied.Add(SurfacePoint);

		const FVector Up = -SurfacePoint.GetSafeNormal();
		const FVector AnchorPoint = SurfacePoint + Up * Config->AnchorHeight;

		// AnchorID는 스폰 번치에 실려야 한다 — 공통 스폰 함수가 Deferred 스폰으로 대입 후 Finish한다(LootDice 페이로드와 같은 규약).
		const FTransform Xform(UKismetMathLibrary::MakeRotFromZ(Up), AnchorPoint);
		const AActor* Anchor = ULNPDynamicTerrainSubsystem::SpawnPlacedActor(*World, AnchorClass, Xform, [NumAnchors](AActor& Spawned)
		{
			CastChecked<ALNPGrappleAnchor>(&Spawned)->AnchorID = NumAnchors;
		});
		if (Anchor == nullptr)
		{
			continue;
		}
		++NumAnchors;
	}

	for (int32 i = 0; LauncherClass != nullptr && i < Config->SpringLauncherCount; ++i)
	{
		FVector SurfacePoint;
		float Yaw = 0.f;
		if (!TryFindPlacement(Rand, /*bIsAnchor=*/false, Occupied, SurfacePoint, Yaw, Stats))
		{
			continue;
		}
		Occupied.Add(SurfacePoint);

		const FVector Up = -SurfacePoint.GetSafeNormal();
		const FQuat YawSpin(Up, FMath::DegreesToRadians(Yaw));
		const FTransform Xform(YawSpin * UKismetMathLibrary::MakeRotFromZ(Up).Quaternion(), SurfacePoint);
		if (ULNPDynamicTerrainSubsystem::SpawnPlacedActor(*World, LauncherClass, Xform, [](AActor&) {}) != nullptr)
		{
			++NumLaunchers;
		}
	}

	// 기각 사유별 계수를 반드시 남긴다 — 전부 0이면 검사가 무력한 것이고, 대부분 기각이면 임계가 과한 것이다.
	UE_LOG(LogLootNPop, Log,
		TEXT("[WorldDevice] Placed anchors=%d/%d launchers=%d/%d in %.1fms (seed %d) | rejected: noHit=%d slope=%d ledge=%d clearance=%d tooClose=%d playerStart=%d"),
		NumAnchors, Config->GrappleAnchorCount, NumLaunchers, Config->SpringLauncherCount,
		(FPlatformTime::Seconds() - StartTime) * 1000.0, Seed,
		Stats.NoHit, Stats.Slope, Stats.Ledge, Stats.Clearance, Stats.TooClose, Stats.PlayerStartCone);
}

bool ULNPWorldDeviceSpawnSubsystem::TryFindPlacement(FRandomStream& Rand, bool bIsAnchor,
	const TArray<FVector>& Occupied, FVector& OutLocation, float& OutYaw, FLNPDevicePlacementStats& Stats) const
{
	const UWorld* World = GetWorld();
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(this);
	const float SphereRadius = GetDefault<ULNPSettings>()->SphereRadius;
	if (Config == nullptr)
	{
		return false;
	}

	// PlayerStart 영역 제외 — Mass 스폰과 같은 규약(FVector::DownVector 기준 10도 원뿔).
	const float CosDownExclude = FMath::Cos(FMath::DegreesToRadians(10.0f));
	const FVector BaseDir = Rand.GetUnitVector();

	for (int32 Retry = 0; Retry < Config->MaxPlacementRetries; ++Retry)
	{
		// 재시도는 완전 재추첨이 아니라 미세 지터다 — Mass 스폰의 위치 추첨과 같은 구조.
		const FVector Dir = (Retry == 0) ? BaseDir : (BaseDir + Rand.GetUnitVector() * 0.05f).GetSafeNormal();

		if (FVector::DotProduct(Dir, FVector::DownVector) > CosDownExclude)
		{
			++Stats.PlayerStartCone;
			continue;
		}

		FHitResult Hit;
		if (!TraceSurface(*World, Dir, SphereRadius, Hit))
		{
			++Stats.NoHit;
			continue;
		}

		const FVector Location = Hit.ImpactPoint;
		const FVector Up = -Location.GetSafeNormal();   // 구 내벽 — Up은 구 중심을 향한다

		// ① 경사. 표면 캐시로는 판정할 수 없다(법선을 저장하지 않는다) — 여기서 트레이스를 쏘는 이유다.
		if (FVector::DotProduct(Hit.ImpactNormal, Up) < Config->MinSurfaceNormalDot)
		{
			++Stats.Slope;
			continue;
		}

		// ② 단차. 경사 검사만으로는 **절벽 꼭대기 가장자리**를 못 거른다 — 가장자리는 평평해서
		//    법선이 완벽한데도 장치가 낭떠러지에 반쯤 걸쳐 놓인 것처럼 보인다.
		const FVector Tangent1 = FVector::CrossProduct(Up, FVector::ForwardVector).GetSafeNormal();
		const FVector Tangent2 = FVector::CrossProduct(Up, Tangent1).GetSafeNormal();
		const float AngularOffset = Config->LedgeSampleOffset / SphereRadius;
		bool bLedge = false;
		for (const FVector& Tangent : { Tangent1, -Tangent1, Tangent2, -Tangent2 })
		{
			FHitResult NeighborHit;
			const FVector NeighborDir = (Dir + Tangent * AngularOffset).GetSafeNormal();
			if (!TraceSurface(*World, NeighborDir, SphereRadius, NeighborHit)
				|| FMath::Abs(NeighborHit.Distance - Hit.Distance) > Config->MaxLedgeStep)
			{
				bLedge = true;
				break;
			}
		}
		if (bLedge)
		{
			++Stats.Ledge;
			continue;
		}

		// ③ 타입별 여유 공간
		const float Yaw = Rand.FRandRange(0.f, 360.f);
		if (bIsAnchor)
		{
			// 앵커는 도달 지점이다 — 바위 속에 박혀 있으면 비행 스윕이 도착 직전에 막힌다.
			if (!HasClearance(Location + Up * Config->AnchorHeight, Config->AnchorClearance))
			{
				++Stats.Clearance;
				continue;
			}
		}
		else
		{
			// 런처는 발사 방향 전방이 비어 있어야 한다. 방향은 이 Yaw로 정해지므로 여기서 함께 본다.
			const FQuat YawSpin(Up, FMath::DegreesToRadians(Yaw));
			const FVector Forward = YawSpin.RotateVector(UKismetMathLibrary::MakeRotFromZ(Up).Quaternion().GetForwardVector());
			const FVector Start = Location + Up * 100.f;
			if (World->LineTraceTestByChannel(Start, Start + Forward * Config->LauncherForwardClearance,
				ECC_WorldStatic, FCollisionQueryParams(SCENE_QUERY_STAT(LNPWorldDeviceClearance), false)))
			{
				++Stats.Clearance;
				continue;
			}
		}

		// ④ 상호 최소 거리 — 앵커와 런처를 같은 목록으로 본다. 두 장치가 한 화면에 몰리면
		//    프롬프트 중재가 매번 개입해야 하고 플레이어도 어느 쪽이 잡혔는지 헷갈린다.
		bool bTooClose = false;
		for (const FVector& Other : Occupied)
		{
			if (FVector::DistSquared(Location, Other) < FMath::Square(Config->MinDistanceBetweenDevices))
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose)
		{
			++Stats.TooClose;
			continue;
		}

		OutLocation = Location;
		OutYaw = Yaw;
		return true;
	}

	return false;
}

bool ULNPWorldDeviceSpawnSubsystem::HasClearance(const FVector& Point, float Radius) const
{
	if (Radius <= 0.f)
	{
		return true;
	}

	return !GetWorld()->OverlapAnyTestByChannel(Point, FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeSphere(Radius),
		FCollisionQueryParams(SCENE_QUERY_STAT(LNPWorldDeviceClearance), /*bTraceComplex=*/false));
}

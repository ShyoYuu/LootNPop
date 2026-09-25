// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPWorldDeviceSpawnSubsystem.generated.h"

/** 배치 기각 사유 — 임계값이 무력한지 과한지를 로그 한 줄로 판단하기 위한 계수. */
struct FLNPDevicePlacementStats
{
	int32 NoHit = 0;
	int32 Slope = 0;
	int32 Ledge = 0;
	int32 Clearance = 0;
	int32 TooClose = 0;
	int32 PlayerStartCone = 0;
};

/**
 * 월드 장치(그래플 앵커·스프링 런처)를 시드 결정론으로 배치하는 **서버 전용** 서브시스템.
 *
 * **왜 서버만 도는가.** 액터를 복제하기 때문이다. 양쪽이 각자 배치하면 대역폭은 0이 되지만
 * 라인트레이스 결과까지 머신 간에 일치해야 하고(어긋나면 조용히 틀린 장치가 생긴다),
 * 동적 스폰된 비복제 액터는 NetGUID가 없어 런처 발동 요청이 참조를 잃는다.
 * 정적 액터 하나의 초기 복제 비용은 약 30B이고 이후 갱신이 없어, 50개를 다 합쳐도 적 1기가 2.5초 쓰는 양이다.
 *
 * **왜 ULNPMassSpawnSubsystem에 얹지 않는가.** 그쪽 위치 추첨은 표면 캐시 스냅샷만 쓰는 순수 수학이라
 * 워커 스레드에서 돈다. 이 배치는 `Hit.ImpactNormal`이 필요해 **라인트레이스가 필수**이고 결과로 Actor를
 * 스폰하므로 게임 스레드 작업이다. 동기 라인트레이스 자체는 워커에서도 안전하다(SurfaceSupportNavigation
 * Phase 3 Gate 0). 표면 캐시에는 법선이 없다(TechDesign_SurfaceCache.md).
 */
UCLASS()
class LOOTNPOP_API ULNPWorldDeviceSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 장치를 배치한다. 서버에서, **표면 베이킹이 끝난 뒤** 한 번 호출한다 —
	 * 옥탄트 레벨이 가시화(콜리전 등록)되고 나서야 트레이스가 지면을 맞힌다.
	 */
	void SpawnDevices();

	/** 서버 전용. 시드 배치와 마커 배치가 공유하는 AnchorID 카운터. 두 경로의 ID가 겹치지 않게 한다. */
	int32 AllocateAnchorID() { return NextAnchorID++; }

private:
	int32 NextAnchorID = 0;

	/**
	 * 조건을 만족하는 배치 지점을 하나 찾는다. 실패하면 false.
	 * @param OutLaunchYaw  런처의 표면 Up축 주위 회전(도). MakeRotFromZ는 Up만 맞추고 Yaw는 구현 정의라
	 *                      수평 성분이 있는 발사 방향이 무작위 나침반 방향을 가리키게 되므로 시드에서 뽑는다.
	 */
	bool TryFindPlacement(FRandomStream& Rand, bool bIsAnchor, const TArray<FVector>& Occupied,
		FVector& OutLocation, float& OutYaw, FLNPDevicePlacementStats& Stats) const;

	/** 지면에서 위로 Height만큼 띄운 지점이 Radius 반경 안에서 비어 있는가. */
	bool HasClearance(const FVector& Point, float Radius) const;
};

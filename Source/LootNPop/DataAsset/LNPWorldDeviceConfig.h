// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPWorldDeviceConfig.generated.h"

class ALNPGrappleAnchor;
class ALNPSpringLauncher;

/**
 * 월드 장치(그래플 앵커·스프링 런처)의 배치·판정·발사 파라미터.
 *
 * **서버와 클라이언트가 같은 에셋을 읽는다.** 장치 액터가 자기 UPROPERTY로 값을 들고 있으면
 * 배치 시 서버가 넣어 준 값이 복제되지 않아 클라이언트는 CDO 기본값으로 프롬프트를 판정하게 된다.
 * 데이터 에셋은 양쪽에 똑같이 존재하므로 그 문제가 생기지 않는다 — 판정 값을 복제할 이유도 사라진다.
 *
 * 비행 속력·도착 처리 같은 **캐릭터 이동 파라미터는 여기 없다.** 그쪽은 대시와 마찬가지로
 * ULNPCharacterMoverComponent의 UPROPERTY다 — 시뮬레이션 틱이 매번 읽는 값이라 에셋 로드를 끼우지 않는다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPWorldDeviceConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ─────────────── 배치: 무엇을 몇 개 ───────────────

	UPROPERTY(EditAnywhere, Category = "Placement")
	TSoftClassPtr<ALNPGrappleAnchor> GrappleAnchorClass;

	UPROPERTY(EditAnywhere, Category = "Placement")
	TSoftClassPtr<ALNPSpringLauncher> SpringLauncherClass;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0"))
	int32 GrappleAnchorCount = 30;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0"))
	int32 SpringLauncherCount = 20;

	/** 장치 간 최소 거리 (cm). 두 장치가 한 화면에 몰려 프롬프트가 경쟁하는 것을 막는다. */
	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistanceBetweenDevices = 3000.0f;

	/** 장치 1개당 위치 재추첨 최대 횟수. 소진하면 그 장치는 배치하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "1"))
	int32 MaxPlacementRetries = 12;

	// ─────────────── 배치: 지형 검사 ───────────────

	/**
	 * 배치 가능 표면의 최소 법선 코사인 (반지름 방향 기준). 0.71 ≈ 45°.
	 * ⚠️ 이 검사만으로는 **절벽 꼭대기 가장자리**를 못 거른다 — 가장자리는 평평해서 법선이 완벽하다.
	 *    그래서 아래 단차 검사가 따로 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "Placement|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinSurfaceNormalDot = 0.71f;

	/** 단차 검사용 접평면 샘플 거리 (cm). 이 거리만큼 떨어진 4방향의 표면 높이를 본다. */
	UPROPERTY(EditAnywhere, Category = "Placement|Terrain", meta = (ClampMin = "50.0", Units = "cm"))
	float LedgeSampleOffset = 250.0f;

	/** 허용 가능한 이웃 표면 높이차 (cm). 넘으면 절벽 가장자리로 보고 기각한다. */
	UPROPERTY(EditAnywhere, Category = "Placement|Terrain", meta = (ClampMin = "10.0", Units = "cm"))
	float MaxLedgeStep = 150.0f;

	// ─────────────── 그래플 앵커 ───────────────

	/** 앵커를 지표에서 띄우는 높이 (cm). 올려다보고 날아가는 장치이므로 지면에 붙이지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor", meta = (ClampMin = "0.0", Units = "cm"))
	float AnchorHeight = 700.0f;

	/** 앵커 주변에 비어 있어야 하는 반경 (cm). 바위 속에 박힌 앵커를 거른다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor", meta = (ClampMin = "0.0", Units = "cm"))
	float AnchorClearance = 200.0f;

	/** 그래플 가능 최대 거리 (cm). 광역 탐색 반경도 이 값이다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor", meta = (ClampMin = "500.0", Units = "cm"))
	float MaxGrappleDistance = 2500.0f;

	/** 그래플 가능 최소 거리 (cm). 너무 가까우면 순간이동처럼 보여 프롬프트를 띄우지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor", meta = (ClampMin = "0.0", Units = "cm"))
	float MinGrappleDistance = 400.0f;

	/** 조준 판정 반각 (도). 고정 각도라 FOV·종횡비와 무관하게 일정한 화면 반경이 된다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float AnchorAimHalfAngle = 8.0f;

	/** 가림 검사를 할 것인가. 끄면 산 너머 앵커에도 프롬프트가 뜬다. */
	UPROPERTY(EditAnywhere, Category = "Grapple Anchor")
	bool bRequireLineOfSight = true;

	// ─────────────── 스프링 런처 ───────────────

	/** 런처 발사 방향 전방에 비어 있어야 하는 거리 (cm). 천장·바위에 코박고 발사되는 배치를 거른다. */
	UPROPERTY(EditAnywhere, Category = "Spring Launcher", meta = (ClampMin = "0.0", Units = "cm"))
	float LauncherForwardClearance = 2000.0f;

	/** 상호작용 허용 최대 거리 (cm). 각도 제한은 없다 — 어느 방향에서 올라타도 같은 방향으로 날아간다. */
	UPROPERTY(EditAnywhere, Category = "Spring Launcher", meta = (ClampMin = "0.0", Units = "cm"))
	float LauncherInteractionRadius = 250.0f;

	/** 지면(접평면) 기준 발사 앙각 (도). 45가 같은 속력에서 사거리가 최대다. */
	UPROPERTY(EditAnywhere, Category = "Spring Launcher", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float LaunchAngleDegrees = 45.0f;

	/** 발사 속력 (cm/s). 구면 중력에 맡겨 포물선을 그리므로 사거리는 이 값이 지배한다. */
	UPROPERTY(EditAnywhere, Category = "Spring Launcher", meta = (ClampMin = "500.0", ClampMax = "12000.0"))
	float LaunchSpeed = 4500.0f;

	/** F 입력부터 사출까지의 정렬 시간(초). 선딜레이가 있어야 "장치가 작동한다"는 느낌이 난다. */
	UPROPERTY(EditAnywhere, Category = "Spring Launcher", meta = (ClampMin = "0.05", Units = "s"))
	float AlignDuration = 1.0f;

	/** 프로젝트 설정(ULNPSettings)이 가리키는 설정 에셋을 로드해 돌려준다. 없으면 nullptr. */
	static const ULNPWorldDeviceConfig* Get(const UObject* WorldContext);
};

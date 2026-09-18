// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Core/CameraNode.h"
#include "LNPLobbedADSPitchCameraNode.generated.h"

/**
 * 유탄(Lobbed) 무기로 ADS 중일 때 **착탄 가이드 장판이 화면 같은 자리에 걸리도록** 카메라를
 * 제자리에서 아래로 기울이는 Camera Rig 노드.
 *
 * 유탄은 멀리 쏠수록 총구를 위로 들어야 하는데, 장판은 지표면에 깔린다
 * (`ULNPTrajectoryGuideComponent`). 카메라가 발사축과 같은 방향을 보면 장판이 화면 아래로
 * 밀려나 정작 조준할 때 보이지 않는다. 이 노드가 그 둘을 떼어놓는다.
 *
 * **왜 발사각을 올리지 않고 카메라를 내리는가.** 둘은 기하학적으로 같지만(카메라 = 발사축 − θ),
 * 발사각에 θ를 더하면 화면 프레이밍용 값이 서버 탄도 계약의 일부가 된다 — 서버도 같은 θ를
 * 적용해야 예측이 맞는다. 카메라 쪽에 두면 θ는 로컬 렌더에만 존재하고, 재튜닝이 서버를 건드리지 않는다
 * (→ TechDesign_CharacterMovement.md §2.6의 소유권 구분).
 *
 * **전제 — 조준 원본이 카메라가 아니어야 한다.** `ULNPInputHandlerComponent::ComputeCrosshairAimPoint`가
 * 카메라 시선으로 조준점을 잡으면, 카메라를 내리는 순간 조준선도 같이 내려가 플레이어가 그만큼 더
 * 올리게 되고 효과가 정확히 상쇄된다. 그래서 조준 광선은 `ControlRotation`을 본다.
 *
 * **Look 입력과 카메라가 어긋나는 것이 이 노드의 의도다.** θ가 매 프레임 달라지므로 시선을 올린 만큼
 * 화면이 그대로 올라가지 않는다. 상시 조작이면 결함이지만 ADS 한정이라 "조준"이라는 의도에 맞고,
 * 최대 사거리를 넘겨 계속 올리면 θ가 `MaxPitchDownDegrees`에 걸려 **카메라가 장판보다 위로 올라간다** —
 * 사거리 라벨이 한계를 알려주므로 그 위를 보려는 의도로 읽힌다 (2026-09-18 플레이 테스트로 확정).
 *
 * **배치 위치: 리그의 맨 끝.** 제자리 회전이라 위치를 바꾸지 않지만, `DampenPosition`·`Offset`은
 * `CameraPose` 회전을 프레임으로 쓰므로(§2.4) 그들보다 앞에 두면 붐 거리와 감쇠 축까지 기울어진다.
 */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPLobbedADSPitchCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:
	/** 착탄 표식을 화면 중앙에서 아래로 몇 도에 붙잡을 것인가. ADS 수직 반화각이 약 18°다. */
	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float DesiredImpactBelowCenterDeg = 8.f;

	/**
	 * 기울기의 상한 (도). 최대 사거리에서 발사각이 30°를 넘으므로 상한이 없으면 캐릭터가 화면 밖으로 밀린다.
	 * 이 값에 걸리는 구간이 곧 "사거리 밖을 올려다보는" 구간이다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float MaxPitchDownDegrees = 20.f;

	/**
	 * 목표 기울기를 따라가는 속도 (도/초).
	 *
	 * 두 가지를 동시에 맡는다 — ① 조준을 옮기는 동안의 **추적 속도**, ② ADS 진입·해제나 무기 교체로
	 * 게이트가 뒤집힐 때(같은 조준 모드면 ADS가 풀리지 않는다 — §2.6) 각도가 한 프레임에 튀지 않게 하는 수렴.
	 * 올리면 조준 추적이 단단해지는 대신 무기 교체 시 전환이 급해진다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "1"))
	float BlendSpeedDegreesPerSecond = 60.f;

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};

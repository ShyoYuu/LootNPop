// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

/**
 * 월드 좌표 → 스크린 스페이스 마커 위젯의 로컬 좌표.
 *
 * 마커를 그리는 모든 소비처가 이 함수 하나를 쓴다. 두 함정을 여기서 한 번에 닫기 위해서다:
 *
 * ⚠️ **카메라 뒤 대상.** ProjectWorldLocationToScreen은 뒤쪽 대상에 대해서도 true를 돌려주면서
 *    좌표를 뒤집어 준다 — 뒤에 있는 적의 마커가 화면 반대편에 찍힌다. 투영 전에 전방 판정을 건다
 *    (ULNPLockOnComponent::ApplySoftRotation이 이미 쓰는 관례와 같다).
 *
 * ⚠️ **단위.** 투영 결과는 뷰포트 **픽셀**이고 위젯 로컬 좌표는 **슬레이트 단위**다. DPI 스케일이
 *    1이 아닌 화면에서 둘을 섞으면 마커가 대상에서 비례해 밀린다. ViewportScale로 나눈다.
 *    이것이 성립하려면 마커 위젯이 Canvas 앵커 (0,0)-(1,1)·오프셋 0으로 배치돼 있어야 한다.
 */
namespace LNPScreenProjection
{
	inline bool ProjectToWidgetLocal(const APlayerController& PC, const FVector& WorldLocation,
		const FVector& CameraLocation, const FVector& CameraForward, const float ViewportScale,
		FVector2f& OutLocalPos)
	{
		if (FVector::DotProduct(CameraForward, WorldLocation - CameraLocation) <= 0.f)
			return false;

		FVector2D ScreenPos;
		if (!PC.ProjectWorldLocationToScreen(WorldLocation, ScreenPos, false))
			return false;

		const float SafeScale = (ViewportScale > KINDA_SMALL_NUMBER) ? ViewportScale : 1.f;
		OutLocalPos = FVector2f(static_cast<float>(ScreenPos.X) / SafeScale, static_cast<float>(ScreenPos.Y) / SafeScale);
		return true;
	}
}

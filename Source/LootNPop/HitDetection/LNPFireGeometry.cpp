// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPFireGeometry.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"

#include "Components/SkeletalMeshComponent.h"

FVector LNPFireGeometry::ResolveMuzzleLocation(const ALNPCharacterBase& Character, const ULNPWeaponData& WeaponDef)
{
	static const FName MuzzleSocket(TEXT("Muzzle"));

	const USkeletalMeshComponent* WeaponMesh = Character.GetWeaponMesh();
	const FVector BasePos = (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocket))
		? WeaponMesh->GetSocketLocation(MuzzleSocket)
		: Character.GetActorLocation();

	return BasePos + Character.GetActorTransform().TransformVector(WeaponDef.MuzzleOffset);
}

FVector LNPFireGeometry::ResolveAimDirection(const FVector& SpawnPos, const FVector& ViewDirection, const FVector* AimTargetPtr)
{
	// 조준점이 없을 때의 조준선. 플레이어는 발사 순간의 시선, 컨트롤러가 없는 사수(적 NPC)는
	// GetBaseAimRotation(액터 전방 + 상하 조준 Pitch)이 들어온다.
	const FVector FallbackDirection = ViewDirection;

	// 플레이어 폰: 조준점은 **예측 클라이언트와 서버가 같은 값**(발동 RPC에 실린 스냅샷)을 쓴다. 각자 계산하면
	// 서버 판정과 클라이언트 예측이 그 시차만큼 갈라지고, 총구와 카메라가 떨어져 있으므로 그 오차는
	// 거리와 무관한 상수로 남는다 — 게스트가 조준선대로 맞혀도 서버 판정이 나지 않던 원인이 정확히 이것이었다.
	if (nullptr == AimTargetPtr)
		return FallbackDirection;
	const FVector& AimTarget = *AimTargetPtr;

	// 조준점은 소유 클라이언트가 만든 값이라 검증이 필요하다. 다만 시선 회전(ControlRotation)도
	// 이미 같은 클라이언트가 보내는 값이라 새로 생기는 권위는 없고, 여기서 메우는 것은
	// 총구-카메라 시차뿐이다.
	//
	// 검증의 **단위**가 중요하다. 조준점은 카메라 광선 위의 점이므로(A = 카메라 + t·시선),
	// 총구에서 조준점까지의 벡터를 시선 축으로 분해하면 수직 성분은 t에 무관하게
	// **총구-카메라 간격 그 자체**로 남는다. 이것을 각도로 재면 거리에 반비례해 발산해
	// 근거리에서 임계를 넘기고, 그러면 **게이트가 자기가 통과시키려던 시차 보정을 도로 막는다**
	// — 예전 고정 각도(15°) 게이트가 그랬다. 그때 남는 폴백은 총구에서 카메라와 평행하게
	// 나가는 광선이라, 탄이 항상 카메라 쪽(화면 기준 왼쪽)으로 치우쳤다.
	//
	// 그래서 허용치를 각도가 아니라 **시선 축에서의 수직 이탈 거리**로 잰다. 거리와 무관한
	// 상수이고, 방어 관점에서도 각도보다 곧다 — 조준점을 조작해도 탄착점을 이 거리 이상
	// 옆으로 끌 수 없으므로 옆 표적으로 옮겨 맞히는 것이 되지 않는다.
	static constexpr float MaxAimLateralOffsetSq = 150.f * 150.f;

	// 조준점이 총구보다 뒤이거나 거의 붙어 있는 구간(벽에 밀착 등)은 수렴시키면 뒤로 쏘게 되므로
	// 폴백으로 남긴다. 총구 기준이라, 카메라 기준이던 예전 1.5m 사각지대와 달리
	// 눈앞의 적은 이 구간에 들어오지 않는다.
	static constexpr float MinForwardDistance = 50.f;

	const FVector ToTarget    = AimTarget - SpawnPos;
	const double  ForwardDist = FVector::DotProduct(ToTarget, FallbackDirection);
	const double  LateralSq   = FMath::Max(0.0, ToTarget.SizeSquared() - ForwardDist * ForwardDist);

	return ((MinForwardDistance <= ForwardDist) && (LateralSq <= MaxAimLateralOffsetSq))
		? ToTarget.GetSafeNormal()
		: FallbackDirection;
}

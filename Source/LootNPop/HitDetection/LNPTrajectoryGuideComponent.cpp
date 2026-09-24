// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPTrajectoryGuideComponent.h"
#include "HitDetection/LNPFireGeometry.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "Item/LNPWeaponData.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

namespace
{
	const FName PointsParameterName(TEXT("Points"));
}

ULNPTrajectoryGuideComponent::ULNPTrajectoryGuideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPTrajectoryGuideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GuideComponent)
	{
		GuideComponent->DestroyComponent();
		GuideComponent = nullptr;
	}

	if (BlastRadiusDecal)
	{
		BlastRadiusDecal->DestroyComponent();
		BlastRadiusDecal = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ULNPTrajectoryGuideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 게이트를 매 틱 다시 평가한다 — 어느 조건으로 빠져나가도 가이드가 남지 않는다.
	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner());
	if (nullptr == Character || !Character->IsLocallyControlled() || !Character->IsADSActive())
	{
		HideGuide();
		return;
	}

	// 중력이 0인 무기(직선 탄)는 조준선이 곧 궤적이라 가이드가 아무것도 더 알려주지 않는다.
	const ULNPWeaponData* WeaponDef = Character->GetActiveWeaponDef();
	const float GravityAccel = WeaponDef ? WeaponDef->GetEffectiveProjectileGravity() : 0.f;
	if (GravityAccel <= 0.f)
	{
		HideGuide();
		return;
	}

	const UWorld* World = GetWorld();
	const ULNPSurfaceCacheSubsystem* SurfaceCache = World ? World->GetSubsystem<ULNPSurfaceCacheSubsystem>() : nullptr;
	if (nullptr == SurfaceCache)
	{
		HideGuide();
		return;
	}

	// 총구와 조준 방향은 실탄이 쓰는 바로 그 함수로 구한다.
	// 조준점은 입력 핸들러가 틱마다 캐시한 값 — 발사 순간 발동 RPC에 실리는 것과 같은 원본이다.
	const FVector Muzzle = LNPFireGeometry::ResolveMuzzleLocation(*Character, *WeaponDef);
	const ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>();
	FVector AimPoint;
	const bool bHasAimPoint = InputHandler && InputHandler->GetCrosshairAimPoint(AimPoint);
	const FVector Direction = LNPFireGeometry::ResolveAimDirection(Muzzle, Character->GetBaseAimRotation().Vector(),
		bHasAimPoint ? &AimPoint : nullptr);

	// 실탄과 같은 월드 판정 경로를 고른다(LNP.SurfaceNav.ProjectileExact).
	const ULNPMassWorldCollisionSubsystem* WorldCollision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	bool bPredicted = false;
	if (LNPProjectileMotion::UseExactWorldCollision() && WorldCollision)
	{
		// exact 경로는 SurfaceCache 베이크 완료를 옥탄트 로드 완료의 신호로만 쓴다.
		FVector SurfaceProbe;
		bPredicted = SurfaceCache->GetSurfacePoint(Muzzle.GetSafeNormal(), SurfaceProbe);
		if (bPredicted)
		{
			LNPProjectileMotion::PredictArcExact(*WorldCollision, Muzzle, Direction * WeaponDef->ProjectileSpeed,
				GravityAccel, WeaponDef->ProjectileLifetime, ArcPoints);
		}
	}
	else
	{
		bPredicted = LNPProjectileMotion::PredictArc(*SurfaceCache, Muzzle, Direction * WeaponDef->ProjectileSpeed,
			GravityAccel, WeaponDef->ProjectileLifetime, ArcPoints);
	}

	if (!bPredicted)
	{
		// 표면 Cache 베이킹 전 — 지면을 모르는 채로 그리면 궤적이 지형을 뚫고 뻗는다.
		HideGuide();
		return;
	}

	ShowGuide(WeaponDef->ExplosionRadius);
	bGuideVisible = true;
}

bool ULNPTrajectoryGuideComponent::GetImpactPoint(FVector& OutImpactPoint) const
{
	if (!bGuideVisible || ArcPoints.IsEmpty())
		return false;

	// 마지막 표본이 착탄 예상 지점이다 (PredictArc의 계약) — 장판이 놓이는 좌표와 같은 원본이다.
	OutImpactPoint = ArcPoints.Last();
	return true;
}

void ULNPTrajectoryGuideComponent::ShowGuide(const float ExplosionRadius)
{
	ShowBlastRadius(ExplosionRadius);

	if (nullptr == GuideSystem)
		return;

	if (nullptr == GuideComponent)
	{
		// 트레일과 같은 관례 — 풀을 쓰지 않고 직접 소유해 수명을 우리가 통제한다
		// (ULNPProjectileVisualSubsystem::AllocateTrails).
		GuideComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), GuideSystem,
			FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector,
			/*bAutoDestroy=*/false, /*bAutoActivate=*/false, ENCPoolMethod::None);

		if (nullptr == GuideComponent)
			return;
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(GuideComponent, PointsParameterName, ArcPoints);

	if (!GuideComponent->IsActive())
		GuideComponent->Activate();
}

void ULNPTrajectoryGuideComponent::ShowBlastRadius(const float ExplosionRadius)
{
	if (nullptr == BlastRadiusMaterial || ExplosionRadius <= 0.f)
		return;

	if (nullptr == BlastRadiusDecal)
	{
		AActor* Owner = GetOwner();
		if (nullptr == Owner)
			return;

		BlastRadiusDecal = NewObject<UDecalComponent>(Owner);
		BlastRadiusDecal->SetDecalMaterial(BlastRadiusMaterial);

		// 거리에 따라 사라지면 정작 멀리 던질 때 표식이 없다 — 화면 크기 페이드를 끈다.
		BlastRadiusDecal->SetFadeScreenSize(0.f);
		BlastRadiusDecal->RegisterComponent();

		// 착탄점은 사수와 무관한 월드 좌표다. 붙이되 트랜스폼은 절대값으로 둬 폰을 따라가지 않게 한다.
		BlastRadiusDecal->AttachToComponent(Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepWorldTransform);
		BlastRadiusDecal->SetAbsolute(true, true, true);
	}

	// 마지막 표본이 착탄 예상 지점이다 (PredictArc의 계약).
	const FVector ImpactPoint = ArcPoints.Last();

	// 구 내벽 세계라 "아래"는 원점에서 바깥쪽이다. 데칼은 로컬 +X로 투영하므로 X를 아래로 향한다.
	const FVector DownDir = ImpactPoint.GetSafeNormal();

	BlastRadiusDecal->DecalSize = FVector(BlastRadiusDecalDepth, ExplosionRadius, ExplosionRadius);
	BlastRadiusDecal->SetWorldLocationAndRotation(ImpactPoint, FRotationMatrix::MakeFromX(DownDir).ToQuat());
	BlastRadiusDecal->MarkRenderStateDirty();

	if (!BlastRadiusDecal->IsVisible())
		BlastRadiusDecal->SetVisibility(true);
}

void ULNPTrajectoryGuideComponent::HideGuide()
{
	bGuideVisible = false;

	// ⚠️ Deactivate()는 스폰만 멈추고 살아 있는 파티클은 수명이 다할 때까지 둔다. 가이드 파티클은
	//    수명이 사실상 무한(9999초)이라 그대로 두면 ADS를 풀어도 궤적이 화면에 남는다.
	//    DeactivateImmediate()가 즉시 지운다 — 파괴하지 않으므로 재진입 비용은 그대로 없다.
	if (GuideComponent && GuideComponent->IsActive())
		GuideComponent->DeactivateImmediate();

	if (BlastRadiusDecal && BlastRadiusDecal->IsVisible())
		BlastRadiusDecal->SetVisibility(false);
}

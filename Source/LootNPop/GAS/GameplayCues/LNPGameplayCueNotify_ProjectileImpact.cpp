// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_ProjectileImpact.h"
#include "HitDetection/LNPProjectileImpactContext.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"

bool ULNPGameplayCueNotify_ProjectileImpact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FLNPProjectileImpactContext* Ctx = static_cast<const FLNPProjectileImpactContext*>(Parameters.EffectContext.Get());
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!Ctx || !World)
		return true;

	const FLNPGhostKey Key{ Ctx->InstigatorPlayerID, Ctx->PredictionKeyID, Ctx->SpawnIndex };
	ULNPGhostProjectileSubsystem* GhostSub = World->GetSubsystem<ULNPGhostProjectileSubsystem>();

	// 서버 확정 임팩트 — 이 클라이언트에 남아 있는 Ghost(공격자 예측·관전용 공통 키)를 정리한다.
	// 로컬 판정이 이미 파괴했거나 애초에 없던 클라이언트에서는 no-op.
	if (GhostSub)
		GhostSub->DestroyGhost(Key);

	// 로컬 코스메틱 판정이 이미 예측 위치에서 임팩트 VFX를 재생했다면 중복 재생하지 않는다.
	// 기록이 없으면(Ghost가 아직 살아있던 브랜치 B, 예측 경로가 없는 리슨 호스트, 스폰 방송 미수신 등)
	// 서버 확정 위치에 재생한다.
	const bool bAlreadyPlayedLocally = GhostSub && GhostSub->ConsumeRecentLocalImpact(Key);
	if (!bAlreadyPlayedLocally)
	{
		if (ULNPProjectileVisualSubsystem* VisualSub = World->GetSubsystem<ULNPProjectileVisualSubsystem>())
			VisualSub->EnqueueImpact(Ctx->VFXData, Parameters.Location, Parameters.Normal);
	}

	return true;
}

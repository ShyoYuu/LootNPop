// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/LNPPhysicalHitReactAnimInstance.h"
#include "Character/LNPCharacterBase.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** 임펄스를 받는 바디(=본 이름 모디파이어). MoverSample·UEFN PhysicsAsset 양쪽에 있는 가슴 체인. */
	const FName PhysicalHitReactBodies[] = { TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };

	TAutoConsoleVariable<float> CVarPhysHitReactHoldSeconds(
		TEXT("LNP.Anim.PhysHitReactHoldSeconds"), 0.15f,
		TEXT("Spike: seconds the physical hit react stays at full weight after a hit."));

	TAutoConsoleVariable<float> CVarPhysHitReactFadeSeconds(
		TEXT("LNP.Anim.PhysHitReactFadeSeconds"), 0.35f,
		TEXT("Spike: seconds to fade the physical hit react back to pure animation."));
}

void ULNPPhysicalHitReactAnimInstance::QueueHitImpulse(const FVector& VelocityChangeWorld)
{
	PendingVelocityChange += VelocityChangeWorld;
	bHasPendingImpulse = true;
}

void ULNPPhysicalHitReactAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const float FadeSeconds = FMath::Max(CVarPhysHitReactFadeSeconds.GetValueOnGameThread(), UE_KINDA_SMALL_NUMBER);
	HitBlendTimeRemaining = FMath::Max(HitBlendTimeRemaining - DeltaSeconds, 0.f);

	// 노드는 핀 값이 남아 있는 동안 매 Evaluate마다 힘을 준다 — 한 업데이트만 채우고 바로 비운다.
	HitForces.ForcesAndTorques.Reset();
	if (bHasPendingImpulse)
	{
		FPhysicsControlForceAndTorqueData Data;
		Data.ForceAndTorqueSpace = EPhysicsControlSpace::World;
		Data.Type = EPhysicsControlForceType::VelocityChange;
		Data.Linear = PendingVelocityChange;
		for (const FName& BodyName : PhysicalHitReactBodies)
			HitForces.ForcesAndTorques.Add(BodyName, Data);

		PendingVelocityChange = FVector::ZeroVector;
		bHasPendingImpulse = false;
		// 알파가 0이던 동안 노드가 평가되지 않았으므로, 다시 올라가는 첫 평가에서 시간 기반 리셋
		// (EvaluationResetTime)이 바디를 현재 애니 포즈로 옮긴 뒤 이 임펄스를 준다 — 튐 없이 시작한다.
		HitBlendTimeRemaining = CVarPhysHitReactHoldSeconds.GetValueOnGameThread() + FadeSeconds;
	}

	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwningActor());
	const bool bRagdoll = Character && Character->IsRagdollActive();
	PhysicsAlpha = bRagdoll ? 0.f : FMath::Clamp(HitBlendTimeRemaining / FadeSeconds, 0.f, 1.f);
}

// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/LNPPhysicalHitReactAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ANS_LNPMeleeHitWindow.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace
{
	/** 선속도·각속도를 받는 가슴 체인(=본 이름 모디파이어). MoverSample·UEFN PhysicsAsset 양쪽에 있다. */
	const FName ChestBodies[] = { TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };

	/** 지점 임펄스 후보 — 피격 위치에 가장 가까운 바디 하나만 받는다. 두 PhysicsAsset 모두 바디가 있는 본만 둔다. */
	const FName PointImpulseBodies[] = {
		TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"), TEXT("head"),
		TEXT("upperarm_l"), TEXT("upperarm_r") };

	/**
	 * 피격 구간에 강도를 낮추는 컨트롤 세트. RBWC 림 셋업이 "종류_림" 이름으로 세트를 만든다.
	 * ⚠️ pelvis에서 시작하는 Torso 림은 팔·목·다리 컨트롤까지 담는다(LogPhysicsControl Verbose로 확인) — 사실상 온몸이다.
	 */
	const FName SoftenControlSet = TEXT("ParentSpace_Torso");

	/** 두 PhysicsAsset의 척추 체인 후보. 실제 바디의 부모 관계와 일치하는 것만 공급한다. */
	const TPair<FName, FName> SpineBodyPairs[] = {
		{ TEXT("pelvis"), TEXT("spine_01") }, { TEXT("spine_01"), TEXT("spine_02") },
		{ TEXT("pelvis"), TEXT("spine_02") }, { TEXT("spine_02"), TEXT("spine_03") },
		{ TEXT("spine_03"), TEXT("spine_04") }, { TEXT("spine_04"), TEXT("spine_05") } };

	/** 방향성 모드 비트. 조합할 수 있다. */
	enum EHitReactModeBits : int32
	{
		Mode_Linear = 1,	// 가슴 체인 선속도(1A 스파이크 방식)
		Mode_Tilt   = 2,	// Up × PushDir 축 각속도로 상체 젖히기
		Mode_Point  = 4,	// 피격 위치에 가장 가까운 바디에 월드 지점 임펄스
		Mode_Soften = 8,	// 피격 구간 동안 몸통 컨트롤 강도 낮추기
		Mode_Pelvis = 32, // 키네마틱 골반의 월드 공간 밀림·젖힘·비틀림
		Mode_SpineSoften = 64, // 척추 컨트롤만 약하게(8과 함께면 8 우선)
	};

	// 3-2 사용자 선택 116의 물리 구성. 몽타주 경로 제거로 생략 비트 16이 빠져 기본값은 100이다.
	TAutoConsoleVariable<int32> CVarPhysHitReactMode(
		TEXT("LNP.Anim.PhysHitReactMode"), Mode_Point | Mode_Pelvis | Mode_SpineSoften,
		TEXT("Physical hit react direction mode bits: 1=linear chest velocity, 2=tilt (angular about Up x PushDir), 4=point impulse at nearest body, 8=soften all torso-set controls, 32=pelvis offset, 64=soften spine only (8 takes priority). Default 100. Compare 100 (pelvis on) with 68 (pelvis off), keeping point impulse and spine softening unchanged. Hit react montages are no longer used; former bit 16 has no effect. 0 disables the physical hit react."));

	TAutoConsoleVariable<float> CVarPhysHitReactImpulse(
		TEXT("LNP.Anim.PhysHitReactImpulse"), 250.f,
		TEXT("Mode 1: chest velocity change (cm/s) at reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactTiltSpeed(
		TEXT("LNP.Anim.PhysHitReactTiltSpeed"), 8.f,
		TEXT("Mode 2: chest angular velocity change (rad/s) at reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactPointImpulse(
		TEXT("LNP.Anim.PhysHitReactPointImpulse"), 800.f,
		TEXT("Mode 4: velocity change (cm/s) at the impact point on the nearest body, at reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactSoftenStrength(
		TEXT("LNP.Anim.PhysHitReactSoftenStrength"), 0.1f,
		TEXT("Modes 8/64: angular strength multiplier at full physics weight (blends back to 1 with the fade). Mode 8 affects the whole torso set; 64 affects the spine only."));

	TAutoConsoleVariable<float> CVarPhysHitReactPelvisShift(
		TEXT("LNP.Anim.PhysHitReactPelvisShift"), 3.f,
		TEXT("Mode 32: peak pelvis translation along the push direction (cm) at reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactPelvisLean(
		TEXT("LNP.Anim.PhysHitReactPelvisLean"), 6.f,
		TEXT("Mode 32: peak pelvis lean about Up x PushDir (degrees) at reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactPelvisTwist(
		TEXT("LNP.Anim.PhysHitReactPelvisTwist"), 7.5f,
		TEXT("Mode 32: peak pelvis twist about Up (degrees), scaled by impact side and reference strength."));

	TAutoConsoleVariable<float> CVarPhysHitReactPelvisPeakSeconds(
		TEXT("LNP.Anim.PhysHitReactPelvisPeakSeconds"), 0.1f,
		TEXT("Mode 32: animation seconds from hit to peak pelvis offset."));

	TAutoConsoleVariable<float> CVarPhysHitReactHoldSeconds(
		TEXT("LNP.Anim.PhysHitReactHoldSeconds"), 0.15f,
		TEXT("Seconds the physical hit react stays at full weight after a hit."));

	TAutoConsoleVariable<float> CVarPhysHitReactFadeSeconds(
		TEXT("LNP.Anim.PhysHitReactFadeSeconds"), 0.35f,
		TEXT("Seconds to fade the physical hit react back to pure animation."));

	TAutoConsoleVariable<float> CVarPhysHitReactRefDamage(
		TEXT("LNP.Anim.PhysHitReactRefDamage"), 20.f,
		TEXT("Damage that maps to strength 1. Strength = clamp(Damage / RefDamage, 0.5, 2) x weapon kind scale."));

	TAutoConsoleVariable<float> CVarPhysHitReactMeleeScale(
		TEXT("LNP.Anim.PhysHitReactMeleeScale"), 1.5f,
		TEXT("Strength multiplier for melee hits (ranged hits use 1)."));

	TAutoConsoleVariable<bool> CVarPhysHitReactLog(
		TEXT("LNP.Anim.PhysHitReactLog"), false,
		TEXT("Log each physical hit react (owner, mode, strength, nearest body, source montage and position)."));
}

void ULNPPhysicalHitReactAnimInstance::QueueHitReact(const FVector& PushDirWorld, const FVector& ImpactPointWorld, float Damage, bool bMelee)
{
	const float RefDamage = FMath::Max(CVarPhysHitReactRefDamage.GetValueOnGameThread(), UE_KINDA_SMALL_NUMBER);
	const float DamageScale = Damage > 0.f ? FMath::Clamp(Damage / RefDamage, 0.5f, 2.f) : 1.f;
	const float Strength = DamageScale * (bMelee ? CVarPhysHitReactMeleeScale.GetValueOnGameThread() : 1.f);

	// 같은 프레임의 여러 피격(산탄 등)은 합치지 않고 가장 센 것 하나로 — 방향이 서로 상쇄되거나 과하게 쌓이지 않게.
	if (bHasPendingHit && Strength <= PendingHit.Strength)
		return;

	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwningActor());
	PendingHit.PushDir = PushDirWorld;
	PendingHit.ImpactPoint = ImpactPointWorld;
	PendingHit.Up = Character ? Character->GetUpDirection() : FVector::UpVector;
	PendingHit.Strength = Strength;
	bHasPendingHit = true;
}

void ULNPPhysicalHitReactAnimInstance::BuildHitForces()
{
	const int32 Mode = CVarPhysHitReactMode.GetValueOnGameThread();

	FPhysicsControlForceAndTorqueData Data;
	Data.ForceAndTorqueSpace = EPhysicsControlSpace::World;
	Data.Type = EPhysicsControlForceType::VelocityChange;

	if (Mode & (Mode_Linear | Mode_Tilt))
	{
		if (Mode & Mode_Linear)
			Data.Linear = PendingHit.PushDir * (CVarPhysHitReactImpulse.GetValueOnGameThread() * PendingHit.Strength);
		// 축 Up × PushDir로 돌리면 Up 쪽 끝(가슴 위)이 PushDir 쪽으로 넘어간다 — 구면에서도 캐릭터 Up 기준이다.
		if (Mode & Mode_Tilt)
			Data.Angular = FVector::CrossProduct(PendingHit.Up, PendingHit.PushDir).GetSafeNormal()
				* (CVarPhysHitReactTiltSpeed.GetValueOnGameThread() * PendingHit.Strength);
		for (const FName& BodyName : ChestBodies)
			HitForces.ForcesAndTorques.Add(BodyName, Data);
	}

	FName PointBody = NAME_None;
	if (Mode & Mode_Point)
	{
		const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
		const bool bHasImpactPoint = !PendingHit.ImpactPoint.IsNearlyZero();
		if (Mesh && bHasImpactPoint)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			for (const FName& BodyName : PointImpulseBodies)
			{
				if (Mesh->GetBoneIndex(BodyName) == INDEX_NONE)
					continue;
				const float DistSq = FVector::DistSquared(Mesh->GetBoneLocation(BodyName), PendingHit.ImpactPoint);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					PointBody = BodyName;
				}
			}
		}

		FPhysicsControlForceAndTorqueData PointData;
		PointData.ForceAndTorqueSpace = EPhysicsControlSpace::World;
		PointData.Type = EPhysicsControlForceType::VelocityChange;
		PointData.Linear = PendingHit.PushDir * (CVarPhysHitReactPointImpulse.GetValueOnGameThread() * PendingHit.Strength);
		if (PointBody.IsNone())
		{
			// 지점을 모르면 가슴 중심(질량 중심)에 준다.
			PointBody = TEXT("spine_04");
		}
		else
		{
			PointData.LocationSpace = EPhysicsControlSpace::World;
			PointData.Location = PendingHit.ImpactPoint;
		}

		// 가슴 체인 항목과 겹치면 TMap 키가 하나라 덮인다 — 같은 바디면 선·각 성분을 합쳐 지점 쪽으로 보낸다.
		if (const FPhysicsControlForceAndTorqueData* Existing = HitForces.ForcesAndTorques.Find(PointBody))
		{
			PointData.Linear += Existing->Linear;
			PointData.Angular += Existing->Angular;
		}
		HitForces.ForcesAndTorques.Add(PointBody, PointData);
	}

	if (CVarPhysHitReactLog.GetValueOnGameThread())
	{
		// 공격 몽타주 도중에도 힘이 공급되는지 메인 인스턴스의 재생 상태와 함께 확인한다.
		const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwningActor());
		const UAnimInstance* SourceAnim = Character ? Character->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = SourceAnim ? SourceAnim->GetCurrentActiveMontage() : nullptr;
		UE_LOG(LogLootNPop, Log, TEXT("[PhysHitReact] %s mode=%d strength=%.2f push=%s impact=%s pointBody=%s sourceMontage=%s pos=%.3f"),
			*GetNameSafe(GetOwningActor()), Mode, PendingHit.Strength, *PendingHit.PushDir.ToCompactString(),
			*PendingHit.ImpactPoint.ToCompactString(), *PointBody.ToString(), *GetNameSafe(Montage),
			Montage ? SourceAnim->Montage_GetPosition(Montage) : 0.f);
	}
}

void ULNPPhysicalHitReactAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const int32 Mode = CVarPhysHitReactMode.GetValueOnGameThread();
	const float FadeSeconds = FMath::Max(CVarPhysHitReactFadeSeconds.GetValueOnGameThread(), UE_KINDA_SMALL_NUMBER);
	HitBlendTimeRemaining = FMath::Max(HitBlendTimeRemaining - DeltaSeconds, 0.f);
	PelvisHitElapsed += DeltaSeconds;

	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwningActor());
	const bool bRagdoll = Character && Character->IsRagdollActive();
	const UAnimMontage* PlayerMeleeMontage = nullptr;
	if (Character && Character->IsA<ALNPPlayerCharacter>())
	{
		// 로컬 제어 여부·비복제 태그 대신 실제 메인 몽타주를 읽어 원격 플레이어도 같은 억제 기준을 쓴다.
		if (const UAnimInstance* SourceAnim = Character->GetAnimInstance())
			for (const FAnimMontageInstance* Instance : SourceAnim->MontageInstances)
			{
				if (!Instance || !Instance->Montage || (!Instance->IsActive() && Instance->GetWeight() <= ZERO_ANIMWEIGHT_THRESH))
					continue;
				// 판정 창 밖의 준비·회수 동작과 블렌드아웃도 포함해, 잔여 물리가 다음 판정 궤적으로 섞이지 않게 한다.
				if (Instance->Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Notify)
					{ return Notify.NotifyStateClass && Notify.NotifyStateClass->IsA<UANS_LNPMeleeHitWindow>(); }))
				{
					PlayerMeleeMontage = Instance->Montage;
					break;
				}
			}
	}
	const bool bSuppressPhysics = bRagdoll || Mode == 0 || PlayerMeleeMontage != nullptr;
	if (PlayerMeleeMontage && (bHasPendingHit || HitBlendTimeRemaining > 0.f) && CVarPhysHitReactLog.GetValueOnGameThread())
		UE_LOG(LogLootNPop, Log, TEXT("[PhysHitReact] %s suppressed=PlayerMeleeAttack pending=%d active=%d sourceMontage=%s"),
			*GetNameSafe(Character), bHasPendingHit, HitBlendTimeRemaining > 0.f, *GetNameSafe(PlayerMeleeMontage));

	// 노드는 핀 값이 남아 있는 동안 매 Evaluate마다 힘을 준다 — 한 업데이트만 채우고 바로 비운다.
	HitForces.ForcesAndTorques.Reset();
	if (bHasPendingHit)
	{
		// 랙돌·플레이어 근접 공격 중 피격은 버린다 — 종료 뒤 늦게 재생하지 않는다.
		if (!bSuppressPhysics)
		{
			BuildHitForces();
			ActivePelvisHit = PendingHit;
			PelvisHitElapsed = 0.f;
			PelvisHitSide = 0.f;
			if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent(); Mesh && !PendingHit.ImpactPoint.IsNearlyZero())
			{
				const FVector SideAxis = FVector::CrossProduct(PendingHit.Up, PendingHit.PushDir).GetSafeNormal();
				PelvisHitSide = FMath::Clamp(FVector::DotProduct(PendingHit.ImpactPoint - Mesh->GetBoneLocation(TEXT("spine_04")), SideAxis) / 20.f, -1.f, 1.f);
			}
			// 알파가 0이던 동안 노드가 평가되지 않았으므로, 다시 올라가는 첫 평가에서 시간 기반 리셋
			// (EvaluationResetTime)이 바디를 현재 애니 포즈로 옮긴 뒤 이 임펄스를 준다 — 튐 없이 시작한다.
			HitBlendTimeRemaining = CVarPhysHitReactHoldSeconds.GetValueOnGameThread() + FadeSeconds;
		}
		PendingHit = FPendingHit();
		bHasPendingHit = false;
	}
	// 공격 전에 시작된 반응도 즉시 끊어 무기 소켓에서 읽는 플레이어 판정 궤적을 보호한다.
	if (bSuppressPhysics)
		HitBlendTimeRemaining = 0.f;

	PhysicsAlpha = FMath::Clamp(HitBlendTimeRemaining / FadeSeconds, 0.f, 1.f);

	HitKinematicTargets.Targets.Reset();
	if ((Mode & Mode_Pelvis) && PhysicsAlpha > 0.f)
	{
		const float Phase = PelvisHitElapsed / FMath::Max(CVarPhysHitReactPelvisPeakSeconds.GetValueOnGameThread(), UE_KINDA_SMALL_NUMBER);
		const float Envelope = Phase * FMath::Exp(1.f - Phase) * ActivePelvisHit.Strength;
		FRigidBodyKinematicTarget Target;
		Target.TargetPosition = ActivePelvisHit.PushDir * (CVarPhysHitReactPelvisShift.GetValueOnGameThread() * Envelope);
		const FVector LeanAxis = FVector::CrossProduct(ActivePelvisHit.Up, ActivePelvisHit.PushDir).GetSafeNormal();
		const FQuat Lean(LeanAxis, FMath::DegreesToRadians(CVarPhysHitReactPelvisLean.GetValueOnGameThread() * Envelope));
		// 맞은 쪽 어깨가 밀리는 회전은 Up 둘레 음의 방향이다.
		const FQuat Twist(ActivePelvisHit.Up, FMath::DegreesToRadians(-CVarPhysHitReactPelvisTwist.GetValueOnGameThread() * PelvisHitSide * Envelope));
		Target.TargetOrientation = (Twist * Lean).Rotator();
		HitKinematicTargets.Targets.Add(TEXT("pelvis"), Target);
	}

	HitControlParameters.ControlMultiplierParameters.Reset();
	if ((Mode & (Mode_Soften | Mode_SpineSoften)) && PhysicsAlpha > 0.f)
	{
		FPhysicsControlSparseMultiplier Multiplier;
		Multiplier.AngularStrengthMultiplier = FMath::Lerp(1.f, CVarPhysHitReactSoftenStrength.GetValueOnGameThread(), PhysicsAlpha);
		if (Mode & Mode_Soften)
			HitControlParameters.ControlMultiplierParameters.Emplace(SoftenControlSet, Multiplier);
		else
		{
			const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
			const UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
			if (PhysicsAsset)
				for (const TPair<FName, FName>& BodyPair : SpineBodyPairs)
				{
					const int32 ParentBodyIndex = PhysicsAsset->FindBodyIndex(BodyPair.Key);
					if (ParentBodyIndex == INDEX_NONE || PhysicsAsset->FindBodyIndex(BodyPair.Value) == INDEX_NONE)
						continue;
					// 바디가 둘 다 있어도 실제 부모가 다르면 해당 이름의 컨트롤은 생성되지 않는다.
					if (PhysicsAsset->FindParentBodyIndex(Mesh->GetSkeletalMeshAsset(), Mesh->GetBoneIndex(BodyPair.Value)) != ParentBodyIndex)
						continue;
					const FName ControlName(*(BodyPair.Key.ToString() + TEXT("_") + BodyPair.Value.ToString()));
					HitControlParameters.ControlMultiplierParameters.Emplace(ControlName, Multiplier);
				}
		}
	}
}

// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_MeleeAttack.h"
#include "Animation/ANS_LNPMeleeHitWindow.h"
#include "Camera/LNPLockOnComponent.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPWeaponData.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPModifierInputs.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"

namespace LNPMeleeAssist
{
	/** 워프 타겟 이름. 모디파이어와 컴포넌트가 이 이름으로 서로를 찾는다. */
	static const FName WarpTargetName(TEXT("LNPMeleeAssist"));

	static TAutoConsoleVariable<float> CVarStrength(
		TEXT("LNP.Melee.Assist.Strength"), -1.0f,
		TEXT("Melee attack assist strength override (0..1). Negative uses the project setting."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarForceMode(
		TEXT("LNP.Melee.Assist.ForceMode"), -1,
		TEXT("Force a melee assist path. -1: auto (by window root motion), 0: layered move pull, 1: motion warping."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarDebug(
		TEXT("LNP.Melee.Assist.Debug"), 0,
		TEXT("Draw the melee attack assist target and warp destination. 0: off, 1: on"),
		ECVF_Cheat);

	/**
	 * 보정 강도(0~1)를 읽는 단일 지점.
	 * 사용자 환경설정 UI가 생기면 여기서 출처만 바꾸면 된다.
	 */
	static float GetStrength()
	{
		const float Override = CVarStrength.GetValueOnGameThread();
		if (Override >= 0.f)
		{
			return FMath::Clamp(Override, 0.f, 1.f);
		}
		return FMath::Clamp(GetDefault<ULNPSettings>()->MeleeAssistStrength, 0.f, 1.f);
	}

	/**
	 * 구형 월드에서 거리·각도는 반드시 접평면 성분으로만 잰다.
	 * 반지름 방향 성분이 섞이면 같은 높이에 있지 않은 대상의 거리가 실제보다 멀게 나오고,
	 * 워프 지점도 지면에서 떠버린다. (TechDesign_EnemyNPC.md 5.1과 같은 규약)
	 */
	static bool ProjectToTangent(const FVector& UpDir, const FVector& Delta, FVector& OutDir, float& OutDist)
	{
		const FVector Tangent = Delta - UpDir * FVector::DotProduct(Delta, UpDir);
		OutDist = Tangent.Size();
		if (OutDist <= KINDA_SMALL_NUMBER)
		{
			OutDir = FVector::ZeroVector;
			return false;
		}
		OutDir = Tangent / OutDist;
		return true;
	}

	static bool IsTargetDead(const ALNPEnemyCharacter* Enemy)
	{
		const UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
		if (!ASC)
		{
			return false;
		}
		bool bFound = false;
		const float Health = ASC->GetGameplayAttributeValue(ULNPBaseAttributeSet::GetHealthAttribute(), bFound);
		return bFound && Health <= 0.f;
	}

	/**
	 * 락온이 꺼져 있을 때의 타겟 탐색. 캐릭터 전방 기준 각도와 접평면 거리를 정규화해 가중합한다.
	 * 브로드페이즈는 ULNPLockOnComponent::FindBestTarget과 같은 방식을 쓴다 — 다만 저쪽은
	 * 화면(카메라) 중앙 기준이고 여기는 캐릭터 전방 기준이라 점수 함수만 다르다.
	 */
	static ALNPEnemyCharacter* FindForwardTarget(const ALNPCharacterBase* Character, const FVector& UpDir, const ULNPSettings& Settings)
	{
		UWorld* World = Character->GetWorld();
		if (!World)
		{
			return nullptr;
		}

		const float Radius = FMath::Max(Settings.MeleeAssistSearchRadius, 1.f);
		const float MaxAngleDeg = FMath::Max(Settings.MeleeAssistMaxSearchAngleDeg, 1.f);
		const FVector SelfLoc = Character->GetActorLocation();

		FVector ForwardDir;
		float ForwardLen = 0.f;
		if (!ProjectToTangent(UpDir, Character->GetActorForwardVector(), ForwardDir, ForwardLen))
		{
			return nullptr;
		}

		TArray<AActor*> Overlapped;
		UKismetSystemLibrary::SphereOverlapActors(
			World,
			SelfLoc,
			Radius,
			TArray<TEnumAsByte<EObjectTypeQuery>>{ UEngineTypes::ConvertToObjectType(ECC_Pawn) },
			ALNPEnemyCharacter::StaticClass(),
			TArray<AActor*>{ const_cast<ALNPCharacterBase*>(Character) },
			Overlapped);

		ALNPEnemyCharacter* Best = nullptr;
		float BestScore = -MAX_FLT;

		for (AActor* Actor : Overlapped)
		{
			ALNPEnemyCharacter* Enemy = Cast<ALNPEnemyCharacter>(Actor);
			if (!IsValid(Enemy) || IsTargetDead(Enemy))
			{
				continue;
			}

			FVector ToDir;
			float Dist = 0.f;
			if (!ProjectToTangent(UpDir, Enemy->GetActorLocation() - SelfLoc, ToDir, Dist) || Dist > Radius)
			{
				continue;
			}

			const float CosAngle = FMath::Clamp(FVector::DotProduct(ForwardDir, ToDir), -1.f, 1.f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
			if (AngleDeg > MaxAngleDeg)
			{
				continue;
			}

			const float Score = Settings.MeleeAssistAngleWeight * (1.f - AngleDeg / MaxAngleDeg)
			                  + Settings.MeleeAssistDistanceWeight * (1.f - Dist / Radius);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Enemy;
			}
		}

		return Best;
	}

	/**
	 * 한 콤보 섹션의 보정 타이밍. 시각은 전부 몽타주 타임라인 기준(초)이라 재생 속도와 무관하다.
	 *
	 * 두 경로가 서로 다른 구간을 쓴다.
	 * - Motion Warping: 루트모션이 실린 구간(Warp*). 스케일할 원본이 그 구간에만 있기 때문이다.
	 * - LayeredMove: [섹션 시작, 첫 히트윈도우](PullEnd). 선딜에 끌어당겨 칼날이 살아날 때 정렬을 끝낸다.
	 */
	struct FAssistTiming
	{
		float SectionStart = 0.f;

		/** Motion Warping 경로 — 섹션 안에서 루트모션 순 이동량이 최대인 구간. */
		float WarpStart = 0.f;
		float WarpEnd = 0.f;
		/** 그 구간의 루트모션 순 이동량 (cm). 경로 선택의 기준값이다. */
		float WarpRootMotion = 0.f;

		/** LayeredMove 경로 — 끌어당김이 끝나는 시각. 구간은 [SectionStart, PullEnd]. */
		float PullEnd = 0.f;

		bool bValid = false;
	};

	/** 타이밍 캐시의 키. 세 요소 모두 정적 데이터라 결과도 정적이다. */
	struct FAssistTimingKey
	{
		TWeakObjectPtr<const UAnimMontage> Montage;
		FName SectionName;
		/** 부동소수 키는 해시가 불안정하므로 밀리초 정수로 양자화한다. */
		int32 WindowMs = 0;

		bool operator==(const FAssistTimingKey& Other) const
		{
			return Montage == Other.Montage && SectionName == Other.SectionName && WindowMs == Other.WindowMs;
		}

		friend uint32 GetTypeHash(const FAssistTimingKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.Montage), GetTypeHash(Key.SectionName)),
				static_cast<uint32>(Key.WindowMs));
		}
	};

	/**
	 * 섹션의 보정 타이밍을 구한다. 어빌리티 활성화(게임 스레드)에서만 불리고 결과는 캐시된다.
	 *
	 * **워프 구간을 이동량 기준으로 고르는 이유 (2026-08-30 실측).** 스큐 워프는 창 구간의 루트모션을
	 * 스케일해 타겟에 맞춘다. 창에 이동량이 없으면 `URootMotionModifier_SkewWarp::ProcessRootMotion`이
	 * 전혀 다른 경로(`StartTransform`에서 타겟까지 Lerp)로 빠지는데, 그 경로에는 MaxSpeedClampRatio
	 * 클램프가 **없다.** 막혀서 못 나아가면 한 프레임에 보정 거리 전체가 속도가 되어 캐릭터가 날아간다.
	 * 이동량이 실린 구간을 고르면 클램프가 있는 정상 경로만 타게 된다.
	 */
	static FAssistTiming ComputeAssistTiming(const UAnimMontage* Montage, FName SectionName, float WindowSeconds)
	{
		check(IsInGameThread());

		FAssistTiming Result;

		if (!Montage)
		{
			return Result;
		}

		static TMap<FAssistTimingKey, FAssistTiming> TimingCache;
		const FAssistTimingKey Key{ Montage, SectionName, FMath::RoundToInt(WindowSeconds * 1000.f) };
		if (const FAssistTiming* Cached = TimingCache.Find(Key))
		{
			return *Cached;
		}

		int32 SectionIdx = SectionName.IsNone() ? 0 : Montage->GetSectionIndex(SectionName);
		if (SectionIdx == INDEX_NONE)
		{
			SectionIdx = 0;
		}

		float SectionStart = 0.f;
		float SectionEnd = 0.f;
		Montage->GetSectionStartAndEndTime(SectionIdx, SectionStart, SectionEnd);

		const float SectionLength = SectionEnd - SectionStart;
		if (SectionLength <= KINDA_SMALL_NUMBER)
		{
			// 저작 오류로 보고 캐시하지 않는다 — 에셋을 고치면 재시작 없이 반영되게 한다.
			return Result;
		}

		// --- LayeredMove 구간: [섹션 시작, 그 섹션의 첫 ANS_LNPMeleeHitWindow 시작] ---
		float HitWindowStart = -1.f;
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (!Cast<UANS_LNPMeleeHitWindow>(NotifyEvent.NotifyStateClass))
			{
				continue;
			}
			const float TriggerTime = NotifyEvent.GetTriggerTime();
			if (TriggerTime <= SectionStart || TriggerTime >= SectionEnd)
			{
				continue;
			}
			if (HitWindowStart < 0.f || TriggerTime < HitWindowStart)
			{
				HitWindowStart = TriggerTime;
			}
		}
		Result.PullEnd = (HitWindowStart > 0.f) ? HitWindowStart : FMath::Min(SectionStart + WindowSeconds, SectionEnd);

		// --- Motion Warping 구간: 루트모션 순 이동량이 최대인 구간 ---
		const float Window = FMath::Min(FMath::Max(WindowSeconds, 1.f / 60.f), SectionLength);
		const float Step = 1.f / 30.f;

		// 구간별 델타를 미리 더해 쓰지 않는 이유는 루트모션 누적이 벡터 합이 아니라
		// 트랜스폼 합성이라 회전이 섞이면 값이 달라지기 때문이다.
		float BestStart = SectionStart;
		float BestTranslation = -1.f;
		for (float Cursor = SectionStart; Cursor <= SectionEnd - Window + KINDA_SMALL_NUMBER; Cursor += Step)
		{
			const float Translation = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(
				Montage, Cursor, Cursor + Window).GetTranslation().Size();
			if (Translation > BestTranslation)
			{
				BestTranslation = Translation;
				BestStart = Cursor;
			}
		}

		Result.SectionStart = SectionStart;
		Result.WarpStart = BestStart;
		Result.WarpEnd = BestStart + Window;
		Result.WarpRootMotion = FMath::Max(BestTranslation, 0.f);
		Result.bValid = (Result.PullEnd > SectionStart + KINDA_SMALL_NUMBER);

		TimingCache.Add(Key, Result);
		return Result;
	}
}

float ULNPAbility_MeleeAttack::GetKnockbackForCombo(int32 ComboIdx) const
{
	if (ComboKnockbackStrengths.IsValidIndex(ComboIdx))
		return ComboKnockbackStrengths[ComboIdx];
	return KnockbackStrength;
}

float ULNPAbility_MeleeAttack::GetPoiseDamageForCombo(int32 ComboIdx) const
{
	if (ComboPoiseDamages.IsValidIndex(ComboIdx))
		return ComboPoiseDamages[ComboIdx];
	return PoiseDamage;
}

void ULNPAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ALNPCharacterBase* Character = GetOwningCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* AttackMontage = Character->EvaluateMontage(TAG_Montage_Situation_Attack);
	if (!AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 ComboIdx = Character->GetCurrentComboIndex();
	FName SectionName = NAME_None;
	if (ComboIdx > 0)
		SectionName = FName(FString::Printf(TEXT("Section_%d"), ComboIdx + 1));

	// 몽타주가 확정된 뒤, 재생을 시작하기 전에 건다. 워프 창은 몽타주 시각 기준이라
	// 레이어드 무브와 몽타주가 같은 프레임에 시작해야 두 타임라인이 어긋나지 않는다.
	ApplyMeleeAssist(Character, AttackMontage, SectionName);

	// AttackSpeed를 재생 속도로 — 몽타주에 붙은 ANS(히트 윈도우·입력 차단 구간)도 함께 압축되는 것이 의도된 동작이다.
	if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, GetAttackSpeed(), SectionName))
	{
		Task->OnCompleted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnBlendOut.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnInterrupted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->ReadyForActivation();
	}
}

void ULNPAbility_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 몽타주 콜백 4종이 아니라 여기서 정리한다 — CommitAbility 실패 같은 조기 종료 경로까지 한 곳에서 덮인다.
	// 회전 보정이 남으면 캐릭터가 계속 타겟을 바라본 채로 굳는다.
	ClearMeleeAssist();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULNPAbility_MeleeAttack::ApplyMeleeAssist(ALNPCharacterBase* Character, UAnimMontage* Montage, FName SectionName)
{
	// 적 NPC는 대상이 아니다 — StateTree 스티어링과 ComputeStopDistance가 이미 접근 거리를 맞춘다.
	// ULNPLockOnComponent를 가진 쪽이 플레이어 캐릭터다. (컴포넌트를 보유 판정에만 쓴다 —
	// 락온 타겟 자체는 로컬 상태라 서버가 못 보므로 아래에서 InputCmd로 읽는다.)
	if (!Character->FindComponentByClass<ULNPLockOnComponent>())
	{
		return;
	}

	UMotionWarpingComponent* WarpComp = Character->GetMotionWarpingComponent();
	ULNPCharacterMoverComponent* Mover = Character->GetMoverComponent();
	ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>();
	if (!WarpComp || !Mover || !InputHandler)
	{
		return;
	}

	const float Strength = LNPMeleeAssist::GetStrength();
	if (Strength <= 0.f)
	{
		return;
	}

	const ULNPSettings& Settings = *GetDefault<ULNPSettings>();
	const FVector UpDir = Character->GetUpDirection();
	const FVector SelfLoc = Character->GetActorLocation();

	// 락온 타겟과 이동 인풋은 둘 다 InputCmd에서 읽는다. 컴포넌트의 로컬 상태를 읽으면 서버가
	// 원격 클라이언트의 값을 보지 못해 서버만 다른 판단을 내린다.
	const FMoverInputCmdContext& LastInputCmd = Mover->GetLastInputCmd();
	const FLNPModifierInputs* ModifierInputs = LastInputCmd.InputCollection.FindDataByType<FLNPModifierInputs>();

	// 락온은 "자동 탐색이 고른 것 말고 이 적을 치겠다"는 명시적 의사표현이다 — 지목이 있으면 탐색하지 않는다.
	const AActor* LockOnTarget = (ModifierInputs && IsValid(ModifierInputs->LockOnTarget)) ? ModifierInputs->LockOnTarget.Get() : nullptr;
	const bool bLockOnActive = (LockOnTarget != nullptr);
	const AActor* Target = bLockOnActive
		? LockOnTarget
		: static_cast<const AActor*>(LNPMeleeAssist::FindForwardTarget(Character, UpDir, Settings));
	if (!Target)
	{
		return;
	}

	FVector ToTargetDir;
	float TangentDist = 0.f;
	if (!LNPMeleeAssist::ProjectToTangent(UpDir, Target->GetActorLocation() - SelfLoc, ToTargetDir, TangentDist))
	{
		return;
	}
	if (TangentDist > Settings.MeleeAssistSearchRadius)
	{
		return;
	}

	// 회전 보정. 락온 중에는 카메라가 이미 타겟을 추적하고 캐릭터는 카메라 정면을 보므로 넣지 않는다.
	if (!bLockOnActive)
	{
		InputHandler->SetMeleeAssistOrientation(ToTargetDir);
	}

	// 이동 인풋이 들어오고 있으면 위치 보정을 건너뛴다 — 이동 인풋이 무조건 우선이고, 회전 보정만 남는다.
	// 판정은 InputCmd에서 읽는다. ULNPInputHandlerComponent::HasMovementInput()의 CachedMoveInputIntent는
	// 로컬 전용이라 서버가 원격 클라이언트의 값을 보지 못하고, 그러면 서버만 보정을 걸어 위치가 어긋난다.
	const FCharacterDefaultInputs* LastInputs = LastInputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (LastInputs && !LastInputs->GetMoveInput().IsNearlyZero())
	{
		return;
	}

	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	const float IdealDistance = WeaponDef ? WeaponDef->MeleeIdealDistance : 0.f;
	if (IdealDistance <= 0.f)
	{
		// 이 무기는 위치 보정을 쓰지 않는다.
		return;
	}

	// 이미 이상 거리 안이면 Gap이 0이라 보정량도 0 — 겹쳐 박히지 않는다.
	const float Gap = TangentDist - IdealDistance;
	if (Gap <= 0.f)
	{
		return;
	}
	const float CorrectionDistance = FMath::Min(Gap * Strength, Settings.MeleeAssistMaxCorrectionDistance);
	if (CorrectionDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const LNPMeleeAssist::FAssistTiming Timing = LNPMeleeAssist::ComputeAssistTiming(
		Montage, SectionName, Settings.MeleeAssistWarpWindowSeconds);

	// 경로 선택. "어느 쪽이 나은가"가 아니라 **이 애니메이션에 워프할 이동량이 있는가**가 기준이다.
	//
	// 루트모션은 애니메이터가 의도한 캐릭터 이동 그 자체다. 이동량이 실려 있으면 그것을 스케일하는
	// Motion Warping이 가장 자연스럽고, in-place로 만든 애니메이션(= "움직이지 않는 것이 자연스럽다"는
	// 의도)에는 스케일할 원본이 없으므로 코드가 만든 속도를 얹는 LayeredMove로 간다.
	const int32 ForcedMode = LNPMeleeAssist::CVarForceMode.GetValueOnGameThread();
	const bool bUseMotionWarping = (ForcedMode >= 0)
		? (ForcedMode > 0)
		: (Timing.WarpRootMotion >= Settings.MeleeAssistMinWindowRootMotion);

	// 워프 지점은 캐릭터의 **시각 컴포넌트 위치(발밑)** 기준이다.
	//
	// 엔진이 현재 위치로 쓰는 값이 UMotionWarpingBaseAdapter::GetVisualRootLocation()이고,
	// Mover 어댑터에서 그것은 PrimaryVisualComponent(= AnimSourceMesh)의 월드 위치다.
	// 여기에 GetActorLocation()(캡슐 중심)을 넘기면 캡슐 반높이만큼의 수직 성분이 상시로 끼어
	// 캐릭터를 위로 밀어 올리고, 지면을 떠나 Falling으로 전환되면서 멀리 날아간다(2026-08-30 실측).
	// bWarpToFeetLocation = true와 짝이다 — 그래야 정상 경로도 같은 기준점을 쓴다.
	//
	// ToTargetDir은 접평면 단위벡터라 더해도 반지름 높이가 그대로 유지된다.
	// (모디파이어의 bIgnoreZAxis는 월드 Z 기준이라 구면 중력에서는 쓸 수 없어 끄고, 투영은 여기서 직접 한다.)
	const USceneComponent* VisualComp = Mover->GetPrimaryVisualComponent();
	const FVector WarpOrigin = VisualComp ? VisualComp->GetComponentLocation() : SelfLoc;
	const FVector WarpLocation = WarpOrigin + ToTargetDir * CorrectionDistance;

	const float PlayRate = FMath::Max(GetAttackSpeed(), KINDA_SMALL_NUMBER);

#if !UE_BUILD_SHIPPING
	if (LNPMeleeAssist::CVarDebug.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogLootNPop, Log,
			TEXT("[MeleeAssist] section=%s mode=%s valid=%d | warp=[%.3f %.3f] rootMotion=%.2fcm | pull=[%.3f %.3f] | target=%s lockOn=%d dist=%.1f ideal=%.1f correction=%.1f"),
			SectionName.IsNone() ? TEXT("Section_1") : *SectionName.ToString(),
			bUseMotionWarping ? TEXT("Warp") : TEXT("Pull"), Timing.bValid ? 1 : 0,
			Timing.WarpStart, Timing.WarpEnd, Timing.WarpRootMotion,
			Timing.SectionStart, Timing.PullEnd,
			*GetNameSafe(Target), bLockOnActive ? 1 : 0, TangentDist, IdealDistance, CorrectionDistance);
	}
#endif

	if (!Timing.bValid)
	{
		return;
	}

	if (bUseMotionWarping)
	{
		WarpComp->AddOrUpdateWarpTargetFromLocation(LNPMeleeAssist::WarpTargetName, WarpLocation);

		// 창이 끝나는 시점에 레이어드 무브가 먼저 끝나 버려서, 엔진의 모디파이어 제거 조건
		// (URootMotionModifier::Update의 PreviousPosition >= EndTime)이 성립하지 않는다.
		// 그대로 두면 공격 한 번마다 모디파이어가 하나씩 쌓이므로, 같은 (몽타주, 창)이 이미 있으면 재사용한다.
		// 재사용해도 결과는 같다 — 스큐 워프가 쓰는 값은 매 프레임 갱신되는 재생 위치와 워프 타겟이다.
		if (!WarpComp->ContainsModifier(Montage, Timing.WarpStart, Timing.WarpEnd))
		{
			ULNPMeleeAssistWarpModifier* Modifier = NewObject<ULNPMeleeAssistWarpModifier>(WarpComp);
			Modifier->Animation = Montage;
			Modifier->StartTime = Timing.WarpStart;
			Modifier->EndTime = Timing.WarpEnd;
			Modifier->WarpTargetName = LNPMeleeAssist::WarpTargetName;
			Modifier->bWarpTranslation = true;
			Modifier->bIgnoreZAxis = false;
			// 워프 타겟을 시각 컴포넌트(발밑) 기준으로 잡았으므로 엔진도 같은 기준을 쓰게 한다.
			Modifier->bWarpToFeetLocation = true;
			// 회전은 OrientationIntent가 담당한다 — 사유는 ULNPInputHandlerComponent::SetMeleeAssistOrientation 주석 참조.
			Modifier->bWarpRotation = false;
			Modifier->SetMaxSpeedClampRatio(Settings.MeleeAssistMaxSpeedClampRatio);
			WarpComp->AddModifier(Modifier);
		}

		// 워프는 Mover를 통과하는 루트모션만 수정한다. 이 레이어드 무브가 없으면
		// ConvertLocalRootMotionToWorld 자체가 불리지 않아 모디파이어가 놀게 된다.
		//
		// ⚠️ 시작점은 창이 아니라 **섹션 시작**이다. 레이어드 무브는 StartingMontagePosition에서 출발해
		// 자체 시계로 진행하는데, 몽타주는 섹션 시작부터 재생되므로 창 시작을 넣으면 두 타임라인이
		// 어긋나 스윙 구간의 루트모션이 선딜에 적용된다. 창 밖 구간은 모디파이어가 비활성이라
		// 애니메이션 원본 루트모션이 그대로 통과한다 — 애니메이터의 의도대로다.
		TSharedPtr<FLayeredMove_AnimRootMotion> WarpMove = MakeShared<FLayeredMove_AnimRootMotion>();
		WarpMove->MontageState.Montage = Montage;
		WarpMove->MontageState.PlayRate = PlayRate;
		// 재생 중인 몽타주 인스턴스에서 위치를 되읽지 않는다 — 서버와 리시뮬레이션에는 몽타주가 없어 값이 갈린다.
		WarpMove->MontageState.StartingMontagePosition = Timing.SectionStart;
		WarpMove->MontageState.CurrentPosition = Timing.SectionStart;
		// 몽타주는 PlayRate배로 흐르므로, 구간 길이를 실제 시뮬레이션 시간으로 환산한다.
		WarpMove->DurationMs = (Timing.WarpEnd - Timing.SectionStart) / PlayRate * 1000.f;
		// 이동 모드의 제안 위에 얹는다. Override로 두면 보정이 이동을 통째로 대체해 버린다.
		WarpMove->MixMode = EMoveMixMode::AdditiveVelocity;
		Mover->QueueLayeredMove(WarpMove);
	}
	else
	{
		// LayeredMove 경로 — 보정을 "제한 시간 동안 제한된 속도를 더하는 것"으로 표현한다.
		// 총 이동량이 구조적으로 CorrectionDistance를 넘을 수 없고, 장애물에 막히면 Mover가 충돌을
		// 풀면서 그냥 안 움직인다. 워프처럼 "무슨 수를 써서라도 그 점에 도달"하지 않으므로 발산하지 않는다.
		const float PullSeconds = FMath::Max((Timing.PullEnd - Timing.SectionStart) / PlayRate, KINDA_SMALL_NUMBER);

		// 시간이 짧아 요구 속도가 상한을 넘으면 덜 당긴다 — 속도를 넘기지 않는 쪽을 택한다.
		const float PullSpeed = FMath::Min(CorrectionDistance / PullSeconds, Settings.MeleeAssistMaxPullSpeed);

		TSharedPtr<FLayeredMove_LinearVelocity> PullMove = MakeShared<FLayeredMove_LinearVelocity>();
		PullMove->Velocity = ToTargetDir * PullSpeed;
		PullMove->DurationMs = PullSeconds * 1000.f;
		// 이동 모드의 제안 위에 얹는다. Override로 두면 보정이 이동을 통째로 대체해 버린다.
		PullMove->MixMode = EMoveMixMode::AdditiveVelocity;
		Mover->QueueLayeredMove(PullMove);
	}

#if !UE_BUILD_SHIPPING
	if (LNPMeleeAssist::CVarDebug.GetValueOnGameThread() > 0)
	{
		if (UWorld* World = Character->GetWorld())
		{
			DrawDebugSphere(World, Target->GetActorLocation(), 45.f, 12, FColor::Yellow, false, 2.f);
			DrawDebugSphere(World, WarpLocation, 20.f, 12, FColor::Green, false, 2.f);
			DrawDebugLine(World, WarpOrigin, WarpLocation, FColor::Green, false, 2.f, 0, 2.f);
		}
	}
#endif
}

void ULNPAbility_MeleeAttack::ClearMeleeAssist()
{
	ALNPCharacterBase* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	if (UMotionWarpingComponent* WarpComp = Character->GetMotionWarpingComponent())
	{
		WarpComp->RemoveWarpTarget(LNPMeleeAssist::WarpTargetName);
	}
	if (ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>())
	{
		InputHandler->ClearMeleeAssistOrientation();
	}
}

void ULNPAbility_MeleeAttack::OnMontageEnded()
{
	ClearRelativeTag();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULNPAbility_MeleeAttack::OnMontageInterrupted()
{
	ClearRelativeTag();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULNPAbility_MeleeAttack::ClearRelativeTag()
{
	if (ALNPCharacterBase* Character = GetOwningCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(TAG_Block_AttackInput))
				ASC->RemoveLooseGameplayTag(TAG_Block_AttackInput);
			if (ASC->HasMatchingGameplayTag(TAG_State_ComboWindow))
				ASC->RemoveLooseGameplayTag(TAG_State_ComboWindow);
		}
	}
}

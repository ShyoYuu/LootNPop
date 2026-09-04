// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassActorSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "HitDetection/LNPProjectileImpactContext.h"
#include "GAS/LNPPoiseTypes.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

namespace LNPHitDetection
{
	/** LNP 캐릭터 Actor의 ASC를 반환한다. 캐릭터가 아니거나 무효하면 null. */
	inline UAbilitySystemComponent* GetASC(AActor* Actor)
	{
		if (!IsValid(Actor))
			return nullptr;
		ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Actor);
		return Character ? Character->GetAbilitySystemComponent() : nullptr;
	}

	/** 피격 판정용 캡슐 치수. 컴포넌트가 없으면 ALNPCharacterBase 기본 캡슐(42, 96)로 폴백한다. */
	inline void GetCapsuleSize(const UCapsuleComponent* Capsule, float& OutHalfHeight, float& OutRadius)
	{
		OutHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
		OutRadius     = Capsule ? Capsule->GetScaledCapsuleRadius()     : 42.f;
	}

	/**
	 * 적 엔티티의 판정 캡슐 중심을 좌표 규약에 맞게 되돌린다.
	 *
	 * Actor가 붙어 있는 구간(High LOD)에서는 MassAgentCapsuleCollisionSyncTrait(ActorToMass)가
	 * **캡슐 컴포넌트 Transform을 그대로** Fragment에 넣으므로 Transform 위치가 이미 캡슐 중심이다.
	 * 여기서 보정을 한 번 더 하면 판정 캡슐이 HalfHeight만큼 떠올라 몸통 아래 절반이 관통한다.
	 * Actor가 없는 순수 엔티티 경로만 표면점이 들어오므로 그때만 중심으로 올린다.
	 * (규약 원본: ULNPEnemyMovementProcessor의 "엔티티 Transform의 기준점은 캡슐 중심이다" 주석)
	 *
	 * ⚠️ 근접·원거리·디버그 드로우가 모두 이 함수 하나만 쓴다. 같은 분기를 다시 복제하지 말 것 —
	 *    복제돼 있던 시절 원거리 판정 두 곳이 서로 반대 방향으로 틀어져 있었다.
	 */
	inline FVector ResolveEnemyCapsuleCenter(const FVector& EntityLocation, const FVector& UpDir,
		float CapsuleHalfHeight, const AActor* EnemyActor)
	{
		return Cast<ALNPCharacterBase>(EnemyActor)
			? EntityLocation
			: EntityLocation + UpDir * CapsuleHalfHeight;
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// 근접 패링
// ──────────────────────────────────────────────────────────────────────────────

/** 근접 공격 패링 성공 시 발동. 방어자 GA_ParrySuccess + 공격자 GA_Stagger 이벤트 전달. */
struct FLNPMeleeParryCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor> VictimActor;
		FMassEntityHandle      AttackerEntity;
		FVector                ImpactPoint;   // 무기가 맞부딪힌 지점 (월드)
		FVector                ImpactNormal;  // 피격자 → 공격자 방향 (HitFromDirection과 동일 컨벤션)
	};

	FLNPMeleeParryCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, FMassEntityHandle InAttacker, const FVector& InImpactPoint, const FVector& InImpactNormal)
	{
		Entries.Add({ InVictim, InAttacker, InImpactPoint, InImpactNormal });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		UMassActorSubsystem* ActorSub = EntityManager.GetWorld()
			? EntityManager.GetWorld()->GetSubsystem<UMassActorSubsystem>() : nullptr;

		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.VictimActor.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Entry.ImpactPoint;
			CueParams.Normal   = Entry.ImpactNormal;
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			FGameplayEventData EventData;
			EventData.Target     = Victim;
			EventData.Instigator = Attacker;
			VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);

			// 패링 보상은 경직도로 준다 — 전용 스태거 GA를 따로 돌리지 않는다.
			// 두 경로를 병행하면 GA가 먼저 끝나면서 게이지는 아직 T1 위인데 행동이 풀려 그로기가 조용히 깨진다.
			// 경직 시스템에 태우면 지속 시간이 게이지에서 나오고, 이어서 때려 다운까지 밀어붙일 수도 있다.
			if (Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
			{
				LNPPoise::ApplyParryBreak(
					EntityManager.GetFragmentDataPtr<FLNPPoiseFragment>(Entry.AttackerEntity),
					EntityManager.GetWorld() ? EntityManager.GetWorld()->GetTimeSeconds() : 0.0);
			}

			if (ALNPCharacterBase* AttackerPawn = Cast<ALNPCharacterBase>(Attacker))
			{
				// 피격 리액션 몽타주는 얹지 않는다 — 같은 프레임에 경직 진입이 잡히면서
				// FLNPStaggerCommand가 Montage_Stop으로 즉시 끊고 경직 몽타주로 갈아탄다.
				constexpr float DirectionWeight = 0.7f;
				constexpr float UpWeight        = 0.3f;

				const FVector AwayDir      = (AttackerPawn->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal();
				const FVector KnockbackDir = (AwayDir * DirectionWeight + AttackerPawn->GetUpDirection() * UpWeight).GetSafeNormal();
				AttackerPawn->ApplyKnockback(KnockbackDir, 2000.0f);
			}

			if (ALNPCharacterBase* VictimPawn = Cast<ALNPCharacterBase>(Victim))
			{
				VictimPawn->PlayMontage(TAG_Montage_Situation_ParrySuccess, TAG_Montage_Value_Parry_Parrier);
			}

			UE_LOG(LogLootNPop, Log, TEXT("[Parry] Melee parry success"));
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// ──────────────────────────────────────────────────────────────────────────────
// 투사체 패링
// ──────────────────────────────────────────────────────────────────────────────

/** 투사체 패링 성공 시 발동. 방어자 GA_ParrySuccess 이벤트 + VFX 담당 + 반사 발사체 재스폰 방송.
 *  서버 권위 엔티티의 Velocity/InstigatorTeam 반전과 식별자 재발급은 Processor에서 이미 처리 —
 *  여기서는 "구 Ghost 소멸 + 새 Ghost 스폰"을 전 클라이언트에 방송만 한다 (섹션 5.2 반사 개정). */
struct FLNPProjectileParryCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>       Victim;
		FLNPProjectileSharedFragment SharedData;        // 재스폰 Ghost의 아키타입 구성용
		FVector                      SpawnPos;          // 서버 확정 반사 지점 (반사 시점의 발사체 위치)
		FVector                      NewVelocity;
		float                        LifetimeRemaining;
		ELNPInstigatorTeam           NewTeam;
		int32                        OldInstigatorPlayerID;
		int32                        OldKeyOrSalvo;
		uint8                        OldSpawnIndex;
		int32                        NewInstigatorPlayerID;
		int32                        NewKeyOrSalvo;
		FVector                      ImpactPoint;   // 투사체가 튕겨나간 지점 (월드)
		FVector                      ImpactNormal;  // 피격자 → 공격자 방향 (투사체가 날아온 쪽)
	};

	FLNPProjectileParryCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(const FEntry& InEntry)
	{
		Entries.Add(InEntry);
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.Victim.Get();
			if (!IsValid(Victim))
				continue;

			if (UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim))
			{
				FGameplayCueParameters CueParams;
				CueParams.Location = Entry.ImpactPoint;
				CueParams.Normal   = Entry.ImpactNormal;
				VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

				FGameplayEventData EventData;
				EventData.Target = Victim;
				VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);
			}

			// 반사를 "구 Ghost 소멸 + 새 Ghost 스폰"으로 전 클라이언트에 재현한다.
			// 공격자 클라이언트가 오예측(패링을 모른 채 히트 판정)으로 구 Ghost를 이미 파괴했어도
			// 새 스폰으로 반사 발사체가 반드시 보인다. 스폰 경로가 발사 방송과 공용이라 Dead Reckoning도 함께 적용.
			if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(Victim))
				VictimChar->Multicast_RespawnReflectedGhost(Entry.SharedData, Entry.SpawnPos,
					Entry.NewVelocity, Entry.LifetimeRemaining, Entry.NewTeam,
					Entry.OldInstigatorPlayerID, Entry.OldKeyOrSalvo, Entry.OldSpawnIndex,
					Entry.NewInstigatorPlayerID, Entry.NewKeyOrSalvo);

			UE_LOG(LogLootNPop, Log, TEXT("[Parry] Projectile parry success"));
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// ──────────────────────────────────────────────────────────────────────────────
// 가드
// ──────────────────────────────────────────────────────────────────────────────

/** 가드 성공 시 발동. GameplayCue_Guard_Block VFX/SFX 실행. */
struct FLNPGuardBlockCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor> Victim;
		FVector                ImpactPoint;   // 무기·투사체가 막힌 지점 (월드)
		FVector                ImpactNormal;  // 피격자 → 공격자 방향
	};

	FLNPGuardBlockCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, const FVector& InImpactPoint, const FVector& InImpactNormal)
	{
		Entries.Add({ InVictim, InImpactPoint, InImpactNormal });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.Victim.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Entry.ImpactPoint;
			CueParams.Normal   = Entry.ImpactNormal;
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Guard_Block, CueParams);
			UE_LOG(LogLootNPop, Log, TEXT("[Guard] Block success"));
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// ──────────────────────────────────────────────────────────────────────────────
// 원거리 임팩트 큐
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 투사체가 캐릭터에 적중했을 때의 임팩트 GameplayCue 발동.
 *
 * 판정 Processor의 Execute()는 워커 Thread에서 실행될 수 있어 ASC를 직접 건드릴 수 없다.
 * Ghost 재조정에 필요한 토큰(PredictionKeyID/SpawnIndex)과 공격자 ID는 값으로만 실어 두고,
 * FLNPProjectileImpactContext 할당과 큐 실행은 게임 Thread인 Run()에서 수행한다.
 */
struct FLNPProjectileImpactCueCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>  Victim;
		TObjectPtr<ULNPVFXData> VFXData;
		FVector                 ImpactPoint;         // 적중 지점 (월드)
		FVector                 ImpactNormal;        // 피격자 → 적중 지점 방향
		int32                   PredictionKeyID    = 0;
		int32                   InstigatorPlayerID = INDEX_NONE;
		uint8                   SpawnIndex         = 0;
	};

	FLNPProjectileImpactCueCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, TObjectPtr<ULNPVFXData> InVFXData, const FVector& InImpactPoint, const FVector& InImpactNormal,
		int32 InPredictionKeyID, uint8 InSpawnIndex, int32 InInstigatorPlayerID)
	{
		Entries.Add({ InVictim, InVFXData, InImpactPoint, InImpactNormal, InPredictionKeyID, InInstigatorPlayerID, InSpawnIndex });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Entry.Victim.Get());
			if (!IsValid(VictimASC))
				continue;

			FLNPProjectileImpactContext* ImpactCtx = new FLNPProjectileImpactContext();
			ImpactCtx->PredictionKeyID    = Entry.PredictionKeyID;
			ImpactCtx->SpawnIndex         = Entry.SpawnIndex;
			ImpactCtx->InstigatorPlayerID = Entry.InstigatorPlayerID;
			ImpactCtx->VFXData            = Entry.VFXData;

			FGameplayCueParameters CueParams;
			CueParams.Location      = Entry.ImpactPoint;
			CueParams.Normal        = Entry.ImpactNormal;
			CueParams.EffectContext = FGameplayEffectContextHandle(ImpactCtx);
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Projectile_Impact, CueParams);
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// ──────────────────────────────────────────────────────────────────────────────
// 데미지
// ──────────────────────────────────────────────────────────────────────────────

/** 순수 데미지 적용. 판정 로직 없음. */
struct FLNPApplyDamageGECommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>       Victim;
		FMassEntityHandle            AttackerEntity;
		TSubclassOf<UGameplayEffect> EffectClass;
		float                        Damage;
		FVector                      HitFromDirection;
		FVector                      ImpactPoint;       // 무기·투사체가 실제로 닿은 지점 (월드)
		float                        KnockbackStrength;
		bool                         bIsMeleeHit;
	};

	FLNPApplyDamageGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, FMassEntityHandle InAttacker, TSubclassOf<UGameplayEffect> InEffectClass, float InDamage, FVector InHitFromDir, const FVector& InImpactPoint, float InKnockbackStrength = 0.f, bool bInIsMeleeHit = false)
	{
		Entries.Add({ InVictim, InAttacker, InEffectClass, InDamage, InHitFromDir, InImpactPoint, InKnockbackStrength, bInIsMeleeHit });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		UMassActorSubsystem* ActorSub = EntityManager.GetWorld()
			? EntityManager.GetWorld()->GetSubsystem<UMassActorSubsystem>() : nullptr;

		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.Victim.Get();
			UAbilitySystemComponent* ASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(ASC) || !Entry.EffectClass)
				continue;

			const float HpBefore = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());

			FGameplayEffectContextHandle Ctx  = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle    Spec = ASC->MakeOutgoingSpec(Entry.EffectClass, 1.0f, Ctx);
			if (!Spec.IsValid())
				continue;

			Spec.Data->SetSetByCallerMagnitude(TAG_GE_Data_Damage, Entry.Damage);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			const float HpAfter = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());
			if (0.f < HpBefore)
				UE_LOG(LogLootNPop, Log, TEXT("[GE] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, HpAfter, Entry.Damage);

			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			// 피격자 HitReact 몽타주 + HitStop은 GameplayCue를 통해 서버·전 클라이언트에 전파된다 (Run은 서버에서만 실행).
			FGameplayCueParameters CueParams;
			CueParams.Location = Entry.ImpactPoint;
			CueParams.Normal   = Entry.HitFromDirection;
			ASC->ExecuteGameplayCue(TAG_GameplayCue_Character_HitReact, CueParams);
			if (Entry.bIsMeleeHit)
				ASC->ExecuteGameplayCue(TAG_GameplayCue_Melee_Impact, CueParams);

			if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(Victim))
			{
				// Entry.HitFromDirection은 "피격자 → 공격자"(공격이 날아온 쪽) 방향이다 (PlayHitReact의 방향 판정 컨벤션).
				// 넉백은 그 반대, 즉 공격자로부터 밀려나는 방향으로 밀어야 하므로 부호를 반전한다.
				if (Entry.KnockbackStrength > 0.f)
					VictimChar->ApplyKnockback(-Entry.HitFromDirection, Entry.KnockbackStrength);
			}
			// 공격자 HitStop은 근접에서만 재생한다 — 원거리(총기류)는 물리적 충돌감이 없어 어색하다.
			// 공격자 본인 화면은 예측 경로(리슨서버 호스트는 아래 직접 호출, 원격 클라는 ApplyLocalHitFeedback)로 즉시 처리하고,
			// 제3자(구경꾼) 화면은 GameplayCue.LNP.Melee.AttackerHitStop으로 전파한다 — 핸들러가 로컬 컨트롤 여부로 중복을 걸러낸다.
			if (Entry.bIsMeleeHit)
			{
				if (ALNPCharacterBase* AttackerChar = Cast<ALNPCharacterBase>(Attacker))
					AttackerChar->ApplyHitStop(0.2f);

				if (UAbilitySystemComponent* AttackerASC = LNPHitDetection::GetASC(Attacker))
				{
					// 파라미터를 **하나도 싣지 않는다.** 이 큐의 핸들러는 대상 액터만 쓰고
					// (ULNPGameplayCueNotify_AttackerHitStop: 로컬 컨트롤 여부로 걸러 ApplyHitStop만 호출)
					// 파라미터를 읽지 않는다. FGameplayCueParameters는 설정된 필드만 직렬화하므로
					// 비워 두면 히트마다 FVector_NetQuantize10 하나가 전 연결에서 사라진다.
					AttackerASC->ExecuteGameplayCue(TAG_GameplayCue_Melee_AttackerHitStop, FGameplayCueParameters());
				}
			}
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

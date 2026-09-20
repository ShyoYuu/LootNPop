// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"        // FTransformFragment — 엔티티 넉백의 Up 축 유도
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassActorSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "HitDetection/LNPProjectileImpactContext.h"
#include "GAS/LNPPoiseTypes.h"
#include "Enemy/LNPEnemyMassTypes.h"     // FLNPEntityAttackFragment · FLNPEnemySharedFragment
#include "Enemy/LNPEnemyConfig.h"        // FLNPEntityAttackConfig::ParriedRecoveryTime
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
	 * **두 모드 모두 Transform이 이미 캡슐 중심이므로 보정하지 않는다.**
	 * - Actor 구간: `MassAgentCapsuleCollisionSyncTrait`(ActorToMass)가 캡슐 컴포넌트 Transform을 그대로 넣는다.
	 * - 순수 엔티티 구간: `ULNPEnemyMovementProcessor`가 `표면반지름 - CapsuleHalfHeight` 위치에 놓는다
	 *   (구 내벽이라 반지름 감소 방향이 Up이다).
	 *
	 * ⚠️ 과거 이 함수는 Actor가 없으면 `+Up*HalfHeight`를 더했다. 좌표 규약이 캡슐 중심으로 통일되기
	 *    전의 잔재이며, 통일 이후로는 **이중 보정**이라 판정 캡슐이 96cm 떠올랐다. 전투에 들어간 적이
	 *    예외 없이 Actor로 승격되던 동안에는 이 분기가 실전에서 거의 실행되지 않아 드러나지 않았고,
	 *    순수 엔티티(CombatMode::PureEntity)를 도입하면서 표면화됐다.
	 *
	 * ⚠️ 근접·원거리·디버그 드로우가 모두 이 함수 하나만 쓴다. 같은 분기를 다시 복제하지 말 것 —
	 *    복제돼 있던 시절 원거리 판정 두 곳이 서로 반대 방향으로 틀어져 있었다.
	 */
	inline FVector ResolveEnemyCapsuleCenter(const FVector& EntityLocation, const FVector& /*UpDir*/,
		float /*CapsuleHalfHeight*/, const AActor* /*EnemyActor*/)
	{
		return EntityLocation;
	}

	/**
	 * 선분(A→B)이 Center/UpDir/HalfHeight/CombinedRadius의 캡슐과 교차하면 true.
	 * Center는 캡슐 중심(바닥이 아닌 실린더 중앙) — ResolveEnemyCapsuleCenter가 돌려주는 값이다.
	 *
	 * OutHitPoint는 선분 위에서 캡슐 중심에 가장 가까운 점이다. 표면 진입점이 아니라 **몸통 깊이**를
	 * 가리키므로, 조준점을 여기로 수렴시키면 탄이 몸통 한가운데를 지난다.
	 *
	 * ⚠️ 원거리 판정과 조준 질의가 같은 함수를 쓴다. 복제하지 말 것 —
	 *    ResolveEnemyCapsuleCenter 주석의 사유가 그대로 적용된다.
	 */
	inline bool SegmentHitsCapsule(
		FVector A, FVector B,
		FVector Center, FVector UpDir,
		float   CapsuleHalfHeight, float CombinedRadius,
		FVector& OutHitPoint)
	{
		const FVector Closest      = FMath::ClosestPointOnSegment(Center, A, B);
		const FVector Delta        = Closest - Center;
		const float   Axial        = FVector::DotProduct(Delta, UpDir);
		const FVector RadialVec    = Delta - UpDir * Axial;
		const float   RadialDistSq = RadialVec.SizeSquared();

		if (FMath::Abs(Axial) <= CapsuleHalfHeight && RadialDistSq <= FMath::Square(CombinedRadius))
		{
			OutHitPoint = Closest;
			return true;
		}
		return false;
	}

	/**
	 * Actor 없는 적 엔티티에 넉백을 건다 — `ALNPCharacterBase::ApplyKnockback`의 엔티티판이다.
	 *
	 * ⚠️ **Up 성분이 반드시 섞여야 한다.** `ULNPEnemyMovementProcessor`의 공중 분기는 새 위치가
	 *    접지 반지름을 넘는 순간 표면에 스냅하고 속도를 0으로 만든다 — 순수 접평면 속도는
	 *    한 프레임 만에 흡수돼 아무것도 보이지 않는다.
	 *
	 * ⚠️ 방향 인자는 **"피격자 → 공격자"**(공격이 날아온 쪽)다. 밀리는 방향은 그 반대이므로
	 *    여기서 부호를 뒤집는다 — `FLNPApplyDamageGECommand`가 Mover에 넘길 때와 같은 규약이다.
	 */
	inline void ApplyEntityKnockback(FVector& InOutVelocity, const FVector& HitFromDirection,
		const FVector& UpDir, const float Strength)
	{
		if (Strength <= 0.f)
			return;

		// 근접 패링 넉백이 쓰던 가중을 그대로 쓴다 — 두 자리가 다른 곡선을 그릴 이유가 없다.
		constexpr float DirectionWeight = 0.7f;
		constexpr float UpWeight        = 0.3f;

		const FVector PushDir = (-HitFromDirection * DirectionWeight + UpDir * UpWeight).GetSafeNormal();
		InOutVelocity = PushDir * Strength;
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

			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			FGameplayEventData EventData;
			EventData.Target     = Victim;
			EventData.Instigator = Attacker;
			VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);

			// ⚠️ **큐를 이벤트보다 뒤에 낸다.** 방어자 리액션 몽타주를 내는 곳이 둘이라
			// (GA_ParrySuccess의 ReactionMontage / 큐의 Chooser 몽타주) 나중에 부른 쪽이 이긴다.
			// 순서를 뒤집으면 호스트에서 보이던 몽타주가 조용히 바뀐다.
			FGameplayCueParameters CueParams;
			CueParams.Location = Entry.ImpactPoint;
			CueParams.Normal   = Entry.ImpactNormal;
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

			// 패링 보상은 경직도로 준다 — 전용 스태거 GA를 따로 돌리지 않는다.
			// 두 경로를 병행하면 GA가 먼저 끝나면서 게이지는 아직 T1 위인데 행동이 풀려 그로기가 조용히 깨진다.
			// 경직 시스템에 태우면 지속 시간이 게이지에서 나오고, 이어서 때려 다운까지 밀어붙일 수도 있다.
			if (Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
			{
				LNPPoise::ApplyParryBreak(
					EntityManager.GetFragmentDataPtr<FLNPPoiseFragment>(Entry.AttackerEntity),
					EntityManager.GetWorld() ? EntityManager.GetWorld()->GetTimeSeconds() : 0.0);

				// 순수 엔티티에게 "패링당했다"를 알리는 유일한 자리. 경직(ApplyParryBreak)만으로는
				// 행동 상태가 Stagger로 나가 **일반 피격과 같은 그림**이 된다 — 연출이 갈리는 지점이므로
				// 여기서 별도 상태로 표시한다. 플레이어 공격자에게는 이 프래그먼트가 없어 자연히 건너뛴다.
				const FMassEntityView AttackerView(EntityManager, Entry.AttackerEntity);
				if (FLNPEntityAttackFragment* AttackFragment = AttackerView.GetFragmentDataPtr<FLNPEntityAttackFragment>())
				{
					const FLNPEnemySharedFragment* EnemyShared = AttackerView.GetConstSharedFragmentDataPtr<FLNPEnemySharedFragment>();
					const ULNPEnemyConfig* AttackerConfig = EnemyShared ? EnemyShared->Config.Get() : nullptr;
					AttackFragment->ParriedTimeRemaining = AttackerConfig
						? AttackerConfig->EntityAttackConfig.ParriedRecoveryTime
						: 1.f;
				}
			}

			// 넉백은 공격자를 뒤로 밀어낸다. 공격자가 Actor면 Mover로, 순수 엔티티면 속도
			// 프래그먼트로 — **수단만 다르고 세기·방향 공식은 같다.**
			// 예전에는 Actor 분기 하나뿐이라 순수 엔티티를 패링하면 경직만 걸리고 밀려나지는 않았다.
			constexpr float ParryKnockbackStrength = 2000.f;

			if (ALNPCharacterBase* AttackerPawn = Cast<ALNPCharacterBase>(Attacker))
			{
				// 피격 리액션 몽타주는 얹지 않는다 — 같은 프레임에 경직 진입이 잡히면서
				// FLNPStaggerCommand가 Montage_Stop으로 즉시 끊고 경직 몽타주로 갈아탄다.
				constexpr float DirectionWeight = 0.7f;
				constexpr float UpWeight        = 0.3f;

				const FVector AwayDir      = (AttackerPawn->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal();
				const FVector KnockbackDir = (AwayDir * DirectionWeight + AttackerPawn->GetUpDirection() * UpWeight).GetSafeNormal();
				AttackerPawn->ApplyKnockback(KnockbackDir, ParryKnockbackStrength);
			}
			else if (Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
			{
				// 바로 위 ApplyParryBreak과 같은 방식 — 엔티티 프래그먼트를 직접 만진다.
				FLNPEnemyVelocityFragment* AttackerVelocity =
					EntityManager.GetFragmentDataPtr<FLNPEnemyVelocityFragment>(Entry.AttackerEntity);
				const FTransformFragment* AttackerTransform =
					EntityManager.GetFragmentDataPtr<FTransformFragment>(Entry.AttackerEntity);

				if (AttackerVelocity && AttackerTransform)
				{
					// 구 내벽이라 Up은 월드 중심 방향이다 (판정·이동이 공유하는 규약).
					const FVector AttackerLoc = AttackerTransform->GetTransform().GetLocation();
					const FVector UpDir       = (-AttackerLoc).GetSafeNormal();
					// 헬퍼가 "피격자 → 공격자" 방향을 받아 부호를 뒤집으므로, 밀어낼 방향의 반대를 넘긴다.
					const FVector TowardVictim = (Victim->GetActorLocation() - AttackerLoc).GetSafeNormal();
					LNPHitDetection::ApplyEntityKnockback(AttackerVelocity->Velocity, TowardVictim, UpDir, ParryKnockbackStrength);
				}
			}

			// ⚠️ 방어자 몽타주를 여기서 재생하지 않는다 — `Run`은 서버에서만 돌아 게스트 화면에
			// 아무것도 남지 않았다(2026-09-20 확인). 재생은 위 `Parry.Success` 큐가 맡는다.

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
 *  여기서는 "구 Ghost 소멸 + 새 Ghost 스폰"을 전 클라이언트에 방송만 한다 (TechDesign_ParrySystem.md 5장). */
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
// 순수 엔티티 공격자 HitStop
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 순수 엔티티의 공격이 플레이어에게 **닿았다**(가드·피격 무관)는 사실을 공격자에게 남긴다.
 *
 * Actor 공격자는 `FLNPImpactCueCommand`가 `ApplyHitStop`으로 `CustomTimeDilation`을 눌러 처리하지만,
 * 순수 엔티티에는 Actor도 ASC도 없다. 대신 **카운터 하나만** 올려 두면
 * ① 호스트는 자기 프래그먼트를 그대로 읽고 ② 게스트는 복제된 같은 카운터를 읽어,
 * 양쪽이 **같은 코드**(`ULNPEnemyAnimationProcessor`)로 ISKM 트랙 재생 속도를 누른다.
 *
 * ⚠️ **여기서 재생 속도를 건드리지 않는다.** 이 커맨드는 서버에서만 돌고(판정이 서버 전용),
 *    그리는 일은 넷 모드별로 갈리는 표현 경로의 몫이다 — 그 경계를 넘으면 데디 서버에서만
 *    존재하지 않는 컴포넌트를 만지게 된다.
 * ⚠️ **위상(`FLNPEntityAttackFragment::Phase`)에는 손대지 않는다.** 판정 구간이 함께 늘어나면
 *    연출이 게임플레이를 바꾼다.
 */
struct FLNPEntityHitStopCommand : public FMassBatchedCommand
{
	FLNPEntityHitStopCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(const FMassEntityHandle InAttacker)
	{
		Entries.Add(InAttacker);
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FMassEntityHandle& Attacker : Entries)
		{
			if (!Attacker.IsSet() || !EntityManager.IsEntityActive(Attacker))
				continue;

			// 한 스윙이 두 플레이어를 동시에 맞히면 카운터가 두 번 오르지만, 수신 측은 "값이 달라졌는가"만
			// 보므로 연출은 한 번이다 — 적중 횟수를 세는 값이 아니다.
			if (FLNPEnemyActionFragment* Action = EntityManager.GetFragmentDataPtr<FLNPEnemyActionFragment>(Attacker))
				++Action->HitStopSeq;
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FMassEntityHandle> Entries;
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
// 임팩트 큐 (근접·원거리 공용)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 무기·투사체가 적중했을 때의 임팩트 연출. **근접·원거리가 이 커맨드 하나를 쓴다.**
 *
 * ⚠️ **연출은 데미지 적용과 분리되어 있다.** 예전에는 근접 임팩트가 FLNPApplyDamageGECommand
 *    안에 들어 있었는데, 순수 엔티티는 데미지를 GE로 받지 않으므로 그 묶임이 곧
 *    *"GE를 못 받으니 연출도 통째로 못 받는다"* 는 결함이 됐다 — ISM으로 그려지는 적을 베면
 *    타격감이 하나도 없었다. 원거리는 처음부터 분리돼 있었고(판정 뒤 FinishHit이 항상 큐를 낸다),
 *    근접을 그 형태로 맞춘 것이다.
 *
 * ⚠️ **ASC는 연출의 주인이 아니라 복제 전송 수단일 뿐이다.** 핸들러가 실제로 하는 일은 전부
 *    월드 서브시스템 호출(Ghost 정리 · 임팩트 VFX)이고 지점은 CueParameters에서 읽는다.
 *    그래서 피격자가 ASC를 갖지 못하면(순수 엔티티) **공격자 ASC로 나른다** — 그림은 같고
 *    전파 범위만 달라진다. 공격자 경유는 GameplayCue.LNP.Melee.AttackerHitStop이 쓰던 선례다.
 *    **폴백은 전송 수단에만 걸린다** — 피격자가 ASC를 가지면 예전 그대로 그쪽으로 나간다.
 *
 * 판정 Processor의 Execute()는 워커 Thread에서 실행될 수 있어 ASC를 직접 건드릴 수 없다.
 * Ghost 재조정에 필요한 토큰(PredictionKeyID/SpawnIndex)과 공격자 ID는 값으로만 실어 두고,
 * FLNPProjectileImpactContext 할당과 큐 실행은 게임 Thread인 Run()에서 수행한다.
 */
struct FLNPImpactCueCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>  Victim;              // null 가능 — 그때는 공격자 ASC가 큐를 나른다
		FMassEntityHandle       AttackerEntity;      // 전송 대체 경로 + 근접 HitStop 대상
		TObjectPtr<ULNPVFXData> VFXData;             // 근접은 사용하지 않는다
		FVector                 ImpactPoint;         // 적중 지점 (월드)
		FVector                 ImpactNormal;        // 피격자 → 적중 지점 방향
		int32                   PredictionKeyID    = 0;
		int32                   InstigatorPlayerID = INDEX_NONE;
		uint8                   SpawnIndex         = 0;
		bool                    bIsMeleeHit        = false;
	};

	FLNPImpactCueCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	/** 원거리 — Ghost 재조정 토큰을 함께 싣는다. */
	void Add(AActor* InVictim, FMassEntityHandle InAttacker, TObjectPtr<ULNPVFXData> InVFXData,
		const FVector& InImpactPoint, const FVector& InImpactNormal,
		int32 InPredictionKeyID, uint8 InSpawnIndex, int32 InInstigatorPlayerID)
	{
		Entries.Add({ InVictim, InAttacker, InVFXData, InImpactPoint, InImpactNormal,
			InPredictionKeyID, InInstigatorPlayerID, InSpawnIndex, /*bIsMeleeHit*/ false });
		bHasWork = true;
	}

	/** 근접 — Ghost가 없고 대신 공격자 HitStop이 딸린다.
	 *  ⚠️ 이름이 겹치는 것은 `FMassCommandBuffer::PushCommand`가 항상 `Add`를 부르기 때문이다.
	 *     인자 수가 달라 모호해지지 않는다. */
	void Add(AActor* InVictim, FMassEntityHandle InAttacker,
		const FVector& InImpactPoint, const FVector& InImpactNormal)
	{
		Entries.Add({ InVictim, InAttacker, nullptr, InImpactPoint, InImpactNormal,
			0, INDEX_NONE, 0, /*bIsMeleeHit*/ true });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		UMassActorSubsystem* ActorSub = EntityManager.GetWorld()
			? EntityManager.GetWorld()->GetSubsystem<UMassActorSubsystem>() : nullptr;

		for (const FEntry& Entry : Entries)
		{
			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityActive(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			// 전송 수단만 고른다 — 피격자 ASC가 우선이고, 없을 때만 공격자 ASC로 나른다.
			UAbilitySystemComponent* CueASC = LNPHitDetection::GetASC(Entry.Victim.Get());
			if (!IsValid(CueASC))
				CueASC = LNPHitDetection::GetASC(Attacker);

			if (IsValid(CueASC))
			{
				FGameplayCueParameters CueParams;
				CueParams.Location = Entry.ImpactPoint;
				CueParams.Normal   = Entry.ImpactNormal;

				if (Entry.bIsMeleeHit)
				{
					CueASC->ExecuteGameplayCue(TAG_GameplayCue_Melee_Impact, CueParams);
				}
				else
				{
					FLNPProjectileImpactContext* ImpactCtx = new FLNPProjectileImpactContext();
					ImpactCtx->PredictionKeyID    = Entry.PredictionKeyID;
					ImpactCtx->SpawnIndex         = Entry.SpawnIndex;
					ImpactCtx->InstigatorPlayerID = Entry.InstigatorPlayerID;
					ImpactCtx->VFXData            = Entry.VFXData;

					CueParams.EffectContext = FGameplayEffectContextHandle(ImpactCtx);
					CueASC->ExecuteGameplayCue(TAG_GameplayCue_Projectile_Impact, CueParams);
				}
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

// ──────────────────────────────────────────────────────────────────────────────
// 데미지
// ──────────────────────────────────────────────────────────────────────────────

/**
 * 순수 데미지 적용 + 피격자 반응(HitReact 몽타주 · 넉백). 판정 로직 없음.
 *
 * ⚠️ **임팩트 VFX와 공격자 HitStop은 여기 없다** — FLNPImpactCueCommand로 옮겼다.
 *    남은 셋(데미지 GE · HitReact 몽타주 · Mover 넉백)은 **피격자가 Actor일 때만 성립하는
 *    수단**이라 이 커맨드에 남고, 순수 엔티티는 각각 직접 HP 차감 · 행동 상태 플린치 ·
 *    FLNPEnemyVelocityFragment라는 다른 수단으로 같은 일을 한다.
 *    수단이 갈리는 것과 연출이 갈리는 것은 다른 문제이며, 후자만 통일했다.
 */
struct FLNPApplyDamageGECommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>       Victim;
		TSubclassOf<UGameplayEffect> EffectClass;
		float                        Damage;
		FVector                      HitFromDirection;
		FVector                      ImpactPoint;       // 무기·투사체가 실제로 닿은 지점 (월드)
		float                        KnockbackStrength;
	};

	FLNPApplyDamageGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, TSubclassOf<UGameplayEffect> InEffectClass, float InDamage, FVector InHitFromDir, const FVector& InImpactPoint, float InKnockbackStrength = 0.f)
	{
		Entries.Add({ InVictim, InEffectClass, InDamage, InHitFromDir, InImpactPoint, InKnockbackStrength });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
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

			// 피격자 HitReact 몽타주는 GameplayCue를 통해 서버·전 클라이언트에 전파된다 (Run은 서버에서만 실행).
			FGameplayCueParameters CueParams;
			CueParams.Location = Entry.ImpactPoint;
			CueParams.Normal   = Entry.HitFromDirection;
			ASC->ExecuteGameplayCue(TAG_GameplayCue_Character_HitReact, CueParams);

			if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(Victim))
			{
				// Entry.HitFromDirection은 "피격자 → 공격자"(공격이 날아온 쪽) 방향이다 (PlayHitReact의 방향 판정 컨벤션).
				// 넉백은 그 반대, 즉 공격자로부터 밀려나는 방향으로 밀어야 하므로 부호를 반전한다.
				if (Entry.KnockbackStrength > 0.f)
					VictimChar->ApplyKnockback(-Entry.HitFromDirection, Entry.KnockbackStrength);
			}
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

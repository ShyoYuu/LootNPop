// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyConfig.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "Item/LNPWeaponData.h"

/**
 * 순수 엔티티 원거리 공격의 **기하와 식별자를 서버·클라이언트가 공유하는 자리.**
 *
 * 서버는 실제 발사체를, 게스트는 관전용 Ghost를 만든다. 두 경로가 각자 총구와 방향을 계산하면
 * `TechDesign_HitDetection.md` §7.5가 못 박아 둔 **분기 복제 함정**(PvP 쪽에만 패링 체크가 빠져 있던
 * 사고)을 그대로 재생산하게 되므로, 갈라지는 것은 "무엇을 스폰하는가"뿐이어야 한다.
 */
namespace LNPEntityAttack
{
	/**
	 * 구면 규약의 로컬 기저. 기준점은 **캡슐 중심**이고(발밑이 아니다),
	 * 구 내벽이므로 머리 방향(Up)은 구 중심을 향한다 — Mass 판정 경로 전체가 쓰는 관행과 같다.
	 */
	struct FBasis
	{
		FVector Center  = FVector::ZeroVector;
		FVector Up      = FVector::UpVector;
		FVector Forward = FVector::ForwardVector;
		FVector Right   = FVector::RightVector;
	};

	inline FBasis MakeBasis(const FTransform& Transform)
	{
		FBasis Basis;
		Basis.Center = Transform.GetLocation();
		Basis.Up     = (-Basis.Center).GetSafeNormal();

		// 전방은 접평면에 투영해야 한다 — 구면에서 액터 전방은 표면과 미세하게 어긋나 있다.
		const FVector RawForward = Transform.GetUnitAxis(EAxis::X);
		Basis.Forward = (RawForward - Basis.Up * FVector::DotProduct(RawForward, Basis.Up)).GetSafeNormal();
		Basis.Right   = FVector::CrossProduct(Basis.Up, Basis.Forward);
		return Basis;
	}

	/**
	 * 접평면 위 방향을 Yaw로 돌리고 그만큼 위아래로 기울인다.
	 * 기울임 축을 고정 Right로 두면 Yaw가 ±90°에 가까울 때 축과 방향이 겹쳐 회전이 사라지므로,
	 * 접평면 성분을 만든 뒤 Up 쪽으로 들어 올리는 방식으로 계산한다.
	 */
	inline FVector MakeTangentDirection(const FBasis& Basis, const float YawDeg, const float PitchDeg)
	{
		const float YawRad   = FMath::DegreesToRadians(YawDeg);
		const float PitchRad = FMath::DegreesToRadians(PitchDeg);

		const FVector Tangent = Basis.Forward * FMath::Cos(YawRad) + Basis.Right * FMath::Sin(YawRad);
		return (Tangent * FMath::Cos(PitchRad) + Basis.Up * FMath::Sin(PitchRad)).GetSafeNormal();
	}

	/** 총구 — 캡슐 중심 기준 로컬 오프셋(X=전방, Y=우측, Z=Up). */
	inline FVector ComputeMuzzle(const FBasis& Basis, const FLNPEntityAttackConfig& AttackConfig)
	{
		return Basis.Center
			+ Basis.Forward * AttackConfig.MuzzleLocalOffset.X
			+ Basis.Right   * AttackConfig.MuzzleLocalOffset.Y
			+ Basis.Up      * AttackConfig.MuzzleLocalOffset.Z;
	}

	/**
	 * 발사체 상수 — 무기 데이터가 대부분이고, **지금까지 어빌리티 인스턴스가 공급하던 값만**
	 * `FLNPEntityAttackConfig`가 대신한다.
	 */
	inline FLNPProjectileSharedFragment MakeProjectileSharedData(const ULNPWeaponData& WeaponDef,
		const FLNPEntityAttackConfig& AttackConfig)
	{
		FLNPProjectileSharedFragment SharedData;
		SharedData.VFXData           = WeaponDef.ProjectileVFXData;
		SharedData.DamageEffectClass = WeaponDef.ProjectileDamageEffect;
		SharedData.Type              = WeaponDef.ProjectileType;
		SharedData.HitRadius         = WeaponDef.HitRadius;
		SharedData.ExplosionRadius   = WeaponDef.ExplosionRadius;
		SharedData.Damage            = AttackConfig.Damage;
		SharedData.ParryRadius       = AttackConfig.ParryRadius;
		SharedData.KnockbackStrength = AttackConfig.KnockbackStrength;
		SharedData.PoiseDamage       = AttackConfig.PoiseDamage;
		return SharedData;
	}

	/**
	 * 한 발사를 가리키는 **결정론적** Salvo 키 — 서버와 게스트가 같은 값을 스스로 유도한다.
	 *
	 * ⚠️ **이것이 없으면 게스트 Ghost가 임팩트에서 안 사라지고 관통해 날아간다.**
	 * `ULNPGhostProjectileSubsystem`은 `FLNPGhostKey{PlayerID, KeyOrSalvo, SpawnIndex}` **정확 일치**로만
	 * Ghost를 파괴하는데, 서버가 쓰던 `IssueServerSalvoID()`는 전역 카운터라 복제되지 않는다.
	 * NetID와 전이 카운터는 이미 양쪽이 갖고 있으므로 **추가 대역폭 0**으로 같은 키가 나온다.
	 *
	 * ⚠️ **키 공간이 겹치면 안 된다.** 예측 키는 0~65535, `IssueServerSalvoID`는 65536부터이므로
	 * 여기서는 음수 영역만 쓴다. 펠릿 구분은 지금처럼 `SpawnIndex`가 맡는다.
	 */
	inline int32 MakeGhostSalvoKey(const uint32 NetID, const uint8 Seq)
	{
		// 30비트로 접어 int32 음수 표현 범위 안에 확실히 들어가게 한다.
		const uint32 Packed = (((NetID << 5) | (Seq & 0x1Fu)) & 0x3FFFFFFFu);
		return -static_cast<int32>(Packed) - 1;
	}
}

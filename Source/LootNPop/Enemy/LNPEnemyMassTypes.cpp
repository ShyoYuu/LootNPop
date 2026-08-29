// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Item/LNPWeaponData.h"
#include "GAS/LNPStatModifier.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Replication/LNPMassReplication.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "GAS/LNPPoiseTypes.h"

#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassDistanceLODProcessor.h"
#include "MassStateTreeFragments.h"
#include "MassReplicationTrait.h"
#include "StateTree.h"


ULNPEnemyTrait::ULNPEnemyTrait()
{
	ReplicationTrait = CreateDefaultSubobject<UMassReplicationTrait>(TEXT("ReplicationTrait"));
}

void ULNPEnemyTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// 1. 핵심 데이터 Fragment
	//
	// 적의 HP 원본은 엔티티 Fragment다 (Actor의 AttributeSet은 승격 시 SyncFromEntity로 채워지는 사본).
	// 무기가 MaxHealth 스텟을 갖는 경우, Actor 쪽 ASC에는 InitializeOnce가 그 스텟을 GE로 걸어 Max가 오르는데
	// 엔티티 HP는 기초값 그대로라 승격 순간 Cur < Max가 되어 무손상 적에게도 HP 바가 뜬다.
	// 적은 무기를 교체하지 않으므로(고정 장비) 스폰 템플릿 단계에서 무기 스텟을 미리 녹여 넣고
	// Health = MaxHealth로 시작시킨다 — Low LOD 상태의 실효 HP도 함께 맞춰진다.
	FLNPEnemyFragment& EnemyFragment = BuildContext.AddFragment_GetRef<FLNPEnemyFragment>();
	EnemyFragment.MaxHealth = LNPStat::ResolveStatValue(
		ULNPBaseAttributeSet::GetMaxHealthAttribute(),
		GetDefault<ULNPBaseAttributeSet>()->GetMaxHealth(),
		(EnemyConfig && EnemyConfig->WeaponData)
			? EnemyConfig->WeaponData->GetStatModifiersForLevel(1)  // 적은 레벨 개념이 없다 (InitializeOnce와 동일)
			: TConstArrayView<FLNPStatModifier>());
	EnemyFragment.Health = EnemyFragment.MaxHealth;

	// Low LOD(Actor 없음) 피격은 이 값으로 감쇠한다. 시드하지 않으면 0으로 남아 **같은 공격이
	// Low LOD 적에게만 더 아프게** 들어간다 — High LOD는 ASC의 DefensePower(기초 + 무기)를 쓰기 때문이다.
	EnemyFragment.Defense = LNPStat::ResolveStatValue(
		ULNPBaseAttributeSet::GetDefensePowerAttribute(),
		GetDefault<ULNPBaseAttributeSet>()->GetDefensePower(),
		(EnemyConfig && EnemyConfig->WeaponData)
			? EnemyConfig->WeaponData->GetStatModifiersForLevel(1)
			: TConstArrayView<FLNPStatModifier>());

	BuildContext.AddFragment<FLNPEnemyIdleFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingCandidateFragment>();
	BuildContext.AddFragment<FMassMoveTargetFragment>();
	BuildContext.AddFragment<FLNPEnemyVelocityFragment>();
	//BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FLNPPositionHistoryFragment>(); // Lag Compensation용 위치 히스토리 (서버 전용 기록)

	// 경직도. 적은 지속 버프를 받지 않으므로 저항은 여기서 1회 시드하면 끝이다
	// (플레이어는 어트리뷰트가 바뀔 때마다 프래그먼트로 미러링한다).
	FLNPPoiseFragment& PoiseFragment = BuildContext.AddFragment_GetRef<FLNPPoiseFragment>();
	if (EnemyConfig)
	{
		PoiseFragment.Resistance       = EnemyConfig->PoiseResistance;
		PoiseFragment.StaggerThreshold = EnemyConfig->PoiseStaggerThreshold;
		PoiseFragment.DownThreshold    = FMath::Max(EnemyConfig->PoiseDownThreshold, EnemyConfig->PoiseStaggerThreshold + 1.f);
	}

	// 2. Shared Config Fragment
	if (EnemyConfig != nullptr)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		
		// Enemy Config Fragment (Enemy 설정)
		FLNPEnemySharedFragment EnemySharedFragment;
		EnemySharedFragment.Config = EnemyConfig;
		FConstSharedStruct EnemySharedStruct = EntityManager.GetOrCreateConstSharedFragment(EnemySharedFragment);
		BuildContext.AddConstSharedFragment(EnemySharedStruct);
	}

	// 3. 식별 Tag
	BuildContext.AddTag<FLNPEnemyTag>();

	// 4. 필수 Mass 시스템 Fragment
	BuildContext.AddFragment<FMassActorFragment>();

	// 5. Enemy MassReplication (Phase 6) — NM_Standalone이면 UMassReplicationTrait::BuildTemplate이 자체적으로 조기 반환한다.
	//    Params는 DA에서 편집된 ReplicationCullDistance를 반영해야 하므로 생성자가 아니라 여기서 채운다.
	//    Pod과 값이 다르면 FMassReplicationSharedFragment가 타입별로 분리되는데, 엔진이
	//    ForEachSharedFragment로 순회하는 정상 구성이며 버블·리플리케이터는 여전히 하나다 (§7.1 불변식 유지).
	LNP::Replication::ConfigureParams(ReplicationTrait->Params, ReplicationCullDistance);
	ReplicationTrait->BuildTemplate(BuildContext, World);

	// 6. 클라이언트 전용 — 복제 수신(0.1~0.3초 간격) 사이를 메우는 보간 상태.
	//    수신이 없는 서버·Standalone에는 불필요하므로 아키타입에 넣지 않는다.
	//    템플릿 ID는 Config GUID에서만 나오므로(FMassEntityTemplateIDFactory::Make) 서버와 구성이
	//    달라도 복제 스폰은 영향받지 않는다. 템플릿 레지스트리도 월드별로 분리돼 있다.
	if (World.GetNetMode() == NM_Client)
	{
		BuildContext.AddFragment<FLNPReplicatedMovementFragment>();
	}
}

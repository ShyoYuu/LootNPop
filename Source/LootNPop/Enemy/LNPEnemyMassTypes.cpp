// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Item/LNPWeaponData.h"
#include "GAS/LNPStatModifier.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Replication/LNPMassReplication.h"
#include "Replication/LNPMassReplicator.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "GAS/LNPPoiseTypes.h"
#include "LootNPop.h"

#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassVisualizationTrait.h"
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

	// 순수 엔티티 공격 상태. CombatMode와 무관하게 전원에게 붙인다 — 모드로 아키타입을 가르면
	// 쿼리를 두 벌 유지해야 하고 StateTree 외부 데이터 핸들이 Optional이 된다 (FLNPEntityAttackFragment 주석).
	BuildContext.AddFragment<FLNPEntityAttackFragment>();

	// 행동 상태 채널. 서버는 ULNPEnemyActionProcessor가 채우고 게스트는 버블 핸들러가 채운다 —
	// **아키타입이 양쪽에서 같아야** 수신값을 쓸 자리가 생기므로 여기서 무조건 붙인다.
	BuildContext.AddFragment<FLNPEnemyActionFragment>();

	// ISKM 애니 데이터의 자리. ⚠️ **엔진 트레이트가 이 프래그먼트를 붙여 주지 않는다** —
	// UMassVisualizationTrait는 FMassRepresentationLODFragment까지만 넣고, 소비 프로세서
	// (UMassConsumeInstancedSkinnedMeshAnimationProcessor)는 이것을 ReadOnly로 요구한다.
	// 없으면 그 쿼리가 **아무 엔티티도 매칭하지 않아** 경고 하나 없이 그냥 안 움직인다.
	// 엔진의 유일한 선례도 같은 방식이다(MetaHumanMassCrowdVisualizationTrait).
	// FLNPEnemyActionFragment와 같은 이유로 CombatMode와 무관하게 전원에게 붙인다.
	BuildContext.AddFragment<FMassRepresentationAnimationFragment>();

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
	//    ⚠️ ULNPEnemyReplicator는 동작이 아니라 **타입 분리**를 위해 존재한다 — 리플리케이터 클래스가
	//    Pod과 같으면 엔진의 공유 프래그먼트 중복 제거가 두 타입을 하나로 합쳐 컬 거리가 뭉개진다.
	//    근거·실측은 LNP::Replication::ConfigureParams 주석.
	LNP::Replication::ConfigureParams(ReplicationTrait->Params, ULNPEnemyReplicator::StaticClass(), ReplicationCullDistance);
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

bool ULNPEnemyTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
	FAdditionalTraitRequirements& OutTraitRequirements) const
{
	const bool bResult = Super::ValidateTemplate(BuildContext, World, OutTraitRequirements);

	if (EnemyConfig == nullptr)
		return bResult;

	// 표현 매핑은 이웃 트레이트(MassCrowdVisualizationTrait)가 갖고 있다. BuildContext의 템플릿 데이터는
	// protected라 읽을 수 없으므로, 트레이트의 소유 EntityConfig에서 직접 찾는다 —
	// 어차피 검사 대상이 "이 에셋의 설정 조합"이라 자산 쪽에서 보는 편이 의미도 맞는다.
	const UMassEntityConfigAsset* ConfigAsset = GetTypedOuter<UMassEntityConfigAsset>();
	if (ConfigAsset == nullptr)
		return bResult;

	// GetCombinedTraits는 protected라 부모 체인을 직접 훑는다. 가까운 Config가 이긴다 —
	// 자식이 시각화 트레이트를 다시 선언하면 그쪽이 실제로 쓰이기 때문이다.
	const FMassRepresentationParameters* RepParams = nullptr;
	for (const UMassEntityConfigAsset* Asset = ConfigAsset; Asset && RepParams == nullptr; Asset = Asset->GetConfig().GetParent())
	{
		for (const UMassEntityTraitBase* Trait : Asset->GetConfig().GetTraits())
		{
			if (const UMassVisualizationTrait* VisualizationTrait = Cast<UMassVisualizationTrait>(Trait))
			{
				RepParams = &VisualizationTrait->Params;
				break;
			}
		}
	}

	if (RepParams == nullptr)
		return bResult;

	bool bMappingSpawnsActor = false;
	for (int32 LODIndex = 0; LODIndex < EMassLOD::Max; ++LODIndex)
	{
		const EMassRepresentationType RepType = RepParams->LODRepresentation[LODIndex];
		if (RepType == EMassRepresentationType::HighResSpawnedActor || RepType == EMassRepresentationType::LowResSpawnedActor)
		{
			bMappingSpawnsActor = true;
			break;
		}
	}

	const bool bPureEntity = EnemyConfig->CombatMode == ELNPEnemyCombatMode::PureEntity;

	// 경고로만 알린다. false를 돌려주면 템플릿 생성 자체가 오류로 처리되어, 데이터를 고치기 전까지
	// 해당 적이 아예 스폰되지 않는다 — 어긋남은 고쳐야 할 설정이지 스폰을 막을 사유는 아니다.
	if (bPureEntity && bMappingSpawnsActor)
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("Enemy config '%s' is PureEntity but its EntityConfig representation mapping still spawns actors. ")
			TEXT("Set every LODRepresentation entry to a non-actor type (e.g. StaticMeshInstance) and clear the template actors, ")
			TEXT("otherwise the entity is promoted to an actor as soon as the player gets close."),
			*EnemyConfig->GetName());
	}
	else if (!bPureEntity && !bMappingSpawnsActor)
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("Enemy config '%s' is ActorPromoted but its EntityConfig representation mapping never spawns actors. ")
			TEXT("The enemy will never promote, so GAS abilities and montages stay inactive."),
			*EnemyConfig->GetName());
	}

	return bResult;
}

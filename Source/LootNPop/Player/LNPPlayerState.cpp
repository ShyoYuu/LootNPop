// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerState.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"

ALNPPlayerState::ALNPPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	BaseAttributeSet = CreateDefaultSubobject<ULNPBaseAttributeSet>(TEXT("BaseAttributeSet"));

	EquipmentComponent = CreateDefaultSubobject<ULNPEquipmentComponent>(TEXT("EquipmentComponent"));
	InventoryComponent = CreateDefaultSubobject<ULNPInventoryComponent>(TEXT("InventoryComponent"));

	SetNetUpdateFrequency(100.0f);
}

void ALNPPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 기초값만 서버에서 덮어쓴다. 어트리뷰트는 전부 복제되므로 클라이언트는 초기 복제로 같은 값을 받는다.
	if (!HasAuthority() || DefaultAttributeTable == nullptr || AbilitySystemComponent == nullptr)
		return;

	// InitStats는 ASC의 SpawnedAttributes에서 어트리뷰트셋을 찾는데, 그 목록을 채우는 것은
	// ASC의 InitializeComponent(소유 액터의 서브오브젝트 스캔)다. PostInitializeComponents가
	// 그 직후이자 가장 이른 안전 시점이다.
	//
	// ⚠️ 디테일 패널의 UAbilitySystemComponent::DefaultStartingData("Attribute Test" 카테고리)를
	//    대신 쓰면 안 된다. 그쪽은 OnRegister에서 처리돼 SpawnedAttributes가 아직 비어 있고,
	//    GetOrCreateAttributeSubobject가 BaseAttributeSet과 별개인 어트리뷰트셋을 하나 더 만든다.
	AbilitySystemComponent->InitStats(ULNPBaseAttributeSet::StaticClass(), DefaultAttributeTable);

	// 테이블이 MaxHealth만 올리고 Health 행을 빠뜨려도 만피로 시작하게 맞춰준다.
	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(ULNPBaseAttributeSet::GetMaxHealthAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(ULNPBaseAttributeSet::GetHealthAttribute(), MaxHealth);
}

UAbilitySystemComponent* ALNPPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

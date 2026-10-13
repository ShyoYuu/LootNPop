// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerState.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPInventoryComponent.h"

#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

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

void ALNPPlayerState::SetTeamId(uint8 NewTeamId)
{
	if (!HasAuthority())
		return;

	TeamId = NewTeamId;
}

void ALNPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALNPPlayerState, TeamId);
}

namespace
{
	/** 팀 전환 검증용. 인수 없이 부르면 현재 배정을 출력한다.
	 *  Usage: LNP.Team.Set [PlayerIndex TeamId]
	 *   - 권위(호스트/서버) 콘솔에서만 바꾼다. PlayerIndex는 LNP.Debug.KillPlayer와 같은 컨트롤러 순서다.
	 *   - TeamId 0은 NPC 전용이라 거부한다. 모두 같은 값이면 서로 안 맞고, 다르면 서로 맞는다. */
	FAutoConsoleCommandWithWorldAndArgs GLNPTeamSet(
		TEXT("LNP.Team.Set"),
		TEXT("Server-only: assign a player's team (1-255; 0 is reserved for NPCs). Players on different teams can hit each other. No args: list teams. Args: [PlayerIndex TeamId]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
				return;

			TArray<ALNPPlayerState*> PlayerStates;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* PC = It->Get();
				PlayerStates.Add(PC ? PC->GetPlayerState<ALNPPlayerState>() : nullptr);
			}

			if (Args.Num() < 2)
			{
				for (int32 i = 0; i < PlayerStates.Num(); ++i)
				{
					if (PlayerStates[i])
						UE_LOG(LogLootNPop, Display, TEXT("[LNP.Team.Set] Player %d (%s): team %d"),
							i, *PlayerStates[i]->GetPlayerName(), PlayerStates[i]->GetTeamId());
				}
				return;
			}

			const int32 PlayerIndex = FCString::Atoi(*Args[0]);
			const int32 NewTeam     = FCString::Atoi(*Args[1]);
			if (!PlayerStates.IsValidIndex(PlayerIndex) || PlayerStates[PlayerIndex] == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Team.Set] PlayerIndex %d out of range (0..%d)."),
					PlayerIndex, PlayerStates.Num() - 1);
				return;
			}
			if (NewTeam <= LNPTeam::NPCTeam || NewTeam > MAX_uint8)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Team.Set] TeamId %d invalid (1-255; 0 is reserved for NPCs)."), NewTeam);
				return;
			}

			ALNPPlayerState* PS = PlayerStates[PlayerIndex];
			if (!PS->HasAuthority())
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Team.Set] Must run on authority (listen host / server)."));
				return;
			}

			const uint8 OldTeam = PS->GetTeamId();
			PS->SetTeamId(static_cast<uint8>(NewTeam));
			UE_LOG(LogLootNPop, Display, TEXT("[LNP.Team.Set] Player %d (%s): team %d -> %d"),
				PlayerIndex, *PS->GetPlayerName(), OldTeam, NewTeam);
		}));
}

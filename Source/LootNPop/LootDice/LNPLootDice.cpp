// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootDice/LNPLootDice.h"
#include "LootDice/LNPLootDiceRewardTable.h"
#include "Interaction/LNPInteractableRegistrySubsystem.h"
#include "Interaction/LNPInteractionPromptWidget.h"
#include "Config/LNPSettings.h"
#include "GameMode/LNPGameState.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPSkillData.h"
#include "LootNPop.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

ALNPLootDice::ALNPLootDice()
{
	PrimaryActorTick.bCanEverTick = true;

	// 서버 권위 물리 + 표준 ReplicatedMovement 동기화 (§2.3) — Iris가 FRepMovement를
	// RepMovementNetSerializer로 네이티브 처리하며, 물리 상태(각속도 포함)는 bRepPhysics 경로로 나른다.
	bReplicates = true;
	SetReplicatingMovement(true);
	// 바운스 없는 낮은 반발이라 굴림 활성 구간이 수 초로 짧다 — 정지(슬립) 후엔 델타가 없어 트래픽이 멈춘다
	SetNetUpdateFrequency(20.f);
	// 분배 논의는 근접 상황 — 100m 밖에는 보낼 필요 없다
	SetNetCullDistanceSquared(10000.f * 10000.f);

	// 루트 = 물리 큐브. 시뮬레이트 중인 PrimitiveComponent가 루트여야 GatherCurrentMovement가
	// bRepPhysics=true로 선속도·각속도까지 기록한다.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetSimulatePhysics(true);
	// 내장 -Z 중력 차단 — 구형 중력은 Tick의 AddForce(bAccelChange)가 동일한 수학으로 담당한다 (§2.4)
	MeshComponent->SetEnableGravity(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	// 플레이어가 몸으로 밀거나 차지 못하게 — 정지 위치가 곧 분배 논의 대상이므로 획득 입력으로만 상호작용
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 상호작용 프롬프트 위젯 — 로컬 판정으로만 표시되므로 복제와 무관, 기본 숨김 (LootPod과 동일 구성)
	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(RootComponent);
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawAtDesiredSize(true);
	InteractionPromptWidget->SetWidgetClass(ULNPInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetVisibility(false);
}

void ALNPLootDice::BeginPlay()
{
	Super::BeginPlay();

	// 상호작용 탐색용 레지스트리 등록 (LootPod과 동일 파이프라인 — ULNPInteractionComponent가 순회)
	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->RegisterInteractable(this);
	}

	// 페이로드(COND_InitialOnly)는 스폰 번치에 실려 BeginPlay 시점엔 클라이언트에도 도착해 있다
	SetupIconMaterial();
}

void ALNPLootDice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld()))
	{
		Registry->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ALNPLootDice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 구형 중력 — 서버·클라이언트 공통 적용 (클라이언트 로컬 물리 블렌딩 품질 §2.4).
	// 잠든 바디에 매 Tick 힘을 넣으면 영원히 잠들지 못해 "슬립 후 트래픽 소멸"(§2.3)이 무너지므로
	// 깨어 있는 바디에만 준다.
	if (MeshComponent->IsSimulatingPhysics() && MeshComponent->IsAnyRigidBodyAwake())
	{
		MeshComponent->AddForce(ComputeGravityDir() * GravityAccel, NAME_None, /*bAccelChange=*/true);
	}

	UpdateExpiryBlink();
}

void ALNPLootDice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 페이로드는 스폰 후 불변 — 스폰 번치 1회만 복제한다.
	// SpawnDice의 Deferred 스폰이 FinishSpawning 전 대입을 보장한다.
	DOREPLIFETIME_CONDITION(ALNPLootDice, ItemDef, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ALNPLootDice, RemainingDuration, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ALNPLootDice, SpawnServerTime, COND_InitialOnly);
}

FVector ALNPLootDice::ComputeGravityDir() const
{
	// 구 내벽 세계: 아래 = 중심 반대쪽 (RadialOutward — LNPPawnGravityComponent와 동일 부호, Origin=ZeroVector).
	// bIsSphereWorld는 복제 프로퍼티라 서버·클라이언트가 같은 값을 보고, GameState 미도착 프레임은 평면 중력 폴백.
	const ALNPGameState* GS = GetWorld()->GetGameState<ALNPGameState>();
	if (GS != nullptr && GS->bIsSphereWorld)
	{
		return GetActorLocation().GetSafeNormal();
	}
	return FVector::DownVector;
}

bool ALNPLootDice::CanInteract(const APawn* Interactor) const
{
	if (Interactor == nullptr || bClaimed || IsActorBeingDestroyed())
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(GetActorLocation(), Interactor->GetActorLocation());
	return DistSq <= FMath::Square(InteractionRadius);
}

void ALNPLootDice::SetInteractionPromptVisible(bool bVisible)
{
	if (InteractionPromptWidget != nullptr)
	{
		InteractionPromptWidget->SetVisibility(bVisible);
	}
}

void ALNPLootDice::UpdateExpiryBlink()
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS == nullptr || SpawnServerTime <= 0.0f)
	{
		return;
	}

	// 서버 시간 기준 잔여 수명 — SpawnServerTime(초기 복제) + 수명 상수(CDO)만으로 클라이언트가
	// 로컬 계산한다. 추가 복제 불필요 (§2.9).
	const float Remain = DiceLifetime - (GS->GetServerWorldTimeSeconds() - SpawnServerTime);
	if (Remain > BlinkWarnSeconds)
	{
		return;
	}

	// 한 주기(BlinkPeriod) 안에서 앞 반주기는 표시, 뒤 반주기는 숨김 → BlinkPeriod가 작을수록 빨리 깜빡인다.
	const bool bVisible = FMath::Fmod(FMath::Max(Remain, 0.0f), BlinkPeriod) > BlinkPeriod * 0.5f;
	if (bVisible != bBlinkVisible)
	{
		bBlinkVisible = bVisible;
		MeshComponent->SetVisibility(bVisible);
	}
}

void ALNPLootDice::SetupIconMaterial()
{
	// 머티리얼 미지정(placeholder 단계)이면 조용히 스킵 — 물리·복제 검증에는 지장 없다
	if (MeshComponent->GetMaterial(0) == nullptr)
	{
		return;
	}

	IconMID = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	if (IconMID == nullptr)
	{
		return;
	}

	if (ItemDef != nullptr && ItemDef->Icon != nullptr)
	{
		IconMID->SetTextureParameterValue(TEXT("IconTexture"), ItemDef->Icon);
	}

	// 카테고리 색 — 아이콘과 함께 보상 대분류(무기/버프/스킬)를 원거리에서도 식별하게 한다
	FLinearColor CategoryColor = FLinearColor::White;
	if (Cast<ULNPWeaponData>(ItemDef) != nullptr)
	{
		CategoryColor = WeaponCategoryColor;
	}
	else if (Cast<ULNPBuffData>(ItemDef) != nullptr)
	{
		CategoryColor = BuffCategoryColor;
	}
	else if (Cast<ULNPSkillData>(ItemDef) != nullptr)
	{
		CategoryColor = SkillCategoryColor;
	}
	IconMID->SetVectorParameterValue(TEXT("CategoryColor"), CategoryColor);
}

ALNPLootDice* ALNPLootDice::SpawnDice(UWorld& World, const FVector& Location, ULNPItemDefinitionBase* Item,
                                      float InRemainingDuration, float ImpulseScale)
{
	if (World.GetNetMode() == NM_Client)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] SpawnDice는 서버 전용이다"));
		return nullptr;
	}

	UClass* DiceClass = nullptr;
	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
	{
		DiceClass = Settings->LootDiceClass.LoadSynchronous();
	}
	if (DiceClass == nullptr)
	{
		// BP 미지정 시 C++ 기본 클래스 폴백 — 메시 없이도 물리·복제 파이프라인 검증 가능
		UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] LNPSettings.LootDiceClass 미설정 — ALNPLootDice 기본 클래스로 스폰"));
		DiceClass = ALNPLootDice::StaticClass();
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
	ALNPLootDice* Dice = World.SpawnActorDeferred<ALNPLootDice>(DiceClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Dice == nullptr)
	{
		return nullptr;
	}

	// COND_InitialOnly 페이로드는 스폰 번치에만 실린다 — 반드시 FinishSpawning 전에 대입
	Dice->ItemDef = Item;
	Dice->RemainingDuration = InRemainingDuration;
	if (const AGameStateBase* GS = World.GetGameState())
	{
		Dice->SpawnServerTime = GS->GetServerWorldTimeSeconds();
	}
	Dice->FinishSpawning(SpawnTransform);

	// Pop 임펄스 + 고속 랜덤 회전 — 복제 불필요, ReplicatedMovement가 결과 궤적을 나른다 (§2.8).
	// 표면 Up 기준 원뿔 내 랜덤 방향이라 다중 스폰 시 자연스럽게 흩어진다.
	if (Dice->MeshComponent->IsSimulatingPhysics())
	{
		const FVector Up = -Dice->ComputeGravityDir();
		const FVector ImpulseDir = FMath::VRandCone(Up, FMath::DegreesToRadians(Dice->PopConeHalfAngleDeg));
		Dice->MeshComponent->AddImpulse(ImpulseDir * Dice->PopImpulseSpeed * ImpulseScale, NAME_None, /*bVelChange=*/true);
		Dice->MeshComponent->SetPhysicsAngularVelocityInRadians(FMath::VRand() * Dice->SpinSpeedRad);
	}

	// 서버 수명 타이머 — Destroy가 복제로 전 클라이언트에서 제거된다 (§2.9)
	Dice->SetLifeSpan(Dice->DiceLifetime);

	return Dice;
}

void ALNPLootDice::SpawnPodRewards(UWorld& World, int32 PodID, const FVector& PodLocation)
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	ULNPLootDiceRewardTable* Table = (Settings != nullptr) ? Settings->LootDiceRewardTable.LoadSynchronous() : nullptr;
	if (Table == nullptr)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] LNPSettings.LootDiceRewardTable 미설정 — PodID %d 보상 스폰 생략"), PodID);
		return;
	}

	const FLNPLootDiceRewardSet* RewardSet = Table->RewardsByPodID.Find(PodID);
	if (RewardSet == nullptr)
	{
		RewardSet = &Table->DefaultRewards;
	}
	if (RewardSet->Items.IsEmpty())
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] PodID %d 보상 목록이 비어 있음 (DefaultRewards 포함)"), PodID);
		return;
	}

	// 표면 Up으로 살짝 띄워 스폰 — 원뿔 랜덤 임펄스가 산개를 만들므로 위치 오프셋은 최소만.
	// Pod Actor는 이미 파괴됐을 수 있으므로 위치·PodID만 사용한다 (호출부 FLNPPodStateTransitionCommand 참조).
	FVector Up = FVector::UpVector;
	const ALNPGameState* GS = World.GetGameState<ALNPGameState>();
	if (GS != nullptr && GS->bIsSphereWorld)
	{
		Up = -PodLocation.GetSafeNormal();
	}
	const FVector SpawnLocation = PodLocation + Up * 50.0f;

	int32 NumSpawned = 0;
	for (const TObjectPtr<ULNPItemDefinitionBase>& Item : RewardSet->Items)
	{
		if (Item != nullptr && SpawnDice(World, SpawnLocation, Item, 0.0f) != nullptr)
		{
			++NumSpawned;
		}
	}
	UE_LOG(LogLootNPop, Log, TEXT("[LootDice] PodID %d 보상 Dice %d개 Pop!"), PodID, NumSpawned);
}

namespace
{
	/** 디버그 스폰: LNP.Debug.SpawnLootDice [개수=1] [ItemDef 에셋 경로] — 로컬 폰 전방에 스폰 (서버 월드 전용) */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugSpawnLootDice(
		TEXT("LNP.Debug.SpawnLootDice"),
		TEXT("Spawn LootDice in front of the local pawn (server world only). Args: [count=1] [ItemDef asset path]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] SpawnLootDice는 서버 월드에서만 동작한다"));
				return;
			}

			const APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = (PC != nullptr) ? PC->GetPawn() : nullptr;
			if (Pawn == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] SpawnLootDice — 로컬 폰 없음"));
				return;
			}

			const int32 Count = (Args.Num() > 0) ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 32) : 1;

			ULNPItemDefinitionBase* Item = nullptr;
			if (Args.Num() > 1)
			{
				Item = LoadObject<ULNPItemDefinitionBase>(nullptr, *Args[1]);
				if (Item == nullptr)
				{
					UE_LOG(LogLootNPop, Warning, TEXT("[LootDice] ItemDef 로드 실패: %s — 페이로드 없이 스폰"), *Args[1]);
				}
			}

			const FVector Location = Pawn->GetActorLocation()
				+ Pawn->GetActorForwardVector() * 150.0f
				+ Pawn->GetActorUpVector() * 100.0f;

			for (int32 i = 0; i < Count; ++i)
			{
				ALNPLootDice::SpawnDice(*World, Location, Item, 0.0f);
			}
		}),
		ECVF_Cheat);
}

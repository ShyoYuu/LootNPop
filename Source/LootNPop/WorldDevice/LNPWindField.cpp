// Copyright (c) 2026 LootNPop. All rights reserved.

#include "WorldDevice/LNPWindField.h"
#include "WorldDevice/LNPWindFieldSubsystem.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "LootNPop.h"

#include "NiagaraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ALNPWindField::ALNPWindField()
{
	PrimaryActorTick.bCanEverTick = false;

	// 넓이에 상한이 없어 원점 거리 기반 relevancy를 쓸 수 없다 — 끊기면 필드 안의 클라가 필드를 잃고
	// 활공 예측이 서버와 갈린다. 복제되는 가변 상태가 on/off 1비트라 항상 relevant여도 비용이 없다.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EffectComponent"));
	EffectComponent->SetupAttachment(RootComponent);
	EffectComponent->SetAutoActivate(false);
}

void ALNPWindField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALNPWindField, Radius);
	DOREPLIFETIME(ALNPWindField, RiseHeight);
	DOREPLIFETIME(ALNPWindField, bEnabled);
}

void ALNPWindField::InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id)
{
	Radius = Marker.WindFieldRadius;
	RiseHeight = Marker.WindFieldRiseHeight;
	bEnabled = Marker.bWindFieldStartEnabled;
}

float ALNPWindField::GetTargetCenterDistance() const
{
	return (GetActorLocation() + GetActorUpVector() * RiseHeight).Size();
}

void ALNPWindField::SetWindFieldEnabled(bool bInEnabled)
{
	if (!HasAuthority() || bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	RefreshEffect();
}

void ALNPWindField::OnRep_Enabled()
{
	RefreshEffect();
}

void ALNPWindField::RefreshEffect()
{
	if (EffectComponent == nullptr || EffectComponent->GetAsset() == nullptr)
	{
		return;
	}

	// NS_WindField의 입자는 로컬 +Z로 WindFxRiseSpeed만큼 올라간다 — 수명을 높이에 맞춰야 목표 고도에서 사라진다.
	// 스폰률은 면적에 비례시키되 넓이 제한이 없으므로 상한을 둔다.
	constexpr float WindFxRiseSpeed = 900.f;
	constexpr float WindFxSpawnPerSquareMeter = 0.15f;
	const float AreaSquareMeters = UE_PI * FMath::Square(Radius * 0.01f);
	EffectComponent->SetVariableFloat(TEXT("User.Radius"), Radius);
	EffectComponent->SetVariableFloat(TEXT("User.Lifetime"), FMath::Max(RiseHeight, 100.f) / WindFxRiseSpeed);
	EffectComponent->SetVariableFloat(TEXT("User.SpawnRate"), FMath::Clamp(AreaSquareMeters * WindFxSpawnPerSquareMeter, 10.f, 400.f));
	if (bEnabled)
	{
		if (!EffectComponent->IsActive())
		{
			EffectComponent->Activate(/*bReset=*/true);
		}
	}
	else
	{
		EffectComponent->Deactivate();
	}
}

void ALNPWindField::BeginPlay()
{
	Super::BeginPlay();

	if (ULNPWindFieldSubsystem* Subsystem = UWorld::GetSubsystem<ULNPWindFieldSubsystem>(GetWorld()))
	{
		Subsystem->RegisterWindField(this);
	}
	RefreshEffect();
}

void ALNPWindField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPWindFieldSubsystem* Subsystem = UWorld::GetSubsystem<ULNPWindFieldSubsystem>(GetWorld()))
	{
		Subsystem->UnregisterWindField(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void ALNPWindField::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshEffect();
}
#endif

namespace
{
	TArray<ALNPWindField*> CollectWindFields(UWorld* World)
	{
		TArray<ALNPWindField*> Result;
		if (const ULNPWindFieldSubsystem* Subsystem = World ? UWorld::GetSubsystem<ULNPWindFieldSubsystem>(World) : nullptr)
		{
			for (const TWeakObjectPtr<ALNPWindField>& WeakField : Subsystem->GetWindFields())
			{
				if (ALNPWindField* Field = WeakField.Get())
				{
					Result.Add(Field);
				}
			}
		}
		return Result;
	}

	/** 런타임 on/off 검증용. Usage: LNP.WindField.SetEnabled <0|1> — 권위 콘솔에서 모든 윈드필드를 토글한다. */
	FAutoConsoleCommandWithWorldAndArgs GLNPWindFieldSetEnabled(
		TEXT("LNP.WindField.SetEnabled"),
		TEXT("Server-only: enable or disable every wind field. Args: <0|1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || Args.Num() < 1)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.WindField.SetEnabled] Usage: LNP.WindField.SetEnabled <0|1>"));
				return;
			}
			const bool bEnable = FCString::Atoi(*Args[0]) != 0;
			int32 Count = 0;
			for (ALNPWindField* Field : CollectWindFields(World))
			{
				if (Field->HasAuthority())
				{
					Field->SetWindFieldEnabled(bEnable);
					++Count;
				}
			}
			UE_LOG(LogLootNPop, Log, TEXT("[LNP.WindField.SetEnabled] %s %d wind field(s)."), bEnable ? TEXT("Enabled") : TEXT("Disabled"), Count);
		}));

	/**
	 * 맵을 저장하지 않고 검증하기 위한 런타임 스폰. 첫 번째 플레이어 발밑에 Up 정렬로 놓는다.
	 * Usage: LNP.WindField.SpawnAtPlayer [Radius] [RiseHeight] — 권위 콘솔 전용.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPWindFieldSpawnAtPlayer(
		TEXT("LNP.WindField.SpawnAtPlayer"),
		TEXT("Server-only: spawn a wind field under the first player. Args: [Radius] [RiseHeight]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || World->GetNetMode() == NM_Client)
			{
				return;
			}
			const APlayerController* PC = World->GetFirstPlayerController();
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.WindField.SpawnAtPlayer] No player pawn."));
				return;
			}

			UClass* FieldClass = LoadClass<ALNPWindField>(nullptr, TEXT("/Game/WorldDevice/BP_WindField.BP_WindField_C"));
			if (FieldClass == nullptr)
			{
				FieldClass = ALNPWindField::StaticClass();
			}

			const FVector Up = Pawn->GetActorUpVector();
			const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
			const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
			const FTransform Transform(FRotationMatrix::MakeFromZX(Up, Pawn->GetActorForwardVector()).ToQuat(),
				Pawn->GetActorLocation() - Up * HalfHeight);

			ALNPWindField* Field = World->SpawnActorDeferred<ALNPWindField>(FieldClass, Transform);
			if (Field == nullptr)
			{
				return;
			}
			if (Args.Num() >= 1) { Field->Radius = FCString::Atof(*Args[0]); }
			if (Args.Num() >= 2) { Field->RiseHeight = FCString::Atof(*Args[1]); }
			Field->FinishSpawning(Transform);
			UE_LOG(LogLootNPop, Log, TEXT("[LNP.WindField.SpawnAtPlayer] Spawned %s Radius=%.0f RiseHeight=%.0f"),
				*GetNameSafe(FieldClass), Field->Radius, Field->RiseHeight);
		}));
}

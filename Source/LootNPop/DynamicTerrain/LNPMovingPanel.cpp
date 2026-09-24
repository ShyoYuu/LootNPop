// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DynamicTerrain/LNPMovingPanel.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "LootNPop.h"

#include "Algo/BinarySearch.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 곡선 구간당 거리 표 샘플 수. 엔진 스플라인 기본값(10)보다 촘촘하게 둔다. */
	constexpr int32 ArcStepsPerSegment = 16;
}

ALNPMovingPanel::ALNPMovingPanel()
{
	// Mover의 base 추종 틱이 이 Actor 틱을 prerequisite로 건다(BasedMovementUtils::AddTickDependency).
	// TG_PrePhysics라 worker exact query(StartPhysics~)보다 먼저 끝난다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	bAlwaysRelevant = true; // 멀리 있어도 클라 Ghost 투사체·탄도 가이드가 이 패널에 맞아야 한다.
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.f);

	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanelMesh"));
	SetRootComponent(PanelMesh);
	PanelMesh->SetMobility(EComponentMobility::Movable);
	PanelMesh->SetCollisionProfileName(TEXT("LNPDynamicTerrain"));
	PanelMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PanelMesh->SetStaticMesh(CubeMesh.Object);
		PanelMesh->SetRelativeScale3D(FVector(4.f, 4.f, 0.3f)); // 400 x 400 x 30cm
	}
}

void ALNPMovingPanel::InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id)
{
	PlacementId = Id;

	const FTransform MarkerTransform = Marker.GetActorTransform();
	Path.OriginLocation = MarkerTransform.GetLocation();
	Path.OriginRotation = MarkerTransform.GetRotation();
	Path.Curve = Marker.Path->GetSplinePointsPosition();
	Path.Speed = Marker.PathSpeed;
	Path.EndHoldSeconds = Marker.PathEndHoldSeconds;
	Path.Revision = 1;

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Path.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	if (!MarkerTransform.GetScale3D().Equals(FVector::OneVector))
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[DynamicTerrain] Marker %s has scale %s. Panel path ignores marker scale."),
			*Id.ToString(), *MarkerTransform.GetScale3D().ToString());
	}
}

void ALNPMovingPanel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALNPMovingPanel, PlacementId);
	DOREPLIFETIME(ALNPMovingPanel, Path);
}

void ALNPMovingPanel::BeginPlay()
{
	Super::BeginPlay();
	TryActivate();
}

void ALNPMovingPanel::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 클라이언트의 server world time은 GameState 복제 전에는 없다. 그동안 활성화 때 자세에 머문다.
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		UpdatePose(GameState->GetServerWorldTimeSeconds());
	}
}

void ALNPMovingPanel::OnRep_Path()
{
	if (bActive)
	{
		// 경로 revision 변경은 아직 지원하지 않는다. 조용히 옛 경로를 쓰지 않도록 남긴다.
		UE_LOG(LogLootNPop, Warning, TEXT("[DynamicTerrain] Panel %s received path revision %u after activation. Ignored."),
			*PlacementId.ToString(), Path.Revision);
		return;
	}
	TryActivate();
}

void ALNPMovingPanel::TryActivate()
{
	if (bActive || !HasActorBegunPlay())
		return;

	if (!Path.IsValid())
	{
		if (HasAuthority())
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[DynamicTerrain] Panel %s has no valid path (points=%d speed=%.1f). Staying still."),
				*PlacementId.ToString(), Path.Curve.Points.Num(), Path.Speed);
		}
		return;
	}

	bActive = true;
	BuildArcTable();

	SetActorRotation(Path.OriginRotation, ETeleportType::TeleportPhysics);

	// 원점에서 가장 먼 경로 점 + 메시 collision의 Actor 원점 기준 반지름. 회전하지 않으므로 이 합이 상한이다.
	// 샘플 사이 곡선은 현보다 최대 한 스텝만큼만 벗어날 수 있어 가장 긴 스텝을 여유로 더한다.
	double MaxPathRadius = 0.0;
	double MaxStep = 0.0;
	for (int32 Index = 0; Index < ArcKeys.Num(); ++Index)
	{
		const FVector World = Path.OriginLocation + Path.OriginRotation.RotateVector(Path.Curve.Eval(ArcKeys[Index]));
		MaxPathRadius = FMath::Max(MaxPathRadius, World.Size());
		if (Index > 0)
		{
			MaxStep = FMath::Max(MaxStep, ArcDistances[Index] - ArcDistances[Index - 1]);
		}
	}
	const FBoxSphereBounds MeshBounds = PanelMesh->CalcBounds(FTransform(Path.OriginRotation, FVector::ZeroVector, PanelMesh->GetComponentScale()));
	const double MeshRadius = MeshBounds.Origin.Size() + MeshBounds.SphereRadius;
	SweptMaxRadius = static_cast<float>(MaxPathRadius + MaxStep + MeshRadius);

	if (UWorld* World = GetWorld())
	{
		const AGameStateBase* GameState = World->GetGameState();
		UpdatePose(GameState ? GameState->GetServerWorldTimeSeconds() : Path.StartServerTime);
		PreviousTransform = GetActorTransform();

		if (ULNPDynamicTerrainSubsystem* DynamicTerrain = World->GetSubsystem<ULNPDynamicTerrainSubsystem>())
		{
			DynamicTerrain->RegisterPanel(this);
		}
		if (ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>())
		{
			HitIdentity->RegisterRuntimeSource(PanelMesh, PlacementId, SweptMaxRadius);
		}
	}

	UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] Panel %s active (%s): length=%.0fcm speed=%.0f hold=%.1fs start=%.3f swept=%.0fcm"),
		*PlacementId.ToString(), HasAuthority() ? TEXT("server") : TEXT("client"),
		PathLength, Path.Speed, Path.EndHoldSeconds, Path.StartServerTime, SweptMaxRadius);
}

void ALNPMovingPanel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bActive)
	{
		if (ULNPDynamicTerrainSubsystem* DynamicTerrain = UWorld::GetSubsystem<ULNPDynamicTerrainSubsystem>(GetWorld()))
		{
			DynamicTerrain->UnregisterPanel(this);
		}
		if (ULNPHitIdentitySubsystem* HitIdentity = UWorld::GetSubsystem<ULNPHitIdentitySubsystem>(GetWorld()))
		{
			HitIdentity->UnregisterRuntimeSource(PanelMesh);
		}
		bActive = false;
	}

	Super::EndPlay(EndPlayReason);
}

void ALNPMovingPanel::BuildArcTable()
{
	const TArray<FInterpCurvePointVector>& Points = Path.Curve.Points;
	const bool bClosed = Path.Curve.bIsLooped;
	const int32 NumSegments = bClosed ? Points.Num() : Points.Num() - 1;
	const float FirstKey = Points[0].InVal;
	const float LastKey = bClosed ? Points.Last().InVal + Path.Curve.LoopKeyOffset : Points.Last().InVal;
	const int32 NumSteps = NumSegments * ArcStepsPerSegment;

	ArcDistances.Reset(NumSteps + 1);
	ArcKeys.Reset(NumSteps + 1);

	double Distance = 0.0;
	FVector Previous = Path.Curve.Eval(FirstKey);
	ArcDistances.Add(0.0);
	ArcKeys.Add(FirstKey);
	for (int32 Step = 1; Step <= NumSteps; ++Step)
	{
		const float Key = FMath::Lerp(FirstKey, LastKey, static_cast<float>(Step) / NumSteps);
		const FVector Position = Path.Curve.Eval(Key);
		Distance += FVector::Dist(Previous, Position);
		Previous = Position;
		ArcDistances.Add(Distance);
		ArcKeys.Add(Key);
	}
	PathLength = Distance;
}

float ALNPMovingPanel::DistanceToKey(const double Distance) const
{
	const int32 Upper = Algo::LowerBound(ArcDistances, Distance);
	if (Upper <= 0)
		return ArcKeys[0];
	if (Upper >= ArcDistances.Num())
		return ArcKeys.Last();

	const double Span = ArcDistances[Upper] - ArcDistances[Upper - 1];
	const double Alpha = Span > UE_KINDA_SMALL_NUMBER ? (Distance - ArcDistances[Upper - 1]) / Span : 0.0;
	return FMath::Lerp(ArcKeys[Upper - 1], ArcKeys[Upper], static_cast<float>(Alpha));
}

void ALNPMovingPanel::EvaluateDistance(const double ServerTime, double& OutDistance, float& OutDirection) const
{
	const double Elapsed = FMath::Max(0.0, ServerTime - Path.StartServerTime);
	const double TravelSeconds = PathLength / Path.Speed;

	if (Path.Curve.bIsLooped)
	{
		OutDistance = FMath::Fmod(Elapsed * Path.Speed, PathLength);
		OutDirection = 1.f;
		return;
	}

	// 가기 → 끝에서 멈춤 → 돌아오기 → 처음에서 멈춤
	const double Hold = Path.EndHoldSeconds;
	const double Phase = FMath::Fmod(Elapsed, 2.0 * (TravelSeconds + Hold));
	if (Phase < TravelSeconds)
	{
		OutDistance = Phase * Path.Speed;
		OutDirection = 1.f;
	}
	else if (Phase < TravelSeconds + Hold)
	{
		OutDistance = PathLength;
		OutDirection = 0.f;
	}
	else if (Phase < 2.0 * TravelSeconds + Hold)
	{
		OutDistance = PathLength - (Phase - TravelSeconds - Hold) * Path.Speed;
		OutDirection = -1.f;
	}
	else
	{
		OutDistance = 0.0;
		OutDirection = 0.f;
	}
}

void ALNPMovingPanel::UpdatePose(const double ServerTime)
{
	if (!bActive)
		return;

	double Distance = 0.0;
	float Direction = 0.f;
	EvaluateDistance(ServerTime, Distance, Direction);

	const float Key = DistanceToKey(Distance);
	const FVector Location = Path.OriginLocation + Path.OriginRotation.RotateVector(Path.Curve.Eval(Key));

	PreviousTransform = GetActorTransform();

	// 쿼리가 보는 게임 스레드 자세와 실제 자세를 일치시키려고 kinematic target이 아니라 teleport로 옮긴다(DynamicTerrain.md §2).
	SetActorLocation(Location, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// Mover는 base를 떠날 때(걸어 나가기·점프) 물리 body 속도를 관성으로 더한다(GetMovementBaseVelocityAtPoint).
	// teleport로 옮긴 kinematic body의 물리 속도는 0이므로 직접 넣는다. ComponentVelocity는 GetMovementBaseVelocity용이다.
	const FVector LocalTangent = Path.Curve.EvalDerivative(Key).GetSafeNormal();
	CurrentVelocity = Path.OriginRotation.RotateVector(LocalTangent) * (Path.Speed * Direction);
	PanelMesh->ComponentVelocity = CurrentVelocity;
	PanelMesh->SetPhysicsLinearVelocity(CurrentVelocity);

	// server time은 매 프레임 조금씩만 전진해야 한다. 큰 점프는 자세 snap으로 나타나므로 진단을 남긴다.
	if (LastServerTime >= 0.0)
	{
		const double Step = ServerTime - LastServerTime;
		if (Step < -0.05 || Step > 0.25)
		{
			UE_LOG(LogLootNPop, Verbose, TEXT("[DynamicTerrain] Panel %s server time jumped %.3fs. Pose snapped."),
				*PlacementId.ToString(), Step);
			if (ULNPDynamicTerrainSubsystem* DynamicTerrain = UWorld::GetSubsystem<ULNPDynamicTerrainSubsystem>(GetWorld()))
			{
				DynamicTerrain->NotePoseSnap();
			}
		}
	}
	LastServerTime = ServerTime;
}

FVector ALNPMovingPanel::GetLinearVelocity() const
{
	return CurrentVelocity;
}

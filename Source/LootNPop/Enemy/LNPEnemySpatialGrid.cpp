// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemySpatialGrid.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Config/LNPSettings.h"
#include "LNPMassUtils.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"        // UE::Mass::ProcessorGroupNames
#include "MassExecutionContext.h"
#include "MassActorSubsystem.h"

namespace
{
	/** 위도 행의 **중심** 위도(도). 셀 면적 계산과 경도 칸 수 산출이 같은 기준을 써야 한다. */
	float RowCenterLatitudeDegrees(const int32 Row, const int32 LatRes)
	{
		return -90.f + 180.f * (static_cast<float>(Row) + 0.5f) / static_cast<float>(LatRes);
	}

	/** 방향 벡터의 경도(도, [0, 360)). 표면 캐시와 같은 관례다. */
	float LongitudeDegrees(const FVector& Dir)
	{
		float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		if (Lon < 0.f)
			Lon += 360.f;
		return Lon;
	}
}

// ============================================================
// ULNPEnemySpatialGridSubsystem
// ============================================================

void ULNPEnemySpatialGridSubsystem::EnsureLayout()
{
	const int32 DesiredLatRes = FMath::Max(8, GetDefault<ULNPSettings>()->EnemyGridLatResolution);
	if (LatRes == DesiredLatRes && RowOffsets.Num() == LatRes + 1)
		return;

	LatRes = DesiredLatRes;
	const int32 LonResFull = LatRes * 2;

	LonCounts.SetNumUninitialized(LatRes);
	RowOffsets.SetNumUninitialized(LatRes + 1);

	int32 Running = 0;
	for (int32 Row = 0; Row < LatRes; ++Row)
	{
		const float LatRad = FMath::DegreesToRadians(RowCenterLatitudeDegrees(Row, LatRes));
		// 축소 행 — 경도 칸 수를 cos(위도)에 비례해 줄여 셀 면적을 균일하게 만든다.
		// 극점 캡은 자연히 1칸이 되므로 특정 위도에서 규칙이 바뀌는 경계 예외가 없다.
		LonCounts[Row]  = FMath::Max(1, FMath::RoundToInt(LonResFull * FMath::Cos(LatRad)));
		RowOffsets[Row] = Running;
		Running += LonCounts[Row];
	}
	RowOffsets[LatRes] = Running;

	CellStarts.SetNumUninitialized(Running + 1);
}

int32 ULNPEnemySpatialGridSubsystem::RowFromDirection(const FVector& Dir) const
{
	const float LatDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f)));
	return FMath::Clamp(FMath::FloorToInt((LatDeg + 90.f) / 180.f * LatRes), 0, LatRes - 1);
}

int32 ULNPEnemySpatialGridSubsystem::ColFromLongitude(const int32 Row, const float LonDegrees) const
{
	const int32 N   = LonCounts[Row];
	const int32 Raw = FMath::FloorToInt(LonDegrees / 360.f * N);
	return ((Raw % N) + N) % N;
}

void ULNPEnemySpatialGridSubsystem::BeginRebuild(const int32 ExpectedCount)
{
	EnsureLayout();

	bBuilt = false;

	Positions.Reset(ExpectedCount);
	Handles.Reset(ExpectedCount);
	CellOf.Reset(ExpectedCount);
	Ordered.Reset(ExpectedCount);

	// 셀 카운터를 0으로 되돌린다. 접두합 단계에서 시작 인덱스로 덮어쓰이므로 매 프레임 필요하다.
	// 셀 수가 2만 규모라 memset 비용은 마이크로초 단위다.
	FMemory::Memzero(CellStarts.GetData(), CellStarts.Num() * sizeof(int32));
}

void ULNPEnemySpatialGridSubsystem::AddEntity(const FMassEntityHandle Handle, const FVector& Position)
{
	const FVector Dir = Position.GetSafeNormal();
	if (Dir.IsNearlyZero())
		return;   // 월드 중심에 정확히 놓인 개체는 방향이 없다 — 색인할 셀도 없다.

	const int32 Row  = RowFromDirection(Dir);
	const int32 Cell = RowOffsets[Row] + ColFromLongitude(Row, LongitudeDegrees(Dir));

	Positions.Add(Position);
	Handles.Add(Handle);
	CellOf.Add(Cell);

	++CellStarts[Cell];
}

void ULNPEnemySpatialGridSubsystem::FinishRebuild()
{
	const int32 NumCells = CellStarts.Num() - 1;
	if (NumCells <= 0)
		return;

	// 카운트 → 접두합. CellStarts[c]가 셀 c의 시작 인덱스가 되고 CellStarts[c+1]이 끝이 된다.
	int32 Running = 0;
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		const int32 Count = CellStarts[Cell];
		CellStarts[Cell] = Running;
		Running += Count;
	}
	CellStarts[NumCells] = Running;

	// 산포. 커서를 따로 두지 않고 CellStarts를 밀었다가 되돌리는 방식도 있지만,
	// 되돌리는 쪽이 실수하기 쉬워 커서 배열을 하나 쓴다(셀 수 규모라 비용은 무시할 만하다).
	TArray<int32> Cursor = CellStarts;
	Ordered.SetNumUninitialized(Running);
	for (int32 i = 0; i < CellOf.Num(); ++i)
		Ordered[Cursor[CellOf[i]]++] = i;

	bBuilt = true;
}

void ULNPEnemySpatialGridSubsystem::ForEachNeighbor(const FVector& Center, const float Radius,
	TFunctionRef<void(FMassEntityHandle, const FVector&)> Visitor) const
{
	if (!bBuilt || Radius <= 0.f)
		return;

	const float DistFromOrigin = Center.Size();
	if (DistFromOrigin <= KINDA_SMALL_NUMBER)
		return;

	const FVector Dir = Center / DistFromOrigin;

	// cm 반경을 각반경으로 바꾼다. **질의 지점의 실제 반경**으로 나누므로 월드 반지름 설정에
	// 의존하지 않고, 고도가 달라도 어긋나지 않는다.
	const float AngularRadiusDeg = FMath::RadiansToDegrees(Radius / DistFromOrigin);

	const float LatDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f)));
	const float LonDeg = LongitudeDegrees(Dir);

	const int32 Row0 = FMath::Clamp(FMath::FloorToInt((LatDeg - AngularRadiusDeg + 90.f) / 180.f * LatRes), 0, LatRes - 1);
	const int32 Row1 = FMath::Clamp(FMath::FloorToInt((LatDeg + AngularRadiusDeg + 90.f) / 180.f * LatRes), 0, LatRes - 1);

	for (int32 Row = Row0; Row <= Row1; ++Row)
	{
		const int32 N = LonCounts[Row];

		// 같은 각반경이라도 고위도에서는 경도로 더 넓게 벌어진다. 칸 수도 cos(위도)로 함께 줄어들어
		// 훑는 칸 수는 위도와 무관하게 대체로 일정하다 — 축소 행이 노리는 결과가 이것이다.
		const float CosLat     = FMath::Max(FMath::Cos(FMath::DegreesToRadians(RowCenterLatitudeDegrees(Row, LatRes))), 1e-4f);
		const float LonHalfDeg = AngularRadiusDeg / CosLat;

		int32 First = 0;
		int32 Last  = N - 1;
		if (LonHalfDeg < 180.f)
		{
			First = FMath::FloorToInt((LonDeg - LonHalfDeg) / 360.f * N);
			Last  = FMath::FloorToInt((LonDeg + LonHalfDeg) / 360.f * N);
			// 한 바퀴를 넘으면 wrap이 같은 칸을 두 번 방문하게 된다 — 그때는 행 전체로 접는다.
			if (Last - First + 1 >= N)
			{
				First = 0;
				Last  = N - 1;
			}
		}

		for (int32 C = First; C <= Last; ++C)
		{
			const int32 Col  = ((C % N) + N) % N;
			const int32 Cell = RowOffsets[Row] + Col;

			for (int32 s = CellStarts[Cell]; s < CellStarts[Cell + 1]; ++s)
			{
				const int32 Index = Ordered[s];
				Visitor(Handles[Index], Positions[Index]);
			}
		}
	}
}

// ============================================================
// ULNPEnemySpatialGridProcessor
// ============================================================

ULNPEnemySpatialGridProcessor::ULNPEnemySpatialGridProcessor()
	: EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	// 이동 프로세서보다 앞이어야 한다. 사이에 분리 프로세서가 자기 간선으로 끼어든다.
	ExecutionOrder.ExecuteBefore.Add(TEXT("LNPEnemySeparationProcessor"));
}

void ULNPEnemySpatialGridProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// 시체는 밀어낼 대상도, 피해 갈 대상도 아니다.
	EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPEnemySpatialGridSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemySpatialGridProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 유일한 소비처(분리력)가 서버 이동 시뮬레이션의 일부다. 클라이언트에는 지을 이유가 없다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	ULNPEnemySpatialGridSubsystem& Grid = Context.GetMutableSubsystemChecked<ULNPEnemySpatialGridSubsystem>();

	// 직전 프레임의 개체 수를 모르므로 대략치로 시작한다 — 부족하면 TArray가 알아서 늘어난다.
	Grid.BeginRebuild(256);

	EnemyQuery.ForEachEntityChunk(Context, [&Grid](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			Grid.AddEntity(Ctx.GetEntity(i), Transforms[i].GetTransform().GetLocation());
	});

	Grid.FinishRebuild();
}

// ============================================================
// ULNPEnemySeparationProcessor
// ============================================================

ULNPEnemySeparationProcessor::ULNPEnemySeparationProcessor()
	: SeparationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	// 격자가 다 지어진 뒤, 이동이 결과를 소비하기 전.
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemySpatialGridProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("LNPEnemyMovementProcessor"));
}

void ULNPEnemySeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SeparationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SeparationQuery.AddRequirement<FLNPEnemySeparationFragment>(EMassFragmentAccess::ReadWrite);
	SeparationQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);   // 넉백 중이면 비킨다
	SeparationQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);          // Actor면 캡슐이 이미 막는다
	SeparationQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	SeparationQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	SeparationQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	SeparationQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPEnemySpatialGridSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPEnemySeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const ULNPEnemySpatialGridSubsystem& Grid = Context.GetSubsystemChecked<ULNPEnemySpatialGridSubsystem>();

	SeparationQuery.ForEachEntityChunk(Context, [&Grid](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr)
			return;

		const float Radius   = Config->MovementConfig.SeparationRadius;
		const float Strength = Config->MovementConfig.SeparationStrength;

		const TConstArrayView<FTransformFragment>         Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TArrayView<FLNPEnemySeparationFragment>     Separations = Ctx.GetMutableFragmentView<FLNPEnemySeparationFragment>();
		const TConstArrayView<FLNPEnemyVelocityFragment>  Velocities  = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const TConstArrayView<FMassActorFragment>         ActorFrags  = Ctx.GetFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// 생산자가 매 프레임 값을 확정한다 — 밀지 않기로 한 개체에도 0을 명시적으로 쓴다.
			Separations[i].Push = FVector::ZeroVector;

			if (Radius <= 0.f || Strength <= 0.f)
				continue;

			// Actor로 그려지는 동안에는 캡슐 콜리전이 이미 겹침을 막는다. 여기서 또 밀면
			// 이동 의도 벡터에 크기 1 미만이 섞여 Mover의 속도 곱셈 붕괴를 밟는다.
			if (ActorFrags[i].Get() != nullptr)
				continue;

			// 넉백 중에는 공중 분기가 위치의 주인이다 — 두 힘이 같은 프레임에 다투게 두지 않는다.
			if (!Velocities[i].Velocity.IsNearlyZero())
				continue;

			const FMassEntityHandle Self     = Ctx.GetEntity(i);
			const FVector           Center   = Transforms[i].GetTransform().GetLocation();
			const FVector           UpDir    = (-Center).GetSafeNormal();   // 구 내벽 — Up은 월드 중심 방향
			const FVector           SelfFwd  = Transforms[i].GetTransform().GetRotation().GetForwardVector();

			FVector Accumulated = FVector::ZeroVector;

			Grid.ForEachNeighbor(Center, Radius, [&](const FMassEntityHandle Neighbor, const FVector& NeighborPos)
			{
				if (Neighbor == Self)
					return;

				// 거리는 **접평면 성분으로만** 잰다. 반지름 방향 차이(지형 높이차)는 걸어서 좁힐 수
				// 있는 거리가 아니므로 섞으면 위아래로 떨어진 개체까지 서로 밀어낸다.
				const FVector Delta   = Center - NeighborPos;
				const FVector Tangent = FVector::VectorPlaneProject(Delta, UpDir);
				const float   Dist    = Tangent.Size();
				if (Dist >= Radius)
					return;

				FVector PushDir;
				if (Dist > 1.f)
				{
					PushDir = Tangent / Dist;
				}
				else
				{
					// 정확히 겹친 쌍은 밀어낼 방향이 없다. 핸들 순서로 좌우를 갈라 **서로 반대로**
					// 흩어지게 한다 — 난수를 쓰면 두 개체가 같은 쪽을 골라 계속 붙어 있을 수 있다.
					const FVector Right = FVector::CrossProduct(UpDir, SelfFwd).GetSafeNormal();
					PushDir = (Self.Index < Neighbor.Index) ? Right : -Right;
				}

				// 가까울수록 강하게. 반경 밖에서 0이 되어 경계에서 튀지 않는다.
				Accumulated += PushDir * (1.f - Dist / Radius);
			});

			if (!Accumulated.IsNearlyZero())
			{
				// 무리 한가운데서 합이 무한정 커지지 않게 크기를 상한으로 접는다 —
				// 방향은 그대로 두고 세기만 자른다.
				Separations[i].Push = Accumulated.GetClampedToMaxSize(1.f) * Strength;
			}
		}
	});
}

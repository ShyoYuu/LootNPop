// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPDecorationSampler.h"
#include "LNPDecorationRuleData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGGraphExecutionStateInterface.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "LNPDecorationSampler"

DEFINE_LOG_CATEGORY_STATIC(LogLNPDecoration, Log, All);

namespace LNPDecorationSampler
{
	const FName DecorationTag(TEXT("LNP.Surface.Decoration"));
	// Blocker로 승격한 수동 장식은 Decoration 태그를 잃으므로 이 표식으로 간격 대상에 남긴다. 지형 계약 태그가 아니다.
	const FName ManualDecorationTag(TEXT("LNP.Graphics.ManualDecoration"));
	// 교차를 0과 구분하기 위한 음수. 여유가 0이어도 겹침은 반드시 거부된다.
	constexpr double OverlapDistance = -1.0;
	constexpr double NoConstraint = 1.0e9;

	/** RidgeOctantSpec.point/local과 같은 옥탄트 로컬 X/Y ↔ 구면 방향 변환. */
	struct FOctantFrame
	{
		double Radius = 0.0;
		FVector Radial = FVector(1.0, 1.0, 1.0) / FMath::Sqrt(3.0);
		FVector AxisX = FVector(-1.0, -1.0, 2.0) / FMath::Sqrt(6.0);
		FVector AxisY = FVector(-1.0, 1.0, 0.0) / FMath::Sqrt(2.0);

		FVector Direction(const double X, const double Y) const
		{
			return (Radial * Radius + AxisX * X + AxisY * Y).GetSafeNormal();
		}

		FVector2D Local(const FVector& P) const
		{
			const double Depth = FVector::DotProduct(P, Radial);
			return FVector2D(Radius * FVector::DotProduct(P, AxisX) / Depth, Radius * FVector::DotProduct(P, AxisY) / Depth);
		}
	};

	double PointSegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double Length2 = AB.SizeSquared();
		const double T = Length2 > 0.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Length2, 0.0, 1.0) : 0.0;
		return FVector2D::Distance(P, A + AB * T);
	}

	double Cross(const FVector2D& O, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
	}

	bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		const double D1 = Cross(C, D, A);
		const double D2 = Cross(C, D, B);
		const double D3 = Cross(A, B, C);
		const double D4 = Cross(A, B, D);
		return ((D1 > 0.0) != (D2 > 0.0)) && ((D3 > 0.0) != (D4 > 0.0));
	}

	bool PointInPolygon(const FVector2D& P, TConstArrayView<FVector2D> Polygon)
	{
		bool bInside = false;
		for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
		{
			const FVector2D& A = Polygon[I];
			const FVector2D& B = Polygon[J];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	double PointPolygonEdgeDistance(const FVector2D& P, TConstArrayView<FVector2D> Polygon)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
		{
			Best = FMath::Min(Best, PointSegmentDistance(P, Polygon[J], Polygon[I]));
		}
		return Best;
	}

	/** Andrew monotone chain. 메시 범위 8모서리를 로컬 평면에 투영한 발자국이다. */
	TArray<FVector2D> ConvexHull(TArray<FVector2D> Points)
	{
		Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X || (A.X == B.X && A.Y < B.Y); });
		TArray<FVector2D> Hull;
		Hull.SetNum(Points.Num() * 2);
		int32 K = 0;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			while (K >= 2 && Cross(Hull[K - 2], Hull[K - 1], Points[I]) <= 0.0) { --K; }
			Hull[K++] = Points[I];
		}
		for (int32 I = Points.Num() - 2, Lower = K + 1; I >= 0; --I)
		{
			while (K >= Lower && Cross(Hull[K - 2], Hull[K - 1], Points[I]) <= 0.0) { --K; }
			Hull[K++] = Points[I];
		}
		Hull.SetNum(FMath::Max(K - 1, 1));
		return Hull;
	}

	/** 발자국과 선분 사이 거리. 겹치면 OverlapDistance. */
	double HullSegmentDistance(TConstArrayView<FVector2D> Hull, const FVector2D& A, const FVector2D& B)
	{
		if (PointInPolygon(A, Hull) || PointInPolygon(B, Hull))
		{
			return OverlapDistance;
		}
		double Best = TNumericLimits<double>::Max();
		for (int32 I = 0, J = Hull.Num() - 1; I < Hull.Num(); J = I++)
		{
			if (SegmentsIntersect(Hull[J], Hull[I], A, B))
			{
				return OverlapDistance;
			}
			Best = FMath::Min(Best, PointSegmentDistance(Hull[I], A, B));
			Best = FMath::Min(Best, PointSegmentDistance(A, Hull[J], Hull[I]));
			Best = FMath::Min(Best, PointSegmentDistance(B, Hull[J], Hull[I]));
		}
		return Best;
	}

	/** 발자국과 다각형 사이 거리. 겹치면 OverlapDistance. */
	double HullPolygonDistance(TConstArrayView<FVector2D> Hull, TConstArrayView<FVector2D> Polygon)
	{
		for (const FVector2D& P : Hull)
		{
			if (PointInPolygon(P, Polygon)) { return OverlapDistance; }
		}
		for (const FVector2D& P : Polygon)
		{
			if (PointInPolygon(P, Hull)) { return OverlapDistance; }
		}
		double Best = TNumericLimits<double>::Max();
		for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
		{
			const double D = HullSegmentDistance(Hull, Polygon[J], Polygon[I]);
			if (D < 0.0) { return OverlapDistance; }
			Best = FMath::Min(Best, D);
		}
		return Best;
	}

	double KeepOut2DClearance(const FLNPDecorationKeepOut2D& Shape, TConstArrayView<FVector2D> Hull)
	{
		if (Shape.bClosedPolygon)
		{
			TArray<FVector2D> Polygon;
			for (const FVector& P : Shape.Points) { Polygon.Emplace(P.X, P.Y); }
			return HullPolygonDistance(Hull, Polygon) - Shape.Margin;
		}
		if (Shape.Points.Num() == 1)
		{
			const FVector2D C(Shape.Points[0].X, Shape.Points[0].Y);
			const double D = PointInPolygon(C, Hull) ? OverlapDistance : PointPolygonEdgeDistance(C, Hull);
			return D - Shape.Points[0].Z - Shape.Margin;
		}
		double Best = NoConstraint;
		for (int32 I = 1; I < Shape.Points.Num(); ++I)
		{
			const FVector& A = Shape.Points[I - 1];
			const FVector& B = Shape.Points[I];
			// 정점 사이 반폭은 선형 보간이지만, 보수적으로 두 정점 중 큰 값을 쓴다.
			const double Half = FMath::Max(A.Z, B.Z);
			Best = FMath::Min(Best, HullSegmentDistance(Hull, FVector2D(A.X, A.Y), FVector2D(B.X, B.Y)) - Half - Shape.Margin);
		}
		return Best;
	}

	/** 회전된 상자(중심·회전·반크기)와 점 사이 거리. 안이면 0. */
	double PointBoxDistance(const FVector& P, const FVector& Center, const FQuat& Rotation, const FVector& HalfExtent)
	{
		const FVector Local = Rotation.UnrotateVector(P - Center);
		const FVector Clamped(FMath::Clamp(Local.X, -HalfExtent.X, HalfExtent.X),
			FMath::Clamp(Local.Y, -HalfExtent.Y, HalfExtent.Y), FMath::Clamp(Local.Z, -HalfExtent.Z, HalfExtent.Z));
		return FVector::Distance(Local, Clamped);
	}

	double KeepOut3DClearance(const FLNPDecorationKeepOut3D& Shape, const FVector& Center, const FQuat& Rotation, const FVector& HalfExtent)
	{
		const double Length = FVector::Distance(Shape.Start, Shape.End);
		if (Length < UE_KINDA_SMALL_NUMBER)
		{
			return PointBoxDistance(Shape.Start, Center, Rotation, HalfExtent) - Shape.Radius;
		}
		// 캡슐 축을 표본화한다. 표본 간격의 절반만큼 보수적으로 빼서 표본 사이의 근접을 놓치지 않는다.
		const double Step = FMath::Clamp(Shape.Radius * 0.5, 10.0, 100.0);
		const int32 Count = FMath::CeilToInt(Length / Step);
		double Best = TNumericLimits<double>::Max();
		for (int32 I = 0; I <= Count; ++I)
		{
			const FVector P = FMath::Lerp(Shape.Start, Shape.End, double(I) / Count);
			Best = FMath::Min(Best, PointBoxDistance(P, Center, Rotation, HalfExtent));
		}
		return Best - Shape.Radius - (Length / Count) * 0.5;
	}

	enum class EReject : uint8
	{
		NoCrust, Slope, Zone, Seam, KeepOut2D, KeepOut3D, Existing, Contact, Covered, Overlap, Count
	};

	const TCHAR* RejectName(const EReject Reason)
	{
		static const TCHAR* Names[] = {TEXT("NoCrust"), TEXT("Slope"), TEXT("Zone"), TEXT("Seam"), TEXT("KeepOut2D"),
			TEXT("KeepOut3D"), TEXT("Existing"), TEXT("Contact"), TEXT("Covered"), TEXT("Overlap")};
		return Names[static_cast<uint8>(Reason)];
	}

	bool IsIdentity(const FTransform& Transform)
	{
		return Transform.GetLocation().IsNearlyZero(0.01) && Transform.GetRotation().IsIdentity(1.0e-6)
			&& Transform.GetScale3D().Equals(FVector::OneVector, 1.0e-4);
	}
}

TArray<FPCGPinProperties> ULNPDecorationSamplerSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> ULNPDecorationSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGElementPtr ULNPDecorationSamplerSettings::CreateElement() const
{
	return MakeShared<FLNPDecorationSamplerElement>();
}

bool FLNPDecorationSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace LNPDecorationSampler;

	const ULNPDecorationSamplerSettings* Settings = Context->GetInputSettings<ULNPDecorationSamplerSettings>();
	check(Settings);
	const ULNPDecorationRuleData* Rules = Settings->Rules;
	IPCGGraphExecutionSource* Source = Context->ExecutionSource.Get();
	UWorld* World = Source ? Source->GetExecutionState().GetWorld() : nullptr;
	if (Rules == nullptr || World == nullptr || Rules->Zone.Num() < 3)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingRules", "Rules with a zone polygon and a world are required."));
		return true;
	}

	UStaticMesh* CrustMesh = Rules->CrustMesh.LoadSynchronous();
	UStaticMeshComponent* Crust = nullptr;
	TArray<FBox> ExistingBounds;
	TArray<UPrimitiveComponent*> PreviousOutput;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(*It);
		for (UStaticMeshComponent* Component : Components)
		{
			if (CrustMesh != nullptr && Component->GetStaticMesh() == CrustMesh)
			{
				if (Crust != nullptr)
				{
					PCGE_LOG(Error, GraphAndLog, LOCTEXT("MultipleCrust", "More than one component uses the crust mesh."));
					return true;
				}
				Crust = Component;
			}
			// 수동 장식만 피한다. PCG가 만든 컴포넌트는 이번 실행이 다시 만든다.
			if (Component->ComponentTags.Contains(PCGHelpers::DefaultPCGTag))
			{
				// 이전 생성의 Blocker ISM이 정리 전에 남아 있어도 trace·overlap이 자기 결과에 막히지 않게 한다.
				PreviousOutput.Add(Component);
			}
			else if (Component->ComponentTags.Contains(DecorationTag) || Component->ComponentTags.Contains(ManualDecorationTag))
			{
				ExistingBounds.Add(Component->Bounds.GetBox().ExpandBy(Rules->ExistingDecorationClearance));
			}
		}
	}
	if (Crust == nullptr || !IsIdentity(Crust->GetComponentTransform()))
	{
		// 규칙 좌표는 회전 전 옥탄트 원본 레벨 기준이다. 회전된 LevelInstance 안에서 생성하면 좌표가 어긋난다.
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("CrustFrame", "Crust component is missing or not at identity; generate in the source octant level."));
		return true;
	}

	FOctantFrame Frame;
	Frame.Radius = Rules->OctantRadius;
	FBox2D ZoneBounds(Rules->Zone);
	TArray<FVector2D> Zone(Rules->Zone);
	const int32 Seed = Context->GetSeed();

	UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	UPCGMetadata* Metadata = OutPointData->Metadata;
	auto* MeshAttr = Metadata->FindOrCreateAttribute<FString>(TEXT("MeshPath"), FString(), false, true);
	auto* MaterialAttr = Metadata->FindOrCreateAttribute<FString>(TEXT("MaterialPath"), FString(), false, true);
	auto* RoleAttr = Metadata->FindOrCreateAttribute<FName>(TEXT("Role"), NAME_None, false, true);
	auto* SlopeAttr = Metadata->FindOrCreateAttribute<double>(TEXT("Slope"), 0.0, false, true);
	auto* ZoneAttr = Metadata->FindOrCreateAttribute<double>(TEXT("ZoneClearance"), 0.0, false, true);
	auto* Clear2DAttr = Metadata->FindOrCreateAttribute<double>(TEXT("Clearance2D"), 0.0, false, true);
	auto* Clear3DAttr = Metadata->FindOrCreateAttribute<double>(TEXT("Clearance3D"), 0.0, false, true);
	auto* FloatAttr = Metadata->FindOrCreateAttribute<double>(TEXT("CornerFloat"), 0.0, false, true);
	auto* SinkAttr = Metadata->FindOrCreateAttribute<double>(TEXT("CornerSink"), 0.0, false, true);
	// 그래프의 속성 필터(> 0)가 이 값으로 Blocker/Decoration Spawner를 나눈다.
	auto* BlockerAttr = Metadata->FindOrCreateAttribute<double>(TEXT("TerrainBlocker"), 0.0, false, true);
	// 그래프가 이 값으로 지피와 키 큰 식생을 나눠 겹침 우선순위를 정한다(키 큰 식생 > 바위 > 지피).
	auto* GroundCoverAttr = Metadata->FindOrCreateAttribute<double>(TEXT("GroundCover"), 0.0, false, true);
	TArray<FPCGPoint>& Points = OutPointData->GetMutablePoints();

	// 지각 trace는 런타임 지면 조회와 같은 단순 trace(UseComplexAsSimple 메시라 실제 삼각형을 맞힌다).
	const FCollisionQueryParams CrustParams(NAME_None, false);
	FCollisionQueryParams WorldParams(SCENE_QUERY_STAT(LNPDecorationSampler), true);
	WorldParams.AddIgnoredComponent(Crust);
	WorldParams.AddIgnoredComponents(PreviousOutput);
	const FCollisionObjectQueryParams StaticObjects(FCollisionObjectQueryParams::AllStaticObjects);
	const double TraceLength = Frame.Radius * 2.0;

	for (const FLNPDecorationRole& Role : Rules->Roles)
	{
		UStaticMesh* Mesh = Role.Mesh.LoadSynchronous();
		if (Mesh == nullptr || Role.Spacing <= 0.f)
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("BadRole", "Role {0} has no mesh or spacing."), FText::FromName(Role.Id)));
			continue;
		}
		UMaterialInterface* Material = Role.MaterialOverride.IsNull() ? Mesh->GetMaterial(0) : Role.MaterialOverride.LoadSynchronous();
		const FString MeshPath = Mesh->GetPathName();
		const FString MaterialPath = Material ? Material->GetPathName() : FString();
		const FBox MeshBox = Mesh->GetBoundingBox();
		const uint32 RoleHash = FCrc::StrCrc32(*Role.Id.ToString());

		int32 Candidates = 0;
		int32 Accepted = 0;
		int32 Rejects[static_cast<uint8>(EReject::Count)] = {};
		auto Reject = [&Rejects](const EReject Reason) { ++Rejects[static_cast<uint8>(Reason)]; };

		const int32 MinX = FMath::FloorToInt(ZoneBounds.Min.X / Role.Spacing);
		const int32 MaxX = FMath::FloorToInt(ZoneBounds.Max.X / Role.Spacing);
		const int32 MinY = FMath::FloorToInt(ZoneBounds.Min.Y / Role.Spacing);
		const int32 MaxY = FMath::FloorToInt(ZoneBounds.Max.Y / Role.Spacing);
		for (int32 Ix = MinX; Ix <= MaxX; ++Ix)
		{
			for (int32 Iy = MinY; Iy <= MaxY; ++Iy)
			{
				// 칸마다 독립 스트림이라 다른 역할·칸을 바꿔도 이 칸의 결과는 그대로다.
				uint32 Hash = HashCombine(GetTypeHash(Seed), RoleHash);
				Hash = HashCombine(Hash, GetTypeHash(Ix));
				Hash = HashCombine(Hash, GetTypeHash(Iy));
				FRandomStream Random(static_cast<int32>(Hash));
				const bool bTry = Random.FRand() < Role.Probability;
				const double X = (Ix + Random.FRand()) * Role.Spacing;
				const double Y = (Iy + Random.FRand()) * Role.Spacing;
				const double Yaw = Random.FRandRange(0.0, 360.0);
				const double Scale = FMath::Lerp(Role.ScaleRange.X, Role.ScaleRange.Y, Random.FRand());
				if (!bTry || !PointInPolygon(FVector2D(X, Y), Zone))
				{
					continue;
				}
				++Candidates;

				// 구 내벽 세계: 중심에서 바깥으로 쏜 첫 지각 히트가 플레이 표면이다.
				const FVector Direction = Frame.Direction(X, Y);
				FHitResult CrustHit;
				if (!Crust->LineTraceComponent(CrustHit, FVector::ZeroVector, Direction * TraceLength, CrustParams))
				{
					Reject(EReject::NoCrust);
					continue;
				}
				const FVector Ground = CrustHit.ImpactPoint;
				const FVector Up = -Ground.GetSafeNormal();
				FVector Normal = CrustHit.ImpactNormal;
				if (FVector::DotProduct(Normal, Up) < 0.0) { Normal = -Normal; }
				const double Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal, Up), -1.0, 1.0)));
				if (Slope < Role.SlopeRange.X || Slope > Role.SlopeRange.Y)
				{
					Reject(EReject::Slope);
					continue;
				}

				// 메시 Z는 현지 Up과 지각 법선 사이, X는 옥탄트 로컬 X축 기준 회전.
				const FVector ZAxis = FMath::Lerp(Up, Normal, double(Role.NormalAlignment)).GetSafeNormal();
				const FVector Reference = (Frame.AxisX - ZAxis * FVector::DotProduct(Frame.AxisX, ZAxis)).GetSafeNormal();
				const FVector XAxis = Reference.RotateAngleAxis(Yaw, ZAxis);
				const FQuat Rotation = FRotationMatrix::MakeFromZX(ZAxis, XAxis).ToQuat();
				const FVector Location = Ground + ZAxis * (-MeshBox.Min.Z * Scale - Role.Sink);

				// 메시 전체 범위: 스케일·회전 반영, WPO 여유는 옆과 위로만 넓힌다.
				const FVector Pad(Role.WindPadding, Role.WindPadding, 0.0);
				const FVector LocalMin = MeshBox.Min * Scale - Pad;
				const FVector LocalMax = MeshBox.Max * Scale + FVector(Role.WindPadding);
				const FVector LocalCenter = (LocalMin + LocalMax) * 0.5;
				const FVector HalfExtent = (LocalMax - LocalMin) * 0.5;
				const FVector Center = Location + Rotation.RotateVector(LocalCenter);

				TArray<FVector2D> Projected;
				FBox WorldBox(ForceInit);
				double MinComponent = TNumericLimits<double>::Max();
				for (int32 Corner = 0; Corner < 8; ++Corner)
				{
					const FVector LocalCorner((Corner & 1) ? LocalMax.X : LocalMin.X, (Corner & 2) ? LocalMax.Y : LocalMin.Y,
						(Corner & 4) ? LocalMax.Z : LocalMin.Z);
					const FVector WorldCorner = Location + Rotation.RotateVector(LocalCorner);
					Projected.Add(Frame.Local(WorldCorner));
					WorldBox += WorldCorner;
					MinComponent = FMath::Min(MinComponent, WorldCorner.GetMin());
				}
				const TArray<FVector2D> Hull = ConvexHull(Projected);

				double ZoneClearance = NoConstraint;
				bool bZoneOk = true;
				for (const FVector2D& P : Hull)
				{
					bZoneOk &= PointInPolygon(P, Zone);
					ZoneClearance = FMath::Min(ZoneClearance, PointPolygonEdgeDistance(P, Zone) - Rules->ZoneInset);
				}
				for (int32 I = 0, J = Zone.Num() - 1; bZoneOk && I < Zone.Num(); J = I++)
				{
					bZoneOk = HullSegmentDistance(Hull, Zone[J], Zone[I]) >= 0.0;
				}
				if (!bZoneOk || ZoneClearance < 0.0)
				{
					Reject(EReject::Zone);
					continue;
				}
				if (MinComponent < Rules->SeamClearance)
				{
					Reject(EReject::Seam);
					continue;
				}

				double Clearance2D = NoConstraint;
				for (const FLNPDecorationKeepOut2D& Shape : Rules->KeepOut2D)
				{
					if (!(Shape.bAllowGroundCover && Role.bGroundCover))
					{
						Clearance2D = FMath::Min(Clearance2D, KeepOut2DClearance(Shape, Hull));
					}
				}
				if (Clearance2D < double(Role.KeepOut2DPadding))
				{
					Reject(EReject::KeepOut2D);
					continue;
				}
				double Clearance3D = NoConstraint;
				for (const FLNPDecorationKeepOut3D& Shape : Rules->KeepOut3D)
				{
					if (!(Shape.bAllowGroundCover && Role.bGroundCover))
					{
						Clearance3D = FMath::Min(Clearance3D, KeepOut3DClearance(Shape, Center, Rotation, HalfExtent));
					}
				}
				if (Clearance3D < 0.0)
				{
					Reject(EReject::KeepOut3D);
					continue;
				}
				if (ExistingBounds.ContainsByPredicate([&WorldBox](const FBox& Box) { return Box.Intersect(WorldBox); }))
				{
					Reject(EReject::Existing);
					continue;
				}

				// 바닥 4모서리·중심을 현지 Up으로 지각에 대 본다. 양수는 뜸, 음수는 묻힘.
				double MaxFloat = -TNumericLimits<double>::Max();
				double MaxSink = 0.0;
				bool bContact = true;
				for (int32 Corner = 0; Corner < 5 && bContact; ++Corner)
				{
					const FVector LocalBottom = Corner == 4 ? FVector(MeshBox.GetCenter().X, MeshBox.GetCenter().Y, MeshBox.Min.Z) * Scale
						: FVector((Corner & 1) ? MeshBox.Max.X : MeshBox.Min.X, (Corner & 2) ? MeshBox.Max.Y : MeshBox.Min.Y, MeshBox.Min.Z) * Scale;
					const FVector Bottom = Location + Rotation.RotateVector(LocalBottom);
					FHitResult ContactHit;
					const double Reach = 300.0 + Role.MaxCornerSink + Role.MaxCornerFloat;
					bContact = Crust->LineTraceComponent(ContactHit, Bottom + Up * Reach, Bottom - Up * Reach, CrustParams);
					if (bContact)
					{
						const double Gap = FVector::DotProduct(Bottom - ContactHit.ImpactPoint, Up);
						MaxFloat = FMath::Max(MaxFloat, Gap);
						MaxSink = FMath::Max(MaxSink, -Gap);
					}
				}
				if (!bContact || MaxFloat > Role.MaxCornerFloat || MaxSink > Role.MaxCornerSink)
				{
					Reject(EReject::Contact);
					continue;
				}

				// 지각 돌출·암반 지붕 아래가 아닌지, 범위가 암반·차단 메시와 겹치지 않는지 본다(장식은 NoCollision이라 무시된다).
				FHitResult SkyHit;
				if (World->LineTraceSingleByObjectType(SkyHit, Ground + Up * Rules->OpenSkyHeight, Ground + Up * 5.0, StaticObjects, WorldParams)
					|| Crust->LineTraceComponent(SkyHit, Ground + Up * Rules->OpenSkyHeight, Ground + Up * 5.0, CrustParams))
				{
					Reject(EReject::Covered);
					continue;
				}
				if (World->OverlapAnyTestByObjectType(Center, Rotation, StaticObjects, FCollisionShape::MakeBox(HalfExtent), WorldParams))
				{
					Reject(EReject::Overlap);
					continue;
				}

				FPCGPoint& Point = Points.Emplace_GetRef(FTransform(Rotation, Location, FVector(Scale)), 1.f, static_cast<int32>(Hash));
				// 점 범위는 스케일 전 로컬 단위다. 뒤의 Self Pruning이 WPO 여유를 포함한 범위로 겹침을 지운다.
				Point.BoundsMin = LocalMin / Scale;
				Point.BoundsMax = LocalMax / Scale;
				Metadata->InitializeOnSet(Point.MetadataEntry);
				MeshAttr->SetValue(Point.MetadataEntry, MeshPath);
				MaterialAttr->SetValue(Point.MetadataEntry, MaterialPath);
				RoleAttr->SetValue(Point.MetadataEntry, Role.Id);
				SlopeAttr->SetValue(Point.MetadataEntry, Slope);
				ZoneAttr->SetValue(Point.MetadataEntry, ZoneClearance);
				Clear2DAttr->SetValue(Point.MetadataEntry, Clearance2D);
				Clear3DAttr->SetValue(Point.MetadataEntry, Clearance3D);
				FloatAttr->SetValue(Point.MetadataEntry, MaxFloat);
				SinkAttr->SetValue(Point.MetadataEntry, MaxSink);
				BlockerAttr->SetValue(Point.MetadataEntry, Role.bTerrainBlocker ? 1.0 : 0.0);
				GroundCoverAttr->SetValue(Point.MetadataEntry, Role.bGroundCover ? 1.0 : 0.0);
				++Accepted;
			}
		}

		FString Summary;
		for (uint8 Reason = 0; Reason < static_cast<uint8>(EReject::Count); ++Reason)
		{
			Summary += FString::Printf(TEXT(" %s=%d"), RejectName(static_cast<EReject>(Reason)), Rejects[Reason]);
		}
		UE_LOG(LogLNPDecoration, Log, TEXT("Role %s seed %d: candidates=%d accepted=%d rejected:%s"),
			*Role.Id.ToString(), Seed, Candidates, Accepted, *Summary);
	}

	Context->OutputData.TaggedData.Emplace_GetRef().Data = OutPointData;
	return true;
}

#undef LOCTEXT_NAMESPACE

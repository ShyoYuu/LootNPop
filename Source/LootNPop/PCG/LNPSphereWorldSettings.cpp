/// 구체(Sphere) 형태의 지형(Landscape)을 수많은 Plane들의 조합으로 만들어내는 PCG 노드.
/// 수백만개의 Plane을 HISM으로 생성하여 프록시 메시로 에디터 플레이가 가능한 것까지는 확인.
/// 그러나 HLOD 베이킹 등을 위해 실제 메시까지 생성하는건 성능상 무리라고 판단되어 이 노드는 안쓰는 것으로 최종 결정.
/// 자세한 히스토리는 @DiscardedApproaches.md Case 01과 @TechDesign_WorldGeneration.md 참고

#include "PCG/LNPSphereWorldSettings.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#define LOCTEXT_NAMESPACE "LNPSphereWorldSettingsElement"

#if WITH_EDITOR
// 노드의 내부 식별 이름.
FName ULNPSphereWorldSettings::GetDefaultNodeName() const
{
	return FName(TEXT("LNPSphereWorldSettings"));
}

// 그래프 에디터에 표시되는 노드 이름. 공백 포함 가능.
FText ULNPSphereWorldSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "LNP Sphere World Generator");
}

// 노드의 기본 툴팁
FText ULNPSphereWorldSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates a sphere of points based on mathematical formulas.");
}
#endif //WITH_EDITOR

TArray<FPCGPinProperties> ULNPSphereWorldSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

// ExecuteInternal에서 사용할 Element를 생성한다.
FPCGElementPtr ULNPSphereWorldSettings::CreateElement() const
{
	return MakeShared<FLNPSphereWorldSettingsElement>();
}

bool FLNPSphereWorldSettingsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLNPSphereWorldSettingsElement::Execute);

	check(Context);

	const ULNPSphereWorldSettings* Settings = Context->GetInputSettings<ULNPSphereWorldSettings>();
	check(Settings);

	// 1. 파티셔닝용 목표 Bounds 계산
	FBox Bounds(ForceInit);
	for (const FPCGTaggedData& Input : Context->InputData.TaggedData)
	{
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
		{
			Bounds = SpatialData->GetBounds();
			break;
		}
	}
	const bool bIsPartitioned = (Bounds.IsValid != 0);

	// 2. Point Data 초기화
	UPCGPointData* SampledData = NewObject<UPCGPointData>();
	SampledData->InitializeFromData(nullptr);
	TArray<FPCGPoint>& Points = SampledData->GetMutablePoints();

	const float R = Settings->Radius;
	const float S = Settings->TileSize;
	const float F = Settings->OverlapFactor;
	const float Amp = Settings->NoiseAmplitude;
	const float Freq = Settings->NoiseFrequency;
	const FVector Seed = Settings->SeedOffset;

	// 정규화된 방향 벡터에서 노이즈 높이를 반환하는 람다
	auto GetNoiseHeight = [&](const FVector& NormalDir) {
		// Seed를 더해 월드 패턴을 변경
		return FMath::PerlinNoise3D((NormalDir + Seed) * R * Freq) * Amp;
	};

	// 3. 최적화: 셀이 구체 껍질과 교차하는지 확인
	if (bIsPartitioned)
	{
		const float DistSq = Bounds.ComputeSquaredDistanceToPoint(FVector::ZeroVector);
		const float OuterR = R + S + Amp; // 노이즈 진폭 포함
		const float InnerR = FMath::Max(0.0f, R - S - Amp);

		// 너무 멀거나 너무 가까운 경우 (속이 빈 구체 내부)
		if (DistSq > OuterR * OuterR || (Bounds.IsInside(FVector::ZeroVector) == false && Bounds.ComputeSquaredDistanceToPoint(FVector::ZeroVector) < InnerR * InnerR))
		{
			goto Finalize; 
		}
	}

	{
		// 4. 피보나치 구체 분포 (황금 나선)
		const int32 N = FMath::Max(1, FMath::RoundToInt((4.0f * PI * R * R) / (S * S)));

		// 루프 인덱스 범위 설정
		int32 MinI = 0;
		int32 MaxI = N - 1;

		// 5. 파티셔닝을 위한 위도(Z) 범위 최적화
		if (bIsPartitioned && N > 1)
		{
			// 안전한 클리핑을 위해 Z 범위를 진폭만큼 확장
			const float MinZ = FMath::Clamp(Bounds.Min.Z - Amp, -R, R);
			const float MaxZ = FMath::Clamp(Bounds.Max.Z + Amp, -R, R);

			float iForMaxZ = (1.0f - MaxZ / R) * (N - 1) * 0.5f;
			float iForMinZ = (1.0f - MinZ / R) * (N - 1) * 0.5f;

			MinI = FMath::Max(0, FMath::FloorToInt(FMath::Min(iForMaxZ, iForMinZ) - 1));
			MaxI = FMath::Min(N - 1, FMath::CeilToInt(FMath::Max(iForMaxZ, iForMinZ) + 1));
		}

		const float GoldenAngle = 2.39996323f; // PI * (3 - sqrt(5))

		for (int32 i = MinI; i <= MaxI; ++i)
		{
			float z_norm = (N > 1) ? (1.0f - (static_cast<float>(i) / (N - 1)) * 2.0f) : 1.0f;
			float radius_at_z = FMath::Sqrt(FMath::Max(0.0f, 1.0f - z_norm * z_norm));
			float theta = GoldenAngle * i;

			FVector Direction(radius_at_z * FMath::Cos(theta), radius_at_z * FMath::Sin(theta), z_norm);
			
			// 노이즈에서 높이 계산
			float Height = GetNoiseHeight(Direction);
			FVector Position = Direction * (R + Height);

			// 최종 확인: 현재 파티셔닝 Bounds 내부에 있는 경우만 추가
			if (bIsPartitioned && !Bounds.IsInside(Position))
			{
				continue;
			}
			
			// --- 경사 정렬 (법선 계산) ---
			// 인접한 2개 점을 샘플링하여 경사도 계산
			const float Delta = 0.01f;
			FVector TangentX, TangentY;
			Direction.FindBestAxisVectors(TangentX, TangentY);

			FVector P1 = (Direction + TangentX * Delta).GetSafeNormal();
			FVector P2 = (Direction + TangentY * Delta).GetSafeNormal();

			float H1 = GetNoiseHeight(P1);
			float H2 = GetNoiseHeight(P2);

			FVector Pos1 = P1 * (R + H1);
			FVector Pos2 = P2 * (R + H2);

			// 새 표면 법선 계산 (경사 Normal)
			FVector SlopeNormal = FVector::CrossProduct(Pos1 - Position, Pos2 - Position).GetSafeNormal();
			
			// 법선이 내부(중심 방향)를 향하도록 보정
			if (FVector::DotProduct(SlopeNormal, Direction) > 0)
			{
				SlopeNormal = -SlopeNormal;
			}

			// 정렬 기준용 위도 방향의 "수평"(동쪽) 방향 계산
			FVector EastDir = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
			if (EastDir.IsNearlyZero())
				EastDir = FVector::RightVector;

			// Z가 경사 Normal, Y가 위도 방향에 정렬된 회전 생성
			FRotator Rotation = FRotationMatrix::MakeFromZY(SlopeNormal, EastDir).Rotator();

			FPCGPoint& Point = Points.Emplace_GetRef();
			float FinalScale = (S / 100.f) * F;
			Point.Transform = FTransform(Rotation, Position, FVector(FinalScale, FinalScale, 0.1f));
			Point.Seed = GetTypeHash(Position);
			Point.Density = 1.0f;
		}
	}

Finalize:
	// 6. 데이터 출력
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = SampledData;

	return true;
}

#undef LOCTEXT_NAMESPACE
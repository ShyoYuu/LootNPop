/// 구체(Sphere) 형태의 지형(Landscape)을 수많은 Plane들의 조합으로 만들어내는 PCG 노드.
/// 수백만개의 Plane을 HISM으로 생성하여 프록시 메시로 에디터 플레이가 가능한 것까지는 확인.
/// 그러나 HLOD 베이킹 등을 위해 실제 메시까지 생성하는건 성능상 무리라고 판단되어 이 노드는 안쓰는 것으로 최종 결정.
/// 자세한 히스토리는 @DiscardedApproaches.md Case 01과 @TechDesign_WorldGeneration.md 참고

#include "PCG/PCGSphereWorldSettings.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#define LOCTEXT_NAMESPACE "PCGSphereWorldSettingsElement"

#if WITH_EDITOR
// The label the node is known as internally.
FName UPCGSphereWorldSettings::GetDefaultNodeName() const
{
	return FName(TEXT("PCGSphereWorldSettings"));
}

// Default node name shown in the graph editor. Include spaces.
FText UPCGSphereWorldSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Sphere World Generator");
}

// Default tooltip for the node
FText UPCGSphereWorldSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates a sphere of points based on mathematical formulas.");
}
#endif //WITH_EDITOR

TArray<FPCGPinProperties> UPCGSphereWorldSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

// Creates the Element to be used for ExecuteInternal.
FPCGElementPtr UPCGSphereWorldSettings::CreateElement() const
{
	return MakeShared<FPCGSphereWorldSettingsElement>();
}

bool FPCGSphereWorldSettingsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSphereWorldSettingsElement::Execute);

	check(Context);

	const UPCGSphereWorldSettings* Settings = Context->GetInputSettings<UPCGSphereWorldSettings>();
	check(Settings);

	// 1. Get Target Bounds for Partitioning
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

	// 2. Initialize Point Data
	UPCGPointData* SampledData = NewObject<UPCGPointData>();
	SampledData->InitializeFromData(nullptr);
	TArray<FPCGPoint>& Points = SampledData->GetMutablePoints();

	const float R = Settings->Radius;
	const float S = Settings->TileSize;
	const float F = Settings->OverlapFactor;
	const float Amp = Settings->NoiseAmplitude;
	const float Freq = Settings->NoiseFrequency;
	const FVector Seed = Settings->SeedOffset;

	// Lambda to get noise height at a normalized direction
	auto GetNoiseHeight = [&](const FVector& NormalDir) {
		// Shift coordinates by Seed to change the world pattern
		return FMath::PerlinNoise3D((NormalDir + Seed) * R * Freq) * Amp;
	};

	// 3. Optimization: Check if cell even intersects with the sphere shell
	if (bIsPartitioned)
	{
		const float DistSq = Bounds.ComputeSquaredDistanceToPoint(FVector::ZeroVector);
		const float OuterR = R + S + Amp; // Include noise amplitude
		const float InnerR = FMath::Max(0.0f, R - S - Amp);

		// Too far or too close (inside the hollow sphere)
		if (DistSq > OuterR * OuterR || (Bounds.IsInside(FVector::ZeroVector) == false && Bounds.ComputeSquaredDistanceToPoint(FVector::ZeroVector) < InnerR * InnerR))
		{
			goto Finalize; 
		}
	}

	{
		// 4. Fibonacci Sphere Distribution (Golden Spiral)
		const int32 N = FMath::Max(1, FMath::RoundToInt((4.0f * PI * R * R) / (S * S)));

		// Define Index Range for loop
		int32 MinI = 0;
		int32 MaxI = N - 1;

		// 5. Latitude (Z) Range Optimization for Partitioning
		if (bIsPartitioned && N > 1)
		{
			// Expand Z range by Amplitude for safe clipping
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
			
			// Calculate height from noise
			float Height = GetNoiseHeight(Direction);
			FVector Position = Direction * (R + Height);

			// Final Check: Only add if inside current partitioning bounds
			if (bIsPartitioned && !Bounds.IsInside(Position))
			{
				continue;
			}
			
			// --- Slope Alignment (Normal Calculation) ---
			// Sample 2 nearby points to find the slope
			const float Delta = 0.01f;
			FVector TangentX, TangentY;
			Direction.FindBestAxisVectors(TangentX, TangentY);

			FVector P1 = (Direction + TangentX * Delta).GetSafeNormal();
			FVector P2 = (Direction + TangentY * Delta).GetSafeNormal();

			float H1 = GetNoiseHeight(P1);
			float H2 = GetNoiseHeight(P2);

			FVector Pos1 = P1 * (R + H1);
			FVector Pos2 = P2 * (R + H2);

			// Calculate new surface normal (Slope Normal)
			FVector SlopeNormal = FVector::CrossProduct(Pos1 - Position, Pos2 - Position).GetSafeNormal();
			
			// Ensure normal points INWARD (towards center)
			if (FVector::DotProduct(SlopeNormal, Direction) > 0)
			{
				SlopeNormal = -SlopeNormal;
			}

			// Calculate "Horizontal" (East) direction along the latitude for alignment
			FVector EastDir = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
			if (EastDir.IsNearlyZero())
				EastDir = FVector::RightVector;

			// Create rotation where Z is the slope normal and Y is aligned with latitude
			FRotator Rotation = FRotationMatrix::MakeFromZY(SlopeNormal, EastDir).Rotator();

			FPCGPoint& Point = Points.Emplace_GetRef();
			float FinalScale = (S / 100.f) * F;
			Point.Transform = FTransform(Rotation, Position, FVector(FinalScale, FinalScale, 0.1f));
			Point.Seed = GetTypeHash(Position);
			Point.Density = 1.0f;
		}
	}

Finalize:
	// 6. Output Data
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = SampledData;

	return true;
}

#undef LOCTEXT_NAMESPACE
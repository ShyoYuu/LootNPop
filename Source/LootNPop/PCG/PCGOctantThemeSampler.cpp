#include "PCGOctantThemeSampler.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAccessor.h"

#define LOCTEXT_NAMESPACE "PCGOctantThemeSampler"

TArray<FPCGPinProperties> UPCGOctantThemeSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> UPCGOctantThemeSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGElementPtr UPCGOctantThemeSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGOctantThemeSamplerElement>();
}

bool FPCGOctantThemeSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGOctantThemeSamplerSettings* Settings = Context->GetInputSettings<UPCGOctantThemeSamplerSettings>();
	check(Settings);

	// Validate theme data before processing
	if (Settings->ThemeData == nullptr || Settings->ThemeData->PropEntries.Num() == 0)
	{
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Fallback to Actor inputs if no specific pin data is found
	if (Inputs.Num() == 0)
	{
		TArray<FPCGTaggedData> ActorInputs = UPCGDataFunctionLibrary::GetInputsByTag(Context->InputData, TEXT("Actor"));
		if (ActorInputs.Num() > 0)
		{
			Inputs.Add(ActorInputs[0]);
		}
	}

	if (Inputs.Num() == 0)
	{
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (SpatialData == nullptr)
		{
			continue;
		}

		// 1. Calculate Sampling Area and Density
		// For an octant (1/8 of a sphere) starting at origin, the max size of the bounds is the radius.
		const FBox SpatialBounds = SpatialData->GetBounds();
		float Radius = SpatialBounds.GetSize().GetMax();
		if (Radius < 10.0f)
		{
			Radius = 1000.0f; // Default fallback radius
		}

		// Calculate the surface area of the octant: (4 * PI * R^2) / 8 = 0.5 * PI * R^2
		// Note: Using 0.1f as a multiplier based on user preference for density control.
		const float RadiusInMeters = Radius / 100.0f;
		const float OctantAreaM2 = 0.1f * PI * FMath::Square(RadiusInMeters);
		int32 NumSamples = FMath::CeilToInt(OctantAreaM2 * Settings->SamplingDensity);
		NumSamples = FMath::Clamp(NumSamples, 1, 20000);

		// Initialize output point data
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(SpatialData);
		
		TArray<FPCGPoint> ResultPoints;
		UPCGMetadata* Metadata = OutPointData->Metadata;
		FPCGMetadataAttribute<FString>* MeshAttr = Metadata->FindOrCreateAttribute<FString>(TEXT("MeshPath"), TEXT(""), true);

		// Pre-calculate total weight for mesh selection
		float TotalWeight = 0.0f;
		for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
		{
			TotalWeight += Entry.Weight;
		}

		FRandomStream RandomSource(Settings->Seed);

		// 2. Prepare Point Cloud for Projection
		// We use the point data (which already has Perlin noise applied) as the projection target.
		const UPCGPointData* InPointData = SpatialData->ToPointData(Context);
		if (!InPointData || InPointData->GetPoints().Num() == 0)
		{
			continue;
		}

		// 3. Main Sampling Loop
		for (int32 i = 0; i < NumSamples; ++i)
		{
			// --- STEP A: Uniform Directional Sampling ---
			// Generate a uniform random direction within the octant (+X, +Y, +Z quadrant)
			const float Phi = RandomSource.FRandRange(0.0f, PI * 0.5f);
			const float CosTheta = RandomSource.FRand(); 
			const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
			const FVector LocalDir(SinTheta * FMath::Cos(Phi), SinTheta * FMath::Sin(Phi), CosTheta);

			// --- STEP B: Inside-Out Projection ---
			// Start from inside the sphere and project outwards. 
			// In a sphere world with 1m thickness, this ensures we hit the INNER wall first.
			const FVector LocalRayStart = LocalDir * (Radius * 0.5f); 
			const FQuat LocalRayRotation = FRotationMatrix::MakeFromZ(LocalDir).ToQuat();
			const FTransform LocalRayTransform(LocalRayRotation, LocalRayStart);

			FPCGPoint ProjectedPoint;
			FPCGProjectionParams ProjParams;

			// Search box aligned with LocalDir. We look forward from RayStart along the radius.
			const FBox SearchBox(FVector(-50, -50, 0), FVector(50, 50, Radius * 1.5f));
			
			if (InPointData->ProjectPoint(LocalRayTransform, SearchBox, ProjParams, ProjectedPoint, Metadata))
			{
				const FVector SurfaceLocation = ProjectedPoint.Transform.GetLocation();
				const int32 PointSeed = PCGHelpers::ComputeSeedFromPosition(SurfaceLocation);
				FRandomStream PointRandom(PointSeed ^ Settings->Seed);

				// --- STEP C: Surface Alignment and Pivot Correction ---
				// Center of the sphere world is at (0,0,0). Up direction is towards the center.
				const FVector ToCenter = -SurfaceLocation.GetSafeNormal();

				// The mesh has 1m thickness. ProjectPoint hits the middle/outer layers.
				// We offset by 50 units towards the center to place the prop pivot flush on the inner surface.
				const FVector FinalLocation = SurfaceLocation + (ToCenter * 50.0f);
				ProjectedPoint.Transform.SetLocation(FinalLocation);

				// Align Mesh Z (Up) perfectly to Sphere Center
				const FQuat AlignRot = FRotationMatrix::MakeFromZ(ToCenter).ToQuat();
				const float RandomYaw = PointRandom.FRandRange(0.0f, Settings->RandomRotationMax);
				const FQuat FinalRot = AlignRot * FQuat(FVector::UpVector, FMath::DegreesToRadians(RandomYaw));
				ProjectedPoint.Transform.SetRotation(FinalRot);

				// --- STEP D: Appearance and Metadata ---
				// Random Scale based on settings
				const float ScaleAlpha = PointRandom.FRand();
				ProjectedPoint.Transform.SetScale3D(FMath::Lerp(Settings->MinScale, Settings->MaxScale, ScaleAlpha));

				// Weighted Mesh Selection
				float RandomWeight = PointRandom.FRandRange(0.0f, TotalWeight);
				float CurrentWeight = 0.0f;
				FString SelectedMeshPath = TEXT("");

				for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
				{
					CurrentWeight += Entry.Weight;
					if (RandomWeight <= CurrentWeight)
					{
						if (Entry.Mesh)
						{
							SelectedMeshPath = Entry.Mesh->GetPathName();
						}
						break;
					}
				}

				// Finalize point metadata
				Metadata->InitializeOnSet(ProjectedPoint.MetadataEntry);
				MeshAttr->SetValue(ProjectedPoint.MetadataEntry, SelectedMeshPath);
				ResultPoints.Add(ProjectedPoint);
			}
		}

		OutPointData->SetPoints(ResultPoints);
		Outputs.Add_GetRef(Input).Data = OutPointData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

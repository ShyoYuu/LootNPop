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

	if (Settings->ThemeData == nullptr || Settings->ThemeData->PropEntries.Num() == 0)
	{
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	if (Inputs.Num() == 0)
	{
		TArray<FPCGTaggedData> ActorInputs = UPCGDataFunctionLibrary::GetInputsByTag(Context->InputData, TEXT("Actor"));
		if (ActorInputs.Num() > 0)
			Inputs.Add(ActorInputs[0]);
	}

	if (Inputs.Num() == 0)
		return true;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (SpatialData == nullptr)
			continue;

		FBox SpatialBounds = SpatialData->GetBounds();
		
		// 1. Correct Radius Calculation for Octant
		// The bounds of an octant starting from origin give the radius directly via GetSize().
		float Radius = SpatialBounds.GetSize().GetMax();
		if (Radius < 10.0f)
			Radius = 1000.0f; 

		// Area-based Density Calculation
		float RadiusInMeters = Radius / 100.0f;
		float OctantAreaM2 = 0.5f * PI * FMath::Square(RadiusInMeters);
		int32 NumSamples = FMath::CeilToInt(OctantAreaM2 * Settings->SamplingDensity);
		NumSamples = FMath::Clamp(NumSamples, 1, 20000);

		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(SpatialData);
		
		TArray<FPCGPoint> ResultPoints;
		UPCGMetadata* Metadata = OutPointData->Metadata;
		FPCGMetadataAttribute<FString>* MeshAttr = Metadata->FindOrCreateAttribute<FString>(TEXT("MeshPath"), TEXT(""), true);

		float TotalWeight = 0.0f;
		for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
			TotalWeight += Entry.Weight;

		FRandomStream RandomSource(Settings->Seed);

		// 2. Get the noisy surface points
		// We use ToPointData to handle any spatial input. For the noisy sphere world,
		// this will contain the points already deformed by noise.
		const UPCGPointData* InPointData = SpatialData->ToPointData(Context);
		if (!InPointData || InPointData->GetPoints().Num() == 0)
			continue;

		const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();

		for (int32 i = 0; i < NumSamples; ++i)
		{
			// 3. Directly sample from the terrain points to ensure we respect the noise-deformed surface.
			const FPCGPoint& SourcePoint = InPoints[RandomSource.RandRange(0, InPoints.Num() - 1)];
			
			FPCGPoint ProjectedPoint = SourcePoint;
			FVector LocalHitLocation = ProjectedPoint.Transform.GetLocation();

			const int32 PointSeed = PCGHelpers::ComputeSeedFromPosition(LocalHitLocation);
			FRandomStream PointRandom(PointSeed ^ Settings->Seed);

			// 4. Explicitly align UpDirection to Sphere Center (0,0,0)
			// In our inside-out world, 'Up' is the vector from the surface point to the center.
			FVector ToCenter = -LocalHitLocation.GetSafeNormal();
			
			// We prioritize alignment to the center. 
			// If the source point already has a slope-aware rotation, we try to respect it 
			// while ensuring its Z-axis still generally points towards the center.
			FQuat AlignRot = ProjectedPoint.Transform.GetRotation();
			if (FVector::DotProduct(AlignRot.GetUpVector(), ToCenter) < 0.5f)
			{
				// If source rotation is missing or invalid, create a new one pointing to center
				AlignRot = FRotationMatrix::MakeFromZ(ToCenter).ToQuat();
			}

			float RandomYaw = PointRandom.FRandRange(0.0f, Settings->RandomRotationMax);
			// Apply random yaw around the newly calculated Up axis (Z)
			FQuat FinalRot = AlignRot * FQuat(FVector::UpVector, FMath::DegreesToRadians(RandomYaw));
			ProjectedPoint.Transform.SetRotation(FinalRot);

			// Scale & Metadata
			float ScaleAlpha = PointRandom.FRand();
			ProjectedPoint.Transform.SetScale3D(FMath::Lerp(Settings->MinScale, Settings->MaxScale, ScaleAlpha));

			float RandomWeight = PointRandom.FRandRange(0.0f, TotalWeight);
			float CurrentWeight = 0.0f;
			FString SelectedMeshPath = TEXT("");

			for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
			{
				CurrentWeight += Entry.Weight;
				if (RandomWeight <= CurrentWeight)
				{
					if (Entry.Mesh)
						SelectedMeshPath = Entry.Mesh->GetPathName();
					break;
				}
			}

			Metadata->InitializeOnSet(ProjectedPoint.MetadataEntry);
			MeshAttr->SetValue(ProjectedPoint.MetadataEntry, SelectedMeshPath);
			ResultPoints.Add(ProjectedPoint);
		}

		OutPointData->SetPoints(ResultPoints);
		Outputs.Add_GetRef(Input).Data = OutPointData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

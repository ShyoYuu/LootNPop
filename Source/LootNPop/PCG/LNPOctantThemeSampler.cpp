#include "LNPOctantThemeSampler.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGVolumeSampler.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAccessor.h"

#define LOCTEXT_NAMESPACE "LNPOctantThemeSampler"

TArray<FPCGPinProperties> ULNPOctantThemeSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> ULNPOctantThemeSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGElementPtr ULNPOctantThemeSamplerSettings::CreateElement() const
{
	return MakeShared<FLNPOctantThemeSamplerElement>();
}

bool FLNPOctantThemeSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	const ULNPOctantThemeSamplerSettings* Settings = Context->GetInputSettings<ULNPOctantThemeSamplerSettings>();
	check(Settings);

	// 처리 전 Theme 데이터 유효성 확인
	if (Settings->ThemeData == nullptr || Settings->ThemeData->PropEntries.Num() == 0)
	{
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// 핀 데이터가 없는 경우 Actor 입력으로 대체
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

		// 1. 샘플링 면적과 밀도 계산
		// 원점에서 시작하는 Octant(구체의 1/8)의 경우, Bounds 최대 크기는 반지름과 같다.
		const FBox SpatialBounds = SpatialData->GetBounds();
		float Radius = SpatialBounds.GetSize().GetMax();
		if (Radius < 10.0f)
		{
			Radius = 1000.0f; // 기본 Fallback 반지름
		}

		// Octant 표면적 계산: (4 * PI * R^2) / 8 = 0.5 * PI * R^2
		// 밀도 제어를 위해 0.1f 배수를 사용한다.
		const float RadiusInMeters = Radius / 100.0f;
		const float OctantAreaM2 = 0.1f * PI * FMath::Square(RadiusInMeters);
		int32 NumSamples = FMath::CeilToInt(OctantAreaM2 * Settings->SamplingDensity);
		NumSamples = FMath::Clamp(NumSamples, 1, 20000);

		// 출력 Point Data 초기화
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(SpatialData);
		
		TArray<FPCGPoint> ResultPoints;
		UPCGMetadata* Metadata = OutPointData->Metadata;
		FPCGMetadataAttribute<FString>* MeshAttr = Metadata->FindOrCreateAttribute<FString>(TEXT("MeshPath"), TEXT(""), true);

		// 메시 선택을 위한 총 가중치 사전 계산
		float TotalWeight = 0.0f;
		for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
		{
			TotalWeight += Entry.Weight;
		}

		FRandomStream RandomSource(Settings->Seed);

		// 2. 투영용으로 메시 표면을 Point Cloud으로 샘플링.
		// PCG 파티션 Actor의 거대한 Z 범위를 상속받으면 ToPointData()에서 NumZ Overflow가 발생하므로
		// 반지름 기반 Bounds를 사용하여 PCGVolumeSampler를 직접 호출한다.
		PCGVolumeSampler::FVolumeSamplerParams SamplerParams;
		FBox SafeBounds = SpatialBounds;
		SafeBounds.Min.Z = FMath::Max(SpatialBounds.Min.Z, -Radius);
		SafeBounds.Max.Z = FMath::Min(SpatialBounds.Max.Z,  (Radius * 2.0));
		SamplerParams.VoxelSize = FVector(Settings->SamplingVoxelSize);
		SamplerParams.Bounds    = SafeBounds;
		const UPCGPointData* InPointData = Cast<UPCGPointData>(
			PCGVolumeSampler::SampleVolume(Context, UPCGPointData::StaticClass(), SamplerParams, SpatialData));
		if (!InPointData || InPointData->GetPoints().Num() == 0)
		{
			continue;
		}

		// 3. 메인 샘플링 루프
		for (int32 i = 0; i < NumSamples; ++i)
		{
			// --- STEP A: 균등 방향 샘플링 ---
			// Octant 내부(+X, +Y, +Z 사분면)에서 균등한 임의 방향 생성
			const float Phi = RandomSource.FRandRange(0.0f, PI * 0.5f);
			const float CosTheta = RandomSource.FRand(); 
			const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
			const FVector LocalDir(SinTheta * FMath::Cos(Phi), SinTheta * FMath::Sin(Phi), CosTheta);

			// --- STEP B: 내부→외부 투영 ---
			// 구체 내부에서 시작하여 외부 방향으로 투영한다.
			// 두께 1m의 구체 세계에서 내벽에 먼저 닿도록 보장한다.
			const FVector LocalRayStart = LocalDir * (Radius * 0.5f); 
			const FQuat LocalRayRotation = FRotationMatrix::MakeFromZ(LocalDir).ToQuat();
			const FTransform LocalRayTransform(LocalRayRotation, LocalRayStart);

			FPCGPoint ProjectedPoint;
			FPCGProjectionParams ProjParams;

			// LocalDir 방향으로 정렬된 SearchBox. RayStart에서 반지름 방향으로 전방을 탐색한다.
			const FBox SearchBox(FVector(-50, -50, 0), FVector(50, 50, Radius * 1.5f));
			
			if (InPointData->ProjectPoint(LocalRayTransform, SearchBox, ProjParams, ProjectedPoint, Metadata))
			{
				const FVector SurfaceLocation = ProjectedPoint.Transform.GetLocation();
				const int32 PointSeed = PCGHelpers::ComputeSeedFromPosition(SurfaceLocation);
				FRandomStream PointRandom(PointSeed ^ Settings->Seed);

				// --- STEP C: 표면 정렬 및 Pivot 보정 ---
				// 구체 세계의 중심은 (0,0,0). Up 방향은 중심을 향한다.
				const FVector ToCenter = -SurfaceLocation.GetSafeNormal();

				// 메시 두께는 1m. ProjectPoint는 중간/외부 레이어에 닿는다.
				// Prop Pivot이 내벽 표면에 딱 맞도록 중심 방향으로 50유닛 오프셋한다.
				const FVector FinalLocation = SurfaceLocation + (ToCenter * 50.0f);
				ProjectedPoint.Transform.SetLocation(FinalLocation);

				// 메시 Z (Up)을 구체 중심 방향으로 완벽하게 정렬
				const FQuat AlignRot = FRotationMatrix::MakeFromZ(ToCenter).ToQuat();
				const float RandomYaw = PointRandom.FRandRange(0.0f, Settings->RandomRotationMax);
				const FQuat FinalRot = AlignRot * FQuat(FVector::UpVector, FMath::DegreesToRadians(RandomYaw));
				ProjectedPoint.Transform.SetRotation(FinalRot);

				// --- STEP D: 외형 및 Metadata ---
				const float ScaleAlpha = PointRandom.FRand(); // 랜덤 스트림 순서 유지를 위해 먼저 샘플링

				// 가중치 기반 메시 선택
				float RandomWeight = PointRandom.FRandRange(0.0f, TotalWeight);
				float CurrentWeight = 0.0f;
				FString SelectedMeshPath = TEXT("");
				FVector SelectedMinScale(0.8f);
				FVector SelectedMaxScale(1.2f);

				for (const FLNPPropEntry& Entry : Settings->ThemeData->PropEntries)
				{
					CurrentWeight += Entry.Weight;
					if (RandomWeight <= CurrentWeight)
					{
						if (Entry.Mesh)
						{
							SelectedMeshPath = Entry.Mesh->GetPathName();
						}
						SelectedMinScale = Entry.MinScale;
						SelectedMaxScale = Entry.MaxScale;
						break;
					}
				}

				ProjectedPoint.Transform.SetScale3D(FMath::Lerp(SelectedMinScale, SelectedMaxScale, ScaleAlpha));

				// Point Metadata Finalize
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

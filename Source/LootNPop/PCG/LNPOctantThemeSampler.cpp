#include "LNPOctantThemeSampler.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Components/PrimitiveComponent.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPrimitiveData.h"
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

		// 2. 투영 대상 확보 — 지각 콜리전에 직접 라인트레이스한다.
		// ⚠ 복셀 점군(PCGVolumeSampler) 투영을 쓰면 안 된다 — 점이 월드 원점 기준 축정렬 격자에 얹히고,
		//   UPCGBasePointData::ProjectPoint는 최근접이 아니라 겹침 볼륨 가중 평균이라 착지점이 양자화된다.
		// PCG의 Actor 입력은 메시가 아니라 UPrimitiveComponent를 감싼 UPCGPrimitiveData다.
		// 파티션/Union으로 감싸여 올 수 있으므로 엔진 헬퍼로 concrete shape까지 내려간다.
		const UPCGPrimitiveData* PrimitiveData = Cast<UPCGPrimitiveData>(SpatialData);
		if (PrimitiveData == nullptr)
		{
			PrimitiveData = Cast<UPCGPrimitiveData>(SpatialData->FindFirstConcreteShapeFromNetwork());
		}

		UPrimitiveComponent* CrustComponent = (PrimitiveData != nullptr) ? PrimitiveData->GetComponent().Get() : nullptr;
		if (CrustComponent == nullptr)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPrimitiveInput", "Input data has no primitive component to trace against."));
			continue;
		}

		// SurfaceCache의 지면 조회와 같은 조합(bTraceComplex=false). SM_Octant는 UseComplexAsSimple이라
		// 단순 트레이스가 실제 삼각형을 맞히고, 두 시스템이 같은 면을 기준으로 삼게 된다.
		const FCollisionQueryParams TraceParams(NAME_None, /*bInTraceComplex=*/false);

		// 3. 메인 샘플링 루프
		for (int32 i = 0; i < NumSamples; ++i)
		{
			// --- STEP A: 균등 방향 샘플링 ---
			// Octant 내부(+X, +Y, +Z 사분면)에서 균등한 임의 방향 생성
			const float Phi = RandomSource.FRandRange(0.0f, PI * 0.5f);
			const float CosTheta = RandomSource.FRand(); 
			const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
			const FVector LocalDir(SinTheta * FMath::Cos(Phi), SinTheta * FMath::Sin(Phi), CosTheta);

			// --- STEP B: 내부→외부 라인트레이스 ---
			// 구 내벽 세계라 플레이 표면은 지각의 안쪽 면이다 — 중심에서 바깥으로 쏜 첫 히트가 지면이다.
			// 종점은 Radius(입력 Bounds의 최대 변 = 실제 반지름 이상)의 2배라 지각 바깥임이 보장된다.
			FHitResult Hit;
			if (CrustComponent->LineTraceComponent(Hit, FVector::ZeroVector, LocalDir * (Radius * 2.0f), TraceParams))
			{
				const FVector SurfaceLocation = Hit.ImpactPoint;
				const int32 PointSeed = PCGHelpers::ComputeSeedFromPosition(SurfaceLocation);
				FRandomStream PointRandom(PointSeed ^ Settings->Seed);

				FPCGPoint ProjectedPoint;
				ProjectedPoint.Density = 1.0f;
				ProjectedPoint.Seed = PointSeed;

				// --- STEP C: 표면 정렬 및 Pivot 보정 ---
				// 구체 세계의 중심은 (0,0,0). Up 방향은 중심을 향한다.
				const FVector ToCenter = -SurfaceLocation.GetSafeNormal();

				// 위치 확정은 STEP D로 미룬다 — 접지 보정에 선택된 메시의 Bounds와 스케일이 필요하다.

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
				const UStaticMesh* SelectedMesh = nullptr;
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
							SelectedMesh = Entry.Mesh;
						}
						SelectedMinScale = Entry.MinScale;
						SelectedMaxScale = Entry.MaxScale;
						break;
					}
				}

				const FVector FinalScale = FMath::Lerp(SelectedMinScale, SelectedMaxScale, ScaleAlpha);
				ProjectedPoint.Transform.SetScale3D(FinalScale);

				// --- 접지 보정 ---
				// 메시 로컬 Bounds의 최저점까지의 거리만큼 Up으로 밀어 바닥이 표면에 닿게 한다.
				// 바닥 피벗 메시(현재 프랍 전부)는 0이 되어 그대로 붙고, 중심 피벗 메시는 반높이만큼 올라온다.
				// ⚠ 고정 상수를 쓰면 안 된다 — 예전의 50유닛은 두께 1m 지각을 전제한 값이라
				// 지각이 단면이 되고 메시가 바뀌는 순간 그대로 뜨는 높이가 된다.
				const float PivotToBottom = (SelectedMesh != nullptr) ? -SelectedMesh->GetBoundingBox().Min.Z : 0.0f;
				ProjectedPoint.Transform.SetLocation(SurfaceLocation + ToCenter * (PivotToBottom * FinalScale.Z));

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

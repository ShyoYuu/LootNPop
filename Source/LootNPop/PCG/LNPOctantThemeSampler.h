// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PCGSettings.h"
#include "DataAsset/LNPOctantThemeData.h"
#include "LNPOctantThemeSampler.generated.h"

/**
 * 표면을 샘플링하고 Theme 데이터 가중치를 기반으로 Prop을 배치하는 커스텀 PCG 노드.
 */
UCLASS(BlueprintType, MinimalAPI)
class ULNPOctantThemeSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("OctantThemeSampler"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("LNPOctantThemeSampler", "NodeTitle", "Octant Theme Sampler"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("LNPOctantThemeSampler", "NodeTooltip", "Samples surface and assigns theme-based meshes with weights."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<ULNPOctantThemeData> ThemeData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.001"))
	float SamplingDensity = 0.1f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float RandomRotationMax = 360.0f;

	// 메시 표면을 Point Cloud으로 샘플링할 때 사용하는 복셀 크기.
	// 값이 클수록 메모리는 줄어들지만 표면 정밀도가 낮아진다. 기본값 200cm는 대형 구체 세계에 적합.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0"))
	float SamplingVoxelSize = 200.0f;
};

class FLNPOctantThemeSamplerElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

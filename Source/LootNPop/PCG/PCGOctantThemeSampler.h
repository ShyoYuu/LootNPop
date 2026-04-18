// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PCGSettings.h"
#include "DataAsset/LNPOctantThemeData.h"
#include "PCGOctantThemeSampler.generated.h"

/**
 * Custom PCG node that samples a surface and assigns props based on theme data weights.
 */
UCLASS(BlueprintType, MinimalAPI)
class UPCGOctantThemeSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("OctantThemeSampler"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGOctantThemeSampler", "NodeTitle", "Octant Theme Sampler"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGOctantThemeSampler", "NodeTooltip", "Samples surface and assigns theme-based meshes with weights."); }
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
	FVector MinScale = FVector(0.8f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector MaxScale = FVector(1.2f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float RandomRotationMax = 360.0f;
};

class FPCGOctantThemeSamplerElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

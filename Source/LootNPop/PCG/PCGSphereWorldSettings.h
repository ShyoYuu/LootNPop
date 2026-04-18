/// 구체(Sphere) 형태의 지형(Landscape)을 수많은 Plane들의 조합으로 만들어내는 PCG 노드.
/// 수백만개의 Plane을 HISM으로 생성하여 프록시 메시로 에디터 플레이가 가능한 것까지는 확인.
/// 그러나 HLOD 베이킹 등을 위해 실제 메시까지 생성하는건 성능상 무리라고 판단되어 이 노드는 안쓰는 것으로 최종 결정.
/// 자세한 히스토리는 @DiscardedApproaches.md Case 01과 @TechDesign_WorldGeneration.md 참고

#pragma once

#include "PCGSettings.h"
#include "PCGSphereWorldSettings.generated.h"

/**
* Add your tooltip here
*/
UCLASS(MinimalAPI, BlueprintType)
class UPCGSphereWorldSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif //WITH_EDITOR

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMax = "1000000.0"))
	float Radius = 50000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "100.0"))
	float TileSize = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float OverlapFactor = 1.05f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float NoiseAmplitude = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float NoiseFrequency = 0.001f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector SeedOffset = FVector::ZeroVector;
};

class FPCGSphereWorldSettingsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

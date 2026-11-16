// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "LNPDecorationSampler.generated.h"

class ULNPDecorationRuleData;

/**
 * 구면 옥탄트의 대표 구역에 장식 후보를 만드는 커스텀 PCG 노드.
 * 옥탄트 로컬 X/Y 지터 격자 → 실제 지각 trace → 구면 현지 Up/법선 정렬 → 메시 전체 범위(회전·스케일·WPO 여유) 보호 검사.
 * 엔진 표면 샘플러는 월드 Z를 Up으로 가정하므로 쓰지 않는다. 겹침 제거와 ISM 생성은 뒤의 엔진 노드가 맡는다.
 */
UCLASS(BlueprintType, MinimalAPI)
class ULNPDecorationSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("LNPDecorationSampler"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("LNPDecorationSampler", "NodeTitle", "LNP Decoration Sampler"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("LNPDecorationSampler", "NodeTooltip", "Samples a spherical octant zone and rejects candidates whose full mesh bounds violate keep-out rules."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// 엔진 기본(false)이면 노드·컴포넌트 시드를 무시하고 고정값 42를 쓴다. 컴포넌트 시드로 재생성을 제어한다.
	virtual bool UseSeed() const override { return true; }
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TObjectPtr<ULNPDecorationRuleData> Rules;
};

class FLNPDecorationSamplerElement : public IPCGElement
{
public:
	// 월드 trace·overlap·액터 순회·에셋 로드를 하므로 메인 스레드에서만 돈다.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	// 결과가 레벨 상태(지각·암반·수동 장식)에 의존하므로 캐시하지 않는다.
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

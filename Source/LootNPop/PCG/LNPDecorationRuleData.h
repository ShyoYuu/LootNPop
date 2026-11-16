// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPDecorationRuleData.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * 옥탄트 로컬 X/Y 평면의 보호 형상.
 * 열린 형상은 정점(X, Y)을 잇는 통로이고 Z는 그 정점의 반폭이다. 정점 1개면 원이다.
 * 닫힌 형상은 다각형(Z 무시)이다. 장식 메시 전체 범위가 Margin 안으로 들어오면 거부한다.
 */
USTRUCT(BlueprintType)
struct FLNPDecorationKeepOut2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TArray<FVector> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	bool bClosedPolygon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float Margin = 0.f;

	/** true면 지피 역할(bGroundCover)은 이 형상 안에도 들어올 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	bool bAllowGroundCover = false;
};

/** 옥탄트 월드(레벨) 좌표의 3D 보호 형상. Start == End면 구, 아니면 캡슐이다. */
USTRUCT(BlueprintType)
struct FLNPDecorationKeepOut3D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float Radius = 0.f;

	/** true면 지피 역할(bGroundCover)은 이 형상 안에도 들어올 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	bool bAllowGroundCover = false;
};

/** 장식 역할 하나의 배치 규칙. 지터 격자 칸마다 한 번 시도한다. */
USTRUCT(BlueprintType)
struct FLNPDecorationRole
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** 비우면 메시의 기본 슬롯 재질을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/** 옥탄트 로컬 X/Y의 격자 칸 크기(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "10.0"))
	float Spacing = 1000.f;

	/** 칸마다 후보를 만들 확률. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Probability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FVector2D ScaleRange = FVector2D(1.0, 1.0);

	/** 구면 현지 Up 대비 실제 지각 법선의 허용 경사(도). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	FVector2D SlopeRange = FVector2D(0.0, 30.0);

	/** 메시 바닥을 지각 아래로 묻는 깊이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	float Sink = 0.f;

	/** WPO 흔들림을 위한 옆·위 방향 범위 여유(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float WindPadding = 0.f;

	/** 바닥 모서리가 지각 위로 뜰 수 있는 최대 높이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float MaxCornerFloat = 10.f;

	/** 바닥 모서리가 지각 아래로 묻힐 수 있는 최대 깊이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float MaxCornerSink = 30.f;

	/** 엄폐·실루엣을 만들지 않는 낮은 지피. Pod 편성 공간처럼 허용한 보호 형상 안에도 둘 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	bool bGroundCover = false;

	/** 0이면 구면 현지 Up, 1이면 실제 지각 법선에 메시 Z를 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NormalAlignment = 1.f;
};

/**
 * 대표 구역 장식 배치 규칙. 구역·보호 형상은 Ridge 형상 spec에서 재생성 스크립트가 채운다.
 * 좌표는 옥탄트 원본 레벨(회전 전) 기준이다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPDecorationRuleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 접지 trace 대상인 지각 메시. 레벨에서 이 메시를 쓰는 컴포넌트를 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TSoftObjectPtr<UStaticMesh> CrustMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	double OctantRadius = 30000.0;

	/** 옥탄트 로컬 X/Y의 배치 구역 다각형. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TArray<FVector2D> Zone;

	/** 메시 전체 범위가 구역 경계에서 떨어져야 하는 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float ZoneInset = 0.f;

	/** 메시 전체 범위의 모든 월드 좌표 성분이 이 값 이상이어야 한다(이음매 보존 띠). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	float SeamClearance = 0.f;

	/** 이미 배치된 수동 장식(비 PCG)의 범위에서 떨어져야 하는 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float ExistingDecorationClearance = 0.f;

	/** 바로 위에 덮개(암반·지각 돌출)가 없어야 하는 높이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration", meta = (ClampMin = "0.0"))
	float OpenSkyHeight = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TArray<FLNPDecorationKeepOut2D> KeepOut2D;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TArray<FLNPDecorationKeepOut3D> KeepOut3D;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Decoration")
	TArray<FLNPDecorationRole> Roles;
};

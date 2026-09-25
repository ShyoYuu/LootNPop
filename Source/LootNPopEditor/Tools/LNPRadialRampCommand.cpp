// Copyright (c) 2026 LootNPop. All rights reserved.

// 지각 메시에 "지각과 한 몸인 경사 언덕"을 목표 높이 프로필로 직접 만드는 에디터 명령.
//
// Sphere Height Sculpt 브러시를 자동으로 긋는 방식(LNP.MeshTerrain.SphereSculpt)은 단면과 경사를 동시에 맞추기 어려워,
// 정점을 목표 반지름으로 직접 옮긴다. 월드 중심 기준 구면 좌표에서 계산하므로 경사는 방사 방향 기준이다.
//
// 좌표:
// - 시작 방향 S(Start의 방향)와 진행 방향 점 T가 이루는 대원을 경로로 쓴다.
// - Along: 정점 방향을 대원 평면에 투영했을 때 S에서 T 쪽으로 잰 호 길이(시작 반지름 기준).
// - Cross: 정점 방향이 대원 평면에서 벗어난 호 길이.
// 목표 반지름(작을수록 높은 지면):
//   Top(a)    = Start 반지름 + max(0, a) × tan(Slope)          (a < 0은 Behind까지 시작 높이 유지)
//   Excess    = max(0, |c| − Width/2) + max(0, −a − Behind)
//   Target    = Top(a) + Excess × tan(SideSlope)
// 지면은 올리기만 한다(반지름을 줄이기만 한다). 원래 지면이 Target보다 높으면 그대로 두어 발끝과 옆면이 지형과 만난다.
//
// 예: LNP.MeshTerrain.RadialRamp Actor=BP_Octant_Meadow_00 Start=16164,5883,22824 Toward=19579,7126,21884
//         Slope=25 Width=500 SideSlope=35 Behind=50 Commit=0

#include "Editor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPRadialRamp, Log, All);

namespace LNPRadialRamp
{
	bool ParseVector(const FString& Text, FVector& Out)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			return false;
		}
		Out = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
		return true;
	}

	UStaticMeshComponent* FindComponent(UWorld& World, const FString& ActorLabel)
	{
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->GetActorLabel() != ActorLabel)
			{
				continue;
			}
			TInlineComponentArray<UStaticMeshComponent*> Components(*It);
			for (UStaticMeshComponent* Component : Components)
			{
				// PCG 프랍 ISM·HISM을 빼고 정확히 일반 StaticMeshComponent(지각)만 고른다.
				if (Component->GetClass() == UStaticMeshComponent::StaticClass())
				{
					return Component;
				}
			}
		}
		return nullptr;
	}

	double GetOption(const TMap<FString, FString>& Options, const TCHAR* Key, double Default)
	{
		const FString* Value = Options.Find(Key);
		return Value ? FCString::Atod(**Value) : Default;
	}

	void Run(const TArray<FString>& Args)
	{
		TMap<FString, FString> Options;
		for (const FString& Arg : Args)
		{
			FString Key, Value;
			if (Arg.Split(TEXT("="), &Key, &Value))
			{
				Options.Add(Key.ToLower(), Value);
			}
		}

		FVector Start = FVector::ZeroVector;
		FVector Toward = FVector::ZeroVector;
		const FString ActorLabel = Options.FindRef(TEXT("actor"));
		if (ActorLabel.IsEmpty()
			|| !Options.Contains(TEXT("start")) || !ParseVector(Options[TEXT("start")], Start)
			|| !Options.Contains(TEXT("toward")) || !ParseVector(Options[TEXT("toward")], Toward))
		{
			UE_LOG(LogLNPRadialRamp, Error, TEXT("[RadialRamp] Usage: Actor=<label> Start=x,y,z Toward=x,y,z [Slope=deg] [Width=cm] [SideSlope=deg] [Behind=cm] [Commit=0|1]"));
			return;
		}
		const double TanSlope = FMath::Tan(FMath::DegreesToRadians(GetOption(Options, TEXT("slope"), 25.0)));
		const double HalfWidth = GetOption(Options, TEXT("width"), 500.0) * 0.5;
		const double TanSide = FMath::Tan(FMath::DegreesToRadians(GetOption(Options, TEXT("sideslope"), 35.0)));
		const double Behind = GetOption(Options, TEXT("behind"), 50.0);
		const bool bCommit = GetOption(Options, TEXT("commit"), 1.0) != 0.0;

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UStaticMeshComponent* Component = World ? FindComponent(*World, ActorLabel) : nullptr;
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		FMeshDescription* MeshDescription = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
		if (MeshDescription == nullptr)
		{
			UE_LOG(LogLNPRadialRamp, Error, TEXT("[RadialRamp] No editable StaticMesh LOD 0 on actor '%s'"), *ActorLabel);
			return;
		}

		const double StartRadius = Start.Size();
		const FVector S = Start.GetSafeNormal();
		const FVector TowardDir = Toward.GetSafeNormal();
		const FVector PlaneNormal = FVector::CrossProduct(S, TowardDir).GetSafeNormal();
		const FVector Heading = FVector::CrossProduct(PlaneNormal, S).GetSafeNormal();   // S에서 Toward 쪽 접선
		if (PlaneNormal.IsNearlyZero())
		{
			UE_LOG(LogLNPRadialRamp, Error, TEXT("[RadialRamp] Start and Toward must point in different directions"));
			return;
		}

		const FTransform ComponentToWorld = Component->GetComponentTransform();
		FStaticMeshAttributes Attributes(*MeshDescription);
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();

		int32 Raised = 0;
		double MaxRaise = 0.0;
		double MinAlong = TNumericLimits<double>::Max();
		double MaxAlong = -TNumericLimits<double>::Max();
		TArray<TPair<FVertexID, FVector3f>> Updates;
		for (const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs())
		{
			const FVector WorldPos = ComponentToWorld.TransformPosition(FVector(Positions[VertexID]));
			const double Radius = WorldPos.Size();
			if (Radius < UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector U = WorldPos / Radius;

			const double CrossAngle = FMath::Asin(FMath::Clamp(FVector::DotProduct(U, PlaneNormal), -1.0, 1.0));
			const FVector InPlane = (U - PlaneNormal * FVector::DotProduct(U, PlaneNormal)).GetSafeNormal();
			const double AlongAngle = FMath::Atan2(FVector::DotProduct(InPlane, Heading), FVector::DotProduct(InPlane, S));
			// 경로 반대편 반구로 넘어가는 정점은 대상이 아니다.
			if (FMath::Abs(AlongAngle) > UE_HALF_PI)
			{
				continue;
			}
			const double Along = AlongAngle * StartRadius;
			const double Cross = CrossAngle * StartRadius;

			const double Top = StartRadius + FMath::Max(0.0, Along) * TanSlope;
			const double Excess = FMath::Max(0.0, FMath::Abs(Cross) - HalfWidth) + FMath::Max(0.0, -Along - Behind);
			const double Target = Top + Excess * TanSide;
			if (Target >= Radius)
			{
				continue;
			}

			++Raised;
			MaxRaise = FMath::Max(MaxRaise, Radius - Target);
			MinAlong = FMath::Min(MinAlong, Along);
			MaxAlong = FMath::Max(MaxAlong, Along);
			const FVector NewLocal = ComponentToWorld.InverseTransformPosition(U * Target);
			Updates.Emplace(VertexID, FVector3f(NewLocal));
		}

		if (bCommit && Raised > 0)
		{
			Mesh->Modify();
			for (const TPair<FVertexID, FVector3f>& Update : Updates)
			{
				Positions[Update.Key] = Update.Value;
			}
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals(*MeshDescription);
			FStaticMeshOperations::ComputeTangentsAndNormals(*MeshDescription,
				EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents | EComputeNTBsFlags::WeightedNTBs);
			Mesh->CommitMeshDescription(0);
			Mesh->Build();
			Mesh->PostEditChange();
			Mesh->MarkPackageDirty();
		}

		UE_LOG(LogLNPRadialRamp, Display,
			TEXT("[RadialRamp] %s %s StartRadius=%.0f Slope=%.1f HalfWidth=%.0f | Raised=%d MaxRaise=%.1fcm Along=[%.0f, %.0f]cm | %s"),
			*ActorLabel, *Mesh->GetName(), StartRadius, FMath::RadiansToDegrees(FMath::Atan(TanSlope)), HalfWidth,
			Raised, MaxRaise, Raised > 0 ? MinAlong : 0.0, Raised > 0 ? MaxAlong : 0.0,
			bCommit ? TEXT("Committed (asset dirty, save it)") : TEXT("Dry run"));
	}

	static FAutoConsoleCommand Command(
		TEXT("LNP.MeshTerrain.RadialRamp"),
		TEXT("Raise crust vertices onto a radial ramp profile (world-center spherical coordinates). ")
		TEXT("Actor=<label> Start=x,y,z Toward=x,y,z [Slope=deg] [Width=cm] [SideSlope=deg] [Behind=cm] [Commit=0|1]. ")
		TEXT("Only raises ground toward the center; the toe and sides meet existing terrain."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

// Copyright (c) 2026 LootNPop. All rights reserved.

// Mesh Terrain Sphere Height Sculpt를 콘솔에서 경로 스트로크로 적용하는 에디터 명령.
//
// 브러시를 손으로 긋지 않고도 같은 도구(UHeightSculptTool, 기준면 Sphere)를 구동한다. 스트로크는 구 중심에서
// 경로 위 점을 향하는 ray의 연속이며, 도구가 자기 대상 메시에만 hit test를 하므로 사이의 부유섬에 가리지 않는다.
// 내부형 구에서 "지면을 올린다"는 구 중심 쪽으로 미는 것이라 Invert=1이 기본 용도다.
//
// 기준면 두 가지(Reference=):
// - Sphere(기본): 정점을 구 중심에서의 방사 방향으로 민다. 단 엔진이 브러시 영역을 "구 중심에서 메시 bounds 최대 변
//   길이까지"의 원기둥으로 제한하므로(MeshVertexSculptTool.cpp CylinderOnSphere) 중심에서 그보다 먼 정점은 움직이지 않는다.
//   지각(bounds 30,000cm)은 반지름 30,000cm 안쪽만, 섬처럼 작은 메시는 전혀 편집되지 않는다.
// - Plane: 정점을 평면 법선 방향으로 평행하게 민다. 영역이 무한 원기둥이라 위 제한이 없다. 법선은 기본적으로 스트로크 중간점의
//   방사 바깥 방향이라, 국소 편집에서는 방사 방향과의 차이가 (편집 폭 / 반지름) 정도에 그친다.
//
// 예: LNP.MeshTerrain.SphereSculpt Actor=BP_Octant_Meadow_00 Brush=Sculpt Radius=800 Strength=0.5 Invert=1
//         From=16164,5883,22824 To=19579,7126,21884 Steps=40 Passes=1 Commit=0

#include "Editor.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"
#include "Materials/Material.h"
#include "MeshPartitionHeightSculptTool.h"
#include "Sculpting/MeshBrushOpBase.h"
#include "ToolContextInterfaces.h"
#include "ToolTargetManager.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSphereSculpt, Log, All);

namespace LNPSphereSculpt
{
	// UMeshSurfacePointTool::CtrlModifier(protected). Ctrl 토글이 곧 스트로크 반전이다(UMeshSculptToolBase::SaveActiveStrokeModifiers).
	constexpr int32 CtrlModifierID = 2;

	class FQueries final : public IToolsContextQueriesAPI
	{
	public:
		UWorld* World = nullptr;
		UStaticMeshComponent* SelectedComponent = nullptr;
		UInteractiveToolsContext* ToolsContext = nullptr;

		virtual UWorld* GetCurrentEditingWorld() const override { return World; }

		virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const override
		{
			StateOut.World = World;
			StateOut.ToolManager = ToolsContext->ToolManager;
			StateOut.TargetManager = ToolsContext->TargetManager;
			StateOut.GizmoManager = ToolsContext->GizmoManager;
			StateOut.SelectedComponents.Add(SelectedComponent);
		}

		virtual void GetCurrentViewState(FViewCameraState& StateOut) const override
		{
			StateOut.Position = FVector::ZeroVector;
			StateOut.Orientation = FQuat::Identity;
		}

		virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials MaterialType) const override
		{
			return UMaterial::GetDefaultMaterial(MD_Surface);
		}

		virtual FViewport* GetHoveredViewport() const override { return nullptr; }
		virtual FViewport* GetFocusedViewport() const override { return nullptr; }
	};

	class FTransactions final : public IToolsContextTransactionsAPI
	{
	public:
		virtual void DisplayMessage(const FText& Message, EToolMessageLevel Level) override {}
		virtual void PostInvalidation() override {}
		virtual void BeginUndoTransaction(const FText& Description) override {}
		virtual void EndUndoTransaction() override {}
		virtual void CancelUndoTransaction() override {}
		virtual void AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change, const FText& Description) override {}
		virtual bool RequestSelectionChange(const FSelectedObjectsChangeList& SelectionChange) override { return false; }
	};

	TArray<FVector3d> CaptureVertices(UMeshVertexSculptTool* Tool)
	{
		TArray<FVector3d> Positions;
		Tool->UpdateToolMeshes([&Positions](UE::Geometry::FDynamicMesh3& Mesh, int32)
		{
			Positions.Reserve(Mesh.VertexCount());
			for (const int32 VertexID : Mesh.VertexIndicesItr())
			{
				Positions.Add(Mesh.GetVertex(VertexID));
			}
			return TUniquePtr<FMeshRegionChangeBase>();
		});
		return Positions;
	}

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

	UStaticMeshComponent* FindComponent(UWorld& World, const FString& ActorLabel, const FString& ComponentName)
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
				// ISM·HISM(PCG 프랍)도 UStaticMeshComponent 파생이라 이름을 안 주면 정확히 일반 StaticMeshComponent만 고른다.
				const bool bMatch = ComponentName.IsEmpty()
					? Component->GetClass() == UStaticMeshComponent::StaticClass()
					: Component->GetName() == ComponentName;
				if (bMatch)
				{
					return Component;
				}
			}
		}
		return nullptr;
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

		const FString ActorLabel = Options.FindRef(TEXT("actor"));
		const FString BrushName = Options.Contains(TEXT("brush")) ? Options[TEXT("brush")] : TEXT("Sculpt");
		const float Radius = Options.Contains(TEXT("radius")) ? FCString::Atof(*Options[TEXT("radius")]) : 500.f;
		const float Strength = Options.Contains(TEXT("strength")) ? FCString::Atof(*Options[TEXT("strength")]) : 0.5f;
		const bool bInvert = Options.Contains(TEXT("invert")) && FCString::Atoi(*Options[TEXT("invert")]) != 0;
		const int32 Steps = Options.Contains(TEXT("steps")) ? FMath::Max(1, FCString::Atoi(*Options[TEXT("steps")])) : 20;
		const int32 Passes = Options.Contains(TEXT("passes")) ? FMath::Max(1, FCString::Atoi(*Options[TEXT("passes")])) : 1;
		const bool bCommit = !Options.Contains(TEXT("commit")) || FCString::Atoi(*Options[TEXT("commit")]) != 0;
		const bool bPlane = Options.FindRef(TEXT("reference")).Equals(TEXT("Plane"), ESearchCase::IgnoreCase);
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		FVector Center = FVector::ZeroVector;
		if (ActorLabel.IsEmpty() || !Options.Contains(TEXT("from")) || !ParseVector(Options[TEXT("from")], From))
		{
			UE_LOG(LogLNPSphereSculpt, Error, TEXT("[SphereSculpt] Usage: Actor=<label> From=x,y,z [To=x,y,z] [Brush=Sculpt|Flatten|Smooth] [Radius=cm] [Strength=0..1] [Invert=0|1] [Steps=N] [Passes=N] [Center=x,y,z] [Reference=Sphere|Plane] [PlaneNormal=x,y,z] [Component=name] [Commit=0|1]"));
			return;
		}
		To = From;
		if (Options.Contains(TEXT("to")))
		{
			ParseVector(Options[TEXT("to")], To);
		}
		if (Options.Contains(TEXT("center")))
		{
			ParseVector(Options[TEXT("center")], Center);
		}
		// Plane 법선은 Sphere 모드의 스탬프 법선(중심에서 바깥)과 부호를 맞춰 Invert 의미가 두 모드에서 같게 한다.
		FVector PlaneNormal = ((From + To) * 0.5 - Center).GetSafeNormal();
		if (Options.Contains(TEXT("planenormal")))
		{
			ParseVector(Options[TEXT("planenormal")], PlaneNormal);
			PlaneNormal = PlaneNormal.GetSafeNormal();
		}

		using UE::MeshPartition::UHeightSculptTool;
		UHeightSculptTool::EBrushType BrushType = UHeightSculptTool::EBrushType::HeightSculpt;
		if (BrushName.Equals(TEXT("Flatten"), ESearchCase::IgnoreCase))
		{
			BrushType = UHeightSculptTool::EBrushType::HeightFlatten;
		}
		else if (BrushName.Equals(TEXT("Smooth"), ESearchCase::IgnoreCase))
		{
			BrushType = UHeightSculptTool::EBrushType::HeightSmooth;
		}

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UStaticMeshComponent* Component = World ? FindComponent(*World, ActorLabel, Options.FindRef(TEXT("component"))) : nullptr;
		if (Component == nullptr || Component->GetStaticMesh() == nullptr)
		{
			UE_LOG(LogLNPSphereSculpt, Error, TEXT("[SphereSculpt] No StaticMeshComponent on actor '%s' in the editor world"), *ActorLabel);
			return;
		}

		FQueries Queries;
		FTransactions Transactions;
		UInteractiveToolsContext* ToolsContext = NewObject<UInteractiveToolsContext>();
		ToolsContext->AddToRoot();
		Queries.World = World;
		Queries.SelectedComponent = Component;
		Queries.ToolsContext = ToolsContext;
		ToolsContext->Initialize(&Queries, &Transactions);
		ToolsContext->TargetManager->AddTargetFactory(NewObject<UStaticMeshComponentToolTargetFactory>(ToolsContext->TargetManager));

		UE::MeshPartition::UHeightSculptToolBuilder* Builder = NewObject<UE::MeshPartition::UHeightSculptToolBuilder>(ToolsContext->ToolManager);
		Builder->DefaultPrimaryBrushID = static_cast<int32>(BrushType);
		ToolsContext->ToolManager->RegisterToolType(TEXT("LNPSphereSculpt"), Builder);
		ToolsContext->ToolManager->SelectActiveToolType(EToolSide::Left, TEXT("LNPSphereSculpt"));
		ToolsContext->ToolManager->ActivateTool(EToolSide::Left);
		UHeightSculptTool* Tool = Cast<UHeightSculptTool>(ToolsContext->ToolManager->GetActiveTool(EToolSide::Left));
		if (Tool == nullptr)
		{
			UE_LOG(LogLNPSphereSculpt, Error, TEXT("[SphereSculpt] Height Sculpt tool did not activate for %s"), *Component->GetPathName());
			ToolsContext->Shutdown();
			ToolsContext->RemoveFromRoot();
			return;
		}

		for (UObject* PropertyObject : Tool->GetToolProperties(false))
		{
			if (UE::MeshPartition::UHeightSculptToolProperties* HeightProperties = Cast<UE::MeshPartition::UHeightSculptToolProperties>(PropertyObject))
			{
				HeightProperties->ReferenceSurface = bPlane
					? UE::MeshPartition::EHeightSculptReferenceSurface::Plane
					: UE::MeshPartition::EHeightSculptReferenceSurface::Sphere;
			}
			if (UMeshSculptBrushOpProps* BrushOpProperties = Cast<UMeshSculptBrushOpProps>(PropertyObject))
			{
				BrushOpProperties->SetStrength(Strength);
			}
		}
		Tool->SetActiveBrushType(static_cast<int32>(BrushType));
		Tool->SetBrushStrengthPressureEnabled(false);
		Tool->BrushProperties->BrushSize.SizeType = EBrushToolSizeType::World;
		Tool->BrushProperties->BrushSize.WorldRadius = Radius;
		Tool->BrushProperties->BrushSize.bEnablePressureSensitivity = false;
		Tool->BrushProperties->FlowRate = 1.0f;
		// Sphere: 기즈모 위치가 기준 구의 중심이다. 기본값은 대상 컴포넌트 원점이라 반드시 덮어쓴다.
		// Plane: 기즈모 Z축이 평면 법선이다. 위치는 Flatten 높이에만 쓰이므로 스트로크 시작점에 둔다.
		//   기즈모 위치는 월드 좌표지만 회전은 대상 컴포넌트 로컬 기준으로 쓰인다(회전된 섬 메시에서 실측). 월드 법선을 로컬로 바꿔 넣는다.
		const FVector LocalPlaneNormal = Component->GetComponentTransform().InverseTransformVectorNoScale(PlaneNormal);
		Tool->GizmoProperties->Position = bPlane ? From : Center;
		Tool->GizmoProperties->Rotation = bPlane ? FRotationMatrix::MakeFromZ(LocalPlaneNormal).ToQuat() : FQuat::Identity;
		Tool->GizmoProperties->bShowGizmo = false;
		Tool->OnUpdateModifierState(CtrlModifierID, bInvert);

		const TArray<FVector3d> Before = CaptureVertices(Tool);
		int32 StampRays = 0;
		int32 MissedRays = 0;
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			// 왕복 패스는 끝점 누적 편향을 줄인다.
			const bool bReverse = (Pass % 2) == 1;
			bool bStarted = false;
			FRay LastRay;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Alpha = static_cast<double>(bReverse ? Steps - Step : Step) / Steps;
				const FVector Point = FMath::Lerp(From, To, Alpha);
				const FRay Ray(Center, (Point - Center).GetSafeNormal(), true);
				FHitResult Hit;
				if (!Tool->HitTest(Ray, Hit))
				{
					++MissedRays;
					continue;
				}
				if (!bStarted)
				{
					Tool->OnUpdateHover(FInputDeviceRay(Ray));
					Tool->OnBeginDrag(Ray);
					bStarted = true;
				}
				else
				{
					Tool->OnUpdateDrag(Ray);
				}
				ToolsContext->ToolManager->Tick(0.1f);
				LastRay = Ray;
				++StampRays;
			}
			if (bStarted)
			{
				Tool->OnEndDrag(LastRay);
				ToolsContext->ToolManager->Tick(0.1f);
			}
		}
		const TArray<FVector3d> After = CaptureVertices(Tool);

		// 변위 방향이 월드 방사 방향과 얼마나 맞는지 잰다. 섬 메시에서 기준 구 중심이 틀리면 정렬도가 떨어진다.
		const FTransform ComponentToWorld = Component->GetComponentTransform();
		int32 Moved = 0;
		double MaxDisplacement = 0.0;
		double SumAlignment = 0.0;
		double SumRadialChange = 0.0;
		for (int32 Index = 0; Index < FMath::Min(Before.Num(), After.Num()); ++Index)
		{
			const FVector WorldBefore = ComponentToWorld.TransformPosition(FVector(Before[Index]));
			const FVector WorldAfter = ComponentToWorld.TransformPosition(FVector(After[Index]));
			const FVector Delta = WorldAfter - WorldBefore;
			const double Length = Delta.Size();
			if (Length < 0.01)
			{
				continue;
			}
			++Moved;
			MaxDisplacement = FMath::Max(MaxDisplacement, Length);
			SumAlignment += FMath::Abs(FVector::DotProduct(Delta / Length, (WorldBefore - Center).GetSafeNormal()));
			SumRadialChange += (WorldAfter - Center).Size() - (WorldBefore - Center).Size();
		}

		ToolsContext->ToolManager->DeactivateTool(EToolSide::Left, bCommit ? EToolShutdownType::Accept : EToolShutdownType::Cancel);
		FAssetCompilingManager::Get().FinishAllCompilation();
		ToolsContext->Shutdown();
		ToolsContext->RemoveFromRoot();

		UE_LOG(LogLNPSphereSculpt, Display,
			TEXT("[SphereSculpt] %s %s Ref=%s Brush=%s Radius=%.0f Strength=%.2f Invert=%d Rays=%d Missed=%d | Moved=%d MaxDisp=%.1fcm MeanRadialAlign=%.3f MeanRadialChange=%.1fcm | %s"),
			*ActorLabel, *Component->GetStaticMesh()->GetName(), bPlane ? TEXT("Plane") : TEXT("Sphere"), *BrushName, Radius, Strength, bInvert ? 1 : 0,
			StampRays, MissedRays, Moved, MaxDisplacement,
			Moved > 0 ? SumAlignment / Moved : 0.0, Moved > 0 ? SumRadialChange / Moved : 0.0,
			bCommit ? TEXT("Committed (asset dirty, save it)") : TEXT("Cancelled (preview only)"));
	}

	static FAutoConsoleCommand Command(
		TEXT("LNP.MeshTerrain.SphereSculpt"),
		TEXT("Apply a Mesh Terrain Sphere Height Sculpt stroke along From->To to a StaticMeshComponent in the editor world. ")
		TEXT("Actor=<label> From=x,y,z [To=x,y,z] [Brush=Sculpt|Flatten|Smooth] [Radius=cm] [Strength=0..1] [Invert=0|1] ")
		TEXT("[Steps=N] [Passes=N] [Center=x,y,z] [Reference=Sphere|Plane] [PlaneNormal=x,y,z] [Component=name] [Commit=0|1]. ")
		TEXT("Invert=1 raises ground on an inward-facing sphere. Sphere skips vertices farther from Center than the mesh bounds size; use Plane there."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

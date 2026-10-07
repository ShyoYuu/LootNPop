// Copyright (c) 2026 LootNPop. All rights reserved.

// SurfaceSupportNavigation Phase 9 Vertical Slice 옥탄트 LVI를 코드 정의로 다시 만드는 에디터 명령.
//
// 지각: Meadow 지각을 복제하고 이음매 밴드 바깥 정점만 이 옥탄트의 목표 지형으로 옮긴다. 이음매 평면에서 SeamMargin
// 안쪽 정점은 좌표를 바꾸지 않으므로 이음매 샘플과 seam hash가 Meadow와 같다(런타임은 이음매 샘플 반지름이 비트
// 단위로 같아야 게시한다). 목표 지형은 시드 fBm 노이즈이고, 구조물 자리(Pad)는 지정 반지름의 구면으로 평탄화한다.
// 심연(지각을 뚫는 원형 구멍)은 지각을 구 중심을 지나는 평면으로 잘라 낸다.
//
// 좌표: 옥탄트 중앙 기준 (X, Y) 호 길이(지각 반지름 30,000cm에서 잰 cm)와 높이 H(지각 기준 반지름에서 구 중심 쪽 cm).
// `Point(X, Y, H)`의 X·Y가 같은 점은 구 중심을 지나는 직선 위에 있어 그 지점의 중력 방향과 나란하다. 그래서 같은 (X, Y)
// 곡선을 따라 세운 벽은 어느 높이에서나 수직이지만, 위로 갈수록 실제 크기가 (R - H) / R배로 줄어든다. 높이 H에서 실제
// 길이 L은 호 길이 L × Scale(H)로 적는다. 나선 기둥처럼 높이 차가 큰 구조물은 축에서의 실제 거리를 유지한다(기울기 7° 이하).
//
// 구조물: 건물·계단식 성벽·나선 기둥·섬·다리·심연과 공동은 코드로 만든다(D-070 경사로 충돌 + Decoration 계단 칸).
// Authored: Blender가 옥탄트 로컬 좌표(피벗 = 옥탄트 원점)로 만든 메시를 /Game/Maps/<Name>/Authored에 임포트하면
// 이름 접미사로 역할을 정해 원점에 배치한다. _Floor = Support+Blocker+Static, _Coarse = 그에 CoarseSupport 추가,
// _Shell = Blocker+Static, _Deco = Decoration.
// 마커: 그래플 앵커·움직이는 패널 Placement Marker와 Pod 세트 앵커(ALNPMassSpawnPoint)를 둔다.
// 등불: 천장 아래(심연·공동·성문 터널·건물 안·접시 밑)에 그림자 없는 포인트 라이트와 발광 구슬(Decoration)을 둔다.
//
// LVI가 이미 있으면 액터를 모두 지우고 같은 패키지에 다시 채운다. 메시는 제자리 갱신한다.
//
// 예: LNP.SurfaceNav.BuildSliceOctant Bastion_00
//     이어서 LNP.SurfaceNav.BakeOctant /Game/Maps/Bastion_00/LVI_Octant_Bastion_00

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PointLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Config/LNPSettings.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "Editor.h"
#include "Engine/Brush.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSliceOctant, Log, All);

namespace LNPSliceOctant
{
	namespace
	{
		using LNPRegressionFixture::CrustRadius;

		const TCHAR* const SeamSourceMesh = TEXT("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000.SM_Octant_Meadow_00_R30000");
		const FName SupportTag(TEXT("LNP.Surface.Support"));
		const FName BlockerTag(TEXT("LNP.Surface.Blocker"));
		const FName StaticTag(TEXT("LNP.Surface.Static"));
		const FName CoarseTag(TEXT("LNP.Surface.CoarseSupport"));
		const FName DecorationTag(TEXT("LNP.Surface.Decoration"));

		/** 옥탄트 중앙(위도 35.26°, 방위 45°) 기준 로컬 좌표 프레임. X는 위도 증가, Y는 방위 증가 방향 호 길이(cm)다. */
		const LNPRegressionFixture::FCaseFrame& Center()
		{
			static const LNPRegressionFixture::FCaseFrame Frame(35.264389682754654, 45.0);
			return Frame;
		}

		FVector RadialUnnormalized(double X, double Y)
		{
			return Center().Radial * CrustRadius + Center().Bitangent * X + Center().Tangent * Y;
		}

		FVector Radial(double X, double Y)
		{
			return RadialUnnormalized(X, Y).GetSafeNormal();
		}

		/** (X, Y) 위치의 높이 H 점. 같은 (X, Y)의 점은 모두 구 중심을 지나는 한 직선 위에 있다. */
		FVector Point(double X, double Y, double H)
		{
			return Radial(X, Y) * (CrustRadius - H);
		}

		FVector Point(const FVector& Local)
		{
			return Point(Local.X, Local.Y, Local.Z);
		}

		/** 높이 H에서 실제 길이 1cm에 해당하는 호 길이. */
		double Scale(double H)
		{
			return CrustRadius / (CrustRadius - H);
		}

		/** +X에서 +Y 쪽으로 Deg 돌린 방향의 반지름 R 점(호 길이). */
		FVector2D Polar(double R, double Deg)
		{
			const double A = FMath::DegreesToRadians(Deg);
			return {R * FMath::Cos(A), R * FMath::Sin(A)};
		}

		FVector PolarH(double R, double Deg, double H)
		{
			const FVector2D P = Polar(R, Deg);
			return {P.X, P.Y, H};
		}

		/** 로컬 +Z = 그 지점의 Up(구 중심 방향), +X = 위도 증가, +Y = 방위 증가 방향. */
		FQuat Rotation(double X, double Y)
		{
			return FRotationMatrix::MakeFromZY(-Radial(X, Y), Center().Tangent).ToQuat();
		}

		/** 지형 반지름을 Radius로 평탄화하는 원형 자리. 중심 (X, Y), 반경 Size, 바깥 Falloff에 걸쳐 노이즈로 돌아간다. */
		struct FPad
		{
			double X;
			double Y;
			double Size;
			double Falloff;
			double Radius;
		};

		/** 건물 벽의 문. Side 0 = -X, 1 = +X, 2 = -Y, 3 = +Y 벽이고 Offset은 벽 중앙에서 벽을 따라 잰 거리다. */
		struct FDoor
		{
			int32 Side;
			double Offset;
			double Width;
			double Height;
		};

		/**
		 * 건물. 중심 (X, Y), 바닥 높이 BaseH의 구면 위에 선다. 건물 로컬 X 방향 길이 SizeX, Y 방향 폭 SizeY(실제 cm)이고
		 * 로컬 X는 옥탄트 +X에서 Yaw만큼 돌린 방향이다. 층 사이 경사로는 -Y 벽과 +Y 벽을 번갈아 따라간다.
		 * Collapsed 면은 벽이 무너진 잔해 높이만 남고, 그 면 쪽 꼭대기 층 바닥·지붕 일부가 함께 무너진다.
		 */
		struct FBuilding
		{
			FString Id;
			double X = 0.0;
			double Y = 0.0;
			double BaseH = 0.0;
			double Yaw = 0.0;
			double SizeX = 2000.0;
			double SizeY = 1400.0;
			int32 Stories = 2;
			double StoryHeight = 450.0;
			double RampWidth = 300.0;
			TArray<FDoor> Doors;
			TArray<int32> Collapsed;
		};

		/** 원판 섬(InnerRadius가 있으면 고리). 반지름은 윗면 높이에서의 실제 cm다. */
		struct FIsland
		{
			FString Id;
			double X = 0.0;
			double Y = 0.0;
			double Radius = 0.0;
			double InnerRadius = 0.0;
			double TopH = 0.0;
			double Thickness = 300.0;
			bool bCoarse = false;
		};

		/** 공중 직선 다리·경사로. From·To는 (X, Y, H) 윗면 중심선 양 끝(호 길이), Width는 실제 cm다. */
		struct FBridgeRamp
		{
			FString Id;
			FVector From;
			FVector To;
			double Width = 1000.0;
		};

		/** 계단식 성벽의 동심 띠 하나. 반지름 R0~R1(호 길이)의 기둥이 지면에서 높이 H까지 꽉 차 있다. */
		struct FRingBand
		{
			double R0;
			double R1;
			double H;
		};

		/** 모든 띠를 관통하는 문. 각도 Deg(+X에서 +Y 쪽), 폭 Width(지면 실제 cm), 높이 Height. */
		struct FRingGate
		{
			double Deg;
			double Width;
			double Height;
		};

		/** 띠 경계면을 따라 낮은 쪽 윗면 위에 놓인 경사로. 반지름 Radius의 경계면에 붙어 StartDeg부터 각도가 커지는 쪽으로 오른다. */
		struct FRingStair
		{
			double Radius;
			/** true면 경사로가 경계면 바깥(R > Radius)에, false면 안쪽에 놓인다. */
			bool bOutside;
			double StartDeg;
			double Width;
			double H0;
			double H1;
		};

		/** 여러 동심 띠로 쌓은 계단식 성벽. 띠의 윗면이 통로이고 높이가 다른 띠 사이 면이 성벽이다. */
		struct FTieredRing
		{
			FString Id;
			TArray<FRingBand> Bands;
			TArray<FRingGate> Gates;
			TArray<FRingStair> Stairs;
			bool bCoarse = true;
		};

		/**
		 * 나선 기둥. 축 (X, Y)에서 실제 반지름 CoreRadius의 원기둥이 BottomH에서 TopH까지 서고, 폭 SpiralWidth 경사로가
		 * 기둥을 감아 BottomH에서 TopH까지 오른다. 높이 0에서 나선이 각도 ThetaAtZero를 지난다. 한 Support Layer의 반지름 폭
		 * 한계(±81.9m) 때문에 SplitH 높이마다 경사로를 다른 메시로 나눈다.
		 */
		struct FSpire
		{
			FString Id;
			double X = 0.0;
			double Y = 0.0;
			double CoreRadius = 1000.0;
			double SpiralWidth = 900.0;
			double BottomH = 0.0;
			double TopH = 0.0;
			double SlopeDeg = 25.0;
			double ThetaAtZero = 0.0;
			TArray<double> SplitH;
			/**
			 * 0보다 크면 꼭대기가 실제 반지름 DishRadius의 접시(메인 무대)로 벌어진다. 나팔 면은 DishFlareStartH에서
			 * 나선 바깥 반지름으로 시작해 TopH - DishRimThickness에서 접시 반지름이 된다. 나선은 나팔 안쪽을 지나
			 * 접시 윗면의 틈으로 올라온다.
			 */
			double DishRadius = 0.0;
			double DishFlareStartH = 0.0;
			double DishRimThickness = 300.0;
		};

		/** 원형 심연과 그 바닥 원형 공동. 반지름은 호 길이, 지각을 RadiusArc 정다각형 기둥으로 잘라 낸다. */
		struct FAbyss
		{
			FString Id;
			double X = 0.0;
			double Y = 0.0;
			double RadiusArc = 0.0;
			double CavernRadiusArc = 0.0;
			double FloorH = 0.0;
			double CeilingH = 0.0;
			/** 공동 기둥: (반지름 호 길이, 각도, 실제 반폭). */
			TArray<FVector> Pillars;
		};

		/** 그래플 앵커 마커. MaxDistance가 0이면 설정 에셋 거리를 쓴다. */
		struct FAnchor
		{
			double X;
			double Y;
			double H;
			double MaxDistance;
		};

		/** Pod 세트 앵커. */
		struct FSpawn
		{
			double X;
			double Y;
			double H;
		};

		/** 움직이는 패널. From·To는 패널 중심 (X, Y, H)다. 마커는 경로 중간점에 그 지점 Up으로 선다. */
		struct FPanel
		{
			FVector From;
			FVector To;
			double Speed;
			double HoldSeconds;
		};

		/** 등불: 그림자 없는 Movable 포인트 라이트와 발광 구슬. Intensity는 cd, Radius는 감쇠 반경(cm)이다. */
		struct FLamp
		{
			double X;
			double Y;
			double H;
			double Intensity;
			double Radius;
		};

		/** Blocker 기둥 상자(실제 cm). (X, Y)에 바닥 BottomH부터 Height까지 선다. */
		struct FBlock
		{
			double X;
			double Y;
			double BottomH;
			double Half;
			double Height;
		};

		/** 이름 붙은 검토 지점. 빌더가 ATargetPoint로 두고 LNP.Debug.TeleportToReview가 이름으로 찾는다. H는 발 높이다. */
		struct FReviewPoint
		{
			const TCHAR* Name;
			double X;
			double Y;
			double H;
		};

		struct FOctantSpec
		{
			const TCHAR* Name = nullptr;
			/** 마커·Pod 앵커 GUID의 옥탄트 구분값. */
			uint32 GuidTag = 0;
			int32 Seed = 0;
			/** 목표 지형 노이즈의 최대 높이(cm)와 기본 파장(cm). */
			double Amplitude = 0.0;
			double Wavelength = 0.0;
			/** 이음매 평면에서 이 거리(cm) 안쪽은 Meadow 지각 그대로다. 그 바깥 SeamBand에 걸쳐 목표 지형으로 넘어간다. */
			double SeamMargin = 1500.0;
			double SeamBand = 4000.0;
			TArray<FPad> Pads;
			TArray<FBuilding> Buildings;
			TArray<FIsland> Islands;
			TArray<FBridgeRamp> Bridges;
			TArray<FTieredRing> Rings;
			TArray<FSpire> Spires;
			TArray<FAbyss> Abysses;
			TArray<FBlock> Blocks;
			TArray<FAnchor> Anchors;
			TArray<FSpawn> Spawns;
			TArray<FPanel> Panels;
			TArray<FLamp> Lamps;
			TArray<FReviewPoint> ReviewPoints;
		};

		constexpr double SlabThickness = 50.0;
		constexpr double BuryDepth = 50.0;
		constexpr double RampSlopeDeg = 30.0;
		/** 지면에 선 벽·기둥을 묻는 깊이. 노이즈·곡률로 밑동이 뜨지 않게 한다. */
		constexpr double WallSink = 100.0;
		/** 무너진 벽은 양 모서리에 이 길이의 벽 토막만 남는다(높이는 벽의 55%). 가운데는 바닥까지 뚫려 턱이 없다. */
		constexpr double RuinStubLength = 300.0;
		/** 다리·공중 경사로 두께. */
		constexpr double BridgeThickness = 100.0;
		/** 빈 구조물을 감싸는 벽의 실제 두께. 캡슐 지름보다 두꺼워야 겹친 채 sweep해도 반대편으로 빠지지 않는다. */
		constexpr double WallThickness = 100.0;
		/**
		 * 등불은 역제곱 대신 (1 - (d/R)²)^지수 감쇠를 쓴다. 등불 바로 옆에서도 밝기가 최대값에서 멈춰 캐릭터가 튀지 않는다.
		 * 세기는 반경의 LampReferenceFraction 거리에서 spec의 cd 값(역제곱)과 같은 조도가 되도록 환산한다.
		 */
		constexpr double LampFalloffExponent = 4.0;
		constexpr double LampReferenceFraction = 0.25;

		/**
		 * 플레이어 치수표. BP_LNPPlayer 캡슐과 Mover CommonLegacyMovementSettings를 PIE에서 읽은 값이다(2026-10-07).
		 * 값이 바뀌면 Guide_OctantLevelInstance.md의 치수표도 함께 고친다.
		 */
		namespace PlayerMetrics
		{
			constexpr double CapsuleRadius = 42.0;
			constexpr double CapsuleHeight = 192.0;
			constexpr double MaxStepHeight = 40.0;
			/** JumpUpwardsSpeed 750cm/s, 중력 2,000cm/s²: 750² / (2 × 2000). */
			constexpr double JumpHeight = 140.0;
			/** MaxWalkSlopeCosine 0.71. */
			constexpr double MaxWalkSlopeDeg = 44.8;
		}

		/** 치수표에서 정한 레이아웃 규칙. LintSpec이 spec을 이 값으로 검사한다. 성벽 경사로 꼭대기 층계참(경사로 폭 정사각형)은 빌더가 늘 만든다. */
		namespace DesignRules
		{
			/** 경사로 옆에 남길 최소 통로 폭(실제 cm). 2m는 "겨우 지나간다", 8m는 "적절하다"는 평가였다. */
			constexpr double MinWalkway = 300.0;
			/** 다리 기울기 상한. 걷기 한계(44.8°)보다 넉넉히 낮다. */
			constexpr double MaxBridgeSlopeDeg = 35.0;
			/** 위아래로 겹치는 구조물 사이 최소 틈. 캡슐 높이 + 점프 높이보다 크다. */
			constexpr double MinHeadroom = 400.0;
			/** 문 최소 높이. */
			constexpr double MinDoorHeight = 250.0;
		}

		static_assert(WallThickness >= 2.0 * PlayerMetrics::CapsuleRadius, "Walls must be thicker than the capsule diameter");
		static_assert(RampSlopeDeg < PlayerMetrics::MaxWalkSlopeDeg, "Ramps must be walkable");
		static_assert(DesignRules::MaxBridgeSlopeDeg < PlayerMetrics::MaxWalkSlopeDeg, "Bridges must be walkable");
		static_assert(DesignRules::MinHeadroom > PlayerMetrics::CapsuleHeight + PlayerMetrics::JumpHeight, "Headroom must fit a jump");
		static_assert(DesignRules::MinDoorHeight > PlayerMetrics::CapsuleHeight, "Doors must fit the capsule");

		/** 이번 생성에서 두께 0인 양면 벽(Wall의 SolidSide 0)을 만든 수. 빈 구조물 갇힘의 원인이라 규칙 검사가 0을 요구한다. */
		int32 GZeroThicknessWalls = 0;

		double RoofH(const FBuilding& Building)
		{
			return Building.Stories * Building.StoryHeight;
		}

		/** 건물 로컬 (LX, LY)(실제 cm)의 옥탄트 (X, Y, H). */
		FVector BuildingPoint(const FBuilding& Building, double LX, double LY, double H)
		{
			const double Yaw = FMath::DegreesToRadians(Building.Yaw), K = Scale(Building.BaseH);
			return {Building.X + (LX * FMath::Cos(Yaw) - LY * FMath::Sin(Yaw)) * K, Building.Y + (LX * FMath::Sin(Yaw) + LY * FMath::Cos(Yaw)) * K, H};
		}

		/** 성벽 경사로의 반지름 구간(R0, R1). */
		FVector2D StairRadii(const FRingStair& Stair)
		{
			return Stair.bOutside ? FVector2D(Stair.Radius, Stair.Radius + Stair.Width) : FVector2D(Stair.Radius - Stair.Width, Stair.Radius);
		}

		/** 성벽 경사로가 차지하는 각도: X = 경사로(30°), Y = 꼭대기 층계참(경사로 폭 정사각형). */
		FVector2D StairSpanDeg(const FRingStair& Stair)
		{
			const FVector2D R = StairRadii(Stair);
			const double RMid = (R.X + R.Y) * 0.5;
			const double RealLength = (Stair.H1 - Stair.H0) / FMath::Tan(FMath::DegreesToRadians(RampSlopeDeg));
			return {FMath::RadiansToDegrees(RealLength * Scale((Stair.H0 + Stair.H1) * 0.5) / RMid), FMath::RadiansToDegrees(Stair.Width / RMid)};
		}

		// ---------------------------------------------------------------------------------------------
		// A Bastion_00 거인의 성채
		// ---------------------------------------------------------------------------------------------

		/** 안뜰 건물(거인 스케일). 층고 8m, 문 7m × 6.5m, 경사로 폭 6m. 로컬 X가 바깥쪽(반지름 증가)을 향한다. */
		FBuilding GiantBuilding(const FString& Id, double R, double Deg, double SizeX, double SizeY, int32 Stories, TArray<int32> Collapsed)
		{
			FBuilding Building;
			Building.Id = Id;
			const FVector2D P = Polar(R, Deg);
			Building.X = P.X;
			Building.Y = P.Y;
			Building.Yaw = Deg;
			Building.SizeX = SizeX;
			Building.SizeY = SizeY;
			Building.Stories = Stories;
			Building.StoryHeight = 800.0;
			Building.RampWidth = 600.0;
			Building.Collapsed = MoveTemp(Collapsed);
			// 1층 경사로가 -Y 벽을 따라 오르므로 문은 나머지 세 면에 낸다.
			for (const int32 Side : {0, 1, 3})
			{
				if (!Building.Collapsed.Contains(Side))
				{
					Building.Doors.Add({Side, 0.0, 700.0, 650.0});
				}
			}
			return Building;
		}

		/** 두 점 사이 다리를 추가하고 이름을 붙인다. */
		void AddBridge(FOctantSpec& Spec, const FVector& From, const FVector& To, double Width = 1000.0)
		{
			Spec.Bridges.Add({FString::Printf(TEXT("D%02d"), Spec.Bridges.Num() + 1), From, To, Width});
		}

		/** 원판 섬 a의 가장자리에서 b의 가장자리로 잇는 다리. 양 끝은 섬 안쪽 100cm까지 섬 윗면 1cm 아래로 들어간다. */
		void BridgeIslands(FOctantSpec& Spec, const FIsland& A, const FIsland& B)
		{
			const FVector2D Dir = FVector2D(B.X - A.X, B.Y - A.Y).GetSafeNormal();
			const FVector2D From = FVector2D(A.X, A.Y) + Dir * (A.Radius - 100.0) * Scale(A.TopH);
			const FVector2D To = FVector2D(B.X, B.Y) - Dir * (B.Radius - 100.0) * Scale(B.TopH);
			AddBridge(Spec, FVector(From, A.TopH - 1.0), FVector(To, B.TopH - 1.0));
		}

		/**
		 * A `Bastion_00` 거인의 성채(phases/Phase09_VerticalSlice.md §3). 지면 위 200m·아래 100m를 채운다.
		 * - 메인 무대: 중앙 나선 기둥 꼭대기(180m)가 나팔처럼 벌어진 반지름 40m 접시. 나선 경사로는 나팔 안을 지나 접시의 틈으로 올라온다.
		 * - 심연: 기둥을 둘러싼 반지름 32m 구멍이 지하 100m 공동으로 떨어진다. 기둥 나선이 공동 바닥에서 시작한다(긴 경사로 입구).
		 *   심연 자체가 수직 통로 입구다.
		 * - 성채: 심연을 두른 4단 계단식 성채(20/40/60/80m), 단마다 경사로. 바깥 2단 외성벽(20/40m)과 문 4개.
		 * - 안뜰: 거인 스케일 폐허 건물 8채와 탑 4개, 지붕끼리·성벽과 다리로 얽힌다.
		 * - 공중: 외성벽 위에서 접시 높이까지 나선형으로 오르며 바깥으로 퍼지는 공중섬 12개(그중 둘에 폐허)와 다리.
		 */
		FOctantSpec Bastion()
		{
			FOctantSpec Spec;
			Spec.Name = TEXT("Bastion_00");
			Spec.GuidTag = 0x09B00000;
			Spec.Seed = 9001;
			Spec.Amplitude = 900.0;
			Spec.Wavelength = 9000.0;
			Spec.Pads = {{0.0, 0.0, 16000.0, 1500.0, CrustRadius}};
			// 외성벽 바깥 경사로가 옥탄트 경계에서 약 40m인 방향도 평지에 놓이도록 Meadow에서 넘어오는 띠를 20m로 줄인다.
			// 이음매 15m 안쪽 정점은 그대로라 이음매 호환은 유지된다.
			Spec.SeamBand = 2000.0;

			// 심연과 공동. 공동 바닥 -100m, 천장 -65m, 공동 실제 반지름 약 60m.
			FAbyss Abyss;
			Abyss.Id = TEXT("A01");
			Abyss.RadiusArc = 3200.0;
			Abyss.CavernRadiusArc = 4500.0;
			Abyss.FloorH = -10000.0;
			Abyss.CeilingH = -6500.0;
			for (int32 I = 0; I < 6; ++I)
			{
				Abyss.Pillars.Add(FVector(3900.0, 30.0 + I * 60.0, 300.0));
			}
			Spec.Abysses.Add(Abyss);

			// 중앙 나선 기둥: 공동 바닥에서 180m 접시까지. 높이 0에서 북문(0°)을, 80m에서 성채 꼭대기 다리 각도를 지난다.
			FSpire Spire;
			Spire.Id = TEXT("P01");
			Spire.CoreRadius = 1000.0;
			Spire.SpiralWidth = 900.0;
			Spire.BottomH = -10000.0;
			Spire.TopH = 18000.0;
			Spire.SlopeDeg = 25.0;
			Spire.ThetaAtZero = 0.0;
			Spire.SplitH = {0.0, 9000.0};
			Spire.DishRadius = 4000.0;
			Spire.DishFlareStartH = 15000.0;
			Spec.Spires.Add(Spire);
			const double SpiralTurnRise = UE_TWO_PI * (Spire.CoreRadius + Spire.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(Spire.SlopeDeg));
			auto SpiralDeg = [&](double H) { return Spire.ThetaAtZero + 360.0 * H / SpiralTurnRise; };
			const double SpiralOuter = Spire.CoreRadius + Spire.SpiralWidth;

			// 성채: 안쪽이 가장 높은 4단. 문 4개(폭 14m, 높이 18m)가 모든 단을 관통한다.
			FTieredRing Citadel;
			Citadel.Id = TEXT("W02");
			Citadel.Bands = {{3200.0, 4400.0, 8000.0}, {4400.0, 5400.0, 6000.0}, {5400.0, 6400.0, 4000.0}, {6400.0, 7600.0, 2000.0}};
			for (const double Deg : {0.0, 90.0, 180.0, 270.0})
			{
				Citadel.Gates.Add({Deg, 1400.0, 1800.0});
			}
			// 단마다 경사로 두 개(폭 6m, 30°). 경계면 바깥쪽 낮은 단 위에 놓인다.
			Citadel.Stairs = {
				{7600.0, true, 30.0, 600.0, 0.0, 2000.0}, {7600.0, true, 210.0, 600.0, 0.0, 2000.0},
				{6400.0, true, 120.0, 600.0, 2000.0, 4000.0}, {6400.0, true, 300.0, 600.0, 2000.0, 4000.0},
				{5400.0, true, 20.0, 600.0, 4000.0, 6000.0}, {5400.0, true, 200.0, 600.0, 4000.0, 6000.0},
				{4400.0, true, 110.0, 600.0, 6000.0, 8000.0}, {4400.0, true, 290.0, 600.0, 6000.0, 8000.0},
			};
			Spec.Rings.Add(Citadel);

			// 외성벽(호 118~156m): 안쪽 20m 단(폭 14m), 가운데 40m 단(폭 10m), 바깥 20m 단(폭 14m). 20m 단은 경사로(6m) 옆으로 8m를 남겨
			// 경사로를 피해 단을 계속 돌 수 있게 한다. 문 4개(폭 20m, 높이 30m).
			FTieredRing Outer;
			Outer.Id = TEXT("W01");
			Outer.Bands = {{11800.0, 13200.0, 2000.0}, {13200.0, 14200.0, 4000.0}, {14200.0, 15600.0, 2000.0}};
			for (const double Deg : {0.0, 90.0, 180.0, 270.0})
			{
				Outer.Gates.Add({Deg, 2000.0, 3000.0});
			}
			for (int32 Q = 0; Q < 4; ++Q)
			{
				const double Mid = 45.0 + Q * 90.0;
				// 경사로 가운데가 사분면 가운데(Mid)에 오도록 시작 각도를 정한다.
				auto Centered = [Mid](FRingStair Stair)
				{
					Stair.StartDeg = Mid - StairSpanDeg(Stair).X * 0.5;
					return Stair;
				};
				Outer.Stairs.Add(Centered({11800.0, false, 0.0, 600.0, 0.0, 2000.0}));
				Outer.Stairs.Add(Centered({13200.0, false, 0.0, 600.0, 2000.0, 4000.0}));
				// 바깥 20m 단: 성 밖 지면에서 오르고, 그 단에서 40m 단으로 오른다.
				Outer.Stairs.Add(Centered({15600.0, true, 0.0, 600.0, 0.0, 2000.0}));
				Outer.Stairs.Add(Centered({14200.0, true, 0.0, 600.0, 2000.0, 4000.0}));
			}
			Spec.Rings.Add(Outer);

			// 안뜰 건물: 45° 간격으로 8채, 층고 12m. 건물 X축을 둘레 방향으로 돌려 문과 무너진 면이 안뜰 통로 쪽을 향한다.
			// 면 0(-X)·1(+X)은 시계·반시계 방향 이웃, 면 2(-Y)는 외성벽, 면 3(+Y)은 성채 쪽이다. 1층 경사로는 외성벽 쪽 벽을 따른다.
			struct FPlan { double Deg; int32 Stories; TArray<int32> Collapsed; };
			const FPlan Plans[] = {
				{22.5, 2, {0}}, {67.5, 3, {1}}, {112.5, 3, {0, 3}}, {157.5, 2, {}},
				{202.5, 3, {1}}, {247.5, 2, {0, 2}}, {292.5, 3, {1}}, {337.5, 2, {0}},
			};
			TArray<int32> BigIndex;
			for (const FPlan& Plan : Plans)
			{
				FBuilding Building = GiantBuilding(FString::Printf(TEXT("B%02d"), BigIndex.Num() + 1), 9700.0, Plan.Deg, 2600.0, 2200.0, Plan.Stories, Plan.Collapsed);
				Building.Yaw = Plan.Deg + 90.0;
				Building.StoryHeight = 1200.0;
				BigIndex.Add(Spec.Buildings.Add(MoveTemp(Building)));
			}

			// 다리: 이웃 건물 지붕끼리 한 바퀴, 건물마다 외성벽 안쪽 20m 단과 성채 바깥 20m 단으로.
			auto RoofEdge = [&Spec](int32 Index, int32 Side)
			{
				const FBuilding& B = Spec.Buildings[Index];
				const double HX = B.SizeX * 0.5 - 500.0, HY = B.SizeY * 0.5 - 500.0;
				const double LX = Side == 0 ? -HX : Side == 1 ? HX : -300.0;
				const double LY = Side == 2 ? -HY : Side == 3 ? HY : -300.0;
				return BuildingPoint(B, LX, LY, RoofH(B) - 1.0);
			};
			for (int32 I = 0; I < BigIndex.Num(); ++I)
			{
				AddBridge(Spec, RoofEdge(BigIndex[I], 1), RoofEdge(BigIndex[(I + 1) % BigIndex.Num()], 0), 1200.0);
				// 3층 지붕(36m)은 40m 단으로, 2층 지붕(24m)은 20m 단으로 잇는다(다리 경사를 완만하게). 성채 40m 단은 경사로(호 54~60m)
				// 바깥 4m 띠에 내린다.
				const bool bTall = RoofH(Spec.Buildings[BigIndex[I]]) > 3000.0;
				AddBridge(Spec, RoofEdge(BigIndex[I], 2), PolarH(bTall ? 13700.0 : 12200.0, Plans[I].Deg, bTall ? 3999.0 : 1999.0));
				AddBridge(Spec, RoofEdge(BigIndex[I], 3), PolarH(bTall ? 6200.0 : 7300.0, Plans[I].Deg, bTall ? 3999.0 : 1999.0));
			}

			// 메인 무대 접시 위 엄폐 석주 6개.
			for (int32 I = 0; I < 6; ++I)
			{
				const FVector2D P = Polar(3000.0 * Scale(Spire.TopH), 15.0 + I * 60.0);
				Spec.Blocks.Add({P.X, P.Y, Spire.TopH, 250.0, 900.0});
			}

			// 공중섬 나선: 외성벽 40m 단 옆에서 시작해 접시 높이 180m까지 오른다. 위로 갈수록 바깥으로 퍼져 접시 밑 나팔을 피하고,
			// 각도 간격도 넓혀 다리 경사를 30° 안팎으로 둔다. 3·7번 섬에는 다리 쪽 두 면이 무너진 폐허가 선다.
			TArray<FIsland> Sky;
			for (int32 I = 0; I < 12; ++I)
			{
				FIsland Island;
				Island.Id = FString::Printf(TEXT("I%02d"), I + 1);
				Island.TopH = FMath::Lerp(4000.0, Spire.TopH, I / 11.0);
				const double Deg = 20.0 + I * 35.0 + I * I * 1.5;
				const FVector2D P = Polar(FMath::Lerp(9200.0, 12800.0, FMath::Square(I / 11.0)), Deg);
				Island.X = P.X;
				Island.Y = P.Y;
				Island.Radius = (I == 3 || I == 7) ? 1400.0 : 1000.0;
				Island.Thickness = 500.0;
				Sky.Add(Island);
			}
			Spec.Islands.Append(Sky);
			// 첫 섬은 외성벽 40m 단에서 들어간다. 섬끼리 차례로 잇고, 마지막 섬에서 접시로 건넌다.
			{
				const FVector2D Dir = FVector2D(Sky[0].X, Sky[0].Y).GetSafeNormal();
				AddBridge(Spec, FVector(Dir * 13700.0, 3999.0), FVector(FVector2D(Sky[0].X, Sky[0].Y) + Dir * (Sky[0].Radius - 100.0) * Scale(Sky[0].TopH), 3999.0));
			}
			for (int32 I = 0; I + 1 < Sky.Num(); ++I)
			{
				BridgeIslands(Spec, Sky[I], Sky[I + 1]);
			}
			{
				const FIsland& Last = Sky.Last();
				const FVector2D Dir = FVector2D(Last.X, Last.Y).GetSafeNormal();
				AddBridge(Spec, FVector(FVector2D(Last.X, Last.Y) - Dir * (Last.Radius - 100.0) * Scale(Last.TopH), Last.TopH - 1.0),
					FVector(Dir * (Spire.DishRadius - 100.0) * Scale(Spire.TopH), Spire.TopH - 1.0));
			}
			for (const int32 I : {3, 7})
			{
				// 폐허가 섬을 거의 채운다. 로컬 X를 앞 섬 → 뒤 섬 방향으로 두어 두 다리가 무너진 ±X 면으로 들어오고,
				// 경사로는 옆면(-Y 벽)을 따라간다.
				FBuilding Ruin = GiantBuilding(FString::Printf(TEXT("S%02d"), I + 1), 0.0, 0.0, 1800.0, 1600.0, 2, {0, 1});
				Ruin.X = Sky[I].X;
				Ruin.Y = Sky[I].Y;
				Ruin.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Sky[I + 1].Y - Sky[I - 1].Y, Sky[I + 1].X - Sky[I - 1].X));
				Ruin.BaseH = Sky[I].TopH;
				Ruin.StoryHeight = 700.0;
				Ruin.RampWidth = 400.0;
				Spec.Buildings.Add(Ruin);
			}

			// 기둥으로 건너는 다리: 북문 땅 높이와 성채 80m 단.
			AddBridge(Spec, PolarH(3350.0, 0.0, -1.0), PolarH((SpiralOuter - 200.0) * Scale(0.0), 0.0, -1.0));
			{
				const double Deg = SpiralDeg(8000.0);
				AddBridge(Spec, PolarH(3600.0, Deg, 7999.0), PolarH((SpiralOuter - 200.0) * Scale(8000.0), Deg, 7999.0));
			}

			// 셔틀 패널: 성채 꼭대기 바깥 가장자리에서 4번 섬 가장자리까지 80m 높이로 오간다.
			{
				const FIsland& Target = Sky[4];
				const double Deg = FMath::RadiansToDegrees(FMath::Atan2(Target.Y, Target.X));
				const double Far = FVector2D(Target.X, Target.Y).Size() - (Target.Radius + 250.0) * Scale(Target.TopH);
				Spec.Panels.Add({PolarH(4400.0 + 250.0 * Scale(8000.0), Deg, 7985.0), PolarH(Far, Deg, Target.TopH - 15.0), 400.0, 2.0});
			}

			// 앵커: 모든 건물·다리·섬에 하나씩. 그 밖에 접시 둘레, 접시 가운데, 심연 오름길, 성채 단.
			for (const FBuilding& B : Spec.Buildings)
			{
				const FVector P = BuildingPoint(B, 0.0, 0.0, B.BaseH + RoofH(B) + 700.0);
				Spec.Anchors.Add({P.X, P.Y, P.Z, 0.0});
			}
			for (const FBridgeRamp& B : Spec.Bridges)
			{
				const FVector Mid = (B.From + B.To) * 0.5;
				Spec.Anchors.Add({Mid.X, Mid.Y, Mid.Z + 600.0, 0.0});
			}
			for (const FIsland& I : Spec.Islands)
			{
				if (I.InnerRadius <= 0.0)
				{
					const FVector2D Off = FVector2D(I.X, I.Y).GetSafeNormal() * (I.Radius * 0.6) * Scale(I.TopH);
					Spec.Anchors.Add({I.X - Off.X, I.Y - Off.Y, I.TopH + 800.0, 0.0});
				}
			}
			for (int32 I = 0; I < 6; ++I)
			{
				const FVector2D P = Polar((Spire.DishRadius + 600.0) * Scale(Spire.TopH), I * 60.0);
				Spec.Anchors.Add({P.X, P.Y, Spire.TopH + 600.0, 4000.0});
			}
			Spec.Anchors.Add({0.0, 0.0, Spire.TopH + 800.0, 0.0});
			for (const double H : {-8000.0, -4500.0, -1000.0})
			{
				const FVector2D P = Polar(2600.0, 180.0);
				Spec.Anchors.Add({P.X, P.Y, H, 4000.0});
			}
			for (int32 Q = 0; Q < 4; ++Q)
			{
				for (const FVector2D& Band : {FVector2D(3800.0, 8000.0), FVector2D(4900.0, 6000.0), FVector2D(5900.0, 4000.0), FVector2D(7000.0, 2000.0)})
				{
					const FVector2D P = Polar(Band.X, 65.0 + Q * 90.0);
					Spec.Anchors.Add({P.X, P.Y, Band.Y + 700.0, 0.0});
				}
			}

			// Pod: 문 앞 안뜰, 외성벽 40m 단, 성채 20·40m 단(60m 이상 단은 Pod 안전 반경 250cm보다 좁다), 공동, 공중섬 둘. 접시는 보스 자리로 비운다.
			for (const double Deg : {0.0, 90.0, 180.0, 270.0})
			{
				const FVector2D P = Polar(9700.0, Deg);
				Spec.Spawns.Add({P.X, P.Y, 0.0});
			}
			for (const FVector& S : {PolarH(13700.0, 15.0, 4000.0), PolarH(13700.0, 195.0, 4000.0), PolarH(5900.0, 75.0, 4000.0),
				PolarH(7000.0, 255.0, 2000.0), PolarH(2600.0, 45.0, -10000.0), PolarH(2600.0, 225.0, -10000.0)})
			{
				Spec.Spawns.Add({S.X, S.Y, S.Z});
			}
			for (const int32 I : {5, 9})
			{
				Spec.Spawns.Add({Sky[I].X, Sky[I].Y, Sky[I].TopH});
			}

			// 등불: 천장 아래만 밝힌다. 노출 EV 6.2에서 지면 중심광이 약 160lux이므로 10m에서 수십~수백 lux가 되게 둔다.
			// 심연 벽 고리 3단, 공동 천장 둘레와 기둥 밑동, 접시 나팔 안, 성채·외성벽 문 터널, 건물 층마다 천장 밑.
			for (int32 Ring = 0; Ring < 3; ++Ring)
			{
				const double H = -5500.0 + Ring * 2000.0;
				for (int32 I = 0; I < 6; ++I)
				{
					const FVector2D P = Polar(Abyss.RadiusArc - 150.0 * Scale(H), 30.0 * Ring + 60.0 * I);
					Spec.Lamps.Add({P.X, P.Y, H, 40000.0, 5000.0});
				}
			}
			for (int32 I = 0; I < 8; ++I)
			{
				const FVector2D P = Polar(4000.0, 22.5 + 45.0 * I);
				Spec.Lamps.Add({P.X, P.Y, Abyss.CeilingH - 500.0, 40000.0, 5000.0});
			}
			for (int32 I = 0; I < 4; ++I)
			{
				const FVector2D P = Polar((SpiralOuter + 400.0) * Scale(Abyss.FloorH), 45.0 + 90.0 * I);
				Spec.Lamps.Add({P.X, P.Y, Abyss.FloorH + 400.0, 20000.0, 3000.0});
			}
			for (const FVector2D& Level : {FVector2D(2100.0, 16000.0), FVector2D(2500.0, 17200.0)})
			{
				for (int32 I = 0; I < 4; ++I)
				{
					const FVector2D P = Polar(Level.X * Scale(Level.Y), 45.0 + 90.0 * I);
					Spec.Lamps.Add({P.X, P.Y, Level.Y, 15000.0, 2500.0});
				}
			}
			for (const double Deg : {0.0, 90.0, 180.0, 270.0})
			{
				for (const double R : {4000.0, 5500.0, 7000.0})
				{
					const FVector2D P = Polar(R, Deg);
					Spec.Lamps.Add({P.X, P.Y, 1500.0, 20000.0, 3000.0});
				}
				for (const double R : {12500.0, 13700.0, 14900.0})
				{
					const FVector2D P = Polar(R, Deg);
					Spec.Lamps.Add({P.X, P.Y, 2600.0, 25000.0, 3500.0});
				}
			}
			for (const FBuilding& B : Spec.Buildings)
			{
				// 층고 8m 기준 값. 층고가 높으면 반경을 같은 비율로, 세기를 제곱으로 키워 바닥 밝기를 맞춘다.
				const double K = FMath::Max(1.0, B.StoryHeight / 800.0);
				for (int32 Story = 0; Story < B.Stories; ++Story)
				{
					const FVector P = BuildingPoint(B, 0.0, 0.0, B.BaseH + (Story + 1) * B.StoryHeight - 150.0);
					Spec.Lamps.Add({P.X, P.Y, P.Z, 15000.0 * K * K, 2500.0 * K});
				}
			}

			// 검토 지점(LNP.Debug.TeleportToReview <Name> [Slot]).
			{
				const FRingStair& OuterGround = Outer.Stairs[2];
				const FVector2D OuterSpan = StairSpanDeg(OuterGround);
				const FVector2D OuterRadii = StairRadii(OuterGround);
				const FVector2D LandingAt = Polar((OuterRadii.X + OuterRadii.Y) * 0.5, OuterGround.StartDeg + OuterSpan.X + OuterSpan.Y * 0.5);
				const FVector2D RampBase = Polar(OuterRadii.Y + 400.0, OuterGround.StartDeg);
				const FVector2D Dish = Polar(2500.0 * Scale(Spire.TopH), 90.0);
				const FVector2D Cavern = Polar(2600.0, 45.0);
				const FVector2D CitadelTop = Polar(3800.0, 45.0);
				const FVector2D NorthGate = Polar(9700.0, 0.0);
				const FVector2D Courtyard = Polar(9700.0, 45.0);
				const FVector2D OuterMiddle = Polar(13700.0, 30.0);
				const FVector2D TallBuilding = Polar(9700.0, 67.5);
				Spec.ReviewPoints = {
					{TEXT("Dish"), Dish.X, Dish.Y, Spire.TopH},
					{TEXT("Cavern"), Cavern.X, Cavern.Y, Abyss.FloorH},
					{TEXT("CitadelTop"), CitadelTop.X, CitadelTop.Y, 8000.0},
					{TEXT("NorthGate"), NorthGate.X, NorthGate.Y, 0.0},
					{TEXT("Courtyard"), Courtyard.X, Courtyard.Y, 0.0},
					{TEXT("TallBuilding"), TallBuilding.X, TallBuilding.Y, 0.0},
					{TEXT("OuterMiddle"), OuterMiddle.X, OuterMiddle.Y, 4000.0},
					{TEXT("OuterRampBase"), RampBase.X, RampBase.Y, 0.0},
					{TEXT("OuterLanding"), LandingAt.X, LandingAt.Y, OuterGround.H1},
					{TEXT("SkyRuinLow"), Sky[3].X, Sky[3].Y, Sky[3].TopH},
					{TEXT("SkyRuinHigh"), Sky[7].X, Sky[7].Y, Sky[7].TopH},
				};
			}
			return Spec;
		}

		/** B `Hollow_00` 협곡·부유섬. 단위 3에서 채운다. 지금은 지각만 있다. */
		FOctantSpec Hollow()
		{
			FOctantSpec Spec;
			Spec.Name = TEXT("Hollow_00");
			Spec.GuidTag = 0x09C00000;
			Spec.Seed = 9002;
			Spec.Amplitude = 1400.0;
			Spec.Wavelength = 7000.0;
			return Spec;
		}

		/** 호출마다 새로 만든다. 함수 안 static이면 Live Coding으로 spec을 고쳐도 옛 값이 남는다. */
		TArray<FOctantSpec> Specs()
		{
			return {Bastion(), Hollow()};
		}

		// ---------------------------------------------------------------------------------------------
		// 지각
		// ---------------------------------------------------------------------------------------------

		double SmoothStep01(double T)
		{
			T = FMath::Clamp(T, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		/** 방향 Dir에서의 [-1, 1] fBm 노이즈. 시드는 노이즈 공간 오프셋으로 넣는다. */
		double Fbm(const FVector& Dir, const FOctantSpec& Spec)
		{
			const FVector Offset(Spec.Seed * 17.137, Spec.Seed * 31.711, Spec.Seed * 7.913);
			double Sum = 0.0, Norm = 0.0, Amplitude = 1.0, Frequency = CrustRadius / Spec.Wavelength;
			for (int32 Octave = 0; Octave < 4; ++Octave)
			{
				Sum += Amplitude * FMath::PerlinNoise3D(Dir * Frequency + Offset);
				Norm += Amplitude;
				Amplitude *= 0.5;
				Frequency *= 2.0;
			}
			return Sum / Norm;
		}

		/** 목표 지형 반지름. 작을수록 높은(구 중심 쪽) 지면이다. */
		double TargetRadius(const FVector& Dir, const FOctantSpec& Spec)
		{
			double Radius = CrustRadius - Spec.Amplitude * Fbm(Dir, Spec);
			for (const FPad& Pad : Spec.Pads)
			{
				const double Arc = FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Radial(Pad.X, Pad.Y)), -1.0, 1.0)) * CrustRadius;
				const double Weight = 1.0 - SmoothStep01((Arc - Pad.Size) / Pad.Falloff);
				Radius = FMath::Lerp(Radius, Pad.Radius, Weight);
			}
			return Radius;
		}

		double SeamWeight(const FVector& Position, const FOctantSpec& Spec)
		{
			// 옥탄트 (+,+,+)에서 이음매 평면 x=0·y=0·z=0까지의 거리는 성분 최솟값이다.
			const double SeamDistance = FMath::Min3(Position.X, Position.Y, Position.Z);
			return SmoothStep01((SeamDistance - Spec.SeamMargin) / Spec.SeamBand);
		}

		/**
		 * 원본 MeshDescription의 삼각형을 읽는다. 넓이 0인 퇴화 삼각형은 버린다. Meadow의 cooked 충돌은 이들을 빼지만
		 * FLNPFixtureMesh로 다시 쓰면 충돌에 남아 법선이 무의미한 hit가 지각 샘플을 Invalid로 만든다.
		 */
		bool ReadMeshTriangles(UStaticMesh& Mesh, FLNPFixtureMesh& Out, int32& OutDegenerate)
		{
			const FMeshDescription* Description = Mesh.GetMeshDescription(0);
			if (!Description)
			{
				return false;
			}
			FStaticMeshConstAttributes Attributes(*Description);
			TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
			TMap<FVertexID, int32> Remap;
			Remap.Reserve(Description->Vertices().Num());
			for (const FVertexID VertexID : Description->Vertices().GetElementIDs())
			{
				Remap.Add(VertexID, Out.AddVertex(FVector(Positions[VertexID])));
			}
			for (const FTriangleID TriangleID : Description->Triangles().GetElementIDs())
			{
				TArrayView<const FVertexID> Vertices = Description->GetTriangleVertices(TriangleID);
				const FIntVector Triangle(Remap[Vertices[0]], Remap[Vertices[1]], Remap[Vertices[2]]);
				const FVector& A = Out.Positions[Triangle.X];
				const FVector& B = Out.Positions[Triangle.Y];
				const FVector& C = Out.Positions[Triangle.Z];
				if (FVector::CrossProduct(B - A, C - A).SizeSquared() < 1.e-6)
				{
					++OutDegenerate;
					continue;
				}
				Out.Triangles.Add(Triangle);
			}
			return Out.Triangles.Num() > 0;
		}

		struct FCrustReport
		{
			int32 Moved = 0;
			int32 Kept = 0;
			int32 Cut = 0;
			double MinRadius = TNumericLimits<double>::Max();
			double MaxRadius = 0.0;
			double MaxShift = 0.0;
		};

		/** Meadow 지각 정점을 목표 지형으로 옮긴다. 이음매 밴드 안쪽 정점은 좌표를 그대로 둔다. */
		FCrustReport DeriveCrust(FLNPFixtureMesh& Crust, const FOctantSpec& Spec)
		{
			FCrustReport Report;
			for (FVector& Position : Crust.Positions)
			{
				const double Weight = SeamWeight(Position, Spec);
				if (Weight > 0.0)
				{
					const double Radius = Position.Size();
					const FVector Dir = Position / Radius;
					const double NewRadius = FMath::Lerp(Radius, TargetRadius(Dir, Spec), Weight);
					Report.MaxShift = FMath::Max(Report.MaxShift, FMath::Abs(NewRadius - Radius));
					Position = Dir * NewRadius;
					++Report.Moved;
				}
				else
				{
					++Report.Kept;
				}
				const double FinalRadius = Position.Size();
				Report.MinRadius = FMath::Min(Report.MinRadius, FinalRadius);
				Report.MaxRadius = FMath::Max(Report.MaxRadius, FinalRadius);
			}
			return Report;
		}

		// ---------------------------------------------------------------------------------------------
		// 옥탄트 공간 메시 도구. 점은 (X, Y, H) 로컬로 받아 Point로 옮긴다.
		// ---------------------------------------------------------------------------------------------

		enum class EFacing : uint8 { Up, Down, Both };

		/** 로컬 사각형 하나. Up/Down은 사각형 중심의 Up 기준이다. */
		void Quad(FLNPFixtureMesh& Mesh, const FVector& A, const FVector& B, const FVector& C, const FVector& D, EFacing Facing)
		{
			const FVector Mid = (A + B + C + D) * 0.25;
			const FVector Up = -Radial(Mid.X, Mid.Y);
			Mesh.AddQuad(Point(A), Point(B), Point(C), Point(D), Facing == EFacing::Down ? -Up : Up, Facing == EFacing::Both);
		}

		/** 두 로컬 곡선 A(t)·B(t) 사이 띠를 Segments 칸으로 나눈다. */
		void Strip(FLNPFixtureMesh& Mesh, TFunctionRef<FVector(double)> A, TFunctionRef<FVector(double)> B, int32 Segments, EFacing Facing)
		{
			for (int32 S = 0; S < Segments; ++S)
			{
				const double T0 = double(S) / Segments, T1 = double(S + 1) / Segments;
				Quad(Mesh, A(T0), A(T1), B(T1), B(T0), Facing);
			}
		}

		/** 로컬 점 At에서 로컬 방향 Dir(XY, 호 길이)을 가리키는 월드 방향. 면 감김 판정용이다. */
		FVector LocalDirection(const FVector& At, const FVector2D& Dir)
		{
			return Point(At.X + Dir.X * 10.0, At.Y + Dir.Y * 10.0, At.Z) - Point(At);
		}

		/**
		 * 로컬 선분 A→B 위에 선 벽. 높이 Bottom~Top(A 끝은 TopA, B 끝은 TopB). 높이 방향은 2,000cm 이하로 나눈다.
		 * SolidSide가 0이면 두께 없는 양면 벽이다. ±1이면 A→B의 왼쪽(+1)·오른쪽(-1), 즉 솔리드 쪽으로 WallThickness만큼
		 * 두께를 준 닫힌 판(앞뒤 면·양 끝 면)이다. 두께 0인 면은 캡슐이 살짝 겹친 채 sweep하면 뒤로 빠져서(Chaos trimesh)
		 * 빈 구조물 속에 갇힌다. 윗면·밑면 마감은 다른 면이 덮지 않을 때만 요청한다(바닥과 같은 높이에 겹치지 않게).
		 */
		void Wall(FLNPFixtureMesh& Mesh, const FVector2D& A, const FVector2D& B, double Bottom, double TopA, double TopB,
			int32 SolidSide, bool bTopCap = false, bool bBottomCap = false)
		{
			const int32 Rows = FMath::Max(1, FMath::CeilToInt((FMath::Max(TopA, TopB) - Bottom) / 2000.0));
			auto Face = [&](const FVector2D& FA, double FTopA, const FVector2D& FB, double FTopB, const FVector2D& Out, bool bBoth)
			{
				for (int32 Row = 0; Row < Rows; ++Row)
				{
					const double TA = double(Row) / Rows, TB = double(Row + 1) / Rows;
					const FVector P0(FA.X, FA.Y, FMath::Lerp(Bottom, FTopA, TA)), P1(FB.X, FB.Y, FMath::Lerp(Bottom, FTopB, TA));
					const FVector P2(FB.X, FB.Y, FMath::Lerp(Bottom, FTopB, TB)), P3(FA.X, FA.Y, FMath::Lerp(Bottom, FTopA, TB));
					Mesh.AddQuad(Point(P0), Point(P1), Point(P2), Point(P3), LocalDirection((P0 + P2) * 0.5, Out), bBoth);
				}
			};
			const FVector2D Along = (B - A).GetSafeNormal();
			const FVector2D Left(-Along.Y, Along.X);
			if (SolidSide == 0)
			{
				++GZeroThicknessWalls;
				Face(A, TopA, B, TopB, Left, true);
				return;
			}
			const FVector2D Into = Left * double(SolidSide) * WallThickness * Scale(FMath::Max(TopA, TopB));
			const FVector2D A2 = A + Into, B2 = B + Into;
			Face(A, TopA, B, TopB, -Into, false);
			Face(B2, TopB, A2, TopA, Into, false);
			// 양 끝 면.
			auto EndCap = [&](const FVector2D& E, const FVector2D& E2, double Top, const FVector2D& Out)
			{
				const FVector Q0(E.X, E.Y, Bottom), Q1(E2.X, E2.Y, Bottom), Q2(E2.X, E2.Y, Top), Q3(E.X, E.Y, Top);
				Mesh.AddQuad(Point(Q0), Point(Q1), Point(Q2), Point(Q3), LocalDirection((Q0 + Q2) * 0.5, Out), false);
			};
			EndCap(A, A2, TopA, -Along);
			EndCap(B, B2, TopB, Along);
			const FVector2D Mid = (A + B + A2 + B2) * 0.25;
			if (bTopCap)
			{
				Mesh.AddQuad(Point({A.X, A.Y, TopA}), Point({B.X, B.Y, TopB}), Point({B2.X, B2.Y, TopB}), Point({A2.X, A2.Y, TopA}),
					-Radial(Mid.X, Mid.Y), false);
			}
			if (bBottomCap)
			{
				Mesh.AddQuad(Point({A.X, A.Y, Bottom}), Point({B.X, B.Y, Bottom}), Point({B2.X, B2.Y, Bottom}), Point({A2.X, A2.Y, Bottom}),
					Radial(Mid.X, Mid.Y), false);
			}
		}

		void Wall(FLNPFixtureMesh& Mesh, const FVector2D& A, const FVector2D& B, double Bottom, double Top, int32 SolidSide,
			bool bTopCap = false, bool bBottomCap = false)
		{
			Wall(Mesh, A, B, Bottom, Top, Top, SolidSide, bTopCap, bBottomCap);
		}

		/** 높이 H의 구면 사각 격자. Skip이 참인 칸(중심 기준)은 비운다. */
		void Grid(FLNPFixtureMesh& Mesh, double X0, double X1, double Y0, double Y1, double H, double Cell, EFacing Facing,
			TFunctionRef<bool(double, double)> Skip)
		{
			const int32 NX = FMath::CeilToInt((X1 - X0) / Cell), NY = FMath::CeilToInt((Y1 - Y0) / Cell);
			for (int32 IY = 0; IY < NY; ++IY)
			{
				for (int32 IX = 0; IX < NX; ++IX)
				{
					const double A = FMath::Lerp(X0, X1, double(IX) / NX), B = FMath::Lerp(X0, X1, double(IX + 1) / NX);
					const double C = FMath::Lerp(Y0, Y1, double(IY) / NY), D = FMath::Lerp(Y0, Y1, double(IY + 1) / NY);
					if (Skip((A + B) * 0.5, (C + D) * 0.5))
					{
						continue;
					}
					Quad(Mesh, {A, C, H}, {B, C, H}, {B, D, H}, {A, D, H}, Facing);
				}
			}
		}

		/**
		 * (X, Y) 중심 고리(Inner = 0이면 원판). 반지름은 호 길이다. 꼭짓점은 Sides 등분이고 고리는 Rings 칸으로 나눈다.
		 * 원판이면 가운데는 부채꼴 삼각형이다.
		 */
		void Annulus(FLNPFixtureMesh& Mesh, double X, double Y, double Inner, double Outer, double H, EFacing Facing, int32 Sides, int32 Rings)
		{
			auto At = [&](double R, int32 S)
			{
				const double Angle = UE_TWO_PI * S / Sides;
				return FVector(X + R * FMath::Cos(Angle), Y + R * FMath::Sin(Angle), H);
			};
			const int32 FirstRing = Inner > 0.0 ? 0 : 1;
			if (Inner <= 0.0)
			{
				const FVector Up = -Radial(X, Y) * (Facing == EFacing::Down ? -1.0 : 1.0);
				const int32 CenterIndex = Mesh.AddVertex(Point(X, Y, H));
				for (int32 S = 0; S < Sides; ++S)
				{
					const int32 A = Mesh.AddVertex(Point(At(Outer / Rings, S))), B = Mesh.AddVertex(Point(At(Outer / Rings, S + 1)));
					Mesh.AddTriangle(CenterIndex, A, B, Up);
					if (Facing == EFacing::Both)
					{
						Mesh.AddTriangle(CenterIndex, A, B, -Up);
					}
				}
			}
			for (int32 R = FirstRing; R < Rings; ++R)
			{
				const double R0 = FMath::Lerp(Inner, Outer, double(R) / Rings), R1 = FMath::Lerp(Inner, Outer, double(R + 1) / Rings);
				for (int32 S = 0; S < Sides; ++S)
				{
					Quad(Mesh, At(R0, S), At(R0, S + 1), At(R1, S + 1), At(R1, S), Facing);
				}
			}
		}

		/** (X, Y) 중심 반지름 R(호 길이) 원기둥 옆면(양면). Annulus와 같은 꼭짓점을 쓴다. */
		/** SolidSide +1이면 원기둥 안쪽, -1이면 바깥쪽이 솔리드다(Wall 참고). */
		void Cylinder(FLNPFixtureMesh& Mesh, double X, double Y, double R, double Bottom, double Top, int32 Sides, int32 SolidSide,
			bool bTopCap = false)
		{
			for (int32 S = 0; S < Sides; ++S)
			{
				const double A0 = UE_TWO_PI * S / Sides, A1 = UE_TWO_PI * (S + 1) / Sides;
				Wall(Mesh, {X + R * FMath::Cos(A0), Y + R * FMath::Sin(A0)}, {X + R * FMath::Cos(A1), Y + R * FMath::Sin(A1)}, Bottom, Top,
					SolidSide, bTopCap);
			}
		}

		/** 로컬 transform의 단위 상자(100cm)를 메시에 더한다. */
		void AppendBox(FLNPFixtureMesh& Mesh, const FTransform& Transform)
		{
			const FLNPFixtureMesh Box = LNPFixtureMesh::BuildUnitBox();
			const int32 Base = Mesh.Positions.Num();
			for (const FVector& P : Box.Positions)
			{
				Mesh.AddVertex(Transform.TransformPosition(P));
			}
			for (const FIntVector& T : Box.Triangles)
			{
				Mesh.Triangles.Emplace(T.X + Base, T.Y + Base, T.Z + Base);
			}
		}

		/** 바닥(Support)과 외피(Blocker)·장식(Decoration) 한 묶음. Coarse면 바닥을 100cm 해상도로 굽는다(D-072). */
		struct FPart
		{
			FString Id;
			FLNPFixtureMesh Floor;
			FLNPFixtureMesh Shell;
			FLNPFixtureMesh Deco;
			FTransform Placement = FTransform::Identity;
			bool bCoarse = false;
		};

		// ---------------------------------------------------------------------------------------------
		// 건물(건물 로컬, 원점 = 바닥 구면 위 건물 중심, +Z = Up, 실제 cm)
		// ---------------------------------------------------------------------------------------------

		/** 건물 로컬 좌표의 구면 바닥 높이. */
		double FloorZ(double X, double Y, double PadRadius)
		{
			return PadRadius - FMath::Sqrt(PadRadius * PadRadius - X * X - Y * Y);
		}

		/**
		 * 건물. 1층 바닥은 받치는 지면이고, 위층 바닥과 지붕을 Support로 둔다. 층 s에서 s+1로 오르는 경사로는 s가 짝수면
		 * -Y 벽을 따라 +X 쪽으로, 홀수면 +Y 벽을 따라 -X 쪽으로 오르고 그 위 바닥을 비운다(D-070). 외벽은 문 자리만 비우고
		 * 문 위 인방을 남긴다. 무너진 면은 잔해 높이 벽만 남고, 그 면 쪽 꼭대기 층 바닥과 지붕의 한쪽 절반 띠가 무너진다.
		 */
		FPart BuildBuilding(const FBuilding& Building)
		{
			FPart Out;
			Out.Id = Building.Id;
			const double PadRadius = CrustRadius - Building.BaseH;
			const double HX = Building.SizeX * 0.5, HY = Building.SizeY * 0.5;
			const double Story = Building.StoryHeight;
			auto P = [PadRadius](double X, double Y, double H) { return FVector(X, Y, FloorZ(X, Y, PadRadius) + H); };

			// 무너진 면 쪽 띠(깊이 35%, 면의 + 절반).
			TArray<FBox2D> Collapse;
			for (const int32 Side : Building.Collapsed)
			{
				const double DX = Building.SizeX * 0.35, DY = Building.SizeY * 0.35;
				switch (Side)
				{
				case 0: Collapse.Add(FBox2D(FVector2D(-HX, 0.0), FVector2D(-HX + DX, HY))); break;
				case 1: Collapse.Add(FBox2D(FVector2D(HX - DX, 0.0), FVector2D(HX, HY))); break;
				case 2: Collapse.Add(FBox2D(FVector2D(0.0, -HY), FVector2D(HX, -HY + DY))); break;
				default: Collapse.Add(FBox2D(FVector2D(0.0, HY - DY), FVector2D(HX, HY))); break;
				}
			}

			// 구면 슬래브를 200cm 이하 칸으로 나눠 윗면(Support)과 SlabThickness 아래 밑면(Shell)을 만든다. 구멍 사각형은 비운다.
			auto Slab = [&](double H, TConstArrayView<FBox2D> Holes)
			{
				const int32 NX = FMath::CeilToInt(Building.SizeX / 200.0), NY = FMath::CeilToInt(Building.SizeY / 200.0);
				for (int32 IY = 0; IY < NY; ++IY)
				{
					for (int32 IX = 0; IX < NX; ++IX)
					{
						const double A = FMath::Lerp(-HX, HX, double(IX) / NX), B = FMath::Lerp(-HX, HX, double(IX + 1) / NX);
						const double C = FMath::Lerp(-HY, HY, double(IY) / NY), D = FMath::Lerp(-HY, HY, double(IY + 1) / NY);
						const FVector2D Mid((A + B) * 0.5, (C + D) * 0.5);
						if (Holes.ContainsByPredicate([&Mid](const FBox2D& Hole) { return Hole.bIsValid && Hole.IsInside(Mid); }))
						{
							continue;
						}
						Out.Floor.AddQuad(P(A, C, H), P(B, C, H), P(B, D, H), P(A, D, H), FVector::UpVector, false);
						Out.Shell.AddQuad(P(A, C, H - SlabThickness), P(B, C, H - SlabThickness),
							P(B, D, H - SlabThickness), P(A, D, H - SlabThickness), -FVector::UpVector, false);
					}
				}
			};

			// 외벽은 바깥쪽으로 WallThickness 두께의 닫힌 판이다(방 크기 유지, Wall 참고). 500cm 이하로 나눠 곡면 바닥과의 틈을
			// BuryDepth 안에 둔다. 윗면은 지붕 가장자리 옆이라 늘 닫고, 밑면은 땅에 묻히지 않을 때(인방)만 닫는다.
			auto WallPiece = [&](const FVector2D& A, const FVector2D& B, double H0, double H1)
			{
				const FVector2D Along = (B - A).GetSafeNormal();
				FVector2D Outward(Along.Y, -Along.X);
				if (FVector2D::DotProduct(Outward, (A + B) * 0.5) < 0.0)
				{
					Outward = -Outward;
				}
				const FVector2D T = Outward * WallThickness;
				const FVector Out3(Outward, 0.0);
				const int32 N = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 500.0));
				for (int32 I = 0; I < N; ++I)
				{
					const FVector2D C = FMath::Lerp(A, B, double(I) / N), D = FMath::Lerp(A, B, double(I + 1) / N);
					const FVector2D C2 = C + T, D2 = D + T;
					Out.Shell.AddQuad(P(C.X, C.Y, H0), P(D.X, D.Y, H0), P(D.X, D.Y, H1), P(C.X, C.Y, H1), -Out3, false);
					Out.Shell.AddQuad(P(C2.X, C2.Y, H0), P(D2.X, D2.Y, H0), P(D2.X, D2.Y, H1), P(C2.X, C2.Y, H1), Out3, false);
					Out.Shell.AddQuad(P(C.X, C.Y, H1), P(D.X, D.Y, H1), P(D2.X, D2.Y, H1), P(C2.X, C2.Y, H1), FVector::UpVector, false);
					if (H0 > 0.0)
					{
						Out.Shell.AddQuad(P(C.X, C.Y, H0), P(D.X, D.Y, H0), P(D2.X, D2.Y, H0), P(C2.X, C2.Y, H0), -FVector::UpVector, false);
					}
				}
				for (const FVector2D& E : {A, B})
				{
					const FVector2D E2 = E + T;
					const FVector Cap(E == A ? -Along : Along, 0.0);
					Out.Shell.AddQuad(P(E.X, E.Y, H0), P(E2.X, E2.Y, H0), P(E2.X, E2.Y, H1), P(E.X, E.Y, H1), Cap, false);
				}
			};

			const double WallTop = RoofH(Building);
			// ±Y 벽은 두께만큼 길게 뻗어 모서리를 채운다.
			const double CX = HX + WallThickness;
			const FVector2D Corners[4][2] = {
				{{-HX, -HY}, {-HX, HY}}, {{HX, -HY}, {HX, HY}}, {{-CX, -HY}, {CX, -HY}}, {{-CX, HY}, {CX, HY}}};
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const FVector2D A = Corners[Side][0], B = Corners[Side][1];
				if (Building.Collapsed.Contains(Side))
				{
					const FVector2D Dir = (B - A).GetSafeNormal();
					WallPiece(A, A + Dir * RuinStubLength, -BuryDepth, WallTop * 0.55);
					WallPiece(B - Dir * RuinStubLength, B, -BuryDepth, WallTop * 0.55);
					continue;
				}
				const double Half = FVector2D::Distance(A, B) * 0.5;
				TArray<FDoor> Doors = Building.Doors.FilterByPredicate([Side](const FDoor& D) { return D.Side == Side; });
				Doors.Sort([](const FDoor& L, const FDoor& R) { return L.Offset < R.Offset; });
				auto At = [&](double S) { return FMath::Lerp(A, B, (S + Half) / (2.0 * Half)); };
				double Cursor = -Half;
				for (const FDoor& Door : Doors)
				{
					const double D0 = Door.Offset - Door.Width * 0.5, D1 = Door.Offset + Door.Width * 0.5;
					WallPiece(At(Cursor), At(D0), -BuryDepth, WallTop);
					WallPiece(At(D0), At(D1), Door.Height, WallTop);
					Cursor = D1;
				}
				WallPiece(At(Cursor), At(Half), -BuryDepth, WallTop);
			}

			// 경사로와 계단 칸(Decoration, 칸 높이 20cm).
			const double RampLength = Story / FMath::Tan(FMath::DegreesToRadians(RampSlopeDeg));
			const int32 RampSegments = FMath::CeilToInt(RampLength / 100.0);
			const int32 StepCount = FMath::RoundToInt(Story / 20.0);
			TArray<FBox2D> Holes;
			for (int32 S = 0; S + 1 < Building.Stories; ++S)
			{
				const bool bEven = S % 2 == 0;
				const double H0 = S * Story;
				const double XLow = bEven ? -HX + 200.0 : HX - 200.0;
				const double XHigh = bEven ? XLow + RampLength : XLow - RampLength;
				const double Y0 = bEven ? -HY : HY - Building.RampWidth, Y1 = bEven ? -HY + Building.RampWidth : HY;
				const double InnerY = bEven ? Y1 : Y0;
				for (int32 Seg = 0; Seg < RampSegments; ++Seg)
				{
					const double T0 = double(Seg) / RampSegments, T1 = double(Seg + 1) / RampSegments;
					const double XA = FMath::Lerp(XLow, XHigh, T0), XB = FMath::Lerp(XLow, XHigh, T1);
					Out.Floor.AddQuad(P(XA, Y0, H0 + Story * T0), P(XB, Y0, H0 + Story * T1),
						P(XB, Y1, H0 + Story * T1), P(XA, Y1, H0 + Story * T0), FVector::UpVector, false);
					const double Bottom = S == 0 ? -BuryDepth : H0 - SlabThickness;
					Out.Shell.AddQuad(P(XA, InnerY, Bottom), P(XB, InnerY, Bottom),
						P(XB, InnerY, H0 + Story * T1), P(XA, InnerY, H0 + Story * T0), FVector::YAxisVector, true);
				}
				for (int32 Step = 0; Step < StepCount; ++Step)
				{
					const double StepLength = RampLength / StepCount;
					const double X = XLow + (bEven ? 1.0 : -1.0) * (Step + 0.5) * StepLength;
					const double Rise = Story * (Step + 1) / StepCount;
					AppendBox(Out.Deco, FTransform(FQuat::Identity, P(X, (Y0 + Y1) * 0.5, H0 + Rise * 0.5 - 2.0),
						FVector(StepLength / 100.0, Building.RampWidth / 100.0, Rise / 100.0)));
				}
				Holes.Add(bEven ? FBox2D(FVector2D(XLow - 50.0, Y0), FVector2D(XHigh, Y1 + 50.0))
					: FBox2D(FVector2D(XHigh, Y0 - 50.0), FVector2D(XLow + 50.0, Y1)));
			}
			for (int32 Story_ = 1; Story_ < Building.Stories; ++Story_)
			{
				TArray<FBox2D> SlabHoles = {Holes[Story_ - 1]};
				if (Story_ == Building.Stories - 1 && Building.Stories >= 3)
				{
					SlabHoles.Append(Collapse);
				}
				Slab(Story_ * Story, SlabHoles);
			}
			Slab(WallTop, Collapse);
			Out.Placement = FTransform(Rotation(Building.X, Building.Y) * FQuat(FVector::UpVector, FMath::DegreesToRadians(Building.Yaw)),
				Radial(Building.X, Building.Y) * PadRadius);
			return Out;
		}

		// ---------------------------------------------------------------------------------------------
		// 옥탄트 공간 구조물
		// ---------------------------------------------------------------------------------------------

		/** 원판·고리 섬: 윗면 Support, 밑면·둘레 Blocker. 반지름은 윗면 높이 실제 cm다. */
		FPart BuildIsland(const FIsland& Island)
		{
			FPart Out;
			Out.Id = Island.Id;
			Out.bCoarse = Island.bCoarse;
			const double K = Scale(Island.TopH);
			const double Outer = Island.Radius * K, Inner = Island.InnerRadius * K;
			const double Bottom = Island.TopH - Island.Thickness;
			const int32 Sides = FMath::Clamp(FMath::CeilToInt(Island.Radius / 150.0), 24, 96);
			const int32 Rings = FMath::Max(2, FMath::CeilToInt((Island.Radius - Island.InnerRadius) / 400.0));
			Annulus(Out.Floor, Island.X, Island.Y, Inner, Outer, Island.TopH, EFacing::Up, Sides, Rings);
			Annulus(Out.Shell, Island.X, Island.Y, Inner, Outer, Bottom, EFacing::Down, Sides, Rings);
			Cylinder(Out.Shell, Island.X, Island.Y, Outer, Bottom, Island.TopH, Sides, +1);
			if (Inner > 0.0)
			{
				Cylinder(Out.Shell, Island.X, Island.Y, Inner, Bottom, Island.TopH, Sides, -1);
			}
			return Out;
		}

		/** 공중 직선 다리: 윗면 Support, 두께 아래 밑면과 양옆 Blocker. 폭은 각 지점 높이의 실제 cm다. */
		FPart BuildBridge(const FBridgeRamp& Bridge)
		{
			FPart Out;
			Out.Id = Bridge.Id;
			const FVector2D Along = FVector2D(Bridge.To - Bridge.From).GetSafeNormal();
			const FVector2D Side(-Along.Y, Along.X);
			const int32 Segments = FMath::Max(2, FMath::CeilToInt(FVector2D::Distance(FVector2D(Bridge.From), FVector2D(Bridge.To)) / 200.0));
			auto Line = [&](double Lateral, double Down)
			{
				return [&, Lateral, Down](double T)
				{
					const FVector P = FMath::Lerp(Bridge.From, Bridge.To, T);
					const FVector2D Offset = Side * (Lateral * Bridge.Width * 0.5 * Scale(P.Z));
					return FVector(P.X + Offset.X, P.Y + Offset.Y, P.Z - Down);
				};
			};
			for (int32 Band = 0; Band < 2; ++Band)
			{
				Strip(Out.Floor, Line(Band - 1.0, 0.0), Line(double(Band), 0.0), Segments, EFacing::Up);
			}
			Strip(Out.Shell, Line(-1.0, BridgeThickness), Line(1.0, BridgeThickness), Segments, EFacing::Down);
			Strip(Out.Shell, Line(-1.0, BridgeThickness), Line(-1.0, 0.0), Segments, EFacing::Both);
			Strip(Out.Shell, Line(1.0, BridgeThickness), Line(1.0, 0.0), Segments, EFacing::Both);
			return Out;
		}

		/** 1D 구간 두 개의 대칭차. 빈 구간은 Lo >= Hi다. */
		void ExposedIntervals(double ALo, double AHi, double BLo, double BHi, TArray<FVector2D>& Out)
		{
			Out.Reset();
			const bool bA = AHi > ALo, bB = BHi > BLo;
			if (!bA && !bB) return;
			if (!bA) { Out.Add({BLo, BHi}); return; }
			if (!bB) { Out.Add({ALo, AHi}); return; }
			const double Lo = FMath::Max(ALo, BLo), Hi = FMath::Min(AHi, BHi);
			if (Hi <= Lo) { Out.Add({ALo, AHi}); Out.Add({BLo, BHi}); return; }
			if (FMath::Abs(ALo - BLo) > 1.0) Out.Add({FMath::Min(ALo, BLo), Lo});
			if (FMath::Abs(AHi - BHi) > 1.0) Out.Add({Hi, FMath::Max(AHi, BHi)});
		}

		/**
		 * 계단식 성벽. 각도 조각마다 띠 윗면(Support), 띠 사이 노출 면(Blocker)을 만든다. 문 조각에서 띠는 문 높이 위만 남고
		 * 그 밑면이 인방이다. 문 가장자리 각도에는 문설주 면을 세운다. 경사로는 경계면에 붙어 낮은 단 위에 놓인다.
		 */
		FPart BuildTieredRing(const FTieredRing& Ring)
		{
			FPart Out;
			Out.Id = Ring.Id;
			Out.bCoarse = Ring.bCoarse;
			const double RMid = (Ring.Bands[0].R0 + Ring.Bands.Last().R1) * 0.5;

			// 각도 분할점: 1.5° 간격 + 문 가장자리.
			TArray<double> Breaks;
			for (int32 I = 0; I < 240; ++I) { Breaks.Add(I * 1.5); }
			for (const FRingGate& Gate : Ring.Gates)
			{
				const double Half = FMath::RadiansToDegrees(Gate.Width * 0.5 / RMid);
				Breaks.Add(FMath::Fmod(Gate.Deg - Half + 360.0, 360.0));
				Breaks.Add(FMath::Fmod(Gate.Deg + Half + 360.0, 360.0));
			}
			Breaks.Sort();
			Breaks.Add(Breaks[0] + 360.0);
			auto GateAt = [&](double Deg) -> const FRingGate*
			{
				for (const FRingGate& Gate : Ring.Gates)
				{
					const double Half = FMath::RadiansToDegrees(Gate.Width * 0.5 / RMid);
					if (FMath::Abs(FMath::FindDeltaAngleDegrees(Gate.Deg, Deg)) < Half) return &Gate;
				}
				return nullptr;
			};
			// 띠 i의 그 조각 안 솔리드 높이 구간.
			auto Solid = [&](int32 Band, const FRingGate* Gate) -> FVector2D
			{
				if (Band < 0 || Band >= Ring.Bands.Num()) return {0.0, 0.0};
				const double H = Ring.Bands[Band].H;
				if (!Gate) return {-WallSink, H};
				return H > Gate->Height ? FVector2D(Gate->Height, H) : FVector2D(0.0, 0.0);
			};

			TArray<FVector2D> Exposed;
			for (int32 Piece = 0; Piece + 1 < Breaks.Num(); ++Piece)
			{
				const double A0 = Breaks[Piece], A1 = Breaks[Piece + 1];
				if (A1 - A0 < 1.e-6) continue;
				const FRingGate* Gate = GateAt((A0 + A1) * 0.5);
				for (int32 Band = 0; Band < Ring.Bands.Num(); ++Band)
				{
					const FRingBand& B = Ring.Bands[Band];
					const FVector2D S = Solid(Band, Gate);
					if (S.Y <= S.X) continue;
					const FVector2D I0 = Polar(B.R0, A0), I1 = Polar(B.R0, A1), O0 = Polar(B.R1, A0), O1 = Polar(B.R1, A1);
					Quad(Out.Floor, {I0.X, I0.Y, B.H}, {I1.X, I1.Y, B.H}, {O1.X, O1.Y, B.H}, {O0.X, O0.Y, B.H}, EFacing::Up);
					// 문 조각은 인방 밑면, 그 밖은 윗면 바로 아래 밑면으로 띠 윗면을 닫는다(속에서 천장이 비어 보이지 않게).
					const double Under = Gate ? S.X : B.H - SlabThickness;
					Quad(Out.Shell, {I0.X, I0.Y, Under}, {I1.X, I1.Y, Under}, {O1.X, O1.Y, Under}, {O0.X, O0.Y, Under}, EFacing::Down);
				}
				for (int32 Boundary = 0; Boundary <= Ring.Bands.Num(); ++Boundary)
				{
					const double R = Boundary < Ring.Bands.Num() ? Ring.Bands[Boundary].R0 : Ring.Bands.Last().R1;
					const FVector2D L = Solid(Boundary - 1, Gate), Rr = Solid(Boundary, Gate);
					ExposedIntervals(L.X, L.Y, Rr.X, Rr.Y, Exposed);
					for (const FVector2D& E : Exposed)
					{
						// 각도가 커지는 방향(A0→A1)의 왼쪽이 안쪽(작은 반지름)이다. 그 구간을 안쪽 띠가 채우면 두께는 안쪽으로.
						const double MidH = (E.X + E.Y) * 0.5;
						const bool bInnerSolid = MidH > L.X && MidH < L.Y;
						Wall(Out.Shell, Polar(R, A0), Polar(R, A1), E.X, E.Y, bInnerSolid ? +1 : -1);
					}
				}
			}
			// 문설주: 문 가장자리 각도에서 문 밖 솔리드와 문 안 솔리드의 차이.
			for (const FRingGate& Gate : Ring.Gates)
			{
				const double Half = FMath::RadiansToDegrees(Gate.Width * 0.5 / RMid);
				for (const double Deg : {Gate.Deg - Half, Gate.Deg + Half})
				{
					// 반지름이 커지는 방향의 왼쪽은 각도가 커지는 쪽이다. 문 밖 솔리드는 문 중심 반대쪽에 있다.
					const int32 OutsideSide = Deg > Gate.Deg ? +1 : -1;
					for (int32 Band = 0; Band < Ring.Bands.Num(); ++Band)
					{
						const FVector2D In = Solid(Band, &Gate), OutS = Solid(Band, nullptr);
						ExposedIntervals(In.X, In.Y, OutS.X, OutS.Y, Exposed);
						for (const FVector2D& E : Exposed)
						{
							const double MidH = (E.X + E.Y) * 0.5;
							const bool bOutsideSolid = MidH > OutS.X && MidH < OutS.Y;
							Wall(Out.Shell, Polar(Ring.Bands[Band].R0, Deg), Polar(Ring.Bands[Band].R1, Deg), E.X, E.Y,
								bOutsideSolid ? OutsideSide : -OutsideSide);
						}
					}
				}
			}
			// 경사로(30°)와 계단 칸.
			for (const FRingStair& Stair : Ring.Stairs)
			{
				const FVector2D Radii = StairRadii(Stair);
				const double R0 = Radii.X, R1 = Radii.Y;
				const double FreeR = Stair.bOutside ? R1 : R0;
				const double RealLength = (Stair.H1 - Stair.H0) / FMath::Tan(FMath::DegreesToRadians(RampSlopeDeg));
				const double Span = StairSpanDeg(Stair).X;
				const int32 Segments = FMath::CeilToInt(Span / 0.5);
				auto At = [&](double R, double T) { const FVector2D P = Polar(R, Stair.StartDeg + Span * T); return FVector(P.X, P.Y, FMath::Lerp(Stair.H0, Stair.H1, T)); };
				Strip(Out.Floor, [&](double T) { return At(R0, T); }, [&](double T) { return At(R1, T); }, Segments, EFacing::Up);
				// 경사로 옆면: 경사로 밑(바깥 경사로면 안쪽, 안쪽 경사로면 바깥쪽)이 솔리드다.
				for (int32 Seg = 0; Seg < Segments; ++Seg)
				{
					const FVector P0 = At(FreeR, double(Seg) / Segments), P1 = At(FreeR, double(Seg + 1) / Segments);
					Wall(Out.Shell, FVector2D(P0), FVector2D(P1), Stair.H0 - WallSink, P0.Z, P1.Z, Stair.bOutside ? +1 : -1);
				}
				// 꼭대기 층계참: 경사로 폭과 같은 정사각형 평지(높이 H1)를 경사로 끝에 잇는다. 끝면은 층계참 끝으로 옮긴다.
				const double LandingSpan = StairSpanDeg(Stair).Y;
				const double LandingStart = Stair.StartDeg + Span;
				const int32 LandingSegments = FMath::CeilToInt(LandingSpan / 0.5);
				auto Landing = [&](double R, double T) { const FVector2D P = Polar(R, LandingStart + LandingSpan * T); return FVector(P.X, P.Y, Stair.H1); };
				Strip(Out.Floor, [&](double T) { return Landing(R0, T); }, [&](double T) { return Landing(R1, T); }, LandingSegments, EFacing::Up);
				for (int32 Seg = 0; Seg < LandingSegments; ++Seg)
				{
					Wall(Out.Shell, FVector2D(Landing(FreeR, double(Seg) / LandingSegments)), FVector2D(Landing(FreeR, double(Seg + 1) / LandingSegments)),
						Stair.H0 - WallSink, Stair.H1, Stair.bOutside ? +1 : -1);
				}
				Wall(Out.Shell, Polar(R0, LandingStart + LandingSpan), Polar(R1, LandingStart + LandingSpan), Stair.H0 - WallSink, Stair.H1, -1);
				const int32 Steps = FMath::RoundToInt((Stair.H1 - Stair.H0) / 20.0);
				for (int32 Step = 0; Step < Steps; ++Step)
				{
					const double T = (Step + 0.5) / Steps;
					const double Top = Stair.H0 + (Stair.H1 - Stair.H0) * (Step + 1) / Steps;
					const double Deg = Stair.StartDeg + Span * T;
					const FVector2D C = Polar((R0 + R1) * 0.5, Deg);
					const double K = Scale((Stair.H0 + Top) * 0.5);
					const FQuat Rot = Rotation(C.X, C.Y) * FQuat(FVector::UpVector, FMath::DegreesToRadians(Deg + 90.0));
					AppendBox(Out.Deco, FTransform(Rot, Point(C.X, C.Y, (Stair.H0 + Top) * 0.5 - 2.0),
						FVector(RealLength / Steps / 100.0, (R1 - R0) / K / 100.0, (Top - Stair.H0) / 100.0)));
				}
			}
			return Out;
		}

		/** 나선 기둥: 코어 원기둥 옆면(Blocker)·꼭대기 원판(Support), 코어를 감는 경사로(SplitH마다 다른 부품). 축에서 실제 거리를 유지한다. */
		TArray<FPart> BuildSpire(const FSpire& Spire)
		{
			const int32 Sides = 40;
			auto Real = [&Spire](double R, double Deg, double H)
			{
				const FVector2D P = Polar(R * Scale(H), Deg);
				return FVector(Spire.X + P.X, Spire.Y + P.Y, H);
			};
			FPart Core;
			Core.Id = Spire.Id + TEXT("_Core");
			// 코어 옆면은 1,000cm 높이마다 나눠 실제 반지름을 따라간다.
			const int32 Rows = FMath::CeilToInt((Spire.TopH - Spire.BottomH + WallSink) / 1000.0);
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				const double H0 = FMath::Lerp(Spire.BottomH - WallSink, Spire.TopH, double(Row) / Rows);
				const double H1 = FMath::Lerp(Spire.BottomH - WallSink, Spire.TopH, double(Row + 1) / Rows);
				// 코어는 속이 빈 원통이라 WallThickness 두께의 안팎 두 면으로 닫는다(Wall 참고). 위는 접시·꼭대기 원판이 덮는다.
				for (int32 S = 0; S < Sides; ++S)
				{
					const double D0 = 360.0 * S / Sides, D1 = 360.0 * (S + 1) / Sides;
					for (const double R : {Spire.CoreRadius, Spire.CoreRadius - WallThickness})
					{
						const FVector Mid = Real(R, (D0 + D1) * 0.5, (H0 + H1) * 0.5);
						const FVector Outward = Point(Real(R + 10.0, (D0 + D1) * 0.5, (H0 + H1) * 0.5)) - Point(Mid);
						Core.Shell.AddQuad(Point(Real(R, D0, H0)), Point(Real(R, D1, H0)), Point(Real(R, D1, H1)), Point(Real(R, D0, H1)),
							R == Spire.CoreRadius ? Outward : -Outward, false);
					}
				}
			}
			const double TurnRise = UE_TWO_PI * (Spire.CoreRadius + Spire.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(Spire.SlopeDeg));
			TArray<FPart> Parts;
			if (Spire.DishRadius <= 0.0)
			{
				Annulus(Core.Floor, Spire.X, Spire.Y, 0.0, Spire.CoreRadius * Scale(Spire.TopH), Spire.TopH, EFacing::Up, Sides, 3);
			}
			else
			{
				// 접시: 윗면은 높이 TopH의 구면(어디서나 수평), 나선이 올라오는 틈(나선 끝 6m 오르막 위)은 비운다.
				FPart Dish;
				Dish.Id = Spire.Id + TEXT("_Dish");
				Dish.bCoarse = true;
				const double EndDeg = Spire.ThetaAtZero + 360.0 * Spire.TopH / TurnRise;
				const double SlotSpan = 360.0 * 600.0 / TurnRise;
				const double SpiralOuterR = Spire.CoreRadius + Spire.SpiralWidth;
				constexpr int32 DishSides = 72;
				const int32 DishRings = FMath::CeilToInt(Spire.DishRadius / 200.0);
				for (int32 Ring = 0; Ring < DishRings; ++Ring)
				{
					const double R0 = Spire.DishRadius * Ring / DishRings, R1 = Spire.DishRadius * (Ring + 1) / DishRings;
					for (int32 S = 0; S < DishSides; ++S)
					{
						const double D0 = 360.0 * S / DishSides, D1 = 360.0 * (S + 1) / DishSides;
						const double MidR = (R0 + R1) * 0.5;
						const double Behind = FMath::Fmod(EndDeg - (D0 + D1) * 0.5 + 720.0, 360.0);
						if (MidR > Spire.CoreRadius - 50.0 && MidR < SpiralOuterR + 100.0 && Behind < SlotSpan + 5.0)
						{
							continue;
						}
						if (Ring == 0)
						{
							const FVector Up = -Radial(Spire.X, Spire.Y);
							Dish.Floor.AddTriangle(Dish.Floor.AddVertex(Point(Real(0.0, 0.0, Spire.TopH))),
								Dish.Floor.AddVertex(Point(Real(R1, D0, Spire.TopH))), Dish.Floor.AddVertex(Point(Real(R1, D1, Spire.TopH))), Up);
							continue;
						}
						Quad(Dish.Floor, Real(R0, D0, Spire.TopH), Real(R0, D1, Spire.TopH), Real(R1, D1, Spire.TopH), Real(R1, D0, Spire.TopH), EFacing::Up);
					}
				}
				// 나팔 면(밑에서 나선 바깥을 감싸며 벌어진다)과 테두리.
				const double RimBottom = Spire.TopH - Spire.DishRimThickness;
				constexpr int32 FlareRows = 16;
				auto FlareR = [&](double T) { return SpiralOuterR + 50.0 + (Spire.DishRadius - SpiralOuterR - 50.0) * FMath::Pow(T, 1.8); };
				for (int32 Row = 0; Row < FlareRows; ++Row)
				{
					const double T0 = double(Row) / FlareRows, T1 = double(Row + 1) / FlareRows;
					const double H0 = FMath::Lerp(Spire.DishFlareStartH, RimBottom, T0), H1 = FMath::Lerp(Spire.DishFlareStartH, RimBottom, T1);
					for (int32 S = 0; S < DishSides; ++S)
					{
						const double D0 = 360.0 * S / DishSides, D1 = 360.0 * (S + 1) / DishSides;
						Quad(Dish.Shell, Real(FlareR(T0), D0, H0), Real(FlareR(T0), D1, H0), Real(FlareR(T1), D1, H1), Real(FlareR(T1), D0, H1), EFacing::Both);
					}
				}
				for (int32 S = 0; S < DishSides; ++S)
				{
					const double D0 = 360.0 * S / DishSides, D1 = 360.0 * (S + 1) / DishSides;
					Quad(Dish.Shell, Real(Spire.DishRadius, D0, RimBottom), Real(Spire.DishRadius, D1, RimBottom),
						Real(Spire.DishRadius, D1, Spire.TopH), Real(Spire.DishRadius, D0, Spire.TopH), EFacing::Both);
				}
				Parts.Add(MoveTemp(Dish));
			}
			Parts.Insert(MoveTemp(Core), 0);
			TArray<double> Cuts = {Spire.BottomH};
			Cuts.Append(Spire.SplitH);
			Cuts.Add(Spire.TopH);
			const double Outer = Spire.CoreRadius + Spire.SpiralWidth;
			for (int32 C = 0; C + 1 < Cuts.Num(); ++C)
			{
				FPart Part;
				Part.Id = FString::Printf(TEXT("%s_Ramp%d"), *Spire.Id, C);
				const double HA = Cuts[C], HB = Cuts[C + 1];
				const int32 Segments = FMath::CeilToInt((HB - HA) / TurnRise * 72.0);
				auto Edge = [&](double R, double Down)
				{
					return [&, R, Down](double T)
					{
						const double H = FMath::Lerp(HA, HB, T);
						return Real(R, Spire.ThetaAtZero + 360.0 * H / TurnRise, H - Down);
					};
				};
				for (int32 Band = 0; Band < 3; ++Band)
				{
					const double R0 = FMath::Lerp(Spire.CoreRadius, Outer, Band / 3.0), R1 = FMath::Lerp(Spire.CoreRadius, Outer, (Band + 1) / 3.0);
					Strip(Part.Floor, Edge(R0, 0.0), Edge(R1, 0.0), Segments, EFacing::Up);
					Strip(Part.Shell, Edge(R0, BridgeThickness), Edge(R1, BridgeThickness), Segments, EFacing::Down);
				}
				Strip(Part.Shell, Edge(Outer, BridgeThickness), Edge(Outer, 0.0), Segments, EFacing::Both);
				Parts.Add(MoveTemp(Part));
			}
			return Parts;
		}

		/** 구 중심을 지나고 두 벡터를 품는 평면. Inside 쪽이 PlaneDot < 0이다. */
		FPlane CenterPlane(const FVector& A, const FVector& B, const FVector& Inside)
		{
			FVector Normal = FVector::CrossProduct(A, B).GetSafeNormal();
			if (FVector::DotProduct(Normal, Inside) > 0.0)
			{
				Normal = -Normal;
			}
			return FPlane(Normal, 0.0);
		}

		/**
		 * 심연과 공동. 지각을 반지름 RadiusArc 정다각형 기둥(구 중심에서 퍼지는 각뿔)으로 잘라 내고, 같은 꼭짓점으로 심연 벽을
		 * 공동 천장에서 지면 위 50cm까지 세운다. 공동 바닥은 CoarseSupport(D-072), 천장·옆벽·기둥은 Blocker.
		 */
		FPart BuildAbyss(const FAbyss& Abyss, FLNPFixtureMesh& Crust, int32& OutCut)
		{
			FPart Out;
			Out.Id = Abyss.Id;
			Out.bCoarse = true;
			constexpr int32 Sides = 48;
			const FVector Inside = RadialUnnormalized(Abyss.X, Abyss.Y);
			TArray<FPlane> Planes;
			for (int32 S = 0; S < Sides; ++S)
			{
				const FVector2D A = FVector2D(Abyss.X, Abyss.Y) + Polar(Abyss.RadiusArc, 360.0 * S / Sides);
				const FVector2D B = FVector2D(Abyss.X, Abyss.Y) + Polar(Abyss.RadiusArc, 360.0 * (S + 1) / Sides);
				Planes.Add(CenterPlane(RadialUnnormalized(A.X, A.Y), RadialUnnormalized(B.X, B.Y), Inside));
			}
			OutCut += Crust.RemoveInsideConvex(Planes);
			Cylinder(Out.Shell, Abyss.X, Abyss.Y, Abyss.RadiusArc, Abyss.CeilingH, 50.0, Sides, -1, true);
			Annulus(Out.Floor, Abyss.X, Abyss.Y, 0.0, Abyss.CavernRadiusArc, Abyss.FloorH, EFacing::Up, Sides, 12);
			Annulus(Out.Shell, Abyss.X, Abyss.Y, Abyss.RadiusArc, Abyss.CavernRadiusArc, Abyss.CeilingH, EFacing::Both, Sides, 3);
			Cylinder(Out.Shell, Abyss.X, Abyss.Y, Abyss.CavernRadiusArc, Abyss.FloorH - WallSink, Abyss.CeilingH, Sides, -1);
			for (const FVector& Pillar : Abyss.Pillars)
			{
				const FVector2D C = FVector2D(Abyss.X, Abyss.Y) + Polar(Pillar.X, Pillar.Y);
				const double H = Pillar.Z * Scale(Abyss.FloorH);
				const FVector2D Corners[4] = {{C.X - H, C.Y - H}, {C.X + H, C.Y - H}, {C.X + H, C.Y + H}, {C.X - H, C.Y + H}};
				for (int32 I = 0; I < 4; ++I)
				{
					Wall(Out.Shell, Corners[I], Corners[(I + 1) % 4], Abyss.FloorH - WallSink, Abyss.CeilingH, +1);
				}
			}
			return Out;
		}

		bool ClearWorld(UWorld& World)
		{
			TArray<AActor*> ToDestroy;
			for (TActorIterator<AActor> It(&World); It; ++It)
			{
				if (!It->IsA<AWorldSettings>() && !It->IsA<ABrush>())
				{
					ToDestroy.Add(*It);
				}
			}
			for (AActor* Actor : ToDestroy)
			{
				World.DestroyActor(Actor);
			}
			return true;
		}

		/** /Game/Maps/<Name>/Authored의 StaticMesh를 접미사 규약으로 원점에 배치한다. 규약 밖 이름은 오류로 보고만 한다. */
		int32 PlaceAuthoredMeshes(UWorld& World, const FString& Folder, int32& OutErrors)
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			Registry.ScanPathsSynchronous({Folder}, true);
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPath(FName(*Folder), Assets, false);
			Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });

			int32 Placed = 0;
			for (const FAssetData& Asset : Assets)
			{
				UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
				if (!Mesh)
				{
					continue;
				}
				const FString Name = Asset.AssetName.ToString();
				TArray<FName> Tags;
				FName Profile;
				if (Name.EndsWith(TEXT("_Floor"))) { Tags = {SupportTag, BlockerTag, StaticTag}; Profile = TEXT("LNPStaticTerrain"); }
				else if (Name.EndsWith(TEXT("_Coarse"))) { Tags = {SupportTag, BlockerTag, StaticTag, CoarseTag}; Profile = TEXT("LNPStaticTerrain"); }
				else if (Name.EndsWith(TEXT("_Shell"))) { Tags = {BlockerTag, StaticTag}; Profile = TEXT("LNPStaticBlocker"); }
				else if (Name.EndsWith(TEXT("_Deco"))) { Tags = {DecorationTag}; Profile = TEXT("LNPDecoration"); }
				else
				{
					UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Authored mesh %s has no role suffix (_Floor/_Coarse/_Shell/_Deco)"), *Name);
					++OutErrors;
					continue;
				}
				if (Mesh->GetBodySetup() == nullptr || Mesh->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple)
				{
					UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Authored mesh %s is not complex-as-simple"), *Name);
					++OutErrors;
				}
				LNPFixtureMesh::SpawnMeshActor(World, *Mesh, Name.RightChop(3), FTransform::Identity, MoveTemp(Tags), Profile);
				++Placed;
			}
			return Placed;
		}

		/** 부품의 바닥·외피·장식 메시를 Generated 폴더에 쓰고 배치한다. 빈 메시는 건너뛴다. */
		bool PlacePart(UWorld& World, const FString& Folder, const FOctantSpec& Spec, const FPart& Part, int32& OutActors, int32& OutTriangles)
		{
			struct FRole { const FLNPFixtureMesh* Mesh; const TCHAR* Suffix; TArray<FName> Tags; const TCHAR* Profile; };
			const FRole Roles[] = {
				{&Part.Floor, Part.bCoarse ? TEXT("_Coarse") : TEXT("_Floor"),
					Part.bCoarse ? TArray<FName>{SupportTag, BlockerTag, StaticTag, CoarseTag} : TArray<FName>{SupportTag, BlockerTag, StaticTag}, TEXT("LNPStaticTerrain")},
				{&Part.Shell, TEXT("_Shell"), {BlockerTag, StaticTag}, TEXT("LNPStaticBlocker")},
				{&Part.Deco, TEXT("_Deco"), {DecorationTag}, TEXT("LNPDecoration")},
			};
			for (const FRole& Role : Roles)
			{
				if (Role.Mesh->Triangles.IsEmpty())
				{
					continue;
				}
				UStaticMesh* Mesh = LNPFixtureMesh::WriteStaticMesh(Folder + TEXT("/Generated"),
					FString::Printf(TEXT("SM_%s_%s%s"), Spec.Name, *Part.Id, Role.Suffix), *Role.Mesh);
				if (!Mesh)
				{
					return false;
				}
				LNPFixtureMesh::SpawnMeshActor(World, *Mesh, Part.Id + Role.Suffix, Part.Placement, Role.Tags, Role.Profile);
				++OutActors;
				OutTriangles += Role.Mesh->Triangles.Num();
			}
			return true;
		}

		/**
		 * 레이아웃 규칙 검사(DesignRules). 위반마다 옥탄트 로컬 위치와 함께 경고 한 줄을 남기고 위반 수를 돌려준다.
		 * 검사: 경사로 옆 통로 폭, 다리 기울기, 다리 끝이 경사로 위에 내리는지, 위아래로 겹치는 구조물 사이 틈,
		 * 지면 구조물이 이음매 전이 띠·평탄화 자리 밖으로 나가는지, 문 높이.
		 */
		int32 LintSpec(const FOctantSpec& Spec)
		{
			int32 Count = 0;
			auto Warn = [&Count, &Spec](const TCHAR* Rule, const FString& What, const FVector& At)
			{
				++Count;
				UE_LOG(LogLNPSliceOctant, Warning, TEXT("[SliceOctant] lint %s %s: %s at local (%.0f, %.0f, %.0f) r=%.0f deg=%.1f"),
					Spec.Name, Rule, *What, At.X, At.Y, At.Z, FVector2D(At.X, At.Y).Length(), FMath::RadiansToDegrees(FMath::Atan2(At.Y, At.X)));
			};
			auto DeltaDeg = [](double From, double To) { return FMath::Fmod(To - From + 720.0, 360.0); };

			// 경사로 옆 통로: 경사로가 놓인 띠에서 경사로를 뺀 폭.
			for (const FTieredRing& Ring : Spec.Rings)
			{
				for (const FRingStair& Stair : Ring.Stairs)
				{
					if (Stair.H0 <= 0.0)
					{
						continue;
					}
					const FRingBand* Band = Ring.Bands.FindByPredicate([&Stair](const FRingBand& B)
					{
						return FMath::IsNearlyEqual(Stair.bOutside ? B.R0 : B.R1, Stair.Radius) && FMath::IsNearlyEqual(B.H, Stair.H0);
					});
					const FVector At = PolarH(Stair.Radius, Stair.StartDeg, Stair.H0);
					if (!Band)
					{
						Warn(TEXT("StairOnBand"), FString::Printf(TEXT("%s stair is not on a band edge at H=%.0f"), *Ring.Id, Stair.H0), At);
						continue;
					}
					const double FreeArc = (Band->R1 - Band->R0) - Stair.Width;
					const double FreeReal = FreeArc / Scale(Stair.H0);
					if (FreeReal < DesignRules::MinWalkway)
					{
						Warn(TEXT("Walkway"), FString::Printf(TEXT("%s stair leaves %.0fcm beside the ramp (min %.0f)"), *Ring.Id, FreeReal, DesignRules::MinWalkway), At);
					}
				}
			}

			// 다리 기울기와 다리 끝.
			for (const FBridgeRamp& Bridge : Spec.Bridges)
			{
				const double Horizontal = FVector2D::Distance(FVector2D(Bridge.From), FVector2D(Bridge.To)) / Scale((Bridge.From.Z + Bridge.To.Z) * 0.5);
				const double Slope = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Bridge.To.Z - Bridge.From.Z), Horizontal));
				if (Slope > DesignRules::MaxBridgeSlopeDeg)
				{
					Warn(TEXT("BridgeSlope"), FString::Printf(TEXT("%s is %.1f deg (max %.0f)"), *Bridge.Id, Slope, DesignRules::MaxBridgeSlopeDeg),
						(Bridge.From + Bridge.To) * 0.5);
				}
				for (const FVector& End : {Bridge.From, Bridge.To})
				{
					const double R = FVector2D(End).Length(), Deg = FMath::RadiansToDegrees(FMath::Atan2(End.Y, End.X));
					for (const FTieredRing& Ring : Spec.Rings)
					{
						for (const FRingStair& Stair : Ring.Stairs)
						{
							const FVector2D Radii = StairRadii(Stair), Span = StairSpanDeg(Stair);
							// 경사로 위 머리 공간 안으로 내리는 다리만 문제다(훨씬 위를 지나는 공중 다리는 뺀다).
							if (R > Radii.X && R < Radii.Y && DeltaDeg(Stair.StartDeg, Deg) < Span.X + Span.Y
								&& End.Z > Stair.H0 - SlabThickness * 2.0 && End.Z < Stair.H1 + DesignRules::MinHeadroom)
							{
								Warn(TEXT("BridgeOnStair"), FString::Printf(TEXT("%s ends on a %s stair"), *Bridge.Id, *Ring.Id), End);
							}
						}
					}
				}
			}

			// 위아래로 겹치는 구조물 사이 틈. 건물·섬은 원, 성벽 띠는 고리로 본다. 섬 위에 선 건물(바닥 = 섬 윗면)은 뺀다.
			struct FVolume
			{
				FString Id;
				FVector2D Center;
				double R0;
				double R1;
				double Bottom;
				double Top;
				bool bAnnulus;
				bool bBuilding;
			};
			TArray<FVolume> Volumes;
			for (const FBuilding& B : Spec.Buildings)
			{
				const double R = FVector2D(B.SizeX * 0.5 + WallThickness, B.SizeY * 0.5 + WallThickness).Length() * Scale(B.BaseH);
				Volumes.Add({B.Id, {B.X, B.Y}, 0.0, R, B.BaseH, B.BaseH + RoofH(B), false, true});
			}
			for (const FIsland& I : Spec.Islands)
			{
				Volumes.Add({I.Id, {I.X, I.Y}, 0.0, I.Radius * Scale(I.TopH), I.TopH - I.Thickness, I.TopH, false, false});
			}
			for (const FTieredRing& Ring : Spec.Rings)
			{
				for (int32 Band = 0; Band < Ring.Bands.Num(); ++Band)
				{
					Volumes.Add({FString::Printf(TEXT("%s band %d"), *Ring.Id, Band), FVector2D::ZeroVector, Ring.Bands[Band].R0, Ring.Bands[Band].R1,
						-WallSink, Ring.Bands[Band].H, true, false});
				}
			}
			for (int32 A = 0; A < Volumes.Num(); ++A)
			{
				for (int32 B = A + 1; B < Volumes.Num(); ++B)
				{
					const FVolume& VA = Volumes[A];
					const FVolume& VB = Volumes[B];
					if (VA.bAnnulus && VB.bAnnulus)
					{
						continue;
					}
					const FVolume& Circle = VA.bAnnulus ? VB : VA;
					const FVolume& Other = VA.bAnnulus ? VA : VB;
					const double D = FVector2D::Distance(Circle.Center, Other.Center);
					const bool bOverlap = Other.bAnnulus ? (D + Circle.R1 > Other.R0 && D - Circle.R1 < Other.R1) : (D < Circle.R1 + Other.R1);
					if (!bOverlap)
					{
						continue;
					}
					const double Gap = FMath::Max(VA.Bottom - VB.Top, VB.Bottom - VA.Top);
					const bool bSitsOn = FMath::Abs(Gap) < 1.0 && VA.bBuilding != VB.bBuilding && !VA.bAnnulus && !VB.bAnnulus;
					if (!bSitsOn && Gap < DesignRules::MinHeadroom)
					{
						const FVector2D At = Other.bAnnulus ? Circle.Center : (Circle.Center + Other.Center) * 0.5;
						Warn(TEXT("Clearance"), FString::Printf(TEXT("%s and %s are %.0fcm apart vertically (min %.0f)"), *VA.Id, *VB.Id, Gap,
							DesignRules::MinHeadroom), FVector(At, FMath::Min(VA.Top, VB.Top)));
					}
				}
			}

			// 지면 구조물은 이음매 전이 띠 밖, 평탄화 자리 안에 있어야 한다(밑동이 뜨거나 묻히지 않게).
			auto CheckGround = [&](const FString& Id, TConstArrayView<FVector2D> Samples)
			{
				for (const FVector2D& S : Samples)
				{
					const FVector World = Point(S.X, S.Y, 0.0);
					const double SeamDistance = FMath::Min3(World.X, World.Y, World.Z);
					if (SeamDistance < Spec.SeamMargin + Spec.SeamBand)
					{
						Warn(TEXT("SeamBand"), FString::Printf(TEXT("%s is %.0fcm from the seam (flat from %.0f)"), *Id, SeamDistance,
							Spec.SeamMargin + Spec.SeamBand), FVector(S, 0.0));
						return;
					}
					const bool bOnPad = Spec.Pads.ContainsByPredicate([&World](const FPad& Pad)
					{
						return FMath::Acos(FMath::Clamp(FVector::DotProduct(World.GetSafeNormal(), Radial(Pad.X, Pad.Y)), -1.0, 1.0)) * CrustRadius <= Pad.Size;
					});
					if (!bOnPad)
					{
						Warn(TEXT("OffPad"), FString::Printf(TEXT("%s stands outside every flattening pad"), *Id), FVector(S, 0.0));
						return;
					}
				}
			};
			for (const FBuilding& B : Spec.Buildings)
			{
				if (B.BaseH > 1.0)
				{
					continue;
				}
				const double HX = B.SizeX * 0.5 + WallThickness, HY = B.SizeY * 0.5 + WallThickness;
				TArray<FVector2D> Corners;
				for (const FVector2D& C : {FVector2D(-HX, -HY), FVector2D(HX, -HY), FVector2D(HX, HY), FVector2D(-HX, HY)})
				{
					Corners.Add(FVector2D(BuildingPoint(B, C.X, C.Y, 0.0)));
				}
				CheckGround(B.Id, Corners);
			}
			for (const FTieredRing& Ring : Spec.Rings)
			{
				TArray<FVector2D> Samples;
				for (int32 Deg = 0; Deg < 360; Deg += 2)
				{
					Samples.Add(Polar(Ring.Bands.Last().R1, Deg));
				}
				for (const FRingStair& Stair : Ring.Stairs)
				{
					const FVector2D Radii = StairRadii(Stair), Span = StairSpanDeg(Stair);
					for (int32 Step = 0; Step <= 8; ++Step)
					{
						Samples.Add(Polar(Radii.Y, Stair.StartDeg + (Span.X + Span.Y) * Step / 8.0));
					}
				}
				CheckGround(Ring.Id, Samples);
			}

			for (const FBuilding& B : Spec.Buildings)
			{
				for (const FDoor& Door : B.Doors)
				{
					if (Door.Height < DesignRules::MinDoorHeight)
					{
						Warn(TEXT("DoorHeight"), FString::Printf(TEXT("%s door on side %d is %.0fcm (min %.0f)"), *B.Id, Door.Side, Door.Height,
							DesignRules::MinDoorHeight), FVector(B.X, B.Y, B.BaseH));
					}
				}
			}
			return Count;
		}

		struct FMarkerReport
		{
			int32 Anchors = 0;
			int32 Panels = 0;
			int32 Spawns = 0;
			int32 Blocks = 0;
			int32 Lamps = 0;
			int32 ReviewPoints = 0;
		};

		void PlaceMarkers(UWorld& World, const FOctantSpec& Spec, UStaticMesh& Cube, FMarkerReport& Report)
		{
			// 검토 지점: 런타임 LNP.Debug.TeleportToReview가 태그로 찾는다. 에디터 전용이라 쿡에서 빠진다.
			for (const FReviewPoint& Review : Spec.ReviewPoints)
			{
				ATargetPoint* Target = World.SpawnActor<ATargetPoint>(Point(Review.X, Review.Y, Review.H), Rotation(Review.X, Review.Y).Rotator());
				Target->SetActorLabel(FString::Printf(TEXT("Review_%s"), Review.Name));
				Target->Tags = {FName(TEXT("LNP.ReviewPoint")), FName(Review.Name)};
				Target->bIsEditorOnlyActor = true;
				++Report.ReviewPoints;
			}
			UClass* AnchorClass = LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"));
			for (int32 Index = 0; Index < Spec.Anchors.Num(); ++Index)
			{
				const FAnchor& Anchor = Spec.Anchors[Index];
				ALNPPlacementMarker* Marker = World.SpawnActor<ALNPPlacementMarker>(Point(Anchor.X, Anchor.Y, Anchor.H), Rotation(Anchor.X, Anchor.Y).Rotator());
				Marker->SetActorLabel(FString::Printf(TEXT("Anchor_%03d"), Index));
				Marker->MarkerId = FGuid(Spec.GuidTag, 1, 0, Index + 1);
				Marker->ElementClass = AnchorClass;
				Marker->GrappleMaxDistance = Anchor.MaxDistance;
				++Report.Anchors;
			}
			for (int32 Index = 0; Index < Spec.Panels.Num(); ++Index)
			{
				const FPanel& Panel = Spec.Panels[Index];
				const FVector Mid = (Panel.From + Panel.To) * 0.5;
				const FTransform MarkerTransform(Rotation(Mid.X, Mid.Y), Point(Mid));
				ALNPPlacementMarker* Marker = World.SpawnActor<ALNPPlacementMarker>(MarkerTransform.GetLocation(), MarkerTransform.Rotator());
				Marker->SetActorLabel(FString::Printf(TEXT("Panel_%02d"), Index));
				Marker->MarkerId = FGuid(Spec.GuidTag, 2, 0, Index + 1);
				Marker->ElementClass = ALNPMovingPanel::StaticClass();
				Marker->PathSpeed = Panel.Speed;
				Marker->PathEndHoldSeconds = Panel.HoldSeconds;
				Marker->Path->ClearSplinePoints(false);
				Marker->Path->AddSplinePoint(MarkerTransform.InverseTransformPosition(Point(Panel.From)), ESplineCoordinateSpace::Local, false);
				Marker->Path->AddSplinePoint(MarkerTransform.InverseTransformPosition(Point(Panel.To)), ESplineCoordinateSpace::Local, false);
				Marker->Path->SetSplinePointType(0, ESplinePointType::Linear, false);
				Marker->Path->SetSplinePointType(1, ESplinePointType::Linear, false);
				Marker->Path->UpdateSpline();
				++Report.Panels;
			}
			for (int32 Index = 0; Index < Spec.Spawns.Num(); ++Index)
			{
				const FSpawn& Spawn = Spec.Spawns[Index];
				ALNPMassSpawnPoint* SpawnPoint = World.SpawnActor<ALNPMassSpawnPoint>(Point(Spawn.X, Spawn.Y, Spawn.H), Rotation(Spawn.X, Spawn.Y).Rotator());
				SpawnPoint->SetActorLabel(FString::Printf(TEXT("Spawn_%02d"), Index));
				SpawnPoint->SpawnPointId = FGuid(Spec.GuidTag, 3, 0, Index + 1);
				++Report.Spawns;
			}
			for (int32 Index = 0; Index < Spec.Blocks.Num(); ++Index)
			{
				const FBlock& Block = Spec.Blocks[Index];
				const FTransform Transform(Rotation(Block.X, Block.Y), Point(Block.X, Block.Y, Block.BottomH + Block.Height * 0.5 - WallSink * 0.5),
					FVector(Block.Half / 50.0, Block.Half / 50.0, (Block.Height + WallSink) / 100.0));
				LNPFixtureMesh::SpawnMeshActor(World, Cube, FString::Printf(TEXT("Block_%02d"), Index), Transform, {BlockerTag, StaticTag}, TEXT("LNPStaticBlocker"));
				++Report.Blocks;
			}

			UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			UMaterialInterface* LampMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Maps/SliceOctant/M_SliceLamp.M_SliceLamp"));
			if (!Sphere || !LampMaterial)
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Lamp sphere or /Game/Maps/SliceOctant/M_SliceLamp is missing; lamps skipped"));
				return;
			}
			for (int32 Index = 0; Index < Spec.Lamps.Num(); ++Index)
			{
				const FLamp& Lamp = Spec.Lamps[Index];
				const FVector Location = Point(Lamp.X, Lamp.Y, Lamp.H);
				const FQuat LampRotation = Rotation(Lamp.X, Lamp.Y);
				APointLight* Light = World.SpawnActor<APointLight>(Location, LampRotation.Rotator());
				Light->SetActorLabel(FString::Printf(TEXT("Lamp_%03d"), Index));
				UPointLightComponent* Component = Light->PointLightComponent;
				Component->SetMobility(EComponentMobility::Movable);
				// 역제곱이 아니면 엔진은 세기를 unitless(= 중심 조도 lux)로 쓴다.
				const double ReferenceMeters = Lamp.Radius * LampReferenceFraction / 100.0;
				const double ReferenceMask = FMath::Pow(1.0 - FMath::Square(LampReferenceFraction), LampFalloffExponent);
				Component->SetUseInverseSquaredFalloff(false);
				Component->SetLightFalloffExponent(LampFalloffExponent);
				Component->SetIntensityUnits(ELightUnits::Unitless);
				Component->SetIntensity(Lamp.Intensity / FMath::Square(ReferenceMeters) / ReferenceMask);
				Component->SetAttenuationRadius(Lamp.Radius);
				Component->SetLightColor(FLinearColor(1.f, 0.72f, 0.45f));
				Component->SetCastShadows(false);
				AStaticMeshActor* Globe = LNPFixtureMesh::SpawnMeshActor(World, *Sphere, FString::Printf(TEXT("LampGlobe_%03d"), Index),
					FTransform(LampRotation, Location, FVector(0.8)), {DecorationTag}, TEXT("LNPDecoration"));
				if (Globe)
				{
					Globe->GetStaticMeshComponent()->SetMaterial(0, LampMaterial);
					Globe->GetStaticMeshComponent()->SetCastShadow(false);
					// 빛은 포인트 라이트가 낸다. 구슬 수백 개가 Lumen Surface Cache 카드를 차지하지 않게 뺀다.
					Globe->GetStaticMeshComponent()->bAffectDynamicIndirectLighting = false;
				}
				++Report.Lamps;
			}
		}

		void Run(const TArray<FString>& Args)
		{
			const FString OctantName = Args.Num() > 0 ? Args[0] : FString();
			const TArray<FOctantSpec> AllSpecs = Specs();
			const FOctantSpec* Spec = AllSpecs.FindByPredicate([&OctantName](const FOctantSpec& S) { return OctantName == S.Name; });
			if (!Spec)
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Usage: LNP.SurfaceNav.BuildSliceOctant <Bastion_00|Hollow_00>"));
				return;
			}
			if (!FMath::IsNearlyEqual(GetDefault<ULNPSettings>()->SphereRadius, CrustRadius))
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] SphereRadius %.0f differs from %.0f"),
					GetDefault<ULNPSettings>()->SphereRadius, CrustRadius);
				return;
			}

			const FString Folder = FString::Printf(TEXT("/Game/Maps/%s"), Spec->Name);
			const FString LevelPackageName = FString::Printf(TEXT("%s/LVI_Octant_%s"), *Folder, Spec->Name);
			UStaticMesh* Source = LoadObject<UStaticMesh>(nullptr, SeamSourceMesh);
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			FLNPFixtureMesh Crust;
			int32 Degenerate = 0;
			if (!Source || !Cube || !ReadMeshTriangles(*Source, Crust, Degenerate))
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Seam source crust %s or the engine cube is missing"), SeamSourceMesh);
				return;
			}
			FCrustReport CrustReport = DeriveCrust(Crust, *Spec);
			GZeroThicknessWalls = 0;

			// 지각을 자르는 구조물을 먼저 만든다.
			TArray<FPart> Parts;
			for (const FAbyss& Abyss : Spec->Abysses)
			{
				Parts.Add(BuildAbyss(Abyss, Crust, CrustReport.Cut));
			}
			for (const FSpire& Spire : Spec->Spires)
			{
				Parts.Append(BuildSpire(Spire));
			}
			for (const FTieredRing& Ring : Spec->Rings)
			{
				Parts.Add(BuildTieredRing(Ring));
			}
			for (const FBuilding& Building : Spec->Buildings)
			{
				Parts.Add(BuildBuilding(Building));
			}
			for (const FIsland& Island : Spec->Islands)
			{
				Parts.Add(BuildIsland(Island));
			}
			for (const FBridgeRamp& Bridge : Spec->Bridges)
			{
				Parts.Add(BuildBridge(Bridge));
			}

			UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(Folder, FString::Printf(TEXT("SM_Octant_%s_R30000"), Spec->Name), Crust);
			if (!CrustMesh)
			{
				return;
			}

			UWorld* World = nullptr;
			const bool bExisting = FPackageName::DoesPackageExist(LevelPackageName);
			if (bExisting)
			{
				World = LoadObject<UWorld>(nullptr, *FString::Printf(TEXT("%s.LVI_Octant_%s"), *LevelPackageName, Spec->Name));
				if (!World || (GEditor && GEditor->GetEditorWorldContext().World() == World))
				{
					UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] %s failed to load or is open in the editor; open another map first"), *LevelPackageName);
					return;
				}
				ClearWorld(*World);
			}
			else
			{
				World = LNPFixtureMesh::CreateLevelWorld(LevelPackageName);
			}

			LNPFixtureMesh::SpawnMeshActor(*World, *CrustMesh, TEXT("Crust"), FTransform::Identity, {SupportTag, BlockerTag, StaticTag}, TEXT("LNPStaticTerrain"));
			int32 PartActors = 0, PartTriangles = 0;
			for (const FPart& Part : Parts)
			{
				if (!PlacePart(*World, Folder, *Spec, Part, PartActors, PartTriangles))
				{
					return;
				}
			}
			FMarkerReport Markers;
			PlaceMarkers(*World, *Spec, *Cube, Markers);

			int32 AuthoredErrors = 0;
			const int32 Authored = PlaceAuthoredMeshes(*World, Folder + TEXT("/Authored"), AuthoredErrors);

			const bool bSaved = bExisting
				? LNPFixtureMesh::SavePackage(*World->GetOutermost(), *World, FPackageName::GetMapPackageExtension())
				: LNPFixtureMesh::SaveWorld(*World);
			int32 Lint = LintSpec(*Spec);
			if (GZeroThicknessWalls > 0)
			{
				++Lint;
				UE_LOG(LogLNPSliceOctant, Warning, TEXT("[SliceOctant] lint %s ZeroThickness: %d zero-thickness walls can trap players inside hollow structures"),
					Spec->Name, GZeroThicknessWalls);
			}
			UE_LOG(LogLNPSliceOctant, Display,
				TEXT("[SliceOctant] %s %s | crust tris=%d degenerateDropped=%d moved=%d kept=%d radius=[%.0f, %.0f] maxShift=%.0f cut=%d | ")
				TEXT("parts=%d partActors=%d partTris=%d buildings=%d bridges=%d islands=%d anchors=%d panels=%d spawns=%d blocks=%d lamps=%d reviewPoints=%d ")
				TEXT("authored=%d authoredErrors=%d | lint=%d"),
				*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"), Crust.Triangles.Num(), Degenerate, CrustReport.Moved, CrustReport.Kept,
				CrustReport.MinRadius, CrustReport.MaxRadius, CrustReport.MaxShift, CrustReport.Cut,
				Parts.Num(), PartActors, PartTriangles, Spec->Buildings.Num(), Spec->Bridges.Num(), Spec->Islands.Num(),
				Markers.Anchors, Markers.Panels, Markers.Spawns, Markers.Blocks, Markers.Lamps, Markers.ReviewPoints, Authored, AuthoredErrors, Lint);
		}

		FAutoConsoleCommand Command(
			TEXT("LNP.SurfaceNav.BuildSliceOctant"),
			TEXT("Rebuild a Phase 9 vertical-slice octant LVI from its code spec: derive the crust from Meadow's seam profile, ")
			TEXT("generate buildings, tiered walls, spires, islands, bridges and abysses, place markers and spawn anchors, and place Blender meshes ")
			TEXT("from /Game/Maps/<Name>/Authored by role suffix. Usage: LNP.SurfaceNav.BuildSliceOctant <Bastion_00|Hollow_00>. ")
			TEXT("Clears and refills an existing LVI."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&Run));

		FAutoConsoleCommand LintCommand(
			TEXT("LNP.SurfaceNav.LintSliceOctant"),
			TEXT("Check a slice-octant code spec against the player-metric layout rules without rebuilding: walkway beside wall ramps, bridge slope, ")
			TEXT("bridges ending on ramps, vertical clearance between stacked structures, ground structures inside the seam band or off the pad, door height. ")
			TEXT("Zero-thickness walls are only counted by BuildSliceOctant. Usage: LNP.SurfaceNav.LintSliceOctant <Bastion_00|Hollow_00>"),
			FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				const FString OctantName = Args.Num() > 0 ? Args[0] : FString();
				const TArray<FOctantSpec> AllSpecs = Specs();
				const FOctantSpec* Spec = AllSpecs.FindByPredicate([&OctantName](const FOctantSpec& S) { return OctantName == S.Name; });
				if (!Spec)
				{
					UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Usage: LNP.SurfaceNav.LintSliceOctant <Bastion_00|Hollow_00>"));
					return;
				}
				UE_LOG(LogLNPSliceOctant, Display, TEXT("[SliceOctant] lint %s: %d warnings"), Spec->Name, LintSpec(*Spec));
			}));
	}
}

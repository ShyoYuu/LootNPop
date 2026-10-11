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
#include "CompGeom/PolygonTriangulation.h"
#include "CompGeom/Delaunay2.h"
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
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "StaticMeshCompiler.h"
#include "DataAsset/LNPWorldDeviceConfig.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityConfigAsset.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Spatial/MeshAABBTree3.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
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
			double Relief = 0.0;
		};

		/** 긴 절벽 암반. Outline은 반시계 둘레 순서의 단순 다각형(옥탄트 로컬 XY)이며 윗면은 구면이다. */
		struct FCliff
		{
			FString Id;
			TArray<FVector2D> Outline;
			double BottomH;
			double TopH;
			/** 인접 길 사이 암반을 같은 높이로 접속하는 윗면. 없으면 일정한 TopH다. */
			TFunction<double(FVector2D)> SurfaceH;
		};

		/** 공중 직선 다리·경사로. From·To는 (X, Y, H) 윗면 중심선 양 끝(호 길이), Width는 실제 cm다. */
		struct FBridgeRamp
		{
			FString Id;
			FVector From;
			FVector To;
			double Width = 1000.0;
			bool bGrounded = false;
			/** 서쪽 절벽 뒤편까지 채우는 암반 윗면. 0이면 독립 다리다. */
			double AttachedBackY = 0.0;
		};

		/** 암반을 실제로 잘라 만드는 경사 통로 또는 막힌 전투 굴. 양옆·천장은 1m 두께다. */
		struct FPassage
		{
			FBridgeRamp Ramp;
			double Height = 900.0;
			bool bClosedEnd = false;
			bool bClosedStart = false;
			/** 굴 입구의 암반 경계. 새 벽·천장은 이 경계 안에만 남긴다. */
			TArray<FVector2D> MouthEdge;
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
			/** 기둥과 경사로 사이 실제 틈. 회전당 상승 높이는 기존 기준을 유지한다. */
			double SpiralGap = 0.0;
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

		/** 수직 통로 입구. 반지름은 로컬 XY, TopH는 해당 지각/메사 윗면 높이다. */
		struct FCaveMouth
		{
			double X;
			double Y;
			double RadiusArc;
			double TopH;
			FString SurfacePartId;
		};

		/** 자유 형태 공동과 수직 통로. 메사 입구는 해당 암반의 윗면·몸체도 같은 둘레로 자른다. */
		struct FCavern
		{
			FCliff FloorSlab;
			double CeilingH;
			TArray<FCaveMouth> Mouths;
		};

		/** 그래플 앵커 마커. MaxDistance가 0이면 설정 에셋 거리를 쓴다. */
		struct FAnchor
		{
			double X;
			double Y;
			double H;
			double MaxDistance;
			FString Id;
			/** Bastion 착지 검증용 높이와 목적지 부품. 공중 중계 앵커는 사용하지 않는다. */
			double LandingH = 0.0;
			FString LandingOwner;
		};

		/** Pod 세트 앵커. */
		struct FSpawn
		{
			double X;
			double Y;
			double H;
			TArray<FLNPSpawnEnemyFormation> EnemyFormations;
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
			TArray<FCliff> Cliffs;
			TArray<FBridgeRamp> Bridges;
			TArray<FPassage> Passages;
			TArray<FTieredRing> Rings;
			TArray<FSpire> Spires;
			TArray<FAbyss> Abysses;
			TArray<FCavern> Caverns;
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
			// BuildBuilding과 같은 구면 바닥·배치 프레임으로 월드 점을 만든 뒤 제작 좌표로 되돌린다.
			// 바닥 높이의 Scale만 적용하면 높은 지붕에서 위치가 어긋나 다리가 외벽에 걸린다.
			const double Radius = CrustRadius - Building.BaseH;
			const double Z = Radius - FMath::Sqrt(Radius * Radius - LX * LX - LY * LY) + H - Building.BaseH;
			const FQuat Facing = Rotation(Building.X, Building.Y) * FQuat(FVector::UpVector, FMath::DegreesToRadians(Building.Yaw));
			const FVector World = Point(Building.X, Building.Y, Building.BaseH) + Facing.RotateVector(FVector(LX, LY, Z));
			const double Depth = FVector::DotProduct(World, Center().Radial);
			return {FVector::DotProduct(World, Center().Bitangent) * CrustRadius / Depth,
				FVector::DotProduct(World, Center().Tangent) * CrustRadius / Depth, CrustRadius - World.Length()};
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
		 * - 안뜰: 거인 스케일 폐허 건물 6채. 북·남 전투 광장을 비우고 지붕·성벽의 우회 경로를 잇는다.
		 * - 공중: 외성벽 위에서 접시 높이까지 나선형으로 오르며 바깥으로 퍼지는 공중섬 12개(그중 둘에 폐허)와 다리.
		 */
		FVector BastionBridgePoint(const FBridgeRamp& B, const FSpire& S, double T, double Lateral);

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
			Spire.SpiralGap = 200.0;
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
			const double SpiralOuter = Spire.CoreRadius + Spire.SpiralGap + Spire.SpiralWidth;

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

			// 안뜰 건물: 45° 간격의 여덟 자리 중 두 곳을 비운 6채, 층고 12m. 문과 무너진 면이 안뜰 통로 쪽을 향한다.
			// 면 0(-X)·1(+X)은 시계·반시계 방향 이웃, 면 2(-Y)는 외성벽, 면 3(+Y)은 성채 쪽이다. 1층 경사로는 외성벽 쪽 벽을 따른다.
			struct FPlan { double Deg; int32 Stories; TArray<int32> Collapsed; };
			const FPlan Plans[] = {
				{22.5, 0, {}}, {67.5, 3, {1, 3}}, {112.5, 3, {0, 3}}, {157.5, 2, {0, 3}},
				{202.5, 0, {}}, {247.5, 2, {0, 2}}, {292.5, 3, {1, 3}}, {337.5, 2, {0, 3}},
			};
			TArray<int32> BigIndex;
			int32 BuildingId = 0;
			for (const FPlan& Plan : Plans)
			{
				++BuildingId;
				// 북·남 안뜰의 건물과 머리 위 교량을 비워 비행 전투와 정상 조망을 확보한다. 나머지 건물 ID는 유지한다.
				if (Plan.Stories == 0)
				{
					continue;
				}
				FBuilding Building = GiantBuilding(FString::Printf(TEXT("B%02d"), BuildingId), 9700.0, Plan.Deg, 2600.0, 2200.0, Plan.Stories, Plan.Collapsed);
				Building.Yaw = Plan.Deg + 90.0;
				Building.StoryHeight = 1200.0;
				BigIndex.Add(Spec.Buildings.Add(MoveTemp(Building)));
			}

			// 다리: 인접 건물의 지붕과 성벽을 잇되 큰 전투 광장 위는 개방한다.
			auto RoofEdge = [&Spec](int32 Index, int32 Side)
			{
				const FBuilding& B = Spec.Buildings[Index];
				const double HX = B.SizeX * 0.5 + 200.0, HY = B.SizeY * 0.5 + 200.0;
				const double LX = Side == 0 ? -HX : Side == 1 ? HX : -300.0;
				const double LY = Side == 2 ? -HY : Side == 3 ? HY : -300.0;
				return BuildingPoint(B, LX, LY, B.BaseH + RoofH(B) - 1.0);
			};
			// 경사진 본 다리는 외벽 밖에서 시작한다. 지붕 안쪽과 벽 위는 지붕 높이의 짧은 발판으로 연결한다.
			for (const int32 Index : BigIndex)
			{
				const FBuilding& B = Spec.Buildings[Index];
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const double HX = B.SizeX * 0.5 - 500.0, HY = B.SizeY * 0.5 - 500.0;
					const double LX = Side == 0 ? -HX : Side == 1 ? HX : -300.0;
					const double LY = Side == 2 ? -HY : Side == 3 ? HY : -300.0;
					AddBridge(Spec, BuildingPoint(B, LX, LY, B.BaseH + RoofH(B) - 1.0), RoofEdge(Index, Side), 1200.0);
				}
			};
			for (int32 I = 0; I < BigIndex.Num(); ++I)
			{
				const FBuilding& Building = Spec.Buildings[BigIndex[I]];
				const FBuilding& Next = Spec.Buildings[BigIndex[(I + 1) % BigIndex.Num()]];
				const double Deg = FMath::RadiansToDegrees(FMath::Atan2(Building.Y, Building.X));
				const double NextDeg = FMath::RadiansToDegrees(FMath::Atan2(Next.Y, Next.X));
				// 큰 전투 광장을 다시 교량으로 덮지 않는다. 끊긴 지붕 경로는 성벽 연결로 우회한다.
				if (FMath::Fmod(NextDeg - Deg + 720.0, 360.0) < 60.0)
				{
					AddBridge(Spec, RoofEdge(BigIndex[I], 1), RoofEdge(BigIndex[(I + 1) % BigIndex.Num()], 0), 1200.0);
				}
				// 3층 지붕(36m)은 40m 단으로, 2층 지붕(24m)은 20m 단으로 잇는다(다리 경사를 완만하게). 성채 40m 단은 경사로(호 54~60m)
				// 바깥 4m 띠에 내린다.
				const bool bTall = RoofH(Spec.Buildings[BigIndex[I]]) > 3000.0;
				AddBridge(Spec, RoofEdge(BigIndex[I], 2), PolarH(bTall ? 13700.0 : 12200.0, Deg, bTall ? 3999.0 : 1999.0));
				AddBridge(Spec, RoofEdge(BigIndex[I], 3), PolarH(bTall ? 6200.0 : 7300.0, Deg, bTall ? 3999.0 : 1999.0));
			}

			// 메인 무대 접시 위 엄폐 석주 6개.
			for (int32 I = 0; I < 6; ++I)
			{
				const FVector2D P = Polar(3000.0 * Scale(Spire.TopH), 15.0 + I * 60.0);
				// 접시 실루엣은 유지하고 낮은 엄폐와 높은 엄폐를 번갈아 배치해 보스 조망과 횡이동을 연다.
				Spec.Blocks.Add({P.X, P.Y, Spire.TopH, 250.0, I % 2 == 0 ? 350.0 : 600.0});
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
				Island.Radius = (I == 3 || I == 7) ? 1400.0 : (I == 0 || I == 5 || I == 9) ? 1800.0 : 1000.0;
				Island.Thickness = 500.0;
				Sky.Add(Island);
			}
			Spec.Islands.Append(Sky);
			// 성채 40m 단의 Pod는 넓어진 첫 공중섬으로 옮기고, 성채에서 직접 진입하게 한다.
			AddBridge(Spec, PolarH(6200.0, 20.0, 3999.0),
				PolarH(9200.0 - (Sky[0].Radius - 100.0) * Scale(Sky[0].TopH), 20.0, 3999.0), 1200.0);

			// 좁은 성벽 대신 사수·넉백 회복 공간을 갖춘 전투 발판 세 곳. 성벽과 겹치지 않고 짧은 다리로 잇는다.
			TArray<FIsland> PodTerraces;
			for (int32 I = 0; I < 3; ++I)
			{
				FIsland Terrace;
				Terrace.Id = FString::Printf(TEXT("T%02d"), I + 1);
				Terrace.TopH = I < 2 ? 4000.0 : 2000.0;
				Terrace.Radius = 1600.0;
				Terrace.Thickness = 500.0;
				const double Deg = I == 0 ? 15.0 : I == 1 ? 195.0 : 202.5;
				const double R = I < 2 ? 17700.0 : 9600.0;
				const FVector2D P = Polar(R, Deg);
				Terrace.X = P.X;
				Terrace.Y = P.Y;
				PodTerraces.Add(Terrace);
				Spec.Islands.Add(Terrace);
				AddBridge(Spec, PolarH(I < 2 ? 13900.0 : 7300.0, Deg, Terrace.TopH - 1.0),
					PolarH(R - (Terrace.Radius - 100.0) * Scale(Terrace.TopH), Deg, Terrace.TopH - 1.0), 1200.0);
			}

			// 공동 양쪽에서 지상으로 복귀하는 착지점. 엇갈린 위치와 높은 앵커로 다음 발판 밑면을 비켜 간다.
			for (int32 Side = 0; Side < 2; ++Side)
			{
				for (int32 Step = 0; Step < 3; ++Step)
				{
					FIsland Landing;
					Landing.Id = FString::Printf(TEXT("C%02d"), Side * 3 + Step + 1);
					const FVector2D P = Polar(2600.0, 90.0 + Side * 180.0 + (Step % 2 == 0 ? -25.0 : 25.0));
					Landing.X = P.X;
					Landing.Y = P.Y;
					Landing.TopH = -8500.0 + Step * 3000.0;
					Landing.Radius = 500.0;
					Landing.Thickness = 200.0;
					Spec.Islands.Add(Landing);
				}
			}

			// 남문에서 정상으로 빠르게 오르는 별도 훅 경로. 접시보다 낮은 발판으로 랜드마크 위계를 유지한다.
			for (int32 I = 0; I < 4; ++I)
			{
				FIsland Landing;
				Landing.Id = FString::Printf(TEXT("V%02d"), I + 1);
				// 다음 발판의 밑면이 직선 훅을 막지 않도록 양쪽으로 엇갈린다. 마지막 앵커는 접시 바닥 위에서 진입한다.
				const FVector2D P = Polar(I == 0 ? 11400.0 : 12500.0, I % 2 == 0 ? 166.0 : 194.0);
				Landing.X = P.X;
				Landing.Y = P.Y;
				Landing.TopH = I == 3 ? 17200.0 : 4000.0 + I * 4000.0;
				Landing.Radius = 800.0;
				Landing.Thickness = 500.0;
				Spec.Islands.Add(Landing);
			}
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
				// 들어오고 나가는 다리 끝을 무너진 양쪽 벽의 중앙에 맞춘다.
				Spec.Bridges[44 + I].To = BuildingPoint(Ruin, -800.0, 0.0, Ruin.BaseH - 1.0);
				Spec.Bridges[45 + I].From = BuildingPoint(Ruin, 800.0, 0.0, Ruin.BaseH - 1.0);
				Spec.Buildings.Add(Ruin);
			}

			// 기둥으로 건너는 다리: 북문 땅 높이와 성채 80m 단.
			AddBridge(Spec, PolarH(4000.0, 0.0, -1.0), PolarH((SpiralOuter - 200.0) * Scale(0.0), 0.0, -1.0), 700.0);
			{
				const double Deg = SpiralDeg(8000.0);
				AddBridge(Spec, PolarH(3600.0, Deg, 7999.0), PolarH((SpiralOuter - 200.0) * Scale(8000.0), Deg, 7999.0), 700.0);
			}

			// 12m 전투 패널: 외성벽 바깥20m 단에서 북쪽 Pod 발판40m 단으로 이동한다.
			// 기존 중앙의 나선·섬·교량 밀집 구역을 피하고, 고정 Pod 다리와 반대쪽에 접안한다.
			{
				const FIsland& Target = PodTerraces[0];
				const FVector2D Side = Polar(2900.0, -75.0);
				Spec.Panels.Add({PolarH(16700.0, -10.0, 1985.0), FVector(Target.X + Side.X, Target.Y + Side.Y, 3985.0), 400.0, 3.0});
			}

			// 시설 하나에 목적지 하나. 다리마다 두었던59개와 넓은 섬의 양쪽 중복 앵커를 제거한다.
			auto Hook = [&](FString Id, FVector P, double LandingH, FString Owner, double Range = 0.0)
			{
				Spec.Anchors.Add({P.X, P.Y, P.Z, Range, MoveTemp(Id), LandingH, MoveTemp(Owner)});
			};
			for (const FBuilding& B : Spec.Buildings)
			{
				const double H = B.BaseH + RoofH(B);
				// 무너진 지붕 모서리의 반대쪽 온전한 띠에 착지한다.
				Hook(B.Id + TEXT("_Roof"), BuildingPoint(B, B.Id == TEXT("B06") ? -350.0 : 0.0,
					B.Id == TEXT("B06") ? -250.0 : -350.0, H + (B.Id.StartsWith(TEXT("S")) ? 800.0 : B.Id == TEXT("B06") ? 450.0 : 550.0)), H, B.Id);
			}
			for (const FIsland& I : Spec.Islands)
			{
				// 폐허섬은 폐허 지붕 앵커가 대표한다.
				if (I.Id == TEXT("I04") || I.Id == TEXT("I08")) continue;
				const bool Cave = I.Id.StartsWith(TEXT("C")), Ascent = I.Id.StartsWith(TEXT("V"));
				const double Lift = Cave ? 1500.0 : Ascent ? (I.Id == TEXT("V04") ? 2600.0 : 1500.0) : I.Id.StartsWith(TEXT("I")) ? 1200.0 : 800.0;
				// 중앙 위에 두어 여러 진입 방향의 종료 관성에 같은 착지 여유를 남긴다.
				Hook(I.Id + TEXT("_Landing"), FVector(I.X, I.Y, I.TopH + Lift), I.TopH, I.Id,
					Cave ? 5500.0 : Ascent ? (I.Id == TEXT("V04") ? 8500.0 : 7500.0) : 0.0);
			}
			// 정상 전체를 대표하는 중앙 앵커. 테두리의6개는 제거한다.
			Hook(TEXT("Dish"), FVector(0.0, 0.0, Spire.TopH + 800.0), Spire.TopH, TEXT("P01_Dish"), 8000.0);
			// 두 실제 탈출 발판 체인의 끝에서 성문 안 지상으로 복귀한다. 바닥 없는 서쪽 중계는 제거한다.
			for (double Deg : {90.0, 270.0})
				Hook(FString::Printf(TEXT("CavernExit_%03.0f"), Deg), PolarH(3550.0, Deg, 700.0), 0.0, TEXT("Crust"), 5500.0);
			// 성채는 단마다 하나씩 엇갈리게 둔다. 수직으로 같은 화면에 겹치던16개를4개로 통합한다.
			const FVector Bands[] = {FVector(7000.0, 0.0, 2000.0), FVector(5900.0, 155.0, 4000.0),
				FVector(4900.0, 245.0, 6000.0), FVector(3800.0, 65.0, 8000.0)};
			for (const FVector& Band : Bands)
				Hook(FString::Printf(TEXT("Citadel_%03.0f"), Band.Z / 100.0), PolarH(Band.X, Band.Y, Band.Z + 500.0), Band.Z, TEXT("W02"), 5500.0);
			// 외성벽 네 사분면과 안뜰 네 전투장도 각각 목적지를 하나만 갖는다.
			for (int32 Q = 0; Q < 4; ++Q)
			{
				Hook(FString::Printf(TEXT("OuterWall_%d"), Q), PolarH(13700.0, 20.0 + Q * 90.0, 4450.0), 4000.0, TEXT("W01"));
				Hook(FString::Printf(TEXT("Courtyard_%d"), Q), PolarH(9700.0, Q * 90.0, 700.0), 0.0, TEXT("Crust"));
			}
			// 중간 다리 앵커를 없앤 공중섬 연결은 양쪽 이웃 시설까지의 실제 거리를 개별 사거리로 준다.
			for (int32 I = 0; I < Sky.Num(); ++I)
			{
				const FString Role = I == 3 ? TEXT("S04_Roof") : I == 7 ? TEXT("S08_Roof") : Sky[I].Id + TEXT("_Landing");
				FAnchor* A = Spec.Anchors.FindByPredicate([&](const FAnchor& H) { return H.Id == Role; });
				for (int32 J : {I - 1, I + 1})
				{
					if (!Sky.IsValidIndex(J)) continue;
					const FVector Launch = Point(Sky[J].X, Sky[J].Y, Sky[J].TopH + 110.0);
					A->MaxDistance = FMath::Max(A->MaxDistance, FMath::CeilToDouble((FVector::Distance(Launch, Point(A->X, A->Y, A->H)) + 500.0) / 100.0) * 100.0);
				}
			}

			// Pod: 문 앞 안뜰, 외성벽 전투 발판, 첫 공중섬, 성채 외곽 발판, 공동, 공중 전투섬 둘. 접시는 보스 자리로 비운다.
			for (const double Deg : {0.0, 90.0, 180.0, 270.0})
			{
				const FVector2D P = Polar(9700.0, Deg);
				Spec.Spawns.Add({P.X, P.Y, 0.0});
			}
			for (const FVector& S : {FVector(PodTerraces[0].X, PodTerraces[0].Y, PodTerraces[0].TopH),
				FVector(PodTerraces[1].X, PodTerraces[1].Y, PodTerraces[1].TopH), FVector(Sky[0].X, Sky[0].Y, Sky[0].TopH),
				FVector(PodTerraces[2].X, PodTerraces[2].Y, PodTerraces[2].TopH), PolarH(2600.0, 45.0, -10000.0), PolarH(2600.0, 225.0, -10000.0)})
			{
				Spec.Spawns.Add({S.X, S.Y, S.Z});
			}
			for (const int32 I : {5, 9})
			{
				Spec.Spawns.Add({Sky[I].X, Sky[I].Y, Sky[I].TopH});
			}

			// 지정 Pod만 고유 편성을 사용한다. 위치는 Pod 회전의 로컬 X/Y이며 실제 안전 후보를 고른다.
			const FName Crawler(TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Crawler01.DA_EnemyEntityConfig_PureEntity_Crawler01"));
			const FName Flyer(TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Flyer01.DA_EnemyEntityConfig_PureEntity_Flyer01"));
			auto Formation = [](FName Config, int32 Count, double X, double Y, float Radius)
			{
				FLNPSpawnEnemyFormation Result;
				Result.EnemyConfigPath = Config;
				Result.Count = Count;
				Result.Offset = FVector2D(X, Y);
				Result.Radius = Radius;
				return Result;
			};
			// 안뜰: 북/남은 열린 광장, 동/서는 건물 사이 양쪽 협공이다.
			Spec.Spawns[0].EnemyFormations = {Formation(Flyer, 5, -900, 600, 900), Formation(Crawler, 6, 500, -2700, 1000)};
			Spec.Spawns[1].EnemyFormations = {Formation(Flyer, 4, 0, -1000, 900), Formation(Crawler, 4, -2700, -400, 1000), Formation(Crawler, 4, 2700, -400, 1000)};
			Spec.Spawns[2].EnemyFormations = {Formation(Flyer, 8, 900, -600, 1100), Formation(Crawler, 4, 500, 2700, 1000)};
			Spec.Spawns[3].EnemyFormations = {Formation(Flyer, 4, 0, 1000, 900), Formation(Crawler, 5, -2700, 400, 1000), Formation(Crawler, 5, 2700, 400, 1000)};
			// 발판: 외곽 두 곳은 비행 위주, 성채·첫 섬은 혼합, 높은 두 섬은 공중 집중이다.
			Spec.Spawns[4].EnemyFormations = {Formation(Flyer, 6, 650, 0, 750), Formation(Crawler, 3, -700, 0, 650)};
			Spec.Spawns[5].EnemyFormations = {Formation(Flyer, 8, -650, 0, 750), Formation(Crawler, 2, 700, 0, 650)};
			Spec.Spawns[6].EnemyFormations = {Formation(Flyer, 4, 600, 400, 800), Formation(Crawler, 4, -650, -350, 750)};
			Spec.Spawns[7].EnemyFormations = {Formation(Flyer, 3, -650, 0, 750), Formation(Crawler, 6, 650, 0, 800)};
			// 공동은 천장 때문에 비행 편성을 강제하지 않는다. 반대쪽 접근을 읽고 기둥 사이로 이동한다.
			Spec.Spawns[8].EnemyFormations = {Formation(Crawler, 5, -850, 0, 850), Formation(Crawler, 5, 0, -850, 850)};
			Spec.Spawns[9].EnemyFormations = {Formation(Crawler, 6, 1100, 0, 1100), Formation(Crawler, 6, 0, 1100, 1100)};
			Spec.Spawns[10].EnemyFormations = {Formation(Flyer, 8, 0, 600, 1100)};
			Spec.Spawns[11].EnemyFormations = {Formation(Flyer, 10, 0, -500, 1100)};

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
					{TEXT("PodTerraceNorth"), PodTerraces[0].X, PodTerraces[0].Y, PodTerraces[0].TopH},
					{TEXT("PodTerraceSouth"), PodTerraces[1].X, PodTerraces[1].Y, PodTerraces[1].TopH},
					{TEXT("PodTerraceCitadel"), PodTerraces[2].X, PodTerraces[2].Y, PodTerraces[2].TopH},
					{TEXT("FastAscentBase"), Spec.Islands[Spec.Islands.Num() - 4].X, Spec.Islands[Spec.Islands.Num() - 4].Y, 4000.0},
					{TEXT("FastAscentTop"), Spec.Islands.Last().X, Spec.Islands.Last().Y, 17200.0},
				};
			}
			return Spec;
		}

		/** 전체 내부 외피는 Blender 소유, 코드에는 부유섬과 장치·편성만 남긴다. */
		const TCHAR* const HollowGeometryPrefix = TEXT("SM_Hollow_00_LowPolyV017_");

		FOctantSpec Hollow()
		{
			FOctantSpec Spec;
			Spec.Name = TEXT("Hollow_00");
			Spec.GuidTag = 0x09C00000;
			Spec.Seed = 9002;
			Spec.Amplitude = 0.0;
			Spec.Wavelength = 7000.0;
			Spec.SeamBand = 2000.0;
			Spec.Islands = {
				{TEXT("Ascent"), -5500.0, 0.0, 1200.0, 0.0, 14500.0, 600.0},
				{TEXT("Arena"), 11000.0, 0.0, 4000.0, 0.0, 18000.0, 800.0},
				{TEXT("CaveLift"), 3150.0, 9800.0, 150.0, 0.0, -5500.0, 200.0},
				{TEXT("Shuttle"), -9000.0, -14500.0, 1800.0, 0.0, 6000.0, 600.0},
			};
			Spec.Panels = {
				{{-8700.0, -7350.0, 6015.0}, {-9000.0, -12000.0, 6015.0}, 600.0, 3.0},
				{{3000.0, -1300.0, 2015.0}, {3000.0, 1300.0, 2015.0}, 400.0, 3.0},
			};
			Spec.Anchors = {
				// 잔도 안쪽 착지 여유와 훅 종료 후 관성 낙하를 함께 확보한다.
				// 서쪽: Low→굴 입구→Mid→High→정상, Mid→Panel 분기. 정상 훅은 Shuttle 복귀도 겸한다.
				{-4500.0, -3000.0, -3000.0, 7000.0, TEXT("WestLow")},
				{-3400.0, -2800.0, 1000.0, 7000.0, TEXT("WestPocketLip")},
				{-2500.0, -2850.0, 3800.0, 8500.0, TEXT("WestMid")},
				{-3500.0, -2700.0, 5500.0, 5500.0, TEXT("WestHigh")},
				{-5000.0, -4700.0, 11000.0, 11000.0, TEXT("WestTop")},
				{2800.0, -2800.0, 3800.0, 6000.0, TEXT("WestPanel")},
				// 동쪽: Low→Panel→Mid→High→정상. 겹치는 윗 잔도 바깥에서 다음 잔도 가장자리로 연결한다.
				{5500.0, 2800.0, -2800.0, 7500.0, TEXT("EastLow")},
				{3750.0, 2500.0, 7000.0, 12500.0, TEXT("EastPanel")},
				{0.0, 2850.0, 6200.0, 7000.0, TEXT("EastMid")},
				{4050.0, 2950.0, 8400.0, 7000.0, TEXT("EastHigh")},
				{3250.0, 5000.0, 15500.0, 9000.0, TEXT("EastTop")},
				{0.0, 0.0, -4500.0, 9000.0, TEXT("BasinExit")},
				// 긴 정상의 추가 도착부와 외곽 추락 후 가까운 경사로 복귀. 인접한 동쪽 보고 두 곳은 하나로 묶는다.
				{4650.0, -4700.0, 11000.0, 10000.0, TEXT("WestTopNorth")},
				{-8500.0, 5000.0, 12000.0, 8000.0, TEXT("EastTopSouth")},
				{-5000.0, 6800.0, 10000.0, 11000.0, TEXT("EastOuterRecovery")},
				{3500.0, -8250.0, 16000.0, 17000.0, TEXT("WestOuterRecovery")},
				{-8500.0, -11000.0, 12000.0, 13000.0, TEXT("MountainWestRecovery")},
				{4500.0, -11000.0, 8500.0, 10000.0, TEXT("MountainEastRecovery")},
				// 협곡 밖의 부유섬·뒤편 잔도·북쪽 출구에 필요한 훅만 유지한다.
				{-5500.0, -2000.0, 17800.0, 14000.0, TEXT("Ascent")},
				{3000.0, 0.0, 20500.0, 9000.0, TEXT("Arena")},
				{-8000.0, -14000.0, 8500.0, 10000.0, TEXT("ShuttleIsland")},
				{6000.0, -6700.0, 3200.0, 4000.0, TEXT("BackPocketLip")},
				{2500.0, 9800.0, -3500.0, 10000.0, TEXT("NorthShaftLower")},
				{2500.0, 9800.0, 2200.0, 9500.0, TEXT("NorthShaftUpper")},
				{4000.0, 11000.0, 4500.0, 6500.0, TEXT("Rim")},
			};
			FLNPSpawnEnemyFormation Flyer;
			Flyer.EnemyConfigPath = TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Flyer01.DA_EnemyEntityConfig_PureEntity_Flyer01");
			Flyer.Count = 4;
			Flyer.Offset = FVector2D(-700.0, 700.0);
			Flyer.Radius = 800.0f;
			FLNPSpawnEnemyFormation Crawler;
			Crawler.EnemyConfigPath = TEXT("/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Crawler01.DA_EnemyEntityConfig_PureEntity_Crawler01");
			Crawler.Count = 6;
			Crawler.Offset = FVector2D(700.0, -700.0);
			Crawler.Radius = 900.0f;
			for (const FVector& P : {FVector(-2500.0, -5400.0, 6000.0), FVector(2500.0, 5400.0, 10000.0),
				FVector(-2500.0, 1000.0, -12000.0), FVector(3000.0, 9600.0, -12000.0),
				FVector(-9000.0, -14500.0, 6000.0), FVector(-2500.0, -5000.0, -2500.0),
				FVector(4000.0, 5000.0, 2000.0), FVector(6000.0, -5300.0, 1500.0)})
			{
				FSpawn Pod{P.X, P.Y, P.Z};
				Pod.EnemyFormations = {Flyer, Crawler};
				Spec.Spawns.Add(Pod);
			}
			Spec.Lamps = {{0.0, 0.0, -9700.0, 1800.0, 4500.0}, {-4500.0, 4500.0, -9700.0, 1800.0, 4500.0},
				{2500.0, 9800.0, -9700.0, 1800.0, 4500.0}, {2500.0, 9800.0, -3500.0, 600.0, 3000.0},
				{-13500.0, 3500.0, -8400.0, 1000.0, 3000.0}, {-13500.0, -8500.0, -3400.0, 1000.0, 3000.0}};
			Spec.Blocks = {{11000.0, -7000.0, 18000.0, 300.0, 1800.0},
				{11000.0, 7000.0, 18000.0, 300.0, 2400.0}};
			Spec.ReviewPoints = {
				{TEXT("HollowEntry"), -9000.0, -13500.0, 0.0},
				{TEXT("HollowPodWest"), -2500.0, -5400.0, 6000.0},
				{TEXT("HollowLandingEast"), 2500.0, 5400.0, 10000.0},
				{TEXT("HollowBasin"), -2500.0, 0.0, -7500.0},
				{TEXT("HollowTopWestNorth"), 4662.0, -3884.0, 6000.0},
				{TEXT("HollowTopEastSouth"), -8206.0, 4184.0, 10000.0},
				{TEXT("HollowRecoveryEast01"), -4645.0, 7295.0, 0.0},
				{TEXT("HollowRecoveryEast02"), -5052.0, 7156.0, 0.0},
				{TEXT("HollowRecoveryWest"), 3116.0, -7429.0, 0.0},
				{TEXT("HollowRecoveryMountainWest"), -8088.0, -9789.0, 0.0},
				{TEXT("HollowRecoveryMountainEast"), 4134.0, -12205.0, 0.0},
				{TEXT("HollowRecoveryEastLaunch"), -4900.0, 7400.0, 0.0},
				{TEXT("HollowRecoveryWestLaunch"), 2750.0, -7429.0, 0.0},
				{TEXT("HollowRecoveryMountainWestLaunch"), -7750.0, -9789.0, 0.0},
				{TEXT("HollowRecoveryMountainEastLaunch"), 3800.0, -12205.0, 0.0},
				{TEXT("HollowCliffLow"), -3500.0, -2900.0, -5000.0},
				{TEXT("HollowCliffPanel"), 3000.0, -2800.0, 2000.0},
				{TEXT("HollowPocketWest"), -2500.0, -5000.0, -2500.0},
				{TEXT("HollowPocketEast"), 4000.0, 5000.0, 2000.0},
				{TEXT("HollowPocketBack"), 6000.0, -5300.0, 1500.0},
				{TEXT("HollowAscent"), -5500.0, 0.0, 14500.0},
				{TEXT("HollowArenaEntry"), 3000.0, 0.0, 18000.0},
				{TEXT("HollowArena"), 11000.0, 0.0, 18000.0},
				{TEXT("HollowCaveBasinEntry"), 0.0, -1400.0, -7500.0},
				{TEXT("HollowCaveMesaEntry"), 1800.0, 9800.0, 0.0},
				{TEXT("HollowCavern"), -2500.0, 1000.0, -12000.0},
				{TEXT("HollowCaveWest"), -4500.0, 4500.0, -12000.0},
				{TEXT("HollowCaveNorth"), 3000.0, 9600.0, -12000.0},
				{TEXT("HollowCaveLift"), 3150.0, 9800.0, -5500.0},
				{TEXT("HollowCaveShaft"), 0.0, 0.0, -12000.0},
				{TEXT("HollowRimIsland"), 4000.0, 11000.0, 2000.0},
				{TEXT("HollowOuterEast"), -13500.0, 13500.0, 0.0},
				{TEXT("HollowNotch"), -6000.0, 6000.0, 4500.0},
				{TEXT("HollowShuttleDock"), -8700.0, -6600.0, 6000.0},
				{TEXT("HollowShuttleIsland"), -9000.0, -14500.0, 6000.0},
				{TEXT("HollowSecretMouth"), -12500.0, -12500.0, 0.0},
				{TEXT("HollowSecretBend"), -13500.0, 3500.0, -9500.0},
				{TEXT("HollowSecretBend02"), -13500.0, -8500.0, -4500.0},
				{TEXT("HollowMountainTurn01"), 5000.0, -11500.0, 2000.0},
				{TEXT("HollowMountainTurn02"), -9000.0, -9500.0, 4000.0},
			};
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

		/** Meadow 동굴 입구를 물려받은 복제 지각의 내부 경계를 닫는다. 이음매는 보존하고 각 옥탄트의 입구는 이후 자른다. */
		int32 SealInteriorCrustBoundaries(FLNPFixtureMesh& Mesh, double SeamKeep, bool bConstrainedDelaunay = false)
		{
			TMap<FVector, int32> Welded;
			TArray<int32> Vertices;
			for (int32 I = 0; I < Mesh.Positions.Num(); ++I)
				Vertices.Add(Welded.FindOrAdd(Mesh.Positions[I], I));
			TMap<FIntPoint, int32> EdgeCounts;
			for (const FIntVector& Triangle : Mesh.Triangles)
			{
				const int32 V[] = {Vertices[Triangle.X], Vertices[Triangle.Y], Vertices[Triangle.Z]};
				for (int32 I = 0; I < 3; ++I)
					++EdgeCounts.FindOrAdd({FMath::Min(V[I], V[(I + 1) % 3]), FMath::Max(V[I], V[(I + 1) % 3])});
			}
			TMap<int32, TArray<int32>> Adjacent;
			for (const auto& Edge : EdgeCounts)
			{
				if (Edge.Value != 1) continue;
				Adjacent.FindOrAdd(Edge.Key.X).Add(Edge.Key.Y);
				Adjacent.FindOrAdd(Edge.Key.Y).Add(Edge.Key.X);
			}
			TSet<int32> Visited;
			int32 Sealed = 0;
			for (const auto& Start : Adjacent)
			{
				if (Visited.Contains(Start.Key)) continue;
				TArray<int32> Loop;
				int32 Current = Start.Key, Previous = INDEX_NONE;
				bool bClosed = false, bInterior = true;
				while (!Visited.Contains(Current))
				{
					Visited.Add(Current);
					Loop.Add(Current);
					const FVector& P = Mesh.Positions[Current];
					bInterior &= FMath::Min3(P.X, P.Y, P.Z) > SeamKeep;
					const TArray<int32>& Neighbors = Adjacent[Current];
					if (Neighbors.Num() != 2) break;
					const int32 Next = Neighbors[0] == Previous ? Neighbors[1] : Neighbors[0];
					Previous = Current;
					Current = Next;
					if (Current == Start.Key) { bClosed = true; break; }
				}
				if (!bClosed || !bInterior || Loop.Num() < 3) continue;
				TArray<FVector2D> Outline;
				for (int32 V : Loop)
				{
					const FVector& P = Mesh.Positions[V];
					const double D = FVector::DotProduct(P, Center().Radial);
					Outline.Add({CrustRadius * FVector::DotProduct(P, Center().Bitangent) / D,
						CrustRadius * FVector::DotProduct(P, Center().Tangent) / D});
				}
				TArray<UE::Geometry::FIndex3i> Triangles;
				if (bConstrainedDelaunay)
				{
					// 거의 일직선인 절단 경계가 ear clipping의 가느다란 구면 삼각형으로 남지 않도록 한다.
					TArray<UE::Geometry::FIndex2i> Edges;
					for (int32 I = 0; I < Loop.Num(); ++I) Edges.Emplace(I, (I + 1) % Loop.Num());
					UE::Geometry::FDelaunay2 Delaunay;
					if (!Delaunay.Triangulate(Outline, Edges)) continue;
					Triangles = Delaunay.GetFilledTriangles(Edges, UE::Geometry::FDelaunay2::EFillMode::Solid);
				}
				else PolygonTriangulation::TriangulateSimplePolygon(Outline, Triangles, false);
				for (const auto& Tri : Triangles)
					Mesh.AddTriangle(Loop[Tri.A], Loop[Tri.B], Loop[Tri.C], -Mesh.Positions[Loop[Tri.A]].GetSafeNormal());
				++Sealed;
			}
			return Sealed;
		}

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

		/** Hollow 부유섬은 같은 격자 경계를 공유하는 닫힌 사각 판으로 만든다. */
		FPart BuildHollowIsland(const FIsland& Island)
		{
			FPart Out;
			Out.Id = Island.Id;
			const double Half = Island.Radius * Scale(Island.TopH);
			const int32 N = FMath::Max(1, FMath::CeilToInt(2.0 * Half / 800.0));
			auto At = [&](int32 X, int32 Y, bool bTop)
			{
				return Point(Island.X - Half + 2.0 * Half * X / N, Island.Y - Half + 2.0 * Half * Y / N,
					Island.TopH - (bTop ? 0.0 : Island.Thickness));
			};
			for (int32 Y = 0; Y < N; ++Y)
			for (int32 X = 0; X < N; ++X)
			{
				Out.Floor.AddQuad(At(X,Y,true), At(X+1,Y,true), At(X+1,Y+1,true), At(X,Y+1,true), -Radial(Island.X, Island.Y), false);
				Out.Shell.AddQuad(At(X,Y,false), At(X+1,Y,false), At(X+1,Y+1,false), At(X,Y+1,false), Radial(Island.X, Island.Y), false);
			}
			for (int32 Side = 0; Side < 4; ++Side)
			for (int32 I = 0; I < N; ++I)
			{
				const int32 AX = Side == 0 ? I : Side == 1 ? N : Side == 2 ? N-I : 0;
				const int32 AY = Side == 0 ? 0 : Side == 1 ? I : Side == 2 ? N : N-I;
				const int32 BX = Side == 0 ? I+1 : Side == 1 ? N : Side == 2 ? N-I-1 : 0;
				const int32 BY = Side == 0 ? 0 : Side == 1 ? I+1 : Side == 2 ? N : N-I-1;
				const FVector A = At(AX,AY,true), B = At(BX,BY,true);
				const FVector Along = B-A, Up = -((A+B)*0.5).GetSafeNormal();
				Out.Shell.AddQuad(At(AX,AY,false), At(BX,BY,false), B, A, FVector::CrossProduct(Along, Up), false);
			}
			return Out;
		}

		/** 오목한 협곡 윤곽도 삼각분할한다. 긴 변을 이등분해 구면 윗면·밑면과 절벽 경계의 곡률을 맞춘다. */
		FPart BuildCliff(const FCliff& Cliff)
		{
			FPart Out;
			Out.Id = Cliff.Id;
			TArray<UE::Geometry::FIndex3i> Triangles;
			PolygonTriangulation::TriangulateSimplePolygon(Cliff.Outline, Triangles, false);
			TFunction<void(FVector2D, FVector2D, FVector2D, double, FLNPFixtureMesh&)> AddSurface;
			AddSurface = [&](FVector2D A, FVector2D B, FVector2D C, double H, FLNPFixtureMesh& Mesh)
			{
				const bool bTop = &Mesh == &Out.Floor;
				auto Height = [&](FVector2D P) { return bTop && Cliff.SurfaceH ? Cliff.SurfaceH(P) : H; };
				// 공유 변은 같은 중점을 재귀적으로 사용한다. 경계의 분할 폭은 절벽면과 같은 3m 이하이다.
				const double AB = (B - A).SquaredLength(), BC = (C - B).SquaredLength(), CA = (A - C).SquaredLength();
				if (FMath::Max3(AB, BC, CA) > FMath::Square(Cliff.SurfaceH ? 100.0 : 300.0))
				{
					if (BC >= AB && BC >= CA) { const FVector2D OldA = A; A = B; B = C; C = OldA; }
					else if (CA >= AB) { const FVector2D OldA = A; A = C; C = B; B = OldA; }
					const FVector2D Mid = (A + B) * 0.5;
					AddSurface(A, Mid, C, H, Mesh);
					AddSurface(Mid, B, C, H, Mesh);
					return;
				}
				const FVector2D CenterXY = (A + B + C) / 3.0;
				Mesh.AddTriangle(Mesh.AddVertex(Point(A.X, A.Y, Height(A))), Mesh.AddVertex(Point(B.X, B.Y, Height(B))),
					Mesh.AddVertex(Point(C.X, C.Y, Height(C))), -Radial(CenterXY.X, CenterXY.Y) * (bTop ? 1.0 : -1.0));
			};
			for (const UE::Geometry::FIndex3i& Triangle : Triangles)
			{
				AddSurface(Cliff.Outline[Triangle[0]], Cliff.Outline[Triangle[1]], Cliff.Outline[Triangle[2]], Cliff.TopH, Out.Floor);
				AddSurface(Cliff.Outline[Triangle[0]], Cliff.Outline[Triangle[1]], Cliff.Outline[Triangle[2]], Cliff.BottomH, Out.Shell);
			}
			const int32 Rows = FMath::CeilToInt((Cliff.TopH - Cliff.BottomH) / 2000.0);
			for (int32 Edge = 0; Edge < Cliff.Outline.Num(); ++Edge)
			{
				const FVector2D A = Cliff.Outline[Edge], B = Cliff.Outline[(Edge + 1) % Cliff.Outline.Num()];
				const FVector2D Along = B - A, Outward(Along.Y, -Along.X);
				int32 Segments = 1;
				while (Along.Length() / Segments > (Cliff.SurfaceH ? 100.0 : 300.0)) Segments *= 2;
				for (int32 Segment = 0; Segment < Segments; ++Segment)
				{
					const FVector2D C = FMath::Lerp(A, B, double(Segment) / Segments);
					const FVector2D D = FMath::Lerp(A, B, double(Segment + 1) / Segments);
					for (int32 Row = 0; Row < Rows; ++Row)
					{
						const double TopC = Cliff.SurfaceH ? Cliff.SurfaceH(C) : Cliff.TopH;
						const double TopD = Cliff.SurfaceH ? Cliff.SurfaceH(D) : Cliff.TopH;
						const double C0 = FMath::Lerp(Cliff.BottomH, TopC, double(Row) / Rows), C1 = FMath::Lerp(Cliff.BottomH, TopC, double(Row + 1) / Rows);
						const double D0 = FMath::Lerp(Cliff.BottomH, TopD, double(Row) / Rows), D1 = FMath::Lerp(Cliff.BottomH, TopD, double(Row + 1) / Rows);
						Out.Shell.AddQuad(Point(C.X, C.Y, C0), Point(D.X, D.Y, D0), Point(D.X, D.Y, D1), Point(C.X, C.Y, C1),
							LocalDirection(FVector((C + D) * 0.5, (C0 + C1 + D0 + D1) * 0.25), Outward), false);
					}
				}
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
			// 넓은 굴 바닥도 2m 이하로 분할해 구면을 성긴 평면으로 근사하는 높이 오차를 줄인다.
			const int32 Bands = FMath::Max(2, FMath::CeilToInt(Bridge.Width / 200.0));
			for (int32 Band = 0; Band < Bands; ++Band)
			{
				Strip(Out.Floor, Line(-1.0 + 2.0 * Band / Bands, 0.0), Line(-1.0 + 2.0 * (Band + 1) / Bands, 0.0), Segments, EFacing::Up);
			}
			if (Bridge.bGrounded)
			{
				const bool bAttached = Bridge.AttachedBackY != 0.0;
				const double InnerSign = Side.Y > 0.0 ? 1.0 : -1.0;
				auto Back = [&](double Down)
				{
					return [&, Down](double T)
					{
						const FVector P = FMath::Lerp(Bridge.From, Bridge.To, T);
						return FVector(P.X, Bridge.AttachedBackY, Down == 0.0 ? P.Z : -2000.0);
					};
				};
				// 절벽과 경사로 사이도 같은 암반으로 채운다. 높은 왕복 구간의 밑동은 낮은 바깥길 뒤에 놓인다.
				const int32 FillBands = FMath::CeilToInt(FMath::Max(FMath::Abs(Bridge.From.Y - Bridge.AttachedBackY),
					FMath::Abs(Bridge.To.Y - Bridge.AttachedBackY)) / 200.0);
				if (bAttached)
				{
					for (int32 Band = 0; Band < FillBands; ++Band)
					{
						auto Fill = [&](int32 I) { return [&, I](double T) { return FMath::Lerp(Line(InnerSign, 0.0)(T), Back(0.0)(T), double(I) / FillBands); }; };
						Strip(Out.Floor, Fill(Band), Fill(Band + 1), Segments, EFacing::Up);
					}
				}
				// 지각 아래까지 채우고 밑동을 넓힌 암반 둑이다. 공중 다리의 얇은 밑면과 구별한다.
				auto Base = [&](double Lateral)
				{
					return [&, Lateral](double T)
					{
						const FVector P = FMath::Lerp(Bridge.From, Bridge.To, T);
						const FVector2D Offset = Side * (Lateral * Bridge.Width * 0.9 * Scale(P.Z));
						return FVector(P.X + Offset.X, P.Y + Offset.Y, -2000.0);
					};
				};
				// 동쪽 교체 경계를 지나는 길은 밑면도 끝막음과 같은 폭 분할을 공유한다.
				if (Bridge.Id == TEXT("RimApproach") || Bridge.Id == TEXT("RimLanding") || Bridge.Id == TEXT("RimCliffExit"))
				{
					for (int32 Band = 0; Band < Bands; ++Band)
						Strip(Out.Shell, Base(-1.0 + 2.0 * Band / Bands), Base(-1.0 + 2.0 * (Band + 1) / Bands), Segments, EFacing::Down);
				}
				else Strip(Out.Shell, Base(-1.0), Base(1.0), Segments, EFacing::Down);
				if (!bAttached || InnerSign > 0.0) Strip(Out.Shell, Base(-1.0), Line(-1.0, 0.0), Segments, EFacing::Both);
				if (!bAttached || InnerSign < 0.0) Strip(Out.Shell, Base(1.0), Line(1.0, 0.0), Segments, EFacing::Both);
				if (bAttached)
				{
					Strip(Out.Shell, Base(InnerSign), Back(1.0), Segments, EFacing::Down);
					Strip(Out.Shell, Back(1.0), Back(0.0), Segments, EFacing::Both);
				}
				// 끝막음도 바닥과 같은 폭 분할을 쓴다. 넓은 단일 평면은 구면 바닥 위로 솟아 접합을 막는다.
				for (double T : {0.0, 1.0})
				{
					for (int32 Band = 0; Band < Bands; ++Band)
					{
						const double L = -1.0 + 2.0 * Band / Bands, R = -1.0 + 2.0 * (Band + 1) / Bands;
						Quad(Out.Shell, Base(L)(T), Base(R)(T), Line(R, 0.0)(T), Line(L, 0.0)(T), EFacing::Both);
					}
					if (bAttached)
					{
						for (int32 Band = 0; Band < FillBands; ++Band)
						{
							const double L = double(Band) / FillBands, R = double(Band + 1) / FillBands;
							Quad(Out.Shell, FMath::Lerp(Base(InnerSign)(T), Back(1.0)(T), L),
								FMath::Lerp(Base(InnerSign)(T), Back(1.0)(T), R),
								FMath::Lerp(Line(InnerSign, 0.0)(T), Back(0.0)(T), R),
								FMath::Lerp(Line(InnerSign, 0.0)(T), Back(0.0)(T), L), EFacing::Both);
						}
					}
				}
				return Out;
			}
			Strip(Out.Shell, Line(-1.0, BridgeThickness), Line(1.0, BridgeThickness), Segments, EFacing::Down);
			Strip(Out.Shell, Line(-1.0, BridgeThickness), Line(-1.0, 0.0), Segments, EFacing::Both);
			Strip(Out.Shell, Line(1.0, BridgeThickness), Line(1.0, 0.0), Segments, EFacing::Both);
			return Out;
		}

		/** 같은 높이의 통로 문을 맞추는 방사형 평면. 천장·바닥 높이를 절단 범위에 넣지 않는다. */
		TArray<FPlane> PassageFootprint(const FBridgeRamp& Ramp, double Inset)
		{
			const FVector2D Along = FVector2D(Ramp.To - Ramp.From).GetSafeNormal(), Side(-Along.Y, Along.X);
			const double Half = Ramp.Width * 0.5 - Inset;
			TArray<FVector> Corners;
			for (const auto& E : {TPair<FVector, double>(Ramp.From, -Half), {Ramp.To, -Half}, {Ramp.To, Half}, {Ramp.From, Half}})
			{
				const FVector2D Offset = Side * E.Value * Scale(E.Key.Z);
				Corners.Add(Point(E.Key.X + Offset.X, E.Key.Y + Offset.Y, 0.0));
			}
			TArray<FPlane> Planes;
			const FVector Center = Point((Ramp.From + Ramp.To) * 0.5);
			for (int32 I = 0; I < 4; ++I)
			{
				FPlane Plane(FVector::ZeroVector, Corners[I], Corners[(I + 1) % 4]);
				if (Plane.PlaneDot(Center) > 0.0) Plane = FPlane(-Plane.X, -Plane.Y, -Plane.Z, -Plane.W);
				Planes.Add(Plane);
			}
			return Planes;
		}

		/** 통로 내부를 짧은 볼록 체적으로 잘라 기존 지각·공동·암반을 열고 닫힌 벽을 덧댄다. */
		FPart BuildPassage(const FPassage& Passage, TConstArrayView<FPassage> AllPassages, FLNPFixtureMesh& Crust, TArray<FPart>& Existing, int32& OutCut,
			TArray<TPair<FString, TArray<FPlane>>>& PassageCuts)
		{
			const FBridgeRamp& Ramp = Passage.Ramp;
			FPart Out = BuildBridge(Ramp);
			FLNPFixtureMesh JunctionRoof;
			const bool bJunction = Ramp.Id.StartsWith(TEXT("SecretJunction"));
			const FVector2D Along = FVector2D(Ramp.To - Ramp.From).GetSafeNormal();
			const FVector2D Side(-Along.Y, Along.X);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D(Ramp.To - Ramp.From).Length() / 200.0));
			auto Corners = [&](const FVector& A, const FVector& B, double Left, double Right, double Bottom, double Top)
			{
				TArray<FVector> P;
				for (double H : {Bottom, Top})
				{
					for (const auto& E : {TPair<FVector, double>(A, Left), {A, Right}, {B, Right}, {B, Left}})
					{
						const FVector2D Offset = Side * E.Value * Scale(E.Key.Z);
						P.Add(Point(E.Key.X + Offset.X, E.Key.Y + Offset.Y, E.Key.Z + H));
					}
				}
				return P;
			};
			const int32 Faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
			auto CenterOf = [](const TArray<FVector>& P)
			{
				FVector Center = FVector::ZeroVector;
				for (const FVector& V : P) Center += V;
				return Center / P.Num();
			};
			auto Solid = [&](const TArray<FVector>& P, bool bRoof = false)
			{
				FLNPFixtureMesh& Mesh = bJunction && bRoof ? JunctionRoof : Out.Shell;
				const FVector Center = CenterOf(P);
				for (const auto& Face : Faces)
				{
					const FVector Normal = (P[Face[0]] + P[Face[1]] + P[Face[2]] + P[Face[3]]) * 0.25 - Center;
					Mesh.AddQuad(P[Face[0]], P[Face[1]], P[Face[2]], P[Face[3]], Normal, false);
				}
			};
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const FVector A = FMath::Lerp(Ramp.From, Ramp.To, double(Step) / Steps);
				const FVector B = FMath::Lerp(Ramp.From, Ramp.To, double(Step + 1) / Steps);
				// 분할 경계를 조금 겹쳐 곡률 때문에 기존 암반이 얇게 남지 않게 한다.
				const FVector Extension(Along * 10.0, 0.0);
				const TArray<FVector> P = Corners(A - Extension, B + Extension, -Ramp.Width * 0.5 - 10.0,
					Ramp.Width * 0.5 + 10.0, -10.0, Passage.Height + 10.0);
				const FVector Center = CenterOf(P);
				TArray<FPlane> Planes;
				for (const auto& Face : Faces)
				{
					FPlane Plane(P[Face[0]], P[Face[1]], P[Face[2]]);
					if (Plane.PlaneDot(Center) > 0.0) Plane = FPlane(-Plane.X, -Plane.Y, -Plane.Z, -Plane.W);
					Planes.Add(Plane);
				}
				// 다른 통로의 천장·바닥을 함께 지우면 절단면을 마감하지 않은 추락 틈이 생긴다.
				// 접합 절단은 내부 공간만 열고 같은 높이의 천장과 바닥 두께는 보존한다.
				const TArray<FVector> JoinCorners = Corners(A - Extension, B + Extension, -Ramp.Width * 0.5 - 10.0,
					Ramp.Width * 0.5 + 10.0, -BridgeThickness - 10.0, Passage.Height - 1.0);
				const FVector JoinCenter = CenterOf(JoinCorners);
				TArray<FPlane> JoinPlanes;
				for (const auto& Face : Faces)
				{
					FPlane Plane(JoinCorners[Face[0]], JoinCorners[Face[1]], JoinCorners[Face[2]]);
					if (Plane.PlaneDot(JoinCenter) > 0.0) Plane = FPlane(-Plane.X, -Plane.Y, -Plane.Z, -Plane.W);
					JoinPlanes.Add(Plane);
				}
				PassageCuts.Emplace(Ramp.Id, JoinPlanes);
				OutCut += Crust.RemoveInsideConvex(Planes);
				for (FPart& Part : Existing)
				{
					const bool bSameSecret = Ramp.Id.StartsWith(TEXT("Secret")) && Part.Id.StartsWith(TEXT("Secret"))
						&& Part.Id != TEXT("SecretMountainEntry");
					// 같은 뒷길의 층계참·경사 바닥은 보존하고 맞닿는 벽만 연다.
					if (!bSameSecret && Part.Id != Ramp.Id + TEXT("Lip"))
						Part.Floor.RemoveInsideConvex(Planes);
					if (!bSameSecret)
						Part.Shell.RemoveInsideConvex(Planes);
				}
				Solid(Corners(A, B, -Ramp.Width * 0.5 - WallThickness, -Ramp.Width * 0.5, -BridgeThickness, Passage.Height + WallThickness));
				Solid(Corners(A, B, Ramp.Width * 0.5, Ramp.Width * 0.5 + WallThickness, -BridgeThickness, Passage.Height + WallThickness));
				Solid(Corners(A, B, -Ramp.Width * 0.5, Ramp.Width * 0.5, Passage.Height, Passage.Height + WallThickness), true);
			}
			if (Passage.bClosedEnd)
			{
				const FVector Extension(Along * WallThickness, 0.0);
				Solid(Corners(Ramp.To, Ramp.To + Extension, -Ramp.Width * 0.5 - WallThickness,
					Ramp.Width * 0.5 + WallThickness, -BridgeThickness, Passage.Height + WallThickness));
			}
			if (Passage.bClosedStart)
			{
				const FVector Extension(Along * WallThickness, 0.0);
				Solid(Corners(Ramp.From - Extension, Ramp.From, -Ramp.Width * 0.5 - WallThickness,
					Ramp.Width * 0.5 + WallThickness, -BridgeThickness, Passage.Height + WallThickness));
			}
			// 회전 공간의 벽에 문만 열고 통로 외피는 방 안에서 끝낸다. 경사와 층계참은 공유 단면으로 맞닿는다.
			// 큰 절단 체적을 서로 적용하면 인접 구간의 벽·천장을 깎아 외부로 열린 틈이 생긴다.
			for (const FPassage& Other : AllPassages)
			{
				if (!(bJunction && Other.Ramp.Id.StartsWith(TEXT("SecretLanding"))) &&
					!(Ramp.Id.StartsWith(TEXT("SecretLanding")) && Other.Ramp.Id.StartsWith(TEXT("SecretJunction")))) continue;
				const FBridgeRamp& Room = bJunction ? Ramp : Other.Ramp;
				const FBridgeRamp& Hall = bJunction ? Other.Ramp : Ramp;
				const FVector Center = (Room.From + Room.To) * 0.5;
				if (FVector::Distance(Hall.From, Center) > 1.0 && FVector::Distance(Hall.To, Center) > 1.0) continue;
				Out.Shell.RemoveInsideConvex(PassageFootprint(bJunction ? Hall : Room, 2.0));
			}
			const int32 RoofBase = Out.Shell.Positions.Num();
			Out.Shell.Positions.Append(JunctionRoof.Positions);
			for (const FIntVector& Tri : JunctionRoof.Triangles)
				Out.Shell.Triangles.Add(Tri + FIntVector(RoofBase));
			for (int32 Edge = 0; Edge + 1 < Passage.MouthEdge.Num(); ++Edge)
			{
				const FVector2D A = Passage.MouthEdge[Edge], B = Passage.MouthEdge[Edge + 1];
				FPlane Outside(FVector::ZeroVector, Point(A.X, A.Y, 0.0), Point(B.X, B.Y, 0.0));
				if (Outside.PlaneDot(Point(Ramp.To)) < 0.0) Outside = FPlane(-Outside.X, -Outside.Y, -Outside.Z, -Outside.W);
				const TArray<FPlane> Clip = {Outside};
				Out.Floor.RemoveInsideConvex(Clip);
				Out.Shell.RemoveInsideConvex(Clip);
			}
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
				const double SpiralOuterR = Spire.CoreRadius + Spire.SpiralGap + Spire.SpiralWidth;
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
			const double Inner = Spire.CoreRadius + Spire.SpiralGap;
			const double Outer = Inner + Spire.SpiralWidth;
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
					const double R0 = FMath::Lerp(Inner, Outer, Band / 3.0), R1 = FMath::Lerp(Inner, Outer, (Band + 1) / 3.0);
					Strip(Part.Floor, Edge(R0, 0.0), Edge(R1, 0.0), Segments, EFacing::Up);
					Strip(Part.Shell, Edge(R0, BridgeThickness), Edge(R1, BridgeThickness), Segments, EFacing::Down);
				}
				Strip(Part.Shell, Edge(Outer, BridgeThickness), Edge(Outer, 0.0), Segments, EFacing::Both);
				if (Spire.SpiralGap > 0.0)
				{
					Strip(Part.Shell, Edge(Inner, 0.0), Edge(Inner, BridgeThickness), Segments, EFacing::Both);
				}
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

		/** 같은 방사형 둘레 평면으로 지각·메사·천장 입구를 자른다. */
		TArray<FPlane> CaveMouthPlanes(const FCaveMouth& Mouth)
		{
			TArray<FPlane> Planes;
			const FVector Inside = RadialUnnormalized(Mouth.X, Mouth.Y);
			for (int32 Side = 0; Side < 48; ++Side)
			{
				const FVector2D A = FVector2D(Mouth.X, Mouth.Y) + Polar(Mouth.RadiusArc, 360.0 * Side / 48.0);
				const FVector2D B = FVector2D(Mouth.X, Mouth.Y) + Polar(Mouth.RadiusArc, 360.0 * (Side + 1) / 48.0);
				Planes.Add(CenterPlane(RadialUnnormalized(A.X, A.Y), RadialUnnormalized(B.X, B.Y), Inside));
			}
			return Planes;
		}

		/** 구면 바닥·두께 있는 천장·외벽을 만들고, 두 수직 입구로 공동과 지표를 잇는다. */
		FPart BuildCavern(const FCavern& Cavern, FLNPFixtureMesh& Crust, int32& OutCut)
		{
			FPart Out = BuildCliff(Cavern.FloorSlab);
			Out.bCoarse = true;
			FCliff Roof = Cavern.FloorSlab;
			Roof.BottomH = Cavern.CeilingH;
			Roof.TopH = Cavern.CeilingH + WallThickness;
			FPart Ceiling = BuildCliff(Roof);
			for (const FCaveMouth& Mouth : Cavern.Mouths)
			{
				const TArray<FPlane> Planes = CaveMouthPlanes(Mouth);
				OutCut += Crust.RemoveInsideConvex(Planes);
				Ceiling.Floor.RemoveInsideConvex(Planes);
				Ceiling.Shell.RemoveInsideConvex(Planes);
				// 입구 턱은 표면보다 1cm 아래에 닫아 50cm짜리 돌출 테두리를 만들지 않는다.
				Cylinder(Out.Shell, Mouth.X, Mouth.Y, Mouth.RadiusArc, Cavern.CeilingH - 1.0, Mouth.TopH - 1.0, 48, -1, true);
			}
			auto AppendShell = [&](const FLNPFixtureMesh& Mesh)
			{
				const int32 Base = Out.Shell.Positions.Num();
				Out.Shell.Positions.Append(Mesh.Positions);
				for (const FIntVector& Triangle : Mesh.Triangles)
				{
					Out.Shell.Triangles.Emplace(Triangle.X + Base, Triangle.Y + Base, Triangle.Z + Base);
				}
			};
			AppendShell(Ceiling.Floor);
			AppendShell(Ceiling.Shell);
			const TArray<FVector2D>& Outline = Cavern.FloorSlab.Outline;
			for (int32 Edge = 0; Edge < Outline.Num(); ++Edge)
			{
				const FVector2D A = Outline[Edge], B = Outline[(Edge + 1) % Outline.Num()];
				int32 Segments = 1;
				while (FVector2D::Distance(A, B) / Segments > 300.0) Segments *= 2;
				for (int32 Segment = 0; Segment < Segments; ++Segment)
				{
					Wall(Out.Shell, FMath::Lerp(A, B, double(Segment) / Segments), FMath::Lerp(A, B, double(Segment + 1) / Segments),
						Cavern.FloorSlab.BottomH, Roof.TopH, -1);
				}
			}
			return Out;
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

		#include "SurfaceNavigation/LNPBastionLowPoly.inl"
		void PlaceDeviceMarkers(UWorld& World, const FOctantSpec& Spec);

		#include "SurfaceNavigation/LNPBastionLowPolyTests.inl"

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
		int32 PlaceAuthoredMeshes(UWorld& World, const FString& Folder, int32& OutErrors, const TCHAR* Prefix = nullptr)
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			Registry.ScanPathsSynchronous({Folder}, true);
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPath(FName(*Folder), Assets, false);
			Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });

			int32 Placed = 0;
			for (const FAssetData& Asset : Assets)
			{
				if (Prefix && !Asset.AssetName.ToString().StartsWith(Prefix)) continue;
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
			TArray<FBridgeRamp> Walkways = Spec.Bridges;
			for (const FPassage& Passage : Spec.Passages) Walkways.Add(Passage.Ramp);
			for (const FBridgeRamp& Bridge : Walkways)
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

			// 절벽의 외곽은 이음매 전이 띠 밖에 둔다. 밑면은 협곡 최저 바닥보다 낮아 지각과 틈이 없다.
			for (const FCliff& Cliff : Spec.Cliffs)
			{
				for (const FVector2D& Vertex : Cliff.Outline)
				{
					const FVector World = Point(Vertex.X, Vertex.Y, 0.0);
					if (FMath::Min3(World.X, World.Y, World.Z) < Spec.SeamMargin + Spec.SeamBand)
					{
						Warn(TEXT("SeamBand"), FString::Printf(TEXT("%s cliff footprint enters the seam transition band"), *Cliff.Id), FVector(Vertex, 0.0));
						break;
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

		void PlaceDeviceMarkers(UWorld& World, const FOctantSpec& Spec)
		{
			for (const FReviewPoint& Review : Spec.ReviewPoints)
			{
				if (!FString(Review.Name).StartsWith(TEXT("Panel"))) continue;
				ATargetPoint* Target = World.SpawnActor<ATargetPoint>(Point(Review.X, Review.Y, Review.H), Rotation(Review.X, Review.Y).Rotator());
				Target->SetActorLabel(FString::Printf(TEXT("Review_%s"), Review.Name));
				Target->Tags = {FName(TEXT("LNP.ReviewPoint")), FName(Review.Name)};
				Target->bIsEditorOnlyActor = true;
			}
			UClass* AnchorClass = LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"));
			UClass* PanelClass = FCString::Strcmp(Spec.Name, TEXT("Hollow_00")) == 0
				? LoadClass<ALNPMovingPanel>(nullptr, TEXT("/Game/Maps/Hollow_00/BP_HollowMovingPanel.BP_HollowMovingPanel_C"))
				: FCString::Strcmp(Spec.Name, TEXT("Bastion_00")) == 0
				? LoadClass<ALNPMovingPanel>(nullptr, TEXT("/Game/Maps/Bastion_00/BP_BastionMovingPanel.BP_BastionMovingPanel_C"))
				: ALNPMovingPanel::StaticClass();
			for (int32 Index = 0; Index < Spec.Anchors.Num(); ++Index)
			{
				const FAnchor& Anchor = Spec.Anchors[Index];
				ALNPPlacementMarker* Marker = World.SpawnActor<ALNPPlacementMarker>(Point(Anchor.X, Anchor.Y, Anchor.H), Rotation(Anchor.X, Anchor.Y).Rotator());
				Marker->SetActorLabel(!Anchor.Id.IsEmpty() ? FString::Printf(TEXT("Anchor_%s"), *Anchor.Id) : FString::Printf(TEXT("Anchor_%03d"), Index));
				Marker->MarkerId = FGuid(Spec.GuidTag, 1, 0, Index + 1);
				Marker->ElementClass = AnchorClass;
				Marker->GrappleMaxDistance = Anchor.MaxDistance;
				if (FCString::Strcmp(Spec.Name, TEXT("Hollow_00")) == 0 || FCString::Strcmp(Spec.Name, TEXT("Bastion_00")) == 0)
					Marker->Tags.Add(TEXT("LNP.Grapple.AuthoredOnly"));
			}
			for (int32 Index = 0; Index < Spec.Panels.Num(); ++Index)
			{
				const FPanel& Panel = Spec.Panels[Index];
				const FVector Mid = (Panel.From + Panel.To) * 0.5;
				const FTransform MarkerTransform(Rotation(Mid.X, Mid.Y), Point(Mid));
				ALNPPlacementMarker* Marker = World.SpawnActor<ALNPPlacementMarker>(MarkerTransform.GetLocation(), MarkerTransform.Rotator());
				Marker->SetActorLabel(FString::Printf(TEXT("Panel_%02d"), Index));
				Marker->MarkerId = FGuid(Spec.GuidTag, 2, 0, Index + 1);
				Marker->ElementClass = PanelClass;
				Marker->PathSpeed = Panel.Speed;
				Marker->PathEndHoldSeconds = Panel.HoldSeconds;
				Marker->Path->ClearSplinePoints(false);
				Marker->Path->AddSplinePoint(MarkerTransform.InverseTransformPosition(Point(Panel.From)), ESplineCoordinateSpace::Local, false);
				Marker->Path->AddSplinePoint(MarkerTransform.InverseTransformPosition(Point(Panel.To)), ESplineCoordinateSpace::Local, false);
				Marker->Path->SetSplinePointType(0, ESplinePointType::Linear, false);
				Marker->Path->SetSplinePointType(1, ESplinePointType::Linear, false);
				Marker->Path->UpdateSpline();
			}
		}

		void PlaceMarkers(UWorld& World, const FOctantSpec& Spec, UStaticMesh& Cube, FMarkerReport& Report)
		{
			// 검토 지점: 런타임 LNP.Debug.TeleportToReview가 태그로 찾는다. 에디터 전용이라 쿡에서 빠진다.
			for (const FReviewPoint& Review : Spec.ReviewPoints)
			{
				if (FString(Review.Name).StartsWith(TEXT("Panel"))) { ++Report.ReviewPoints; continue; }
				ATargetPoint* Target = World.SpawnActor<ATargetPoint>(Point(Review.X, Review.Y, Review.H), Rotation(Review.X, Review.Y).Rotator());
				Target->SetActorLabel(FString::Printf(TEXT("Review_%s"), Review.Name));
				Target->Tags = {FName(TEXT("LNP.ReviewPoint")), FName(Review.Name)};
				Target->bIsEditorOnlyActor = true;
				++Report.ReviewPoints;
			}
			PlaceDeviceMarkers(World, Spec);
			Report.Anchors = Spec.Anchors.Num(); Report.Panels = Spec.Panels.Num();
			for (int32 Index = 0; Index < Spec.Spawns.Num(); ++Index)
			{
				const FSpawn& Spawn = Spec.Spawns[Index];
				ALNPMassSpawnPoint* SpawnPoint = World.SpawnActor<ALNPMassSpawnPoint>(Point(Spawn.X, Spawn.Y, Spawn.H), Rotation(Spawn.X, Spawn.Y).Rotator());
				SpawnPoint->SetActorLabel(FString::Printf(TEXT("Spawn_%02d"), Index));
				SpawnPoint->SpawnPointId = FGuid(Spec.GuidTag, 3, 0, Index + 1);
				SpawnPoint->EnemyFormations = Spawn.EnemyFormations;
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
			if (OctantName == TEXT("Bastion_00") && !LoadClass<ALNPMovingPanel>(nullptr,
				TEXT("/Game/Maps/Bastion_00/BP_BastionMovingPanel.BP_BastionMovingPanel_C")))
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Bastion moving panel class is missing; no assets changed")); return;
			}
			if (OctantName == TEXT("Hollow_00"))
			{
				if (!LoadClass<ALNPMovingPanel>(nullptr, TEXT("/Game/Maps/Hollow_00/BP_HollowMovingPanel.BP_HollowMovingPanel_C")))
				{
					UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Hollow moving panel class is missing; no assets changed"));
					return;
				}
				for (const TCHAR* Role : {TEXT("Coarse0_Coarse"), TEXT("Coarse1_Coarse"), TEXT("Coarse2_Coarse"),
					TEXT("Floor0_Floor"), TEXT("Floor1_Floor"), TEXT("Floor2_Floor"), TEXT("Floor3_Floor"), TEXT("Shell")})
				{
					const FString Path = Folder + TEXT("/Authored/") + HollowGeometryPrefix + Role;
					UStaticMesh* Authored = LoadObject<UStaticMesh>(nullptr, *Path);
					if (!Authored || !Authored->GetBodySetup() || Authored->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple)
					{
						UE_LOG(LogLNPSliceOctant, Error, TEXT("[SliceOctant] Whole-terrain replacement requires complex-as-simple %s; no assets changed"), *Path);
						return;
					}
				}
			}
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
			if (OctantName == TEXT("Hollow_00") || OctantName == TEXT("Bastion_00"))
				UE_LOG(LogLNPSliceOctant, Display, TEXT("[SliceOctant] Sealed %d inherited interior crust boundaries"),
					SealInteriorCrustBoundaries(Crust, Spec->SeamMargin + Spec->SeamBand, OctantName == TEXT("Bastion_00")));
			GZeroThicknessWalls = 0;

			// 지각을 자르는 구조물을 먼저 만든다.
			TArray<FPart> Parts;
			for (const FAbyss& Abyss : Spec->Abysses)
			{
				Parts.Add(OctantName == TEXT("Bastion_00") ? BuildBastionAbyss(Abyss, Crust, CrustReport.Cut) : BuildAbyss(Abyss, Crust, CrustReport.Cut));
			}
			for (const FCavern& Cavern : Spec->Caverns)
			{
				Parts.Add(BuildCavern(Cavern, Crust, CrustReport.Cut));
			}
			for (const FSpire& Spire : Spec->Spires)
			{
				Parts.Append(OctantName == TEXT("Bastion_00") ? BuildBastionSpire(Spire) : BuildSpire(Spire));
			}
			for (const FTieredRing& Ring : Spec->Rings)
			{
				Parts.Add(OctantName == TEXT("Bastion_00") ? BuildBastionRing(Ring) : BuildTieredRing(Ring));
			}
			for (const FBuilding& Building : Spec->Buildings)
			{
				Parts.Add(OctantName == TEXT("Bastion_00") ? BuildBastionBuilding(Building) : BuildBuilding(Building));
			}
			for (const FIsland& Island : Spec->Islands)
			{
				Parts.Add(OctantName == TEXT("Hollow_00") ? BuildHollowIsland(Island) : BuildBastionIsland(Island));
			}
			for (const FCliff& Cliff : Spec->Cliffs)
			{
				FPart Part = BuildCliff(Cliff);
				for (const FCavern& Cavern : Spec->Caverns)
				{
					for (const FCaveMouth& Mouth : Cavern.Mouths)
					{
						if (Cliff.Id == Mouth.SurfacePartId)
						{
							const TArray<FPlane> Planes = CaveMouthPlanes(Mouth);
							Part.Floor.RemoveInsideConvex(Planes);
							Part.Shell.RemoveInsideConvex(Planes);
						}
					}
				}
				Parts.Add(MoveTemp(Part));
			}
			for (const FBridgeRamp& Bridge : Spec->Bridges)
			{
				Parts.Add(OctantName == TEXT("Bastion_00") ? BuildBastionBridge(Bridge, Spec->Spires[0]) : BuildBridge(Bridge));
			}
			// 통로끼리 만나는 지점의 벽도 잘라 열린 접합을 만든다. 바닥은 같은 높이로 맞춘다.
			TArray<TPair<FString, TArray<FPlane>>> PassageCuts;
			const int32 FirstPassage = Parts.Num();
			for (const FPassage& Passage : Spec->Passages)
			{
				FPart Part = BuildPassage(Passage, Spec->Passages, Crust, Parts, CrustReport.Cut, PassageCuts);
				Parts.Add(MoveTemp(Part));
			}
			// 뒤에 생성한 통로의 새 측벽도 앞 구간을 가릴 수 있어 교차부를 양방향으로 연다.
			for (int32 Index = FirstPassage; Index < Parts.Num(); ++Index)
			{
				for (const auto& Cut : PassageCuts)
				{
					if (Parts[Index].Id != Cut.Key && !(Parts[Index].Id.StartsWith(TEXT("Secret")) && Cut.Key.StartsWith(TEXT("Secret"))))
						Parts[Index].Shell.RemoveInsideConvex(Cut.Value);
				}
			}

			if (OctantName == TEXT("Hollow_00"))
			{
				// 모든 램프를 감싸는 Blender 소유 영역 바깥의 2% 띠는 지각 아래로 매입된다.
				const FVector2D Boundary[] = {{-15500.0,-15500.0}, {6000.0,-15500.0}, {15500.0,-8500.0},
					{15500.0,8500.0}, {6000.0,15500.0}, {-15500.0,15500.0}};
				TArray<FPlane> Region;
				for (int32 I = 0; I < UE_ARRAY_COUNT(Boundary); ++I)
				{
					const FVector2D A = Boundary[I]*0.98, B = Boundary[(I+1)%UE_ARRAY_COUNT(Boundary)]*0.98;
					Region.Add(CenterPlane(RadialUnnormalized(A.X,A.Y), RadialUnnormalized(B.X,B.Y), Center().Radial));
				}
				CrustReport.Cut += Crust.RemoveInsideConvex(Region);
			}
			if (OctantName == TEXT("Bastion_00") && !ValidateBastionParts(Parts))
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[BastionLowPoly] Geometry preflight failed; no assets changed"));
				return;
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
			const int32 Authored = PlaceAuthoredMeshes(*World, Folder + TEXT("/Authored"), AuthoredErrors,
				OctantName == TEXT("Hollow_00") ? HollowGeometryPrefix : nullptr);
			// Hollow의 단순 사암 층무늬는 재생성 후에도 유지한다. 조명 구슬·마커의 머티리얼은 건드리지 않는다.
			if (OctantName == TEXT("Hollow_00"))
			{
				if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Maps/Hollow_00/M_Hollow_Strata.M_Hollow_Strata")))
				{
					for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
					{
						UStaticMeshComponent* Component = It->GetStaticMeshComponent();
						if (Component->ComponentHasTag(StaticTag)) Component->SetMaterial(0, Material);
					}
				}
			}

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
				TEXT("parts=%d partActors=%d partTris=%d buildings=%d bridges=%d islands=%d cliffs=%d anchors=%d panels=%d spawns=%d blocks=%d lamps=%d reviewPoints=%d ")
				TEXT("authored=%d authoredErrors=%d | lint=%d"),
				*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"), Crust.Triangles.Num(), Degenerate, CrustReport.Moved, CrustReport.Kept,
				CrustReport.MinRadius, CrustReport.MaxRadius, CrustReport.MaxShift, CrustReport.Cut,
				Parts.Num(), PartActors, PartTriangles, Spec->Buildings.Num(), Spec->Bridges.Num(), Spec->Islands.Num(), Spec->Cliffs.Num(),
				Markers.Anchors, Markers.Panels, Markers.Spawns, Markers.Blocks, Markers.Lamps, Markers.ReviewPoints, Authored, AuthoredErrors, Lint);
		}

		// 승인한 지형·Pod·광원은 보존하고 수동 장치 마커만 제자리 갱신한다.
		void UpdateBastionDevices()
		{
			if (!GEditor || GEditor->PlayWorld) return;
			if (!LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"))
				|| !LoadClass<ALNPMovingPanel>(nullptr, TEXT("/Game/Maps/Bastion_00/BP_BastionMovingPanel.BP_BastionMovingPanel_C")))
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[BastionDevices] Required device classes missing; no assets changed")); return;
			}
			UWorld* World = LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/Bastion_00/LVI_Octant_Bastion_00.LVI_Octant_Bastion_00"));
			if (!World || World == GEditor->GetEditorWorldContext().World())
			{
				UE_LOG(LogLNPSliceOctant, Error, TEXT("[BastionDevices] Open TestMap03 with PIE off first")); return;
			}
			TArray<AActor*> Old;
			for (AActor* Actor : World->PersistentLevel->Actors)
			{
				if (Cast<ATargetPoint>(Actor) && Actor->GetActorLabel().StartsWith(TEXT("Review_Panel"))) Old.Add(Actor);
				if (ALNPPlacementMarker* Marker = Cast<ALNPPlacementMarker>(Actor))
					if (Marker->ElementClass && (Marker->ElementClass->IsChildOf(ALNPMovingPanel::StaticClass())
						|| Marker->GetActorLabel().StartsWith(TEXT("Anchor_")))) Old.Add(Marker);
			}
			for (AActor* Actor : Old) World->DestroyActor(Actor);
			const FOctantSpec Spec = Bastion();
			PlaceDeviceMarkers(*World, Spec);
			const bool Saved = LNPFixtureMesh::SavePackage(*World->GetOutermost(), *World, FPackageName::GetMapPackageExtension());
			UE_LOG(LogLNPSliceOctant, Display, TEXT("[BastionDevices] %s removed=%d hooks=%d panels=%d; meshes, Pods, lights preserved; rebake required"),
				Saved ? TEXT("saved") : TEXT("FAILED"), Old.Num(), Spec.Anchors.Num(), Spec.Panels.Num());
		}

		FAutoConsoleCommand DevicesCommand(TEXT("LNP.SurfaceNav.UpdateBastionDevices"),
			TEXT("Replace only Bastion hook and panel markers from the current spec. Open TestMap03 with PIE off, then rebake Bastion."),
			FConsoleCommandDelegate::CreateStatic(&UpdateBastionDevices));

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

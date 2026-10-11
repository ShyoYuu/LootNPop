// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPBastionCrustIntegrityTest,
	"LootNPop.SurfaceNavigation.Bake.BastionCrustIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionCrustIntegrityTest::RunTest(const FString& Parameters)
{
	const FOctantSpec Spec = Bastion();
	UStaticMesh* Source = LoadObject<UStaticMesh>(nullptr, SeamSourceMesh);
	if (!TestNotNull(TEXT("Source crust loads"), Source)) return false;
	// 원본 콘텐츠의 절단을 물려받지 않도록 의도된 Bastion 입구 절단 이전의 지각 전체를 검사한다.
	auto Inspect = [&](const TCHAR* Stage, const FLNPFixtureMesh& Mesh, bool bRequireClosed)
	{
		TMap<FIntVector, int32> Welded;
		TArray<int32> Vertices;
		TArray<FVector> Positions;
		for (const FVector& P : Mesh.Positions)
		{
			const FIntVector Key(FMath::RoundToInt(P.X * 100.0), FMath::RoundToInt(P.Y * 100.0), FMath::RoundToInt(P.Z * 100.0));
			if (!Welded.Contains(Key)) { Welded.Add(Key, Positions.Num()); Positions.Add(P); }
			Vertices.Add(Welded.FindChecked(Key));
		}
		TMap<FIntPoint, int32> Edges;
		for (const FIntVector& T : Mesh.Triangles)
		{
			const int32 V[] = {Vertices[T.X], Vertices[T.Y], Vertices[T.Z]};
			for (int32 I = 0; I < 3; ++I)
				++Edges.FindOrAdd({FMath::Min(V[I], V[(I + 1) % 3]), FMath::Max(V[I], V[(I + 1) % 3])});
		}
		TMap<int32, TArray<int32>> Adjacent;
		for (const auto& E : Edges) if (E.Value == 1)
		{
			Adjacent.FindOrAdd(E.Key.X).Add(E.Key.Y);
			Adjacent.FindOrAdd(E.Key.Y).Add(E.Key.X);
		}
		TSet<int32> Visited;
		int32 Unexpected = 0;
		for (const auto& Start : Adjacent)
		{
			if (Visited.Contains(Start.Key)) continue;
			TArray<int32> Pending = {Start.Key};
			FBox2D Bounds(ForceInit);
			int32 Count = 0, Branches = 0;
			bool bInterior = true;
			double Nearest = TNumericLimits<double>::Max();
			while (!Pending.IsEmpty())
			{
				const int32 V = Pending.Pop(EAllowShrinking::No);
				if (Visited.Contains(V)) continue;
				Visited.Add(V); ++Count;
				const FVector P = Positions[V];
				const double D = FVector::DotProduct(P, Center().Radial);
				const FVector2D XY(CrustRadius * FVector::DotProduct(P, Center().Bitangent) / D,
					CrustRadius * FVector::DotProduct(P, Center().Tangent) / D);
				Bounds += XY;
				Nearest = FMath::Min(Nearest, FVector2D::Distance(XY, FVector2D(-7587.0, 7978.0)));
				bInterior &= FMath::Min3(P.X, P.Y, P.Z) > Spec.SeamMargin + Spec.SeamBand;
				Branches += Adjacent[V].Num() != 2;
				Pending.Append(Adjacent[V]);
			}
			if (bInterior)
			{
				++Unexpected;
				AddInfo(FString::Printf(TEXT("CrustBoundary stage=%s vertices=%d branches=%d localMin=%s localMax=%s nearestReport=%.1f"),
					Stage, Count, Branches, *Bounds.Min.ToString(), *Bounds.Max.ToString(), Nearest));
			}
		}
		AddInfo(FString::Printf(TEXT("CrustIntegrity stage=%s triangles=%d unexpectedInteriorBoundaries=%d"), Stage, Mesh.Triangles.Num(), Unexpected));
		if (bRequireClosed) TestEqual(FString::Printf(TEXT("%s has no unintended interior crust openings"), Stage), Unexpected, 0);
	};
	FLNPFixtureMesh Original;
	int32 Degenerate = 0;
	if (!TestTrue(TEXT("Read source crust"), ReadMeshTriangles(*Source, Original, Degenerate))) return false;
	Inspect(TEXT("MeadowSource"), Original, false);
	FLNPFixtureMesh Derived = Original;
	DeriveCrust(Derived, Spec);
	SealInteriorCrustBoundaries(Derived, Spec.SeamMargin + Spec.SeamBand, true);
	Inspect(TEXT("BastionBeforeIntentionalCuts"), Derived, true);
	for (int32 I = 0; I < Original.Positions.Num(); ++I)
		if (FMath::Min3(Original.Positions[I].X, Original.Positions[I].Y, Original.Positions[I].Z) <= Spec.SeamMargin)
			if (!TestTrue(TEXT("Crust repair preserves every seam vertex exactly"), Original.Positions[I] == Derived.Positions[I])) return false;
	for (int32 I = Original.Triangles.Num(); I < Derived.Triangles.Num(); ++I)
	{
		const FIntVector T = Derived.Triangles[I];
		const FVector A = Derived.Positions[T.X], B = Derived.Positions[T.Y], C = Derived.Positions[T.Z];
		const double Cosine = FVector::DotProduct(FVector::CrossProduct(C - A, B - A).GetSafeNormal(), -(A + B + C).GetSafeNormal());
		if (!TestTrue(TEXT("Restored crust faces point inward and remain walkable"), Cosine >= 0.71))
			AddInfo(FString::Printf(TEXT("RestoredFace triangle=%d cosine=%.6f a=%s b=%s c=%s crossSquared=%.12f"),
				I, Cosine, *A.ToString(), *B.ToString(), *C.ToString(), FVector::CrossProduct(C - A, B - A).SizeSquared()));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPBastionLowPolySourceTest,
	"LootNPop.SurfaceNavigation.Bake.BastionLowPolySource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionLowPolySourceTest::RunTest(const FString& Parameters)
{
	const FOctantSpec S = Bastion();
	TArray<FPart> Parts;
	FLNPFixtureMesh Crust;
	int32 Cut = 0;
	for (const FAbyss& A : S.Abysses) Parts.Add(BuildBastionAbyss(A, Crust, Cut));
	for (const FSpire& P : S.Spires) Parts.Append(BuildBastionSpire(P));
	for (const FTieredRing& R : S.Rings) Parts.Add(BuildBastionRing(R));
	for (const FBuilding& B : S.Buildings) Parts.Add(BuildBastionBuilding(B));
	for (const FIsland& I : S.Islands) Parts.Add(BuildBastionIsland(I));
	for (const FBridgeRamp& B : S.Bridges) Parts.Add(BuildBastionBridge(B, S.Spires[0]));
	TestTrue(TEXT("All proposed Bastion solid boundaries close before writing assets"), ValidateBastionParts(Parts));
	TMap<FString, TArray<const FPart*>> Groups;
	for (const FPart& P : Parts) Groups.FindOrAdd(P.Id.StartsWith(TEXT("P01_Ramp")) ? TEXT("P01_Ramps") : P.Id).Add(&P);
	for (const auto& Group : Groups)
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		TMap<FIntVector, int32> Welded;
		for (const FPart* P : Group.Value) for (const FLNPFixtureMesh* M : {&P->Floor, &P->Shell})
		{
			TArray<int32> Vertices;
			for (FVector V : M->Positions)
			{
				const FIntVector Key(FMath::RoundToInt(V.X * 100.0), FMath::RoundToInt(V.Y * 100.0), FMath::RoundToInt(V.Z * 100.0));
				if (!Welded.Contains(Key)) Welded.Add(Key, Mesh.AppendVertex(V));
				Vertices.Add(Welded.FindChecked(Key));
			}
			for (const FIntVector& T : M->Triangles) Mesh.AppendTriangle(Vertices[T.X], Vertices[T.Y], Vertices[T.Z]);
		}
		UE::Geometry::TMeshAABBTree3<UE::Geometry::FDynamicMesh3> Tree(&Mesh, true);
		const auto Intersections = Tree.FindAllSelfIntersections(true, {}, [](UE::Geometry::FIntrTriangle3Triangle3d& Intr)
		{
			Intr.SetReportCoplanarIntersection(true);
			return Intr.Find();
		});
		const int32 Count = Intersections.Points.Num() + Intersections.Segments.Num() + Intersections.Polygons.Num();
		TestEqual(FString::Printf(TEXT("Nonadjacent source triangle intersections group=%s"), *Group.Key), Count, 0);
		if (Count > 0)
		{
			AddInfo(FString::Printf(TEXT("IntersectionProbe %s points=%d segments=%d polygons=%d"), *Group.Key,
				Intersections.Points.Num(), Intersections.Segments.Num(), Intersections.Polygons.Num()));
			if (!Intersections.Segments.IsEmpty())
			{
				const auto& Hit = Intersections.Segments[0];
				AddInfo(FString::Printf(TEXT("IntersectionProbe %s triangles=%d,%d p0=%s p1=%s"), *Group.Key,
					Hit.TriangleID[0], Hit.TriangleID[1], *Hit.Point[0].ToString(), *Hit.Point[1].ToString()));
			}
		}
	}
	for (const FPart& P : Parts)
	{
		double MinCosine = 1.0;
		for (const FIntVector& T : P.Floor.Triangles)
		{
			const FVector A = P.Placement.TransformPosition(P.Floor.Positions[T.X]);
			const FVector B = P.Placement.TransformPosition(P.Floor.Positions[T.Y]);
			const FVector C = P.Placement.TransformPosition(P.Floor.Positions[T.Z]);
			MinCosine = FMath::Min(MinCosine, FVector::DotProduct(FVector::CrossProduct(C - A, B - A).GetSafeNormal(), -(A + B + C).GetSafeNormal()));
		}
		TestTrue(FString::Printf(TEXT("Proposed Support slope is walkable part=%s minimum cosine=%.3f"), *P.Id, MinCosine), MinCosine >= 0.71);
	}
	TestTrue(TEXT("Authored hooks reduced by more than half"), S.Anchors.Num() <= 67);
	TSet<FString> HookIds;
	TestNotNull(TEXT("Device-only update console command is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("LNP.SurfaceNav.UpdateBastionDevices")));
	for (const FAnchor& A : S.Anchors)
	{
		TestFalse(TEXT("Every hook names a unique destination"), A.Id.IsEmpty() || HookIds.Contains(A.Id));
		TestFalse(TEXT("Every hook declares its landing owner"), A.LandingOwner.IsEmpty());
		HookIds.Add(A.Id);
	}
	for (const FBuilding& B : S.Buildings) TestTrue(TEXT("Every building has one roof destination"), HookIds.Contains(B.Id + TEXT("_Roof")));
	for (const FIsland& I : S.Islands)
		if (I.Id != TEXT("I04") && I.Id != TEXT("I08")) TestTrue(TEXT("Every independent facility has one landing destination"), HookIds.Contains(I.Id + TEXT("_Landing")));
	for (int32 I = 0; I < S.Anchors.Num(); ++I) for (int32 J = I + 1; J < S.Anchors.Num(); ++J)
		TestTrue(FString::Printf(TEXT("Hooks %s / %s are spatially distinct"), *S.Anchors[I].Id, *S.Anchors[J].Id),
			FVector::Distance(Point(S.Anchors[I].X, S.Anchors[I].Y, S.Anchors[I].H), Point(S.Anchors[J].X, S.Anchors[J].Y, S.Anchors[J].H)) >= 1200.0);
	TestEqual(TEXT("Existing authored encounters remain"), S.Spawns.Num(), 12);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPBastionLowPolyContentTest,
	"LootNPop.SurfaceNavigation.Bake.BastionLowPolyContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionLowPolyContentTest::RunTest(const FString& Parameters)
{
	UWorld* Source = LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/Bastion_00/LVI_Octant_Bastion_00.LVI_Octant_Bastion_00"));
	if (!TestNotNull(TEXT("Saved Bastion assembly loads"), Source)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BastionCollisionTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	const FOctantSpec Spec = Bastion();
	const bool Proposal = Parameters == TEXT("Proposal");
	const bool SpireProposal = Parameters == TEXT("SpireProposal");
	const TArray<FPart> SpireParts = SpireProposal ? BuildBastionSpire(Spec.Spires[0]) : TArray<FPart>();
	UWorld* Devices = Proposal ? World : Source;
	if (Proposal) PlaceDeviceMarkers(*World, Spec);
	TMap<FString, ALNPPlacementMarker*> Hooks;
	for (AActor* Actor : Devices->PersistentLevel->Actors)
		if (ALNPPlacementMarker* Marker = Cast<ALNPPlacementMarker>(Actor))
			if (Marker->GetActorLabel().StartsWith(TEXT("Anchor_"))) Hooks.Add(Marker->GetActorLabel(), Marker);
	TSet<UStaticMesh*> Meshes;
	for (AActor* Actor : Source->PersistentLevel->Actors)
	{
		AStaticMeshActor* Terrain = Cast<AStaticMeshActor>(Actor);
		if (!Terrain || !Terrain->GetStaticMeshComponent()->ComponentHasTag(StaticTag)) continue;
		UStaticMeshComponent* Original = Terrain->GetStaticMeshComponent();
		UStaticMesh* Mesh = Original->GetStaticMesh();
		if (!TestNotNull(TEXT("Terrain has a mesh"), Mesh)) return false;
		if (Mesh->GetPathName().StartsWith(TEXT("/Game/Maps/Bastion_00/")))
		{
			TestFalse(TEXT("Generated terrain has a single actor owner"), Meshes.Contains(Mesh));
			TestTrue(TEXT("LOD0 owns single-sided complex collision"), Mesh->LODForCollision == 0 && !Mesh->ComplexCollisionMesh
				&& Mesh->GetBodySetup() && Mesh->GetBodySetup()->CollisionTraceFlag == CTF_UseComplexAsSimple
				&& !Mesh->GetBodySetup()->bDoubleSidedGeometry);
		}
		Meshes.Add(Mesh);
		// 나팔 수정 후보만 임시 메시로 대체한다. 저장 에셋과 나머지 승인 조립은 변경하지 않는다.
		if (SpireProposal && Terrain->GetActorLabel().StartsWith(TEXT("P01_Dish_")))
		{
			const FLNPFixtureMesh& Candidate = Terrain->GetActorLabel().EndsWith(TEXT("_Shell")) ? SpireParts[0].Shell : SpireParts[0].Floor;
			FMeshDescription Description;
			FStaticMeshAttributes Attributes(Description); Attributes.Register();
			const FPolygonGroupID Group = Description.CreatePolygonGroup();
			for (const FIntVector& Triangle : Candidate.Triangles)
			{
				TArray<FVertexInstanceID, TFixedAllocator<3>> Instances;
				for (int32 K = 0; K < 3; ++K)
				{
					const FVertexID V = Description.CreateVertex();
					Attributes.GetVertexPositions()[V] = FVector3f(Candidate.Positions[Triangle[K]]);
					Instances.Add(Description.CreateVertexInstance(V));
				}
				Description.CreateTriangle(Group, Instances);
			}
			Mesh = NewObject<UStaticMesh>(World);
			Mesh->AddSourceModel();
			Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs = false;
			Mesh->GetStaticMaterials() = Original->GetStaticMesh()->GetStaticMaterials();
			Mesh->CreateMeshDescription(0, MoveTemp(Description)); Mesh->CommitMeshDescription(0);
			Mesh->CreateBodySetup(); Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
			Mesh->Build(false);
		}
		FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
		AActor* Owner = World->SpawnActor<AActor>();
		Owner->SetActorLabel(Terrain->GetActorLabel());
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
		Copy->SetStaticMesh(Mesh); Copy->SetMobility(EComponentMobility::Static);
		Copy->SetCollisionProfileName(Original->GetCollisionProfileName());
		Copy->SetWorldTransform(Original->GetComponentTransform());
		Owner->SetRootComponent(Copy); Copy->RegisterComponent();
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BastionLowPolyContent), false);
	int32 Floors = 0, Sweeps = 0;
	// 보고된 나선–나팔 틈: 현지 중력 방향으로 캡슐 전체가 경사판 밑까지 빠져나와야 한다.
	const FSpire& DropSpire = Spec.Spires[0];
	const double DropRise = UE_TWO_PI * (DropSpire.CoreRadius + DropSpire.SpiralWidth * 0.5)
		* FMath::Tan(FMath::DegreesToRadians(DropSpire.SlopeDeg));
	int32 SpiralDrops = 0, BlockedSpiralDrops = 0;
	for (double H = 15500.0; H <= 17500.0; H += 50.0)
	{
		const double Deg = DropSpire.ThetaAtZero + H / DropRise * 360.0;
		const double R = DropSpire.CoreRadius + DropSpire.SpiralGap + DropSpire.SpiralWidth + 55.0;
		const FVector Start = Point(PolarH(R * Scale(H + 150.0), Deg, H + 150.0));
		const FVector Up = -Start.GetSafeNormal();
		FHitResult Hit;
		++SpiralDrops;
		if (World->SweepSingleByChannel(Hit, Start, Start - Up * (150.0 + BridgeThickness + 110.0),
			FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params))
		{
			if (BlockedSpiralDrops++ == 0)
				AddInfo(FString::Printf(TEXT("Spiral gap first blocked H=%.0f deg=%.2f mesh=%s startPenetrating=%d hitTime=%.3f"),
					H, Deg, Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"), Hit.bStartPenetrating, Hit.Time));
		}
	}
	AddInfo(FString::Printf(TEXT("Spiral gap capsule drops=%d blocked=%d"), SpiralDrops, BlockedSpiralDrops));
	TestEqual(TEXT("Spiral outer gap lets the complete player capsule fall below the ramp"), BlockedSpiralDrops, 0);
	// Meadow 동굴 입구가 남았던 전체 영역과 주변을 저장 지각의 실제 Chaos 충돌로 검사한다.
	int32 CrustProbes = 0, MissingCrust = 0;
	FVector FirstMissing = FVector::ZeroVector;
	for (double X = -8200.0; X <= -7000.0; X += 50.0) for (double Y = 7300.0; Y <= 8200.0; Y += 50.0)
	{
		FHitResult Hit;
		const FVector Feet = Point(X, Y, 0.0), Up = -Feet.GetSafeNormal();
		++CrustProbes;
		if (!World->LineTraceSingleByChannel(Hit, Feet + Up * 80.0, Feet - Up * 80.0, ECC_Pawn, Params)
			|| FVector::DotProduct(Hit.ImpactNormal, Up) < 0.71)
		{
			if (MissingCrust++ == 0) FirstMissing = FVector(X, Y, 0.0);
		}
	}
	AddInfo(FString::Printf(TEXT("Inherited cave opening Chaos probes=%d missing=%d firstMissing=%s"), CrustProbes, MissingCrust, *FirstMissing.ToString()));
	TestEqual(TEXT("Saved crust covers the inherited Meadow cave opening and its perimeter"), MissingCrust, 0);
	auto Route = [&](const FString& Name, int32 Steps, TFunctionRef<FVector(double)> FeetAt)
	{
		FVector Previous = FVector::ZeroVector;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const FVector Feet = FeetAt(double(I) / Steps), Up = -Feet.GetSafeNormal();
			FHitResult Floor; ++Floors;
			if (!World->LineTraceSingleByChannel(Floor, Feet + Up * 80.0, Feet - Up * 80.0, ECC_Pawn, Params)
				&& !World->SweepSingleByChannel(Floor, Feet + Up * 80.0, Feet - Up * 80.0, FQuat::Identity,
					ECC_Pawn, FCollisionShape::MakeSphere(1.0), Params))
			{
				AddError(FString::Printf(TEXT("Missing floor route=%s step=%d feet=%s"), *Name, I, *Feet.ToString())); return;
			}
			const double NormalUp = FVector::DotProduct(Floor.ImpactNormal, Up);
			if (NormalUp < 0.71) { AddError(FString::Printf(TEXT("Unwalkable floor route=%s step=%d cosine=%.3f"), *Name, I, NormalUp)); return; }
			// 경사면에서도 구형 캡슐 밑면에14cm 여유를 유지한다.
			const FVector Center = Floor.ImpactPoint + Up * (68.0 + 42.0 / NormalUp);
			if (I > 0) for (bool Reverse : {false, true})
			{
				FHitResult Hit; ++Sweeps;
				if (World->SweepSingleByChannel(Hit, Reverse ? Center : Previous, Reverse ? Previous : Center,
					FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params))
				{
					// 실제 보행기의40cm 스텝을 포함한다. 올린 몸체의 상승/횡단/하강 세 경로가 모두 비어야 한다.
					const FVector A = Reverse ? Center : Previous, B = Reverse ? Previous : Center;
					const FQuat Q = FQuat::FindBetweenNormals(FVector::UpVector, Up);
					FHitResult StepHit;
					if (!World->SweepSingleByChannel(StepHit, A, A + Up * 40.0, Q, ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)
						&& !World->SweepSingleByChannel(StepHit, A + Up * 40.0, B + Up * 40.0, Q, ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)
						&& !World->SweepSingleByChannel(StepHit, B + Up * 40.0, B, Q, ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)) continue;
					AddError(FString::Printf(TEXT("Blocked capsule route=%s step=%d reverse=%d mesh=%s feetH=%.1f hitH=%.1f normal=%.2f point=%s"), *Name, I, Reverse,
						Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"), CrustRadius - Floor.ImpactPoint.Length(),
						CrustRadius - Hit.ImpactPoint.Length(), FVector::DotProduct(Hit.ImpactNormal, Up), *Feet.ToString()));
					if (Hit.GetComponent()) AddInfo(FString::Printf(TEXT("RouteProbe %s localFeet=%s localHit=%s stepHit=%s penetration=%d"), *Name,
						*Hit.GetComponent()->GetComponentTransform().InverseTransformPosition(Feet).ToString(),
						*Hit.GetComponent()->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint).ToString(),
						StepHit.GetActor() ? *StepHit.GetActor()->GetActorLabel() : TEXT("none"), StepHit.bStartPenetrating));
					return;
				}
			}
			Previous = Center;
		}
	};
	// 다리 전체 폭을 세 트랙으로 횡단한다. 자유 끝 바깥은 바닥이 없으므로 별도 접합 경로에서 검사한다.
	for (const FBridgeRamp& B : Spec.Bridges) for (double Lane : {-0.75, 0.0, 0.75})
	{
		const double Length = FVector2D::Distance(FVector2D(B.From), FVector2D(B.To));
		Route(FString::Printf(TEXT("%s lane=%.2f"), *B.Id, Lane), FMath::CeilToInt(Length / 100.0), [&](double T)
		{
			const double A = FMath::Lerp(1.0 / Length, 1.0 - 1.0 / Length, T);
			return Point(BastionBridgePoint(B, Spec.Spires[0], A, Lane * B.Width * 0.5));
		});
	}
	for (const FSpire& S : Spec.Spires)
	{
		const double Rise = UE_TWO_PI * (S.CoreRadius + S.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(S.SlopeDeg));
		for (double Lane : {100.0, S.SpiralWidth * 0.5, S.SpiralWidth - 100.0})
		{
			Route(FString::Printf(TEXT("Spiral lane=%.0f"), Lane), 800, [&](double T)
			{
				const double H = FMath::Lerp(S.BottomH + 50.0, S.TopH, T);
				return Point(PolarH((S.CoreRadius + S.SpiralGap + Lane) * Scale(H), S.ThetaAtZero + H / Rise * 360.0, H));
			});
			Route(FString::Printf(TEXT("Spiral dish exit lane=%.0f"), Lane), 20, [&](double T)
			{
				return Point(PolarH((S.CoreRadius + S.SpiralGap + Lane) * Scale(S.TopH),
					S.ThetaAtZero + S.TopH / Rise * 360.0 + 10.0 * T, S.TopH));
			});
		}
	}
	for (const FTieredRing& R : Spec.Rings) for (const FRingStair& S : R.Stairs)
	{
		const FVector2D Radii = StairRadii(S), Span = StairSpanDeg(S);
		for (double Lane : {100.0, (Radii.Y - Radii.X) * 0.5, Radii.Y - Radii.X - 100.0})
			Route(FString::Printf(TEXT("%s stair=%.1f lane=%.0f"), *R.Id, S.StartDeg, Lane), 70, [&](double T)
			{
				const double Edge = FMath::RadiansToDegrees(1.0 / Radii.Y);
				const double A = FMath::Lerp(Edge, Span.X + Span.Y - Edge, T);
				return Point(PolarH(Radii.X + Lane, S.StartDeg + A, FMath::Lerp(S.H0, S.H1, FMath::Min(A / Span.X, 1.0))));
			});
	}
	for (const FBuilding& B : Spec.Buildings)
	{
		const double HX = B.SizeX * 0.5, HY = B.SizeY * 0.5;
		const double Length = B.StoryHeight / FMath::Tan(FMath::DegreesToRadians(RampSlopeDeg));
		for (int32 Story = 0; Story + 1 < B.Stories; ++Story) for (double Side : {-0.65, 0.0, 0.65})
		{
			const bool Even = Story % 2 == 0;
			const double X0 = Even ? -HX + 200.0 : HX - 200.0, X1 = X0 + (Even ? Length : -Length);
			const double Y = (Even ? -HY + B.RampWidth * 0.5 : HY - B.RampWidth * 0.5) + Side * B.RampWidth * 0.5;
			Route(FString::Printf(TEXT("%s internal ramp=%d side=%.2f"), *B.Id, Story, Side), 40, [&](double T)
			{
				return Point(BuildingPoint(B, FMath::Lerp(X0, X1, T), Y, B.BaseH + (Story + T) * B.StoryHeight));
			});
		}
	}
	for (double Deg : {0.0, 90.0, 180.0, 270.0}) for (double Side : {-200.0, 0.0, 200.0})
		Route(FString::Printf(TEXT("Natural gate %.0f lane=%.0f"), Deg, Side), 125, [&](double T)
		{
			const FVector2D XY = Polar(FMath::Lerp(16000.0, Deg == 0.0 ? 1900.0 : 3500.0, T), Deg) + Polar(Side, Deg + 90.0);
			return Point(XY.X, XY.Y, 0.0);
		});
	// 지면 아래 마감판이 베이커의1cm 여유 Nav 캡슐을 막지 않아야 한다.
	for (double Side : {-200.0, 0.0, 200.0})
	{
		FVector Previous = FVector::ZeroVector;
		for (int32 I = 0; I <= 40; ++I)
		{
			const FVector Feet = Point(FMath::Lerp(5000.0, 3500.0, I / 40.0), Side, 0.0), Up = -Feet.GetSafeNormal();
			FHitResult Floor;
			if (!World->LineTraceSingleByChannel(Floor, Feet + Up * 80.0, Feet - Up * 80.0, LNPCollisionChannels::SurfaceSupport, Params))
			{ AddError(TEXT("North ground Nav approach has no Support floor")); break; }
			const double Dot = FMath::Max(FVector::DotProduct(Up, Floor.ImpactNormal), 0.01);
			const FVector Center = Floor.ImpactPoint + Up * (88.0 + 50.0 * (1.0 / Dot - 1.0) + 1.0);
			if (I == 0) Previous = Center;
			bool Blocked = false;
			for (bool Reverse : {false, true})
			{
				FHitResult Hit;
				if (!World->SweepSingleByChannel(Hit, Reverse ? Center : Previous, Reverse ? Previous : Center,
					FQuat::FindBetweenNormals(FVector::UpVector, Up), LNPCollisionChannels::WorldExact,
					FCollisionShape::MakeCapsule(50.0, 88.0), Params)) continue;
				AddError(FString::Printf(TEXT("North ground Nav capsule blocked lane=%.0f step=%d mesh=%s H=%.2f"), Side, I,
					Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"), CrustRadius - Hit.ImpactPoint.Length()));
				Blocked = true; break;
			}
			if (Blocked) break;
			Previous = Center;
		}
	}
	// 모든 마커의 저장 위치와 개별 사거리를 원본과 대조한다.
	TestEqual(TEXT("Saved assembly retains all hook roles"), Hooks.Num(), Spec.Anchors.Num());
	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(nullptr);
	if (!TestNotNull(TEXT("Effective grapple config loads"), Config)) return false;
	// 연속 상승은 목적지 근처 임의 바닥 접근과 구분해 실제 이전 시설물에서 검사한다.
	TMap<FString, TArray<FVector>> Connections;
	auto HookLink = [&](const FIsland& From, const FString& Target, bool AirChain = false)
	{
		const FAnchor* A = Spec.Anchors.FindByPredicate([&](const FAnchor& H) { return H.Id == Target; });
		if (!TestNotNull(TEXT("Hook connection has a destination"), A)) return;
		const FVector2D Dir = (FVector2D(A->X, A->Y) - FVector2D(From.X, From.Y)).GetSafeNormal();
		const FAnchor* Previous = Spec.Anchors.FindByPredicate([&](const FAnchor& H) { return H.Id == From.Id + TEXT("_Landing"); });
		FVector Launch = AirChain && Previous ? Point(Previous->X, Previous->Y, Previous->H - 60.0)
			: Point(From.X + Dir.X * From.Radius * 0.8 * Scale(From.TopH), From.Y + Dir.Y * From.Radius * 0.8 * Scale(From.TopH), From.TopH + 110.0);
		// 하강은 발판 밖으로 한 걸음 나와 당기는 경로다. 자기 발판을 뚫는 직선으로 검사하지 않는다.
		if (!AirChain && From.TopH > A->H)
			Launch = Point(From.X + Dir.X * (From.Radius + 120.0) * Scale(From.TopH),
				From.Y + Dir.Y * (From.Radius + 120.0) * Scale(From.TopH), From.TopH + 140.0);
		if (From.Id == TEXT("I04") || From.Id == TEXT("I08"))
		{
			const FString BuildingId = From.Id == TEXT("I04") ? TEXT("S04") : TEXT("S08");
			const FBuilding* B = Spec.Buildings.FindByPredicate([&](const FBuilding& Building) { return Building.Id == BuildingId; });
			const FVector Roof = Point(BuildingPoint(*B, 0.0, 0.0, B->BaseH + RoofH(*B)));
			const FVector Toward = FVector::VectorPlaneProject(Point(A->X, A->Y, A->H) - Roof, -Roof.GetSafeNormal()).GetSafeNormal();
			Launch = Roof + Toward * 1150.0 - Roof.GetSafeNormal() * 140.0;
		}
		const FVector End = Point(A->X, A->Y, A->H), Up = -End.GetSafeNormal();
		const double Range = A->MaxDistance > 0.0 ? A->MaxDistance : Config->MaxGrappleDistance;
		FHitResult Hit;
		const bool Clear =
			FVector::Distance(Launch, End) <= Range && !World->LineTraceSingleByChannel(Hit, Launch, End, ECC_Visibility, Params)
			&& !World->SweepSingleByChannel(Hit, Launch, End - (End - Launch).GetSafeNormal() * 150.0,
				FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params);
		TestTrue(FString::Printf(TEXT("Hook connection %s -> %s range and body path distance=%.0f mesh=%s hitH=%.0f"), *From.Id, *Target,
			FVector::Distance(Launch, End), Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"), CrustRadius - Hit.ImpactPoint.Length()), Clear);
		if (Clear) Connections.FindOrAdd(Target).Add(Launch);
	};
	for (const TCHAR* Prefix : {TEXT("C"), TEXT("V")})
	{
		TArray<const FIsland*> Chain;
		for (const FIsland& I : Spec.Islands) if (I.Id.StartsWith(Prefix)) Chain.Add(&I);
		for (int32 I = 0; I + 1 < Chain.Num(); ++I)
		{
			if (Chain[I]->Id == TEXT("C03")) continue;
			HookLink(*Chain[I], Chain[I + 1]->Id + TEXT("_Landing"));
		}
	}
	for (const FIsland& I : Spec.Islands)
		if (I.Id == TEXT("C03") || I.Id == TEXT("C06")) HookLink(I, I.Id == TEXT("C03") ? TEXT("CavernExit_090") : TEXT("CavernExit_270"), true);
	TArray<const FIsland*> Sky;
	for (const FIsland& I : Spec.Islands) if (I.Id.StartsWith(TEXT("I"))) Sky.Add(&I);
	for (int32 I = 0; I + 1 < Sky.Num(); ++I) for (bool Reverse : {false, true})
	{
		const FIsland& Target = *Sky[I + (Reverse ? 0 : 1)];
		const FString Id = Target.Id == TEXT("I04") ? TEXT("S04_Roof") : Target.Id == TEXT("I08") ? TEXT("S08_Roof") : Target.Id + TEXT("_Landing");
		HookLink(*Sky[I + (Reverse ? 1 : 0)], Id);
	}
	int32 HookApproaches = 0, HookLandings = 0, HookFalls = 0, LandingMargins = 0;
	for (int32 I = 0; I < Spec.Anchors.Num(); ++I)
	{
		const FAnchor& A = Spec.Anchors[I];
		ALNPPlacementMarker* const* Found = Hooks.Find(TEXT("Anchor_") + A.Id);
		if (!TestNotNull(TEXT("Hook exists"), Found)) continue;
		ALNPPlacementMarker* Marker = *Found;
		TestTrue(TEXT("Saved hook agrees with original position"), Marker->GetActorLocation().Equals(Point(A.X, A.Y, A.H), 0.1));
		TestEqual(TEXT("Saved hook retains its distance override"), Marker->GrappleMaxDistance, float(A.MaxDistance));
		TestTrue(TEXT("Manual Bastion hooks exclude seed hooks"), Marker->Tags.Contains(TEXT("LNP.Grapple.AuthoredOnly")));
		const double Range = A.MaxDistance > 0.0 ? A.MaxDistance : Config->MaxGrappleDistance;
		const FVector End = Marker->GetActorLocation(), Up = -End.GetSafeNormal();
		FHitResult Floor;
		bool Accessible = false; FVector RepresentativeLaunch = FVector::ZeroVector;
		TArray<FVector> Launches;
		for (double Radius : {0.0, 400.0, 800.0, 1400.0}) for (int32 D = 0; D < 8; ++D)
		{
			const FVector Direction = Rotation(A.X, A.Y).RotateVector(FVector(FMath::Cos(D * UE_PI / 4.0), FMath::Sin(D * UE_PI / 4.0), 0.0));
			const FVector Start = End + Direction * Radius;
			if (!World->LineTraceSingleByChannel(Floor, Start - Up * 100.0, Start - Up * Range, ECC_Pawn, Params)) continue;
			const FVector Launch = Floor.ImpactPoint - Floor.ImpactPoint.GetSafeNormal() * 110.0;
			const FVector Delta = End - Launch;
			if (Delta.Length() > Range || Delta.Length() < Config->MinGrappleDistance) continue;
			const FVector Stop = End - Delta.GetSafeNormal() * 150.0;
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Launch + Up * 70.0, End, ECC_Visibility, Params)) continue;
			if (World->SweepSingleByChannel(Hit, Launch, Stop, FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn,
				FCollisionShape::MakeCapsule(42.0, 96.0), Params)) continue;
			Accessible = true; RepresentativeLaunch = Launch; Launches.AddUnique(Launch); ++HookApproaches;
		}
		if (const TArray<FVector>* Required = Connections.Find(A.Id)) for (FVector Launch : *Required)
		{
			Launches.AddUnique(Launch); RepresentativeLaunch = Launch; Accessible = true; ++HookApproaches;
		}
		TestTrue(FString::Printf(TEXT("Hook %03d has an unobstructed in-range body approach"), I), Accessible);
		FHitResult Landing;
		const double FallDistance = A.H - A.LandingH;
		// 삼각형 공유 꼭짓점의 광선 수치 누락은1cm 구체로 재검사한다. 몸체 관성 낙하는 별도로 필수다.
		if (!World->LineTraceSingleByChannel(Landing, End, End - Up * (FallDistance + 100.0), ECC_Pawn, Params)
			&& !World->SweepSingleByChannel(Landing, End, End - Up * (FallDistance + 100.0), FQuat::Identity,
				ECC_Pawn, FCollisionShape::MakeSphere(1.0), Params))
		{
			AddError(FString::Printf(TEXT("Hook %03d role=%s has no intended first floor"), I, *A.Id)); continue;
		}
		const double ExpectedH = A.LandingH;
		if (FMath::Abs(CrustRadius - Landing.ImpactPoint.Length() - ExpectedH) > 50.0)
			AddError(FString::Printf(TEXT("Hook %03d role=%s first floor height=%.1f expected=%.1f mesh=%s"), I, *A.Id,
				CrustRadius - Landing.ImpactPoint.Length(), ExpectedH, Landing.GetActor() ? *Landing.GetActor()->GetActorLabel() : TEXT("none")));
		TestTrue(FString::Printf(TEXT("Hook %s first floor belongs to %s"), *A.Id, *A.LandingOwner),
			Landing.GetActor() && (Landing.GetActor()->GetActorLabel() == A.LandingOwner
				|| Landing.GetActor()->GetActorLabel().StartsWith(A.LandingOwner + TEXT("_"))));
		++HookLandings;
		for (int32 D = 0; D < 8; ++D)
		{
			const FVector Offset = Rotation(A.X, A.Y).RotateVector(FVector(FMath::Cos(D * UE_PI / 4.0), FMath::Sin(D * UE_PI / 4.0), 0.0)) * 100.0;
			const FVector Probe = Landing.ImpactPoint + Offset;
			FHitResult Margin; ++LandingMargins;
			TestTrue(FString::Printf(TEXT("Hook %03d role=%s retains 100cm landing margin direction=%d"), I, *A.Id, D),
				World->LineTraceSingleByChannel(Margin, Probe + Up * 160.0, Probe - Up * 160.0, ECC_Pawn, Params)
				&& FVector::DotProduct(Margin.ImpactNormal, Up) >= 0.71);
		}
		if (!Accessible || RepresentativeLaunch.IsNearlyZero()) continue;
		// 실조회 BP의 정지150/종료600, 중력2000, 감속200, 종단2000으로 무입력 도착을 예측한다.
		// 실제 Mover 입력·착지감은 마지막 PIE에서 확인한다.
		auto Fall = [&](FVector Launch, bool CheckMargin = true)
		{
			const FVector Direction = (End - Launch).GetSafeNormal();
			FVector Position = End - Direction * 150.0, Velocity = Direction * 600.0;
			++HookFalls;
			for (int32 Step = 0; Step < 900; ++Step)
			{
				const FVector FallUp = -Position.GetSafeNormal();
				FVector Lateral = FVector::VectorPlaneProject(Velocity, FallUp);
				Lateral *= FMath::Max(0.0, 1.0 - (200.0 / 60.0) / FMath::Max(Lateral.Length(), 0.001));
				Velocity = Lateral + FallUp * FMath::Max(-2000.0, FVector::DotProduct(Velocity, FallUp) - 2000.0 / 60.0);
				const FVector Next = Position + Velocity / 60.0;
				FHitResult Hit;
				if (World->SweepSingleByChannel(Hit, Position, Next, FQuat::FindBetweenNormals(FVector::UpVector, FallUp), ECC_Pawn,
					FCollisionShape::MakeCapsule(42.0, 96.0), Params))
				{
					const FVector ArrivalUp = -Hit.ImpactPoint.GetSafeNormal();
					if (Hit.bStartPenetrating || FVector::DotProduct(Hit.ImpactNormal, ArrivalUp) < 0.7
						|| FMath::Abs(CrustRadius - Hit.ImpactPoint.Length() - ExpectedH) > 50.0
						|| !Hit.GetActor() || !(Hit.GetActor()->GetActorLabel() == A.LandingOwner
							|| Hit.GetActor()->GetActorLabel().StartsWith(A.LandingOwner + TEXT("_"))))
						AddError(FString::Printf(TEXT("Hook %03d role=%s inertia missed height=%.1f expected=%.1f normal=%.2f mesh=%s"), I, *A.Id,
							CrustRadius - Hit.ImpactPoint.Length(), ExpectedH, FVector::DotProduct(Hit.ImpactNormal, ArrivalUp),
							Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none")));
					if (CheckMargin) for (int32 D = 0; D < 8; ++D)
					{
						const TArray<FVector>* Required = Connections.Find(A.Id);
						const double MarginRadius = Required && Required->Contains(Launch) ? 100.0 : 50.0;
						const FVector Offset = Rotation(A.X, A.Y).RotateVector(FVector(FMath::Cos(D * UE_PI / 4.0), FMath::Sin(D * UE_PI / 4.0), 0.0)) * MarginRadius;
						FHitResult Margin; ++LandingMargins;
						const FVector Probe = Hit.ImpactPoint + Offset;
						if (!World->LineTraceSingleByChannel(Margin, Probe + ArrivalUp * 100.0, Probe - ArrivalUp * 100.0, ECC_Pawn, Params)
							|| !Margin.GetActor() || !(Margin.GetActor()->GetActorLabel() == A.LandingOwner || Margin.GetActor()->GetActorLabel().StartsWith(A.LandingOwner + TEXT("_")))
							|| FMath::Abs(CrustRadius - Margin.ImpactPoint.Length() - ExpectedH) > 50.0 || FVector::DotProduct(Margin.ImpactNormal, ArrivalUp) < 0.7)
							AddError(FString::Printf(TEXT("Hook %s actual inertia arrival lacks %.0fcm margin direction=%d mesh=%s"), *A.Id, MarginRadius, D,
								Margin.GetActor() ? *Margin.GetActor()->GetActorLabel() : TEXT("none")));
					}

					return;
				}
				Position = Next;
			}
			AddError(FString::Printf(TEXT("Hook %03d role=%s inertia has no landing"), I, *A.Id));
		};
		for (FVector Launch : Launches) Fall(Launch);
		for (int32 D = 0; D < 8; ++D)
		{
			const FVector Offset = Marker->GetActorForwardVector() * (200.0 * FMath::Cos(D * UE_PI / 4.0))
				+ Marker->GetActorRightVector() * (200.0 * FMath::Sin(D * UE_PI / 4.0));
			const FVector Start = RepresentativeLaunch + Offset;
			FHitResult Hit;
			if (FVector::Distance(Start, End) > Range || World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) continue;
			if (World->SweepSingleByChannel(Hit, Start, End - (End - Start).GetSafeNormal() * 150.0,
				FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)) continue;
			Fall(Start);
		}
		for (int32 D = 0; D < 16; ++D)
		{
			const FVector Offset = Rotation(A.X, A.Y).RotateVector(FVector(FMath::Cos(D * UE_PI / 8.0), FMath::Sin(D * UE_PI / 8.0), 0.0));
			const FVector Start = End + Offset * FMath::Min(3000.0, Range - 200.0) - Up * 600.0;
			FHitResult Hit;
			if (FVector::Distance(Start, End) > Range || World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) continue;
			if (!World->SweepSingleByChannel(Hit, Start, End - (End - Start).GetSafeNormal() * 150.0,
				FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)) Fall(Start, false);
		}
	}
	int32 Panels = 0, PanelSweeps = 0;
	for (AActor* Actor : Devices->PersistentLevel->Actors)
	{
		ALNPPlacementMarker* Marker = Cast<ALNPPlacementMarker>(Actor);
		if (!Marker || !Marker->ElementClass || !Marker->ElementClass->IsChildOf(ALNPMovingPanel::StaticClass())) continue;
		++Panels;
		const ALNPMovingPanel* Defaults = Marker->ElementClass->GetDefaultObject<ALNPMovingPanel>();
		const FVector Size = Defaults->GetPanelMesh()->GetStaticMesh()->GetBoundingBox().GetSize()
			* Defaults->GetPanelMesh()->GetRelativeScale3D();
		TestTrue(TEXT("Combat panel measures 1200 x 1200 x 30cm"), Size.Equals(FVector(1200.0, 1200.0, 30.0), 0.1));
		FLNPPanelPath Path;
		Path.OriginLocation = Marker->GetActorLocation(); Path.OriginRotation = Marker->GetActorQuat();
		Path.Curve = Marker->Path->GetSplinePointsPosition();
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Marker->Path->GetSplineLength() / 100.0));
		for (int32 I = 0; I <= Steps; ++I)
		{
			const double T = double(I) / Steps;
			const FVector Position = Marker->Path->GetLocationAtDistanceAlongSpline(Marker->Path->GetSplineLength() * T, ESplineCoordinateSpace::World);
			const FQuat Q = Path.EvaluateRotation(T);
			FHitResult Hit;
			if (World->SweepSingleByChannel(Hit, Marker->Path->GetLocationAtDistanceAlongSpline(Marker->Path->GetSplineLength() * FMath::Max(0.0, T - 1.0 / Steps), ESplineCoordinateSpace::World),
				Position, Q, ECC_Pawn, FCollisionShape::MakeBox(Size * 0.5), Params))
			{
				AddError(FString::Printf(TEXT("Panel full body blocked step=%d mesh=%s"), I, Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"))); break;
			}
		}
		for (double X : {-Size.X * 0.5 + 60.0, 0.0, Size.X * 0.5 - 60.0})
		for (double Y : {-Size.Y * 0.5 + 60.0, 0.0, Size.Y * 0.5 - 60.0})
		{
			FVector Previous = FVector::ZeroVector;
			for (int32 I = 0; I <= Steps; ++I)
			{
				const FQuat Q = Path.EvaluateRotation(double(I) / Steps);
				const FVector Origin = Marker->Path->GetLocationAtDistanceAlongSpline(
					Marker->Path->GetSplineLength() * I / Steps, ESplineCoordinateSpace::World);
				const FVector Up = Q.GetAxisZ(), Center = Origin + Q.RotateVector(FVector(X, Y, Size.Z * 0.5 + 110.0));
				TestTrue(TEXT("Panel remains horizontal to local gravity"), FVector::DotProduct(Up, -Origin.GetSafeNormal()) > 0.999);
				if (I > 0) for (bool Reverse : {false, true})
				{
					FHitResult Hit; ++PanelSweeps;
					if (World->SweepSingleByChannel(Hit, Reverse ? Center : Previous, Reverse ? Previous : Center,
						Q, ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params))
					{
						AddError(FString::Printf(TEXT("Panel rider blocked step=%d x=%.0f y=%.0f reverse=%d mesh=%s"), I, X, Y, Reverse,
							Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("none"))); break;
					}
				}
				Previous = Center;
			}
		}
		for (int32 End : {0, 1})
		{
			const FQuat Q = Path.EvaluateRotation(End); const FVector Up = Q.GetAxisZ();
			const FVector Origin = Marker->Path->GetLocationAtDistanceAlongSpline(
				Marker->Path->GetSplineLength() * End, ESplineCoordinateSpace::World) + Up * Size.Z * 0.5;
			bool Docked = false;
			// 확대 패널의 접안 폭6m에서 세 차선의 실제 점프 경로를 양방향으로 검사한다.
			// BP 점프750/중력2000의 최고점140.6cm 안에서100cm 포물선을 양방향으로 검사한다.
			for (int32 D = 0; D < 16 && !Docked; ++D)
			{
				const FVector Direction = Q.RotateVector(FVector(FMath::Cos(D * UE_PI / 8.0), FMath::Sin(D * UE_PI / 8.0), 0.0));
				const FVector Side = FVector::CrossProduct(Up, Direction);
				Docked = true;
				for (double Lane : {-300.0, 0.0, 300.0})
				{
					FHitResult Floor; const FVector P = Origin + Side * Lane + Direction * (FMath::Min(Size.X, Size.Y) * 0.5 + 350.0);
					if (!World->LineTraceSingleByChannel(Floor, P + Up * 140.0, P - Up * 140.0, ECC_Pawn, Params)
						|| FVector::DotProduct(Floor.ImpactNormal, Up) < 0.7) { Docked = false; break; }
					const FVector A = Origin + Side * Lane + Direction * (FMath::Min(Size.X, Size.Y) * 0.5 - 60.0) + Up * 110.0;
					const FVector B = Floor.ImpactPoint + Up * 110.0;
					for (bool Reverse : {false, true})
					{
						FVector Previous = Reverse ? B : A;
						for (int32 I = 1; I <= 16; ++I)
						{
							const double T = double(I) / 16.0;
							const FVector Next = FMath::Lerp(Reverse ? B : A, Reverse ? A : B, T) + Up * (400.0 * T * (1.0 - T));
							FHitResult Hit;
							if (World->SweepSingleByChannel(Hit, Previous, Next, Q, ECC_Pawn, FCollisionShape::MakeCapsule(42.0, 96.0), Params)) Docked = false;
							Previous = Next;
						}
					}
				}
			}
			TestTrue(FString::Printf(TEXT("Panel endpoint %d has a clear bidirectional docking jump"), End), Docked);
		}
	}
	TestEqual(TEXT("Existing Bastion shuttle remains"), Panels, Spec.Panels.Num());
	AddInfo(FString::Printf(TEXT("Bastion saved Chaos floors=%d bidirectional capsule sweeps=%d hook approaches=%d first landings=%d landing margins=%d inertia falls=%d panel sweeps=%d"), Floors, Sweeps, HookApproaches, HookLandings, LandingMargins, HookFalls, PanelSweeps));
	return !HasAnyErrors();
}

// 저장 지형에 후보 장치만 임시 배치해 production 변경 전에 같은 Chaos 검사를 실행한다.
class FLNPBastionDeviceProposalBase : public FLNPBastionLowPolyContentTest
{
public:
	FLNPBastionDeviceProposalBase(const FString& Name, bool) : FLNPBastionLowPolyContentTest(Name) {}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FLNPBastionDeviceProposalTest, FLNPBastionDeviceProposalBase,
	"LootNPop.SurfaceNavigation.Bake.BastionDeviceProposal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionDeviceProposalTest::RunTest(const FString& Parameters)
{
	return FLNPBastionLowPolyContentTest::RunTest(TEXT("Proposal"));
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FLNPBastionSpireProposalTest, FLNPBastionDeviceProposalBase,
	"LootNPop.SurfaceNavigation.Bake.BastionSpireProposal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionSpireProposalTest::RunTest(const FString& Parameters)
{
	return FLNPBastionLowPolyContentTest::RunTest(TEXT("SpireProposal"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPBastionLowPolyNavTest,
	"LootNPop.SurfaceNavigation.Bake.BastionLowPolyNav",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionLowPolyNavTest::RunTest(const FString& Parameters)
{
	ULNPOctantSurfaceData* Data = LoadObject<ULNPOctantSurfaceData>(nullptr,
		TEXT("/Game/Maps/Bastion_00/DA_OctantSurface_Bastion_00.DA_OctantSurface_Bastion_00"));
	if (!TestNotNull(TEXT("Bastion bake loads"), Data)) return false;
	FLNPOctantDefinition Definition;
	Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Bastion_00/LVI_Octant_Bastion_00.LVI_Octant_Bastion_00")));
	Definition.SurfaceData = Data; Definition.SeamSignature = TEXT("BastionLowPolyTest");
	TArray<FLNPOctantDefinition> Definitions; Definitions.Init(Definition, 8);
	TArray<ULNPOctantSurfaceData*> Loaded; Loaded.Init(Data, 8);
	FLNPSurfaceDataSnapshot Snapshot; FString Error;
	if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, Loaded, 1, Snapshot, Error)) { AddError(Error); return false; }
	const FOctantSpec Spec = Bastion();
	FLNPNavSearchScratch Scratch;
	int32 Paths = 0, PodProjections = 0;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		auto Project = [&](FVector Local)
		{
			FLNPNavProjection Best; Best.Distance = MAX_dbl;
			for (const auto& Layer : Snapshot.Slots[Slot].Navigation->Layers)
			{
				FLNPNavProjection Candidate;
				if (LNPNavQuery::ProjectToNode(Snapshot, Snapshot.Slots[Slot].SlotRotation.RotateVector(Local),
					{uint16(Slot), Layer.LocalSupportLayerId, Snapshot.Generation}, 300.0, Candidate) && Candidate.Distance < Best.Distance) Best = Candidate;
			}
			return Best.Distance < MAX_dbl ? LNPNavGraph::ToGraphNode(Snapshot.Nav, Best.Node) : INDEX_NONE;
		};
		auto Walk = [&](const FString& Name, FVector A, FVector B, bool ExpectedReach = true)
		{
			const int32 Start = Project(A), Goal = Project(B);
			if (Start == INDEX_NONE || Goal == INDEX_NONE) { AddError(FString::Printf(TEXT("Nav projection failed route=%s slot=%d"), *Name, Slot)); return; }
			for (bool Reverse : {false, true})
			{
				FLNPNavSearchParams SearchParams; SearchParams.MaxExpansions = Snapshot.Nav.Graph.GetNodeCount();
				FLNPNavSearch Search;
				LNPNavPathfinding::BeginSearch(Snapshot.Nav, Reverse ? Goal : Start, Reverse ? Start : Goal, SearchParams, Scratch, Search);
				if (Search.Status == ELNPNavSearchStatus::Running) LNPNavPathfinding::StepSearch(Snapshot.Nav, Scratch, Search, SearchParams.MaxExpansions);
				++Paths;
				TestTrue(FString::Printf(TEXT("Nav route=%s slot=%d reverse=%d expectedReach=%d"), *Name, Slot, Reverse, ExpectedReach),
					ExpectedReach ? Search.Status == ELNPNavSearchStatus::Found
						: Search.Status == ELNPNavSearchStatus::NoPath || Search.Status == ELNPNavSearchStatus::Unreachable);
				if (!ExpectedReach && Slot == 0) AddInfo(FString::Printf(TEXT("Disconnected Nav route=%s reverse=%d status=%d"), *Name, Reverse, int32(Search.Status)));
			}
		};
		for (const auto& Anchor : Snapshot.Slots[Slot].Spawn->AuthoredAnchors)
		{
			FLNPNavProjection Projection;
			TestTrue(TEXT("Pod projects onto its declared baked layer in every slot"), LNPNavQuery::ProjectToNode(Snapshot,
				Snapshot.Slots[Slot].SlotRotation.RotateVector(FVector(Anchor.LocalTransform.GetLocation())),
				{uint16(Slot), Anchor.LocalLayerId, Snapshot.Generation}, 300.0, Projection));
			++PodProjections;
			TestTrue(TEXT("Pod retains 250cm edge clearance in every slot"), Anchor.EdgeClearance >= 250.0f);
			TestTrue(TEXT("Pod retains player capsule headroom in every slot"), Anchor.Headroom >= 192.0f);
			for (const auto& Formation : Anchor.EnemyFormations)
			{
				const UMassEntityConfigAsset* Enemy = LoadObject<UMassEntityConfigAsset>(nullptr, *Formation.EnemyConfigPath.ToString());
				const ULNPEnemyTrait* Trait = nullptr;
				for (const UMassEntityConfigAsset* Asset = Enemy; Asset && !Trait; Asset = Asset->GetConfig().GetParent())
					Trait = Cast<ULNPEnemyTrait>(Asset->FindTrait(ULNPEnemyTrait::StaticClass()));
				if (TestNotNull(TEXT("Authored enemy config retains its trait"), Trait) && Trait->EnemyConfig)
					TestTrue(TEXT("Pod headroom meets authored enemy config in every slot"), Anchor.Headroom >= Trait->EnemyConfig->GetSpawnRequiredHeadroom());
			}
		}
		for (const FBridgeRamp& Bridge : Spec.Bridges) Walk(Bridge.Id, Point(Bridge.From), Point(Bridge.To));
		int32 BridgeIndex = 0;
		for (const FBuilding& B : Spec.Buildings)
		{
			if (B.Id.StartsWith(TEXT("S"))) continue;
			for (int32 Side = 0; Side < 4; ++Side)
				Walk(B.Id + FString::Printf(TEXT("_RoofSide%d"), Side), Point(BuildingPoint(B, 0.0, 0.0, RoofH(B))), Point(Spec.Bridges[BridgeIndex++].From));
		}
		Walk(TEXT("NaturalGroundSpiral"), Point(9000.0, 0.0, 0.0), Point(1900.0, 0.0, 0.0));
		for (const FSpire& S : Spec.Spires)
		{
			const double Rise = UE_TWO_PI * (S.CoreRadius + S.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(S.SlopeDeg));
			auto At = [&](double H) { return Point(PolarH((S.CoreRadius + S.SpiralGap + S.SpiralWidth * 0.5) * Scale(H), S.ThetaAtZero + H / Rise * 360.0, H)); };
			Walk(TEXT("CavernGroundSpiral"), At(S.BottomH + 100.0), At(0.0));
			Walk(TEXT("GroundDishSpiral"), At(0.0), At(S.TopH));
		}
		for (const FIsland& I : Spec.Islands)
			if (I.Id == TEXT("C01") || I.Id == TEXT("V01"))
				Walk(I.Id + TEXT("_GrappleOnly"), Point(9000.0, 0.0, 0.0), Point(I.X, I.Y, I.TopH), false);
	}
	TestEqual(TEXT("All twelve Pods project in all eight slots"), PodProjections, 96);
	AddInfo(FString::Printf(TEXT("Bastion baked nav bidirectional paths=%d Pod projections=%d"), Paths, PodProjections));
	return !HasAnyErrors();
}

#endif

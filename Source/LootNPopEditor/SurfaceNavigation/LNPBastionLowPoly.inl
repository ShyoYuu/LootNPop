// Copyright (c) 2026 LootNPop. All rights reserved.

// LNPSliceOctant의 제작 좌표와 spec을 공유하는 Bastion 전용 외피 생성기다.
// XY 셀의 수직 솔리드 구간을 합치고 이웃 셀에 가려진 면을 제거한다.
struct FBastionInterval
{
	double Lo[4], Hi[4];
};

using FBastionColumn = TArray<FBastionInterval>;

/** 곡률 분할과 문/경사로 경계를 함께 보존한다. */
TArray<double> BastionBreaks(TArray<double> Points, double MaxSpan)
{
	Points.Sort();
	TArray<double> Result;
	for (double P : Points)
	{
		if (!Result.IsEmpty() && P - Result.Last() < 1.e-6) continue;
		if (!Result.IsEmpty())
		{
			const double A = Result.Last();
			const int32 N = FMath::CeilToInt((P - A) / MaxSpan);
			for (int32 I = 1; I < N; ++I) Result.Add(FMath::Lerp(A, P, double(I) / N));
		}
		Result.Add(P);
	}
	return Result;
}

/** 높이 구간의 교차점은 UV 분할점으로 미리 넣는다. 셀 내부에서 위아래 순서가 바뀌지 않는다. */
void BastionAddInterval(FBastionColumn& Column, const FVector2D UV[4],
	TFunctionRef<double(FVector2D)> Lo, TFunctionRef<double(FVector2D)> Hi)
{
	FBastionInterval Added;
	for (int32 K = 0; K < 4; ++K) { Added.Lo[K] = Lo(UV[K]); Added.Hi[K] = Hi(UV[K]); }
	auto Mid = [](const double H[4]) { return (H[0] + H[1] + H[2] + H[3]) * 0.25; };
	if (Mid(Added.Hi) <= Mid(Added.Lo) + 1.e-6) return;
	for (int32 I = Column.Num() - 1; I >= 0; --I)
	{
		const FBastionInterval& Other = Column[I];
		if (Mid(Added.Lo) > Mid(Other.Hi) + 1.e-6 || Mid(Other.Lo) > Mid(Added.Hi) + 1.e-6) continue;
		for (int32 K = 0; K < 4; ++K)
		{
			Added.Lo[K] = FMath::Min(Added.Lo[K], Other.Lo[K]);
			Added.Hi[K] = FMath::Max(Added.Hi[K], Other.Hi[K]);
		}
		Column.RemoveAt(I);
	}
	Column.Add(Added);
}

/** 공통 UV 격자에서 솔리드 합집합의 외피만 출력한다. 열린 역할 조각의 법선은 다시 계산하지 않는다. */
FPart BastionVolume(const FString& Id, const TArray<double>& U, const TArray<double>& V, bool bPeriodic,
	TFunctionRef<void(const FVector2D*, FBastionColumn&)> Fill, TFunctionRef<FVector(FVector)> Map, bool bOpenV = false)
{
	FPart Out;
	Out.Id = Id;
	const int32 NX = U.Num() - 1, NY = V.Num() - 1;
	TArray<FBastionColumn> Cells;
	Cells.SetNum(NX * NY);
	auto Corners = [&](int32 X, int32 Y, FVector2D P[4])
	{
		P[0] = {U[X], V[Y]}; P[1] = {U[X + 1], V[Y]};
		P[2] = {U[X + 1], V[Y + 1]}; P[3] = {U[X], V[Y + 1]};
	};
	auto Key = [&](FVector2D P) { if (bPeriodic && FMath::IsNearlyEqual(P.Y, V.Last(), 1.e-6)) P.Y = V[0]; return P; };
	TMap<FVector2D, TArray<double>> Levels;
	for (int32 Y = 0; Y < NY; ++Y) for (int32 X = 0; X < NX; ++X)
	{
		FVector2D P[4]; Corners(X, Y, P);
		Fill(P, Cells[Y * NX + X]);
		for (const FBastionInterval& I : Cells[Y * NX + X]) for (int32 K = 0; K < 4; ++K)
		{
			Levels.FindOrAdd(Key(P[K])).Add(I.Lo[K]);
			Levels.FindOrAdd(Key(P[K])).Add(I.Hi[K]);
		}
	}
	auto Face = [&](FLNPFixtureMesh& Mesh, const TArray<FVector>& Polygon, FVector Normal)
	{
		if (Polygon.Num() < 3) return;
		FVector Mid = FVector::ZeroVector;
		for (FVector P : Polygon) Mid += P;
		Mid /= Polygon.Num();
		const FVector Desired = Map(Mid + Normal * 0.01) - Map(Mid);
		if (Polygon.Num() == 4)
		{
			const FVector A = Map(Polygon[0]), B = Map(Polygon[1]), C = Map(Polygon[2]), D = Map(Polygon[3]);
			if (FVector::CrossProduct(B - A, C - A).SizeSquared() > 1.e-12
				&& FVector::CrossProduct(C - A, D - A).SizeSquared() > 1.e-12)
			{
				Mesh.AddQuad(A, B, C, D, Desired, false);
				return;
			}
		}
		const int32 C = Mesh.AddVertex(Map(Mid));
		for (int32 K = 0; K < Polygon.Num(); ++K)
		{
			const FVector A = Map(Polygon[K]), B = Map(Polygon[(K + 1) % Polygon.Num()]);
			if (FVector::CrossProduct(A - Map(Mid), B - Map(Mid)).SizeSquared() < 1.e-12) continue;
			Mesh.AddTriangle(C, Mesh.AddVertex(A), Mesh.AddVertex(B), Desired);
		}
	};
	for (int32 Y = 0; Y < NY; ++Y) for (int32 X = 0; X < NX; ++X)
	{
		FVector2D P[4]; Corners(X, Y, P);
		for (const FBastionInterval& I : Cells[Y * NX + X])
		{
			TArray<FVector> Top, Bottom;
			for (int32 K = 0; K < 4; ++K) { Top.Emplace(P[K], I.Hi[K]); Bottom.Emplace(P[K], I.Lo[K]); }
			Face(Out.Floor, Top, FVector::UpVector);
			Face(Out.Shell, Bottom, -FVector::UpVector);
			for (int32 E = 0; E < 4; ++E)
			{
				if (bOpenV && ((E == 0 && Y == 0) || (E == 2 && Y == NY - 1))) continue;
				const int32 B = (E + 1) % 4;
				int32 XX = X + (E == 1 ? 1 : E == 3 ? -1 : 0);
				int32 YY = Y + (E == 2 ? 1 : E == 0 ? -1 : 0);
				if (bPeriodic) YY = (YY + NY) % NY;
				TArray<FVector4d> Strips = {FVector4d(I.Lo[E], I.Lo[B], I.Hi[E], I.Hi[B])};
				if (XX >= 0 && XX < NX && YY >= 0 && YY < NY)
				{
					const int32 OtherA[] = {3, 0, 1, 2}, OtherB[] = {2, 3, 0, 1};
					for (const FBastionInterval& N : Cells[YY * NX + XX])
					{
						TArray<FVector4d> Next;
						for (const FVector4d& S : Strips)
						{
							FVector4d Below(S.X, S.Y, FMath::Min(S.Z, N.Lo[OtherA[E]]), FMath::Min(S.W, N.Lo[OtherB[E]]));
							FVector4d Above(FMath::Max(S.X, N.Hi[OtherA[E]]), FMath::Max(S.Y, N.Hi[OtherB[E]]), S.Z, S.W);
							if (Below.Z + Below.W > Below.X + Below.Y + 1.e-6) Next.Add(Below);
							if (Above.Z + Above.W > Above.X + Above.Y + 1.e-6) Next.Add(Above);
						}
						Strips = MoveTemp(Next);
					}
				}
				for (const FVector4d& S : Strips)
				{
					TArray<FVector> Side = {FVector(P[E], S.X), FVector(P[B], S.Y)};
					// 교차 높이를 공유해 수직 T 접합을 남기지 않는다.
					TArray<double> Right = Levels.FindChecked(Key(P[B])), Left = Levels.FindChecked(Key(P[E]));
					Right.Sort(); Left.Sort(TGreater<double>());
					for (double H : Right) if (H > S.Y + 1.e-6 && H < S.W - 1.e-6) Side.AddUnique(FVector(P[B], H));
					Side.Emplace(P[B], S.W); Side.Emplace(P[E], S.Z);
					for (double H : Left) if (H > S.X + 1.e-6 && H < S.Z - 1.e-6) Side.AddUnique(FVector(P[E], H));
					const FVector2D CenterUV = (P[0] + P[2]) * 0.5;
					Face(Out.Shell, Side, FVector(((P[E] + P[B]) * 0.5 - CenterUV).GetSafeNormal(), 0.0));
				}
			}
		}
	}
	return Out;
}

FPart BuildBastionIsland(const FIsland& I)
{
	const double K = Scale(I.TopH);
	return BastionVolume(I.Id, BastionBreaks({I.InnerRadius * K, I.Radius * K}, 600.0),
		BastionBreaks({0.0, 360.0}, 15.0), true, [&](const FVector2D* P, FBastionColumn& C)
	{
		BastionAddInterval(C, P, [&](FVector2D) { return I.TopH - I.Thickness; }, [&](FVector2D) { return I.TopH; });
	}, [&](FVector P)
	{
		const FVector2D XY = FVector2D(I.X, I.Y) + Polar(P.X, P.Y);
		return Point(XY.X, XY.Y, P.Z);
	});
}

FVector2D BastionBridgeLandings(const FBridgeRamp& B)
{
	const double Length = FVector2D::Distance(FVector2D(B.From), FVector2D(B.To));
	const double Limit = Length / Scale((B.From.Z + B.To.Z) * 0.5) < 2000.0 ? 0.2 : 0.35;
	const bool Sky = B.Id >= TEXT("D46") && B.Id <= TEXT("D56");
	return {FMath::Min((B.Id == TEXT("D49") || B.Id == TEXT("D53") ? 800.0 : 300.0) * Scale(B.From.Z), Length * Limit),
		FMath::Min((Sky && B.Id != TEXT("D48") && B.Id != TEXT("D52") ? 300.0 : 800.0) * Scale(B.To.Z), Length * Limit)};
}

/** 섬/성벽 안에 경사를 묻지 않도록 양 끝에 수평 접속부를 둔다. */
FVector BastionBridgePoint(const FBridgeRamp& B, const FSpire& S, double T, double Lateral)
{
	const FVector2D Along = FVector2D(B.To - B.From).GetSafeNormal(), Side(-Along.Y, Along.X);
	const double Length = FVector2D::Distance(FVector2D(B.From), FVector2D(B.To));
	const FVector2D Landings = BastionBridgeLandings(B);
	FVector P = FMath::Lerp(B.From, B.To, T);
	P.Z = FMath::Lerp(B.From.Z, B.To.Z, FMath::Clamp((T * Length - Landings.X) / (Length - Landings.X - Landings.Y), 0.0, 1.0));
	FVector2D XY = FVector2D(P) + Side * Lateral * Scale(P.Z);
	if (B.Id == TEXT("D56"))
	{
		// 마지막 상승은 고속 상승 발판과 접시 나팔을 바깥으로 우회한다.
		const double A0 = FMath::RadiansToDegrees(FMath::Atan2(B.From.Y, B.From.X));
		const double A1 = FMath::RadiansToDegrees(FMath::Atan2(B.To.Y, B.To.X));
		const double Angle = A0 + FMath::FindDeltaAngleDegrees(A0, A1) * T;
		const double Radius = FMath::Lerp(FVector2D(B.From).Length(), FVector2D(B.To).Length(), T) + 4500.0 * FMath::Sin(UE_PI * T);
		XY = Polar(Radius + Lateral * Scale(P.Z), Angle);
	}
	// 나선 접속부는 폭 방향 높이도 나선과 공유한다. 수평 판의 옆면이 나선 보행을 막지 않게 한다.
	if (B.Id == TEXT("D58") || B.Id == TEXT("D59"))
	{
		const double Rise = UE_TWO_PI * (S.CoreRadius + S.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(S.SlopeDeg));
		const double CenterAngle = FMath::RadiansToDegrees(FMath::Atan2(B.To.Y, B.To.X));
		const double Angle = FMath::RadiansToDegrees(FMath::Atan2(XY.Y, XY.X));
		const double BlendStart = B.Id == TEXT("D59") ? 3100.0 : FVector2D(B.From).Length();
		const double Blend = FMath::Clamp((BlendStart - XY.Length())
			/ (BlendStart - (S.CoreRadius + S.SpiralGap + S.SpiralWidth + 50.0) * Scale(B.To.Z)), 0.0, 1.0);
		P.Z += FMath::FindDeltaAngleDegrees(CenterAngle, Angle) * Rise / 360.0 * Blend;
	}
	return FVector(XY, P.Z);
}

FPart BuildBastionBridge(const FBridgeRamp& B, const FSpire& S)
{
	const double Length = FVector2D::Distance(FVector2D(B.From), FVector2D(B.To));
	const FVector2D Landings = BastionBridgeLandings(B);
	TArray<double> Stations = {0.0, Landings.X, Length - Landings.Y, Length};
	const bool SpiralDock = B.Id == TEXT("D58") || B.Id == TEXT("D59");
	if (SpiralDock)
	{
		const double FromRadius = FVector2D(B.From).Length();
		Stations.Add(FromRadius - (B.Id == TEXT("D59") ? 3100.0 : FromRadius));
		Stations.Add(FromRadius - (S.CoreRadius + S.SpiralGap + S.SpiralWidth + 50.0) * Scale(B.To.Z));
	}
	return BastionVolume(B.Id, BastionBreaks({-B.Width * 0.5, B.Width * 0.5}, 600.0),
		BastionBreaks(Stations, SpiralDock ? 100.0 : 400.0), false, [&](const FVector2D* P, FBastionColumn& C)
	{
		BastionAddInterval(C, P, [](FVector2D) { return -BridgeThickness; }, [](FVector2D) { return 0.0; });
	}, [&](FVector P)
	{
		FVector A = BastionBridgePoint(B, S, P.Y / Length, P.X);
		A.Z += P.Z;
		return Point(A);
	});
}

TArray<FPart> BuildBastionSpire(const FSpire& S)
{
	const double TurnRise = UE_TWO_PI * (S.CoreRadius + S.SpiralWidth * 0.5) * FMath::Tan(FMath::DegreesToRadians(S.SlopeDeg));
	auto Real = [&](double R, double A, double H)
	{
		const FVector2D XY = FVector2D(S.X, S.Y) + Polar(R * Scale(H), A);
		return Point(XY.X, XY.Y, H);
	};
	const double Inner = S.CoreRadius + S.SpiralGap, Outer = Inner + S.SpiralWidth;
	// 구면 중력 방향으로 기운 캡슐이 경사판 아래까지 빠질 수 있도록 나팔 벽을1m 더 물린다.
	const double FlareInner = Outer + 200.0;
	const double End = FMath::Fmod(S.ThetaAtZero + S.TopH / TurnRise * 360.0, 360.0);
	const double Slot = 900.0 / TurnRise * 360.0;
	TArray<double> Angles = {0.0, 360.0, End, FMath::Fmod(End - Slot + 360.0, 360.0), FMath::Fmod(End + 5.0, 360.0)};
	TArray<FPart> Parts;
	// 나팔은 기둥과 하나의 몸체다. 마지막 나선 구간의 머리 공간은 두께 있는 개구부로 비운다.
	FPart Dish = BastionVolume(S.Id + TEXT("_Dish"), BastionBreaks({0.0, S.CoreRadius, FlareInner, S.DishRadius}, 500.0),
		BastionBreaks(Angles, 7.5), true, [&](const FVector2D* P, FBastionColumn& C)
	{
		const FVector2D M = (P[0] + P[2]) * 0.5;
		const double Behind = FMath::Fmod(End - M.Y + 360.0, 360.0);
		if (M.X > S.CoreRadius && M.X < FlareInner && (Behind < Slot || Behind > 355.0)) return;
		BastionAddInterval(C, P, [&](FVector2D Q)
		{
			if (M.X < S.CoreRadius) return S.BottomH - WallSink;
			if (M.X < FlareInner) return S.TopH - S.DishRimThickness;
			const double T = FMath::Clamp((Q.X - Outer - 50.0) / (S.DishRadius - Outer - 50.0), 0.0, 1.0);
			return FMath::Lerp(S.DishFlareStartH, S.TopH - S.DishRimThickness, FMath::Pow(T, 1.0 / 1.8));
		}, [&](FVector2D) { return S.TopH; });
	}, [&](FVector P) { return Real(P.X, P.Y, P.Z); });
	Dish.bCoarse = true;
	Parts.Add(MoveTemp(Dish));
	TArray<double> Cuts = {S.BottomH}; Cuts.Append(S.SplitH); Cuts.Add(S.TopH);
	for (int32 I = 0; I + 1 < Cuts.Num(); ++I)
	{
		const double A = Cuts[I], B = Cuts[I + 1] + (I + 2 == Cuts.Num() ? TurnRise * 5.0 / 360.0 : 0.0);
		FPart Ramp = BastionVolume(FString::Printf(TEXT("%s_Ramp%d"), *S.Id, I), BastionBreaks({Inner, Outer}, 450.0),
			BastionBreaks(I + 2 == Cuts.Num() ? TArray<double>{A, S.TopH, B} : TArray<double>{A, B}, TurnRise / 72.0), false, [&](const FVector2D* P, FBastionColumn& C)
		{
			BastionAddInterval(C, P, [](FVector2D) { return -BridgeThickness; }, [](FVector2D) { return 0.0; });
		}, [&](FVector P) { return Real(P.X, S.ThetaAtZero + P.Y / TurnRise * 360.0, FMath::Min(P.Y, S.TopH) + P.Z); }, true);
		// 분할 메시 사이 단면은 공유하고, 실제 나선의 두 끝만 닫는다.
		for (bool Top : {false, true})
		{
			if ((!Top && I != 0) || (Top && I + 2 != Cuts.Num())) continue;
			const double Parameter = Top ? B : A, H = FMath::Min(Parameter, S.TopH), Deg = S.ThetaAtZero + Parameter / TurnRise * 360.0;
			const FVector Normal = Real(Inner, Deg + (Top ? 0.01 : -0.01), H) - Real(Inner, Deg, H);
			const TArray<double> R = BastionBreaks({Inner, Outer}, 450.0);
			for (int32 K = 1; K < R.Num(); ++K)
				Ramp.Shell.AddQuad(Real(R[K - 1], Deg, H), Real(R[K], Deg, H), Real(R[K], Deg, H - BridgeThickness),
					Real(R[K - 1], Deg, H - BridgeThickness), Normal, false);
		}
		Parts.Add(MoveTemp(Ramp));
	}
	return Parts;
}

FPart BuildBastionAbyss(const FAbyss& A, FLNPFixtureMesh& Crust, int32& Cut)
{
	// 원래 지각 절단 경계는 그대로 유지한다. 외피는 그 48각형과 같은 단면을 사용한다.
	TArray<FPlane> Planes;
	const FVector Inside = RadialUnnormalized(A.X, A.Y);
	for (int32 I = 0; I < 48; ++I)
	{
		const FVector2D P = FVector2D(A.X, A.Y) + Polar(A.RadiusArc, I * 7.5);
		const FVector2D Q = FVector2D(A.X, A.Y) + Polar(A.RadiusArc, (I + 1) * 7.5);
		Planes.Add(CenterPlane(RadialUnnormalized(P.X, P.Y), RadialUnnormalized(Q.X, Q.Y), Inside));
	}
	Cut += Crust.RemoveInsideConvex(Planes);
	TArray<double> Radii = {0.0, A.RadiusArc, A.RadiusArc + WallThickness, A.CavernRadiusArc, A.CavernRadiusArc + WallThickness};
	TArray<double> Angles = {0.0, 360.0};
	for (const FVector& Pillar : A.Pillars)
	{
		const double Half = Pillar.Z * Scale(A.FloorH), HalfAngle = FMath::RadiansToDegrees(FMath::Atan(Half / Pillar.X));
		Radii.Add(Pillar.X - Half); Radii.Add(Pillar.X + Half);
		Angles.Add(Pillar.Y - HalfAngle); Angles.Add(Pillar.Y + HalfAngle);
	}
	FPart Out = BastionVolume(A.Id, BastionBreaks(Radii, 600.0), BastionBreaks(Angles, 7.5), true,
		[&](const FVector2D* P, FBastionColumn& C)
	{
		const double R = (P[0].X + P[2].X) * 0.5;
		auto Add = [&](double Lo, double Hi) { BastionAddInterval(C, P, [Lo](FVector2D) { return Lo; }, [Hi](FVector2D) { return Hi; }); };
		Add(A.FloorH - 300.0, A.FloorH);
		// 구면 지각 아래의 마감판은 묻는다. 같은 높이의 거친 삼각판은 지각 위로 튀어나와 Nav 캡슐을 막는다.
		if (R > A.RadiusArc) BastionAddInterval(C, P, [&](FVector2D) { return A.CeilingH; }, [&](FVector2D Q)
		{
			return -WallSink * FMath::Clamp((Q.X - A.RadiusArc) / WallThickness, 0.0, 1.0);
		});
		if (R > A.CavernRadiusArc) Add(A.FloorH, A.CeilingH);
		for (const FVector& Pillar : A.Pillars)
		{
			const double Half = Pillar.Z * Scale(A.FloorH), HalfAngle = FMath::RadiansToDegrees(FMath::Atan(Half / Pillar.X));
			if (FMath::Abs(R - Pillar.X) < Half && FMath::Abs(FMath::FindDeltaAngleDegrees(Pillar.Y, (P[0].Y + P[2].Y) * 0.5)) < HalfAngle)
				Add(A.FloorH, A.CeilingH);
		}
	}, [&](FVector P)
	{
		const FVector2D XY = FVector2D(A.X, A.Y) + Polar(P.X, P.Y);
		return Point(XY.X, XY.Y, P.Z);
	});
	// 지각에 묻힌 윗마감과 바깥 벽은 Blocker다. 공동 바닥만 Support로 남긴다.
	FLNPFixtureMesh Floor;
	for (const FIntVector& T : Out.Floor.Triangles)
	{
		const FVector P = Out.Floor.Positions[T.X], Q = Out.Floor.Positions[T.Y], R = Out.Floor.Positions[T.Z];
		const double H = CrustRadius - ((P + Q + R) / 3.0).Length();
		FLNPFixtureMesh& Destination = H < A.FloorH + 100.0 ? Floor : Out.Shell;
		Destination.Triangles.Emplace(Destination.AddVertex(P), Destination.AddVertex(Q), Destination.AddVertex(R));
	}
	Out.Floor = MoveTemp(Floor);
	Out.bCoarse = true;
	return Out;
}

FPart BuildBastionRing(const FTieredRing& Ring)
{
	TArray<double> Radii, Angles = {0.0, 360.0};
	const double MidR = (Ring.Bands[0].R0 + Ring.Bands.Last().R1) * 0.5;
	for (const FRingBand& B : Ring.Bands) { Radii.Add(B.R0); Radii.Add(B.R1); }
	for (const FRingGate& G : Ring.Gates)
	{
		const double Half = FMath::RadiansToDegrees(G.Width * 0.5 / MidR);
		Angles.Add(FMath::Fmod(G.Deg - Half + 360.0, 360.0));
		Angles.Add(FMath::Fmod(G.Deg + Half + 360.0, 360.0));
	}
	for (const FRingStair& S : Ring.Stairs)
	{
		const FVector2D R = StairRadii(S), Span = StairSpanDeg(S);
		Radii.Add(R.X); Radii.Add(R.Y);
		for (double A : {S.StartDeg, S.StartDeg + Span.X, S.StartDeg + Span.X + Span.Y})
			Angles.Add(FMath::Fmod(A + 360.0, 360.0));
	}
	Radii = BastionBreaks(Radii, 600.0);
	Angles = BastionBreaks(Angles, FMath::RadiansToDegrees(500.0 / Radii.Last()));
	FPart Out = BastionVolume(Ring.Id, Radii, Angles, true, [&](const FVector2D* P, FBastionColumn& C)
	{
		const FVector2D M = (P[0] + P[2]) * 0.5;
		double Base = -WallSink;
		for (const FRingGate& G : Ring.Gates)
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(G.Deg, M.Y)) < FMath::RadiansToDegrees(G.Width * 0.5 / MidR)) Base = G.Height;
		for (const FRingBand& B : Ring.Bands)
			if (M.X > B.R0 && M.X < B.R1) BastionAddInterval(C, P, [Base](FVector2D) { return Base; }, [&B](FVector2D) { return B.H; });
		for (const FRingStair& S : Ring.Stairs)
		{
			const FVector2D R = StairRadii(S), Span = StairSpanDeg(S);
			const double A = FMath::Fmod(M.Y - S.StartDeg + 360.0, 360.0);
			if (M.X < R.X || M.X > R.Y || A > Span.X + Span.Y) continue;
			BastionAddInterval(C, P, [Base](FVector2D) { return Base; }, [&](FVector2D Q)
			{
				const double T = FMath::Clamp((Q.Y - S.StartDeg) / Span.X, 0.0, 1.0);
				return FMath::Lerp(S.H0, S.H1, T);
			});
		}
	}, [](FVector P) { return Point(PolarH(P.X, P.Y, P.Z)); });
	Out.bCoarse = Ring.bCoarse;
	return Out;
}

FPart BuildBastionBuilding(const FBuilding& B)
{
	const double HX = B.SizeX * 0.5, HY = B.SizeY * 0.5, Roof = RoofH(B);
	const double Stub = B.Id.StartsWith(TEXT("S")) ? 150.0 : RuinStubLength;
	const double Length = B.StoryHeight / FMath::Tan(FMath::DegreesToRadians(RampSlopeDeg));
	TArray<double> X = {-HX - WallThickness, -HX, 0.0, HX, HX + WallThickness};
	TArray<double> Y = {-HY - WallThickness, -HY, 0.0, HY, HY + WallThickness};
	TArray<FBox2D> Collapse;
	for (int32 S : B.Collapsed)
	{
		FBox2D H;
		switch (S)
		{
		case 0: H = FBox2D({-HX, 0.0}, {-HX + B.SizeX * 0.35, HY}); break;
		case 1: H = FBox2D({HX - B.SizeX * 0.35, 0.0}, {HX, HY}); break;
		case 2: H = FBox2D({0.0, -HY}, {HX, -HY + B.SizeY * 0.35}); break;
		default: H = FBox2D({0.0, HY - B.SizeY * 0.35}, {HX, HY}); break;
		}
		Collapse.Add(H); X.Add(H.Min.X); X.Add(H.Max.X); Y.Add(H.Min.Y); Y.Add(H.Max.Y);
	}
	for (const FDoor& D : B.Doors)
	{
		TArray<double>& Axis = D.Side < 2 ? Y : X;
		Axis.Add(D.Offset - D.Width * 0.5); Axis.Add(D.Offset + D.Width * 0.5);
	}
	X.Append({-HX + Stub, HX - Stub}); Y.Append({-HY + Stub, HY - Stub});
	X.Append({-HX + 200.0, -HX + 200.0 + Length, HX - 200.0, HX - 200.0 - Length});
	// 램프 윗면과 옆 슬래브 밑면이 만나는 위치도 공유 분할한다.
	const double SlabCrossing = Length * (1.0 - SlabThickness / B.StoryHeight);
	X.Append({-HX + 200.0 + SlabCrossing, HX - 200.0 - SlabCrossing});
	X.Append({-HX + 200.0 + Length - 200.0, -HX + 200.0 + Length + 200.0,
		HX - 200.0 - Length - 200.0, HX - 200.0 - Length + 200.0});
	Y.Append({-HY + B.RampWidth, HY - B.RampWidth});
	X = BastionBreaks(X, 600.0); Y = BastionBreaks(Y, 600.0);
	FPart Out = BastionVolume(B.Id, X, Y, false, [&](const FVector2D* P, FBastionColumn& C)
	{
		const FVector2D M = (P[0] + P[2]) * 0.5;
		auto Add = [&](double Lo, double Hi) { BastionAddInterval(C, P, [Lo](FVector2D) { return Lo; }, [Hi](FVector2D) { return Hi; }); };
		const bool Inside = FMath::Abs(M.X) < HX && FMath::Abs(M.Y) < HY;
		if (Inside)
		{
			for (int32 Story = 1; Story <= B.Stories; ++Story)
			{
				const bool Even = (Story - 1) % 2 == 0;
				const double Low = Even ? -HX + 200.0 : HX - 200.0;
				const double High = Low + (Even ? Length : -Length);
				const bool RampHole = Story < B.Stories && M.X > FMath::Min(Low, High) && M.X < FMath::Max(Low, High)
					&& (Even ? M.Y < -HY + B.RampWidth : M.Y > HY - B.RampWidth);
				const bool Broken = Story >= B.Stories - (B.Stories >= 3 ? 1 : 0)
					&& Collapse.ContainsByPredicate([&](const FBox2D& H) { return H.IsInside(M); });
				// 폐허 개구부를 남기되 경사로 종료점에서 온전한 중앙 바닥으로 돌아설400cm 띠는 보존한다.
				const bool ExitLanding = Story < B.Stories && FMath::Abs(M.X - High) < 200.0
					&& (Even ? M.Y < 0.0 : M.Y > 0.0);
				if (!RampHole && (!Broken || ExitLanding)) Add(Story * B.StoryHeight - SlabThickness, Story * B.StoryHeight);
			}
			for (int32 Story = 0; Story + 1 < B.Stories; ++Story)
			{
				const bool Even = Story % 2 == 0;
				const double Low = Even ? -HX + 200.0 : HX - 200.0, High = Low + (Even ? Length : -Length);
				if (M.X <= FMath::Min(Low, High) || M.X >= FMath::Max(Low, High)
					|| !(Even ? M.Y < -HY + B.RampWidth : M.Y > HY - B.RampWidth)) continue;
				BastionAddInterval(C, P, [&, Story](FVector2D) { return Story == 0 ? -BuryDepth : Story * B.StoryHeight - SlabThickness; },
					[&, Story, Low, High](FVector2D Q) { return (Story + (Q.X - Low) / (High - Low)) * B.StoryHeight; });
			}
		}
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const bool WallCell = Side == 0 ? M.X < -HX : Side == 1 ? M.X > HX : Side == 2 ? M.Y < -HY : M.Y > HY;
			if (!WallCell) continue;
			const double Along = Side < 2 ? M.Y : M.X, Half = Side < 2 ? HY : HX;
			if (B.Collapsed.Contains(Side))
			{
				if (FMath::Abs(Along) > Half - Stub) Add(-BuryDepth, Roof * 0.55);
				continue;
			}
			double Bottom = -BuryDepth;
			for (const FDoor& D : B.Doors) if (D.Side == Side && FMath::Abs(Along - D.Offset) < D.Width * 0.5) Bottom = D.Height;
			Add(Bottom, Roof);
		}
	}, [&](FVector P) { return FVector(P.X, P.Y, FloorZ(P.X, P.Y, CrustRadius - B.BaseH) + P.Z); });
	Out.Placement = FTransform(Rotation(B.X, B.Y) * FQuat(FVector::UpVector, FMath::DegreesToRadians(B.Yaw)), Point(B.X, B.Y, B.BaseH));
	return Out;
}

/** 에셋을 갱신하기 전에 닫힌 외피와 역할 정점의 인코딩 범위를 검사한다. 나선의 높이 분할은 함께 검사한다. */
bool ValidateBastionParts(const TArray<FPart>& Parts)
{
	TMap<FString, TArray<const FPart*>> Groups;
	for (const FPart& P : Parts) Groups.FindOrAdd(P.Id.StartsWith(TEXT("P01_Ramp")) ? TEXT("P01_Ramps") : P.Id).Add(&P);
	int32 TotalTriangles = 0, Errors = 0;
	for (const auto& Group : Groups)
	{
		TMap<FIntVector, int32> Welded;
		TArray<FVector> WeldPositions;
		TMap<FIntPoint, FIntPoint> Edges;
		TSet<FIntVector> Faces;
		int32 Degenerate = 0, Duplicates = 0, RangeErrors = 0, GroupTriangles = 0;
		for (const FPart* P : Group.Value)
		{
			for (const FLNPFixtureMesh* M : {&P->Floor, &P->Shell})
			{
				TArray<int32> Indices;
				for (FVector V : M->Positions)
				{
					if (M == &P->Floor)
					{
						const FVector World = P->Placement.TransformPosition(V);
						if (World.GetAbsMax() > 32767.0) ++RangeErrors;
					}
					const FIntVector Key(FMath::RoundToInt(V.X * 100.0), FMath::RoundToInt(V.Y * 100.0), FMath::RoundToInt(V.Z * 100.0));
					const int32 Next = Welded.Num();
					if (!Welded.Contains(Key)) WeldPositions.Add(V);
					Indices.Add(Welded.FindOrAdd(Key, Next));
				}
				for (const FIntVector& T : M->Triangles)
				{
					++GroupTriangles;
					int32 V[] = {Indices[T.X], Indices[T.Y], Indices[T.Z]};
					if (V[0] == V[1] || V[1] == V[2] || V[2] == V[0]) { ++Degenerate; continue; }
					int32 Sorted[] = {V[0], V[1], V[2]};
					if (Sorted[0] > Sorted[1]) Swap(Sorted[0], Sorted[1]);
					if (Sorted[1] > Sorted[2]) Swap(Sorted[1], Sorted[2]);
					if (Sorted[0] > Sorted[1]) Swap(Sorted[0], Sorted[1]);
					const FIntVector Face(Sorted[0], Sorted[1], Sorted[2]);
					if (Faces.Contains(Face)) ++Duplicates;
					Faces.Add(Face);
					for (int32 E = 0; E < 3; ++E)
					{
						const int32 A = V[E], B = V[(E + 1) % 3];
						FIntPoint& Count = Edges.FindOrAdd({FMath::Min(A, B), FMath::Max(A, B)}, FIntPoint::ZeroValue);
						++Count.X; Count.Y += A < B ? 1 : -1;
					}
				}
			}
		}
		int32 Boundary = 0, NonManifold = 0, Winding = 0;
		for (const auto& E : Edges)
		{
			Boundary += E.Value.X == 1; NonManifold += E.Value.X > 2;
			Winding += E.Value.X == 2 && E.Value.Y != 0;
			if (E.Value.X == 1) UE_LOG(LogLNPSliceOctant, Display, TEXT("[BastionBoundary] %s %s -> %s"),
				*Group.Key, *WeldPositions[E.Key.X].ToString(), *WeldPositions[E.Key.Y].ToString());
		}
		TotalTriangles += GroupTriangles;
		const bool Passed = Boundary + NonManifold + Winding + Degenerate + Duplicates + RangeErrors == 0;
		if (!Passed) ++Errors;
		UE_LOG(LogLNPSliceOctant, Display, TEXT("[BastionLowPoly] %s tris=%d boundary=%d nonManifold=%d winding=%d degenerate=%d duplicate=%d int16=%d"),
			*Group.Key, GroupTriangles, Boundary, NonManifold, Winding, Degenerate, Duplicates, RangeErrors);
	}
	UE_LOG(LogLNPSliceOctant, Display, TEXT("[BastionLowPoly] preflight groups=%d triangles=%d errors=%d (self intersections require collision validation)"), Groups.Num(), TotalTriangles, Errors);
	return Errors == 0;
}

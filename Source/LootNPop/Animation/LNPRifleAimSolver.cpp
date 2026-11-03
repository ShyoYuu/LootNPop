// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/LNPRifleAimSolver.h"

#include "HitDetection/LNPFireGeometry.h"
#include "HitDetection/LNPProjectileMotion.h"

FLNPRifleAimSolver::FLNPRifleAimSolver(const FVector& InPivot, const FTransform& MuzzleWorld, const FVector& InAimTarget, const FVector& InViewDirection,
	const FVector& InMuzzleOffsetWorld, const FVector& InActorLocation, const ULNPMassWorldCollisionSubsystem* InWorldCollision)
	: Pivot(InPivot)
	, Lever(MuzzleWorld.GetLocation() - InPivot)
	, Barrel(MuzzleWorld.GetUnitAxis(EAxis::X))
	, AimTarget(InAimTarget)
	, ViewDirection(InViewDirection)
	, MuzzleOffsetWorld(InMuzzleOffsetWorld)
	, ActorLocation(InActorLocation)
	, WorldCollision(InWorldCollision)
{
}

FVector FLNPRifleAimSolver::SpawnPosition(const FQuat& Rotation) const
{
	return LNPProjectileMotion::ClipMuzzleToWorld(WorldCollision, ActorLocation, Pivot + Rotation.RotateVector(Lever) + MuzzleOffsetWorld);
}

FVector FLNPRifleAimSolver::AimDirection(const FQuat& Rotation) const
{
	return LNPFireGeometry::ResolveAimDirection(SpawnPosition(Rotation), ViewDirection, &AimTarget);
}

FQuat FLNPRifleAimSolver::Solve() const
{
	// 피벗에서 표적까지의 구와 총열 반직선의 교점을 회전시킨다. 총구 이동까지 함께 푼다.
	// 평행 시선 폴백으로 시작하고, 보정 후에도 실제 발사 게이트를 통과하는 표적 해만 채택한다.
	FQuat Correction = FQuat::FindBetweenNormals(Barrel, ViewDirection);
	const FVector ToTarget = AimTarget - Pivot - MuzzleOffsetWorld;
	const double Projection = FVector::DotProduct(Lever, Barrel);
	const double Discriminant = Projection * Projection + ToTarget.SizeSquared() - Lever.SizeSquared();
	if (Discriminant >= 0.0)
	{
		const double AlongBarrel = FMath::Sqrt(Discriminant) - Projection;
		if (AlongBarrel >= 0.0 && !ToTarget.IsNearlyZero())
		{
			const FQuat Candidate = FQuat::FindBetweenNormals((Lever + AlongBarrel * Barrel).GetSafeNormal(), ToTarget.GetSafeNormal());
			if (Candidate.RotateVector(Barrel).Equals(AimDirection(Candidate), 1.e-7))
				Correction = Candidate;
		}
	}
	// 벽에 의해 발사 원점이 잘린 경우도 같은 발사 기하로 다시 푼다.
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		const FVector Current = Correction.RotateVector(Barrel);
		const FVector Desired = AimDirection(Correction);
		if (Current.Equals(Desired, 1.e-7))
			break;
		Correction = (FQuat::FindBetweenNormals(Current, Desired) * Correction).GetNormalized();
	}
	return Correction;
}

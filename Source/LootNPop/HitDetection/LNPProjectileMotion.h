// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPSurfaceCacheSubsystem;
class ULNPMassWorldCollisionSubsystem;
struct FLNPWorldHit;

/**
 * 발사체 궤적의 **단일 정의** — 실제 비행(Mass 이동 Processor)과 예상 궤도(ADS 가이드)가
 * 반드시 같은 식을 봐야 한다. 같은 판정식을 두 곳에 적으면 언젠가 갈라진다
 * (→ TechDesign_HitDetection.md §7.5·§7.6).
 */
namespace LNPProjectileMotion
{
	/**
	 * 발사체 1스텝 적분. 구 내벽 세계이므로 "아래"는 원점에서 바깥쪽 방향이다.
	 *
	 * 속도 Verlet을 쓴다. 명시적 오일러(P += V*Dt)는 같은 구간을 몇 스텝으로 쪼개느냐에 따라
	 * 궤적이 달라져, 예측 스텝(고정 1/30초)과 실행 스텝(프레임 Dt)이 다른 순간 가이드와
	 * 실탄이 갈린다. Verlet은 상수 중력에서 스텝 분할에 불변이다.
	 *
	 * GravityAccel이 0이면 결과가 등속 직선과 정확히 같으므로 타입 분기가 필요 없다.
	 */
	FORCEINLINE void Step(FVector& Pos, FVector& Vel, const float GravityAccel, const float Dt)
	{
		if (GravityAccel <= 0.f)
		{
			Pos += Vel * Dt;
			return;
		}

		const FVector G = Pos.GetSafeNormal() * GravityAccel;
		Pos += Vel * Dt + G * (0.5f * Dt * Dt);
		Vel += G * Dt;
	}

	/**
	 * 이 지점이 지표면 바깥(= 지면을 통과)인가 — **착탄 판정의 단일 정의**.
	 * 구 내벽 세계라 원점에서 표면보다 먼 쪽이 지면 너머다.
	 * 실제 종말 판정(ULNPProjectileHitDetectionProcessor)과 예상 궤도가 같은 함수를 봐야
	 * 가이드가 가리키는 곳과 폭발하는 곳이 어긋나지 않는다.
	 * 베이킹 미완이면 false(= 아직 지면이 없음)를 돌려준다.
	 */
	bool IsUnderSurface(const ULNPSurfaceCacheSubsystem& SurfaceCache, const FVector& Pos);

	/**
	 * 예측 궤적의 표본 수 — **고정**이다. ADS 가이드 Niagara가 이 수만큼 리본 정점을 버스트하고
	 * ExecIndex로 배열을 읽으므로, 길이를 런타임에 바꾸면 Niagara 쪽에 배열 길이 조회와
	 * 여분 정점 숨기기가 따라붙는다. 대신 궤적을 이 개수로 **재표집**해 그 복잡도를 없앴다.
	 */
	inline constexpr int32 ArcPointCount = 64;

	/** 궤적 시뮬레이션의 고정 시간 간격 (초). */
	inline constexpr float ArcStepSeconds = 1.f / 30.f;

	/** 시뮬레이션 스텝 상한 — 지면에도 닿지 않고 수명도 안 끝나는 궤적의 안전판. */
	inline constexpr int32 MaxArcSimSteps = 256;

	/**
	 * 발사체 궤적을 지표면과 만날 때까지(또는 수명이 다할 때까지) 시뮬레이션한 뒤,
	 * 호 길이를 따라 **정확히 ArcPointCount개**로 고르게 재표집해 돌려준다.
	 *
	 * 지면 판정은 IsUnderSurface — 실제 착탄 판정과 같은 함수다.
	 * 마지막 표본이 곧 착탄 예상 지점이므로 별도로 넘길 필요가 없다.
	 *
	 * @param MaxSeconds  시뮬레이션 상한 (보통 무기의 ProjectileLifetime).
	 * @param OutPoints   재표집된 궤적. 성공 시 항상 ArcPointCount개다.
	 * @return SurfaceCache 베이킹이 끝나지 않았으면 false — 호출자는 가이드를 숨겨야 한다.
	 */
	bool PredictArc(const ULNPSurfaceCacheSubsystem& SurfaceCache,
		const FVector& Start, const FVector& Velocity, float GravityAccel,
		float MaxSeconds, TArray<FVector>& OutPoints);

	/**
	 * 투사체 월드 충돌을 exact 경로(LNPWorldExact)로 판정하는가 — CVar `LNP.SurfaceNav.ProjectileExact`.
	 * production 기본값은 legacy(IsUnderSurface)다. audit·8-slot oracle을 통과하기 전에는 바꾸지 않는다(D-036).
	 * 모든 스레드.
	 */
	bool UseExactWorldCollision();

	/**
	 * exact 경로의 **월드 착탄 판정 단일 정의** — 서버 투사체·클라이언트 Ghost·예상 궤도가 같은 함수를 본다.
	 * From→To 선분을 LNPWorldExact로 line trace한다. 투사체는 월드 판정용 반지름이 없고, HitRadius는
	 * 캐릭터 캡슐을 부풀리는 값이라 지형에 쓰면 턱·모서리에 먼저 걸린다.
	 * 모든 스레드(Mass worker는 동기 query만, D-025).
	 */
	bool TraceWorld(const ULNPMassWorldCollisionSubsystem& WorldCollision, const FVector& From, const FVector& To, FLNPWorldHit& OutHit);

	/**
	 * PredictArc의 exact 판. 스텝마다 TraceWorld로 선분을 검사하고 첫 hit의 ImpactPoint에서 끝낸다.
	 * 월드가 아직 로드되지 않았으면 수명 끝까지 뻗는다 — 준비 여부는 호출자가 가린다.
	 */
	void PredictArcExact(const ULNPMassWorldCollisionSubsystem& WorldCollision,
		const FVector& Start, const FVector& Velocity, float GravityAccel,
		float MaxSeconds, TArray<FVector>& OutPoints);
}

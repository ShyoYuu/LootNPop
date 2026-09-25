// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

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
	 * **월드 착탄 판정의 단일 정의** — 서버 투사체·클라이언트 Ghost·예상 궤도가 같은 함수를 본다.
	 * From→To 선분을 LNPWorldExact로 line trace한다. 투사체는 월드 판정용 반지름이 없고, HitRadius는
	 * 캐릭터 캡슐을 부풀리는 값이라 지형에 쓰면 턱·모서리에 먼저 걸린다.
	 * 모든 스레드(Mass worker는 동기 query만, D-025).
	 */
	bool TraceWorld(const ULNPMassWorldCollisionSubsystem& WorldCollision, const FVector& From, const FVector& To, FLNPWorldHit& OutHit);

	/**
	 * **world/entity earliest hit의 단일 정의** — 이번 프레임 선분의 월드 hit를 먼저 구하고 캐릭터 판정 선분의 끝을
	 * 거기로 자른다. 잘린 선분 위의 캐릭터 hit는 월드 hit보다 항상 이르므로, 캐릭터 판정이 먼저 맞으면 그것이
	 * earliest hit이고 아니면 월드 hit이다 — 섬 측벽·동굴 벽 뒤의 적은 맞지 않는다.
	 * WorldCollision이 null(서브시스템이 없는 월드 타입)이면 월드 판정 없이 To를 돌려준다. 모든 스레드.
	 */
	FVector ClipSegmentToWorld(const ULNPMassWorldCollisionSubsystem* WorldCollision, const FVector& From, const FVector& To,
		FLNPWorldHit& OutHit);

	/**
	 * envelope 안전망의 여유(cm). envelope 밖에서는 중력(바깥쪽)과 반지름 방향 속도가 모두 바깥을 향해 돌아올 수 없으므로,
	 * 여유는 판정 경계를 geometry에서 떼어 놓는 용도뿐이다.
	 */
	inline constexpr float WorldEnvelopeMargin = 500.f;

	/**
	 * **최외곽 반지름 안전망** — 월드 판정이 빗나가 지각 밖으로 나간 탄인가(RuntimeCollision.md).
	 * EnvelopeRadius는 ULNPMassWorldCollisionSubsystem::GetWorldEnvelopeRadius다. 0(source 없음)이면 항상 false.
	 * 모든 스레드.
	 */
	FORCEINLINE bool IsOutsideWorldEnvelope(const float EnvelopeRadius, const FVector& Pos)
	{
		return EnvelopeRadius > 0.f && Pos.SizeSquared() > FMath::Square(static_cast<double>(EnvelopeRadius + WorldEnvelopeMargin));
	}

	/**
	 * 발사체 궤적을 월드와 만날 때까지(또는 수명이 다할 때까지) 시뮬레이션한 뒤,
	 * 호 길이를 따라 **정확히 ArcPointCount개**로 고르게 재표집해 돌려준다.
	 *
	 * 스텝마다 TraceWorld로 선분을 검사하고 첫 hit의 ImpactPoint에서 끝낸다 — 실탄이 프레임마다
	 * PreviousPos→현재 위치를 검사하는 것과 같은 함수다. 실탄과 같이 envelope 밖으로 나간 스텝에서도 끝낸다.
	 * 마지막 표본이 곧 착탄 예상 지점이므로 별도로 넘길 필요가 없다.
	 * 월드가 아직 로드되지 않았으면 수명 끝까지 뻗는다 — 준비 여부는 호출자가 가린다.
	 *
	 * @param MaxSeconds  시뮬레이션 상한 (보통 무기의 ProjectileLifetime).
	 * @param OutPoints   재표집된 궤적. 항상 ArcPointCount개다.
	 */
	void PredictArc(const ULNPMassWorldCollisionSubsystem& WorldCollision,
		const FVector& Start, const FVector& Velocity, float GravityAccel,
		float MaxSeconds, TArray<FVector>& OutPoints);
}

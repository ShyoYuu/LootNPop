// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyUpDeviationProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPMassUtils.h"
#include "LootNPop.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassActorSubsystem.h"
#include "MassReplicationFragments.h"   // FMassNetworkIDFragment — 호스트/게스트 로그 대조용 공유 식별자

namespace
{
	TAutoConsoleVariable<int32> CVarUpDeviationCheck(
		TEXT("LNP.Enemy.UpDeviationCheck"), 0,
		TEXT("Measure the angle between each enemy entity's rotation Z axis and the sphere-centre up direction.\n")
		TEXT("Runs on the server and the client. Logs per-state maximum and sample count every window.\n")
		TEXT("  0: off (default)\n")
		TEXT("  1: on"),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarUpDeviationWindow(
		TEXT("LNP.Enemy.UpDeviationWindowSeconds"), 5.f,
		TEXT("Report window of LNP.Enemy.UpDeviationCheck in seconds."),
		ECVF_Default);

	const TCHAR* CategoryName(const int32 Index)
	{
		static const TCHAR* Names[] = { TEXT("Dying"), TEXT("Flying"), TEXT("Airborne"), TEXT("Promoted"),
			TEXT("Dormant"), TEXT("Wander"), TEXT("Grounded") };
		return Names[Index];
	}
}

ULNPEnemyUpDeviationProcessor::ULNPEnemyUpDeviationProcessor()
	: EnemyQuery(*this)
{
	// 게스트의 복제 회전(Slerp 보간·배회 복원)도 확인 대상이므로 서버·클라이언트 모두 돈다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	// 이동·보간이 끝난 뒤의 프레임 최종 자세를 잰다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPEnemyUpDeviationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// 벽보행 개체의 회전 Z는 표면 법선이라 구 중심 방향과 다른 것이 정상이다.
	EnemyQuery.AddTagRequirement<FLNPEnemyCrawlTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyUpDeviationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (CVarUpDeviationCheck.GetValueOnGameThread() == 0)
	{
		// 껐다 켜면 새 창으로 시작한다.
		WindowStartTime = -1.0;
		return;
	}

	const UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
		return;

	const double Now = World->GetTimeSeconds();
	if (WindowStartTime < 0.0)
	{
		WindowStartTime = Now;
		for (FStat& Stat : Stats)
			Stat = FStat();
	}

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>        Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyVelocityFragment> Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const TConstArrayView<FLNPEnemyIdleFragment>     Idles      = Ctx.GetFragmentView<FLNPEnemyIdleFragment>();
		const TConstArrayView<FMassActorFragment>        ActorFrags = Ctx.GetFragmentView<FMassActorFragment>();
		const TConstArrayView<FMassNetworkIDFragment>    NetIDs     = Ctx.GetFragmentView<FMassNetworkIDFragment>();

		const bool bDying   = Ctx.DoesArchetypeHaveTag<FLNPEnemyDyingTag>();
		const bool bFlying  = Ctx.DoesArchetypeHaveTag<FLNPEnemyFlyingTag>();
		const bool bDormant = Ctx.DoesArchetypeHaveTag<FLNPEnemyDormantTag>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FTransform& Transform = Transforms[i].GetTransform();
			// 판정 경로와 같은 규약: 구 내벽이라 Up은 월드 중심(원점)을 향한다.
			const FVector RadialUp = (-Transform.GetLocation()).GetSafeNormal();
			const FVector EntityUp = LNPMass::GetEntityUp(Transform);
			const double Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(RadialUp, EntityUp), -1.0, 1.0)));

			ECategory Category = ECategory::Grounded;
			if (bDying)
				Category = ECategory::Dying;
			else if (bFlying)
				Category = ECategory::Flying;
			else if (Velocities.IsValidIndex(i) && !Velocities[i].Velocity.IsNearlyZero())
				Category = ECategory::Airborne;
			else if (ActorFrags.IsValidIndex(i) && ActorFrags[i].Get() != nullptr)
				Category = ECategory::Promoted;
			else if (bDormant)
				Category = ECategory::Dormant;
			else if (Idles.IsValidIndex(i) && Idles[i].WanderSegment.bValid)
				Category = ECategory::Wander;

			FStat& Stat = Stats[static_cast<int32>(Category)];
			++Stat.Samples;
			if (Deg > Stat.MaxDeg)
			{
				Stat.MaxDeg = Deg;
				Stat.MaxNetID = NetIDs.IsValidIndex(i) ? NetIDs[i].NetID.GetValue() : 0u;
			}
		}
	});

	const double Window = FMath::Max(0.1, static_cast<double>(CVarUpDeviationWindow.GetValueOnGameThread()));
	if (Now - WindowStartTime < Window)
		return;

	const TCHAR* Role = LNPMass::IsClientWorld(EntityManager) ? TEXT("Client") : TEXT("Server");
	for (int32 Index = 0; Index < static_cast<int32>(ECategory::Num); ++Index)
	{
		const FStat& Stat = Stats[Index];
		if (Stat.Samples == 0)
			continue;
		UE_LOG(LogLootNPop, Log, TEXT("EnemyUpDeviation role=%s window=%.1fs state=%s samples=%lld maxDeg=%.4f netid=%u"),
			Role, Now - WindowStartTime, CategoryName(Index), Stat.Samples, Stat.MaxDeg, Stat.MaxNetID);
	}

	WindowStartTime = Now;
	for (FStat& Stat : Stats)
		Stat = FStat();
}

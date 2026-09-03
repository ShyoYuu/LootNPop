// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Debug/LNPNetBudgetDebug.h"

#include "LootNPop.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "LootPod/LNPLootPod.h"
#include "LootDice/LNPLootDice.h"

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringBuilder.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	TAutoConsoleVariable<float> CVarNetBudget(
		TEXT("LNP.Net.Budget"),
		0.f,
		TEXT("Log per-connection send rate and the object counts behind it, every N seconds. Server only.\n")
		TEXT("  0    : off (default)\n")
		TEXT("  3    : log every 3 seconds\n")
		TEXT("Pair this with '-trace=net,frame' + 'NetTrace.SetTraceVerbosity 2' for a per-object byte breakdown."),
		ECVF_Cheat);

	// 절제 A/B — 같은 세션·같은 월드 상태에서 버블 갱신을 켰다 껐다 하며 재면
	// 두 상태의 송신량 차이가 곧 버블의 몫이다. 별도 실행 두 번보다 교란이 적다.
	TAutoConsoleVariable<int32> CVarNetAblate(
		TEXT("LNP.Net.Ablate"),
		0,
		TEXT("1 = toggle LNP.Net.FreezeBubble every logging period, so the log alternates between\n")
		TEXT("bubble-on and bubble-off samples. The difference is the Mass bubble's share of the send rate."),
		ECVF_Cheat);

	/** 서버가 스폰해 둔 Actor 수. 릴러번시가 아니라 '존재 수'다 — Actor 승격 비용의 분모. */
	template<typename TActor>
	int32 CountActors(const UWorld& World)
	{
		int32 Count = 0;
		for (TActorIterator<TActor> It(&World); It; ++It)
		{
			if (IsValid(*It))
			{
				++Count;
			}
		}
		return Count;
	}
}

TStatId ULNPNetBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPNetBudgetSubsystem, STATGROUP_Tickables);
}

void ULNPNetBudgetSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Period = CVarNetBudget.GetValueOnGameThread();
	if (Period <= 0.f)
	{
		TimeUntilNextLog = 0.f;
		return;
	}

	UWorld* World = GetWorld();
	// 넷 모드는 월드 초기화 중 신뢰할 수 없다(EngineAnalysis_MassReplication.md §7.10). 틱 시점은 안전하다.
	if (!World || World->GetNetMode() == NM_Client || World->GetNetMode() == NM_Standalone)
	{
		return;
	}

	TimeUntilNextLog -= DeltaTime;
	if (0.f < TimeUntilNextLog)
	{
		return;
	}
	TimeUntilNextLog = Period;

	if (!bReportedReplicationMode)
	{
		if (const UNetDriver* NetDriver = World->GetNetDriver())
		{
			bReportedReplicationMode = true;
			UE_LOG(LogLootNPop, Log, TEXT("[NetBudget] replication=%s  netServerMaxTickRate=%d"),
				NetDriver->GetReplicationSystem() ? TEXT("Iris") : TEXT("Legacy"),
				NetDriver->GetNetServerMaxTickRate());
		}
	}

	// Actor 수는 연결과 무관하므로 한 번만 센다.
	const int32 EnemyActors = CountActors<ALNPEnemyCharacter>(*World);
	const int32 PodActors   = CountActors<ALNPLootPod>(*World);
	const int32 DiceActors  = CountActors<ALNPLootDice>(*World);

	// 방금 끝난 구간 동안 실제로 적용돼 있던 절제 상태.
	IConsoleVariable* FreezeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Net.FreezeBubble"));
	const int32 FreezeState = FreezeVar ? FreezeVar->GetInt() : 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC || PC->IsLocalController())
		{
			continue;   // 리슨 호스트 자신은 복제 단계가 없어 잴 것이 없다
		}

		const UNetConnection* Conn = PC->GetNetConnection();
		if (!Conn)
		{
			continue;
		}

		// 이 연결의 버블을 찾는다 (버블 Owner = 해당 PlayerController).
		int32 BubbleTotal = 0;
		int32 BubbleTagged = 0;
		// 복제 LOD는 그 클라이언트의 뷰어 기준으로 계산된다. 뷰어에서 각 에이전트까지의 거리를 직접 재면
		// "컬 거리(ReplicationCullDistance)가 실제로 걸리고 있는가"를 추론 없이 판정할 수 있다.
		const APawn* ViewPawn = PC->GetPawn();
		const FVector ViewLoc = ViewPawn ? ViewPawn->GetActorLocation() : FVector::ZeroVector;
		int32 Within12k = 0, Within24k = 0, Beyond24k = 0;
		double MaxDist = 0.0;
		// 아키타입 구성은 TemplateID로 센다 — EnemyTypeTag는 채워지지 않을 수 있고(실측 enemy=0),
		// TemplateID는 엔진이 Add 시점에 무조건 넣으므로 신뢰할 수 있는 유일한 타입 표식이다.
		TMap<uint64, int32> CountByTemplate;
#if UE_REPLICATION_COMPILE_SERVER_CODE
		for (TActorIterator<ALNPMassClientBubbleInfo> BubbleIt(World); BubbleIt; ++BubbleIt)
		{
			if (BubbleIt->GetOwner() != PC)
			{
				continue;
			}
			for (const FLNPMassFastArrayItem& Item : BubbleIt->GetAgentSerializer().Bubble.GetAgents())
			{
				++BubbleTotal;
				BubbleTagged += Item.Agent.GetEnemyTypeTag().IsValid() ? 1 : 0;
				++CountByTemplate.FindOrAdd(Item.Agent.GetTemplateID().GetHash64());

				if (ViewPawn)
				{
					const double Dist = FVector::Dist(ViewLoc, Item.Agent.GetReplicatedPositionYawData().GetPosition());
					MaxDist = FMath::Max(MaxDist, Dist);
					if (Dist <= 12000.0)      { ++Within12k; }
					else if (Dist <= 24000.0) { ++Within24k; }
					else                      { ++Beyond24k; }
				}
			}
			break;
		}
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

		TStringBuilder<256> TemplateBreakdown;
		for (const TPair<uint64, int32>& Pair : CountByTemplate)
		{
			TemplateBreakdown.Appendf(TEXT(" %llx:%d"), Pair.Key, Pair.Value);
		}

		const int32 Cap = Conn->CurrentNetSpeed;
		const int32 Out = Conn->OutBytesPerSecond;
		UE_LOG(LogLootNPop, Log,
			TEXT("[NetBudget] t=%.1f %s freeze=%d out=%d/%d B/s (%d%%) bubble=%d (tagged=%d) dist: <12k=%d 12-24k=%d >24k=%d max=%.0f actors: enemy=%d pod=%d dice=%d templates:%s"),
			World->GetTimeSeconds(), *GetNameSafe(PC), FreezeState,
			Out, Cap, (0 < Cap) ? (Out * 100 / Cap) : 0,
			BubbleTotal, BubbleTagged,
			Within12k, Within24k, Beyond24k, MaxDist,
			EnemyActors, PodActors, DiceActors,
			TemplateBreakdown.ToString());
	}

	// 절제 토글은 로그를 찍은 뒤에 뒤집는다 — 위에 찍힌 out은 방금 끝난 구간의 상태(FreezeState) 값이다.
	if (FreezeVar && 0 != CVarNetAblate.GetValueOnGameThread())
	{
		FreezeVar->Set(FreezeState ? 0 : 1, ECVF_SetByCode);
	}
}

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
#include "Engine/Engine.h"
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

	// 반증용 스위치. 소스 판독으로는 **버블의 폴 빈도가 송신량을 좌우하지 않는다**가 결론이다:
	// Iris는 net.Iris.EnableForceNetUpdate가 기본 false라 Dirty로 표시된 오브젝트를 폴 주기와
	// 무관하게 매 프레임 폴링한다(FObjectPollFrequencyLimiter::Update가 DirtyObjects를 OR 한다).
	// 이 값을 30 → 5로 훑어도 송신량이 안 변해야 그 판독이 맞다. 변하면 판독이 틀린 것이고,
	// 갱신 주기 게이트(ULNPMassReplicator)의 기대 효과도 다시 계산해야 한다.
	// Iris는 SetNetUpdateFrequency 브로드캐스트를 받아 폴 주기를 즉시 갱신한다
	// (UEngineReplicationBridge::OnNetUpdateFrequencyChanged, net.Iris.EnableDynamicNetUpdateFrequency 기본 on).
	TAutoConsoleVariable<float> CVarNetBubbleHz(
		TEXT("LNP.Net.BubbleHz"),
		0.f,
		TEXT("Override the Mass client bubble actor's net update frequency, in Hz. Server only.\n")
		TEXT("  0    : keep the built-in default (AInfo sets 10 Hz)\n")
		TEXT("  30   : poll the bubble every server net tick\n")
		TEXT("Expected to make no difference: dirty objects bypass Iris poll frequency by default.\n")
		TEXT("Toggle 'net.Iris.EnableForceNetUpdate 1' to make poll frequency actually govern."),
		ECVF_Cheat);

	// 같은 세션 A/B용. 0보다 크면 로그 주기마다 BubbleHz와 이 값을 번갈아 적용한다.
	// 별도 실행 두 번은 스폰 위치·엔티티 구성이 달라져 6배짜리 효과도 묻힐 수 있다 —
	// FreezeBubble 절제와 같은 이유로 한 세션 안에서 뒤집는다.
	TAutoConsoleVariable<float> CVarNetBubbleHzAlt(
		TEXT("LNP.Net.BubbleHzAlt"),
		0.f,
		TEXT("Second value for a same-session A/B of the bubble's net update frequency, in Hz.\n")
		TEXT("When > 0, the logger alternates between LNP.Net.BubbleHz and this value each period.\n")
		TEXT("Read the 'hz=' field to tell the samples apart."),
		ECVF_Cheat);

	/** 현재 살아 있는 모든 버블에 빈도를 적용한다. 0 이하면 건드리지 않는다(생성자 기본값 유지). */
	void ApplyBubbleNetUpdateFrequency(float Hz)
	{
		if (Hz <= 0.f || !GEngine)
		{
			return;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (!World || World->GetNetMode() == NM_Client || World->GetNetMode() == NM_Standalone)
			{
				continue;
			}

			for (TActorIterator<ALNPMassClientBubbleInfo> It(World); It; ++It)
			{
				It->SetNetUpdateFrequency(Hz);
			}
		}

		UE_LOG(LogLootNPop, Log, TEXT("[NetBudget] bubble net update frequency overridden to %.1f Hz"), Hz);
	}

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

void ULNPNetBudgetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 콜백은 값이 바뀔 때만 돈다 — 매 틱 액터를 훑지 않기 위해서다.
	// 세션 도중 접속한 클라이언트의 버블은 생성자 기본값으로 만들어지므로, 아래 Tick이 로그 주기마다
	// 다시 적용해 새 버블도 따라오게 한다.
	CVarNetBubbleHz.AsVariable()->SetOnChangedCallback(
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var)
		{
			ApplyBubbleNetUpdateFrequency(Var->GetFloat());
		}));
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

	// 방금 끝난 구간에 실제로 적용돼 있던 버블 빈도. 아래 A/B 토글이 어느 쪽 차례인지 판단하는 기준이다.
	float BubbleHzApplied = 0.f;

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
		// 스윕 중에는 샘플마다 빈도가 다르므로 로그 자체가 그 값을 들고 있어야 사후에 구간을 가를 수 있다.
		float BubbleHz = 0.f;
		// 복제 LOD는 그 클라이언트의 뷰어 기준으로 계산된다. 뷰어에서 각 에이전트까지의 거리를 직접 재면
		// "컬 거리(ReplicationCullDistance)가 실제로 걸리고 있는가"를 추론 없이 판정할 수 있다.
		// 거리 구간은 LNP::Replication::ConfigureParams의 LOD 경계(1,000 / 5,000 / 12,000)에 맞춘다 —
		// LOD마다 갱신 주기가 다르므로 엔티티 수만으로는 단가를 역산할 수 없다.
		const APawn* ViewPawn = PC->GetPawn();
		const FVector ViewLoc = ViewPawn ? ViewPawn->GetActorLocation() : FVector::ZeroVector;
		int32 High = 0, Medium = 0, Low = 0, Beyond12k = 0;
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
			BubbleHz = BubbleHzApplied = BubbleIt->GetNetUpdateFrequency();
			for (const FLNPMassFastArrayItem& Item : BubbleIt->GetAgentSerializer().Bubble.GetAgents())
			{
				++BubbleTotal;
				BubbleTagged += Item.Agent.GetEnemyTypeTag().IsValid() ? 1 : 0;
				++CountByTemplate.FindOrAdd(Item.Agent.GetTemplateID().GetHash64());

				if (ViewPawn)
				{
					const double Dist = FVector::Dist(ViewLoc, Item.Agent.GetReplicatedPositionYawData().GetPosition());
					MaxDist = FMath::Max(MaxDist, Dist);
					if (Dist <= 1000.0)       { ++High; }
					else if (Dist <= 5000.0)  { ++Medium; }
					else if (Dist <= 12000.0) { ++Low; }
					else                      { ++Beyond12k; }
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
			TEXT("[NetBudget] t=%.1f %s freeze=%d hz=%.0f out=%d/%d B/s (%d%%) bubble=%d (tagged=%d) lod: high=%d med=%d low=%d beyond=%d max=%.0f actors: enemy=%d pod=%d dice=%d templates:%s"),
			World->GetTimeSeconds(), *GetNameSafe(PC), FreezeState, BubbleHz,
			Out, Cap, (0 < Cap) ? (Out * 100 / Cap) : 0,
			BubbleTotal, BubbleTagged,
			High, Medium, Low, Beyond12k, MaxDist,
			EnemyActors, PodActors, DiceActors,
			TemplateBreakdown.ToString());
	}

	// 절제 토글은 로그를 찍은 뒤에 뒤집는다 — 위에 찍힌 out은 방금 끝난 구간의 상태(FreezeState) 값이다.
	if (FreezeVar && 0 != CVarNetAblate.GetValueOnGameThread())
	{
		FreezeVar->Set(FreezeState ? 0 : 1, ECVF_SetByCode);
	}

	// 빈도 A/B도 같은 이유로 로그 뒤에 뒤집는다. Alt가 꺼져 있으면 단순히 현재 값을 다시 적용해,
	// 세션 도중 접속한 클라이언트의 새 버블도 지정 값을 따라오게 한다.
	const float HzA = CVarNetBubbleHz.GetValueOnGameThread();
	const float HzAlt = CVarNetBubbleHzAlt.GetValueOnGameThread();
	if (0.f < HzAlt && 0.f < HzA)
	{
		// 방금 찍힌 BubbleHz가 어느 쪽이었는지로 판단한다 — 두 값을 왕복시킨다.
		ApplyBubbleNetUpdateFrequency(FMath::IsNearlyEqual(BubbleHzApplied, HzA) ? HzAlt : HzA);
	}
	else
	{
		ApplyBubbleNetUpdateFrequency(HzA);
	}
}

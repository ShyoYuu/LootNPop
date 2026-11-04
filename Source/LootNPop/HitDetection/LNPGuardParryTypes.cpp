// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPGuardParryTypes.h"

#include "LootNPop.h"

#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDodgeDebug(
		TEXT("LNP.Debug.Dodge"), 0,
		TEXT("Dash i-frame debug log. Logs i-frame windows opened by dashes and hits ignored by them on the server. 0: off, 1: on"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarDodgeScreen(
		TEXT("LNP.Debug.ShowDodge"), 0,
		TEXT("Dash i-frame on-screen text. Shows the local pawn's i-frame window and every hit it blocked on the server. 0: off, 1: on"),
		ECVF_Cheat);

	/** 경로마다 한 줄 — 근접·화염은 한 공격이 여러 프레임 걸리므로 같은 줄을 덮어써 쌓이지 않게 한다. */
	constexpr uint64 GLNPDodgeBlockMsgKeyBase = 0x4C4E'4400;
	constexpr float  GLNPDodgeBlockMsgSeconds = 1.0f;
	// 저스트 회피 색 계열(보라, J-009). 막아낸 순간은 짙게, 단순히 창이 열린 것은 연하게 — 한눈에 구분되게 한다.
	const FColor     GLNPDodgeBlockMsgColor(140, 50, 230);
	const FColor     GLNPDodgeWindowMsgColor(215, 185, 255);
	// 성공은 보상이 따르는 순간이라 가장 눈에 띄게 — 밝은 자홍 쪽 보라.
	const FColor     GLNPJustDodgeMsgColor(255, 90, 255);
	constexpr float  GLNPJustDodgeMsgSeconds = 1.5f;
}

bool LNPGuardParry::IsDodgeDebugEnabled()
{
	return CVarDodgeDebug.GetValueOnAnyThread() != 0;
}

bool LNPGuardParry::IsDodgeScreenEnabled()
{
	return CVarDodgeScreen.GetValueOnAnyThread() != 0;
}

void LNPGuardParry::ReportDodgeIgnored(const TCHAR* Path, const AActor* Victim)
{
	if (IsDodgeDebugEnabled())
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] IFrame ignored path=%s player=%s"), Path, *GetNameSafe(Victim));

	if (IsDodgeScreenEnabled())
	{
		const uint64  Key  = GLNPDodgeBlockMsgKeyBase + (FCrc::StrCrc32(Path) & 0xFF);
		const FString Text = FString::Printf(TEXT("IFRAME BLOCK  %s  (%s)"), Path, *GetNameSafe(Victim));
		// GEngine 화면 메시지는 스레드 안전하지 않다 — 판정 프로세서는 Worker Thread에서 돈다.
		AsyncTask(ENamedThreads::GameThread, [Key, Text]
		{
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(Key, GLNPDodgeBlockMsgSeconds, GLNPDodgeBlockMsgColor, Text);
		});
	}
}

void LNPGuardParry::ReportDodgeWindowOpened(const AActor* Owner, const double ExpiryTime, const float Duration, const float JustDuration)
{
	const APawn* Pawn = Cast<APawn>(Owner);
	if (IsDodgeDebugEnabled() && Pawn)
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] IFrame open %s role=%s expiry=%.3f just=%.2f"),
			*Pawn->GetName(), *UEnum::GetValueAsString(Pawn->GetLocalRole()), ExpiryTime, JustDuration);

	// 창 길이만큼만 떠 있다 — 문구가 보이는 동안이 곧 무적 창이다. 원격 폰은 그 화면의 주인이 아니라 띄우지 않는다.
	if (IsDodgeScreenEnabled() && GEngine && Pawn && Pawn->IsLocallyControlled())
		GEngine->AddOnScreenDebugMessage(GLNPDodgeBlockMsgKeyBase - 1, Duration, GLNPDodgeWindowMsgColor,
			FString::Printf(TEXT("IFRAME  %.2fs  JUST %.2fs  (%s)"), Duration, JustDuration, *Pawn->GetName()));
}

void LNPGuardParry::ReportGhostImpactSkipped(const AActor* Victim)
{
	if (IsDodgeDebugEnabled())
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Ghost impact skipped player=%s"), *GetNameSafe(Victim));
}

void LNPGuardParry::ReportDodgeHit(const TCHAR* Path, const AActor* Victim, const FLNPParryStateFragment& PS, const double Now)
{
	if (!IsDodgeDebugEnabled())
		return;

	// 1초가 지난 창은 그 피격과 무관한 대시다 — 숫자 대신 '-'로 남겨 집계에서 이른 놓침으로 세지 않게 한다.
	const double SinceJust = Now - PS.JustDodgeExpiryTime;
	if (PS.JustDodgeExpiryTime >= 0.0 && SinceJust <= 1.0)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Hit path=%s player=%s sinceJust=%.0f"), Path, *GetNameSafe(Victim), SinceJust * 1000.0);
	}
	else
	{
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Hit path=%s player=%s sinceJust=-"), Path, *GetNameSafe(Victim));
	}
}

void LNPGuardParry::ReportJustDodgeSuccess(const TCHAR* Attack, const AActor* Defender)
{
	if (IsDodgeDebugEnabled())
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] JustDodge success player=%s attack=%s"), *GetNameSafe(Defender), Attack);

	// 서버 판정 시점에 띄운다 — 단일 프로세스 PIE에서는 모든 창에 보이고, 별도 프로세스 게스트에게는 보이지 않는다.
	if (IsDodgeScreenEnabled() && GEngine)
		GEngine->AddOnScreenDebugMessage(GLNPDodgeBlockMsgKeyBase - 2, GLNPJustDodgeMsgSeconds, GLNPJustDodgeMsgColor,
			FString::Printf(TEXT("JUST DODGE  %s  (%s)"), Attack, *GetNameSafe(Defender)));
}

FLNPParryStateFragment* LNPGuardParry::FindFragment(const AActor* Owner)
{
	if (!Owner)
		return nullptr;

	const UWorld* World = Owner->GetWorld();
	if (!World)
		return nullptr;

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSub)
		return nullptr;

	const UMassAgentComponent* AgentComp = Owner->FindComponentByClass<UMassAgentComponent>();
	if (!AgentComp)
		return nullptr;

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();
	const FMassEntityHandle Handle = AgentComp->GetEntityHandle();
	if (!EM.IsEntityActive(Handle))
		return nullptr;

	return EM.GetFragmentDataPtr<FLNPParryStateFragment>(Handle);
}

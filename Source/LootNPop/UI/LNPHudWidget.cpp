// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPHudWidget.h"
#include "UI/LNPHudViewModel.h"
#include "UI/LNPScreenProjection.h"
#include "Camera/LNPLockOnComponent.h"
#include "Enemy/LNPEnemyMarkerSubsystem.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "View/MVVMView.h"
#include "Widgets/LNPRadialCooldownWidget.h"
#include "Widgets/LNPScreenMarkerWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void ULNPHudWidget::InitViewModel(UAbilitySystemComponent* InASC, ULNPCharacterMoverComponent* InMover)
{
	if (!HudViewModel)
		HudViewModel = NewObject<ULNPHudViewModel>(this);

	HudViewModel->Initialize(InASC);

	// Blueprint View Model 패널에 "HUD_ViewModel"로 등록된 슬롯에 인스턴스를 주입한다.
	if (UMVVMView* View = GetExtension<UMVVMView>())
		View->SetViewModel(FName("HUD_ViewModel"), HudViewModel);

	// 폰이 바뀔 수 있으므로 기존 구독을 먼저 끊는다 (재빙의 시 중복 구독 방지).
	if (ULNPCharacterMoverComponent* PrevMover = BoundMover.Get())
		PrevMover->OnDashExecuted.Remove(DashExecutedHandle);

	BoundMover = InMover;
	DashExecutedHandle.Reset();

	if (InMover)
		DashExecutedHandle = InMover->OnDashExecuted.AddUObject(this, &ULNPHudWidget::HandleDashExecuted);

	if (DashCooldownWidget)
		DashCooldownWidget->ClearCooldown();
}

void ULNPHudWidget::DeinitViewModel()
{
	if (HudViewModel)
		HudViewModel->Deinitialize();

	if (ULNPCharacterMoverComponent* Mover = BoundMover.Get())
		Mover->OnDashExecuted.Remove(DashExecutedHandle);

	BoundMover.Reset();
	DashExecutedHandle.Reset();

	if (DashCooldownWidget)
		DashCooldownWidget->ClearCooldown();
}

void ULNPHudWidget::HandleDashExecuted()
{
	const ULNPCharacterMoverComponent* Mover = BoundMover.Get();
	if (!Mover || !DashCooldownWidget)
		return;

	DashCooldownWidget->StartCooldown(Mover->GetDashCooldown());
}

void ULNPHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 마커는 "값"이 아니라 매 프레임 다시 계산되는 화면 좌표라 MVVM을 거치지 않는다.
	if (!LockOnMarkerWidget && !EnemyHpBarWidget)
		return;

	const APlayerController* PC = GetOwningPlayer();
	if (!PC)
		return;

	FVector  CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector();

	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

	UpdateLockOnMarker(*PC, CameraLocation, CameraForward, ViewportScale);
	UpdateEnemyHpBars(InDeltaTime, *PC, CameraLocation, CameraForward, ViewportScale);
}

void ULNPHudWidget::UpdateLockOnMarker(const APlayerController& PC, const FVector& CameraLocation, const FVector& CameraForward, const float ViewportScale)
{
	if (!LockOnMarkerWidget)
		return;

	const APawn* Pawn = PC.GetPawn();
	if (CachedPawn.Get() != Pawn)
	{
		CachedPawn   = Pawn;
		CachedLockOn = Pawn ? Pawn->FindComponentByClass<ULNPLockOnComponent>() : nullptr;
	}

	TArray<FLNPScreenMarker> Markers;

	// ⚠️ 대상 위치는 Track 질의가 이미 채워 둔 **캐시값**을 쓴다. 엔티티 핸들을 여기서 다시 해석하면
	//    "대상이 사라져서 락온이 풀리는" 가장 흔한 경로에서 이미 없는 엔티티를 조회하게 된다 —
	//    2026-09-06에 게스트를 크래시시킨 것이 정확히 그 패턴이다 (TechDesign_HUD.md §11.7).
	const ULNPLockOnComponent* LockOn = CachedLockOn.Get();
	FVector   TargetLocation;
	FVector2f LocalPos;
	if (LockOn && LockOn->GetLockOnTargetLocation(TargetLocation)
		&& LNPScreenProjection::ProjectToWidgetLocal(PC, TargetLocation, CameraLocation, CameraForward, ViewportScale, LocalPos))
	{
		// 락온 마커는 거리와 무관하게 일정한 크기다 — 내가 지목한 대상을 가리키는 UI이지 월드 오브젝트가 아니다.
		FLNPScreenMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.LocalPos = LocalPos;
	}

	LockOnMarkerWidget->SetMarkers(MoveTemp(Markers));
}

void ULNPHudWidget::UpdateEnemyHpBars(const float DeltaTime, const APlayerController& PC, const FVector& CameraLocation, const FVector& CameraForward, const float ViewportScale)
{
	UWorld* World = GetWorld();
	ULNPEnemyMarkerSubsystem* MarkerSub = World ? World->GetSubsystem<ULNPEnemyMarkerSubsystem>() : nullptr;
	if (!MarkerSub)
		return;

	if (!EnemyHpBarWidget || HpBarMaxCount <= 0)
	{
		MarkerSub->ClearParams();
		HpBarStates.Reset();
		return;
	}

	// 다음 프레임 수집 파라미터. **상한에 이탈 여유를 더해 받아 둔다** — 히스테리시스의 절반이 여기다.
	FLNPEnemyMarkerParams Params;
	Params.Origin             = CameraLocation;
	Params.Direction          = CameraForward;
	Params.MaxDistance        = HpBarMaxDistance;
	Params.MaxAngleDeg        = HpBarMaxAngleDeg;
	Params.MaxCount           = HpBarMaxCount + HpBarExitMargin;
	Params.RecentDamageWindow = HpBarRecentDamageSeconds;
	MarkerSub->SetParams(Params);

	TArray<FLNPEnemyMarkerEntry> Entries;
	MarkerSub->GetEntries(Entries);

	for (TPair<FMassEntityHandle, FHpBarState>& Pair : HpBarStates)
		Pair.Value.bSeen = false;

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FLNPEnemyMarkerEntry& Entry = Entries[i];

		// 나머지 절반 — **진입은 상위 HpBarMaxCount만**, 이미 떠 있는 것은 여유 순위까지 버틴다.
		FHpBarState* Existing = HpBarStates.Find(Entry.Entity);
		if (i >= HpBarMaxCount && !Existing)
			continue;

		// 구 내벽이라 머리 방향은 위치에서 곧바로 나온다.
		const FVector HeadLocation = Entry.Location + (-Entry.Location).GetSafeNormal() * HpBarHeightOffset;

		FVector2f LocalPos;
		if (!LNPScreenProjection::ProjectToWidgetLocal(PC, HeadLocation, CameraLocation, CameraForward, ViewportScale, LocalPos))
			continue;

		FHpBarState& State = Existing ? *Existing : HpBarStates.Add(Entry.Entity);
		State.LocalPos = LocalPos;
		State.Ratio    = Entry.Ratio;
		State.Scale    = FMath::Clamp(HpBarScaleDistance / Entry.Distance, HpBarMinScale, 1.f);
		State.bSeen    = true;
	}

	const float FadeStep = (HpBarFadeSeconds > KINDA_SMALL_NUMBER) ? DeltaTime / HpBarFadeSeconds : 1.f;

	TArray<FLNPScreenMarker> Markers;
	Markers.Reserve(HpBarStates.Num());

	for (TMap<FMassEntityHandle, FHpBarState>::TIterator It(HpBarStates); It; ++It)
	{
		FHpBarState& State = It.Value();
		State.Alpha = FMath::Clamp(State.Alpha + (State.bSeen ? FadeStep : -FadeStep), 0.f, 1.f);

		// 사라진 대상은 마지막 좌표에서 흐려지다 없어진다 — 순위 경계에서 깜빡이는 것을 이것이 덮는다.
		if (State.Alpha <= 0.f && !State.bSeen)
		{
			It.RemoveCurrent();
			continue;
		}

		FLNPScreenMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.LocalPos = State.LocalPos;
		Marker.Alpha    = State.Alpha;
		Marker.Ratio    = State.Ratio;
		Marker.Scale    = State.Scale;
	}

	EnemyHpBarWidget->SetMarkers(MoveTemp(Markers));
}

void ULNPHudWidget::NativeDestruct()
{
	DeinitViewModel();
	Super::NativeDestruct();
}

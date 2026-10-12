// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPBurnVisualizationProcessor.h"

#include "GAS/LNPBurnTypes.h"
#include "GAS/LNPBurnProcessor.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Config/LNPSettings.h"
#include "LNPMassUtils.h"
#include "LootNPop.h"

#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace LNPBurnVisual
{
	/** NS_BurnLoop이 노출하는 중첩 수 User 파라미터. */
	static const FName StacksParameterName(TEXT("BurnStacks"));
}

ULNPBurnVisualizationProcessor::ULNPBurnVisualizationProcessor()
	: BurnQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	bRequiresGameThreadExecution = true;   // Niagara 컴포넌트 생성·이동
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;

	// 서버에서는 같은 프레임에 정산(만료·사망 정지)된 값을 그린다. 페이즈가 같아야 순서 선언이 유효하다.
	ExecutionOrder.ExecuteAfter.Add(ULNPBurnProcessor::StaticClass()->GetFName());
}

void ULNPBurnVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BurnQuery.AddRequirement<FLNPBurnFragment>(EMassFragmentAccess::ReadOnly);
	BurnQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BurnQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::Optional);
	BurnQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	BurnQuery.RegisterWithProcessor(*this);
}

void ULNPBurnVisualizationProcessor::Release(FBurnVisual& Visual)
{
	if (UNiagaraComponent* Comp = Visual.Component.Get())
	{
		Comp->SetAutoDestroy(true);
		Comp->Deactivate();
	}
	Visual.Component.Reset();
}

void ULNPBurnVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	if (!bBurnSystemResolved)
	{
		bBurnSystemResolved = true;
		BurnSystem = GetDefault<ULNPSettings>()->EnemyBurnVFX.LoadSynchronous();
		if (!BurnSystem)
			UE_LOG(LogLootNPop, Warning, TEXT("[BurnVFX] ULNPSettings::EnemyBurnVFX is not set or failed to load - enemy burn VFX disabled"));
	}
	if (!BurnSystem)
		return;

	++Frame;
	const bool bLog = LNPBurn::IsLogEnabled();

	BurnQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FLNPBurnFragment>   Burns      = Ctx.GetFragmentView<FLNPBurnFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		const FLNPEnemySharedFragment* Shared = Ctx.GetConstSharedFragmentPtr<FLNPEnemySharedFragment>();
		const float HitOffset = (Shared && Shared->Config) ? Shared->Config->HitCenterUpOffset : 0.f;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const uint8 Stacks = Burns[i].Stacks;
			if (Stacks == 0)
				continue;

			// 판정과 같은 몸통 중심. 로컬 +Z(불길 상승)를 엔티티 Up에 맞춘다.
			const FTransform& Transform = Transforms[i].GetTransform();
			const FVector UpDir  = LNPMass::GetEntityUp(Transform);
			const FVector Center = LNPHitDetection::ResolveEnemyCapsuleCenter(Transform.GetLocation(), UpDir, HitOffset, nullptr);
			const FRotator Rot   = FRotationMatrix::MakeFromZ(UpDir).Rotator();

			const FMassEntityHandle Entity = Ctx.GetEntity(i);
			FBurnVisual& Visual = ActiveVisuals.FindOrAdd(Entity);
			Visual.SeenFrame = Frame;

			UNiagaraComponent* Comp = Visual.Component.Get();
			if (!Comp)
			{
				Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BurnSystem, Center, Rot, FVector::OneVector,
					/*bAutoDestroy=*/false, /*bAutoActivate=*/true, ENCPoolMethod::None);
				Visual.Component     = Comp;
				Visual.AppliedStacks = 0;
				if (bLog)
					UE_LOG(LogLootNPop, Log, TEXT("[BurnVFX] Alloc Entity=%s Stacks=%u Active=%d"), *Entity.DebugGetDescription(), Stacks, ActiveVisuals.Num());
				if (!Comp)
					continue;
			}
			else
			{
				Comp->SetWorldLocationAndRotation(Center, Rot);
			}

			if (Visual.AppliedStacks != Stacks)
			{
				Comp->SetVariableFloat(LNPBurnVisual::StacksParameterName, static_cast<float>(Stacks));
				Visual.AppliedStacks = Stacks;
			}
		}
	});

	// 이번 프레임에 "중첩 > 0"으로 보이지 않은 항목 = 0으로 지워졌거나 엔티티가 사라졌다.
	for (auto It = ActiveVisuals.CreateIterator(); It; ++It)
	{
		if (It->Value.SeenFrame == Frame)
			continue;

		const FMassEntityHandle Entity = It->Key;
		Release(It->Value);
		It.RemoveCurrent();
		if (bLog)
			UE_LOG(LogLootNPop, Log, TEXT("[BurnVFX] Release Entity=%s Active=%d"), *Entity.DebugGetDescription(), ActiveVisuals.Num());
	}
}

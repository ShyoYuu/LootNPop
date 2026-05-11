// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPTargetingSubsystem.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassStateTreeTypes.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationTypes.h"
#include "MassMovementFragments.h"
#include "MassRepresentationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassNavigationFragments.h"
#include "MassDebugDrawHelpers.h"

// --- Scoring Processor ---

ULNPEnemyScoringProcessor::ULNPEnemyScoringProcessor()
	: ScoringQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// Prepare candidates for the NEXT frame after everyone has moved
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
}

void ULNPEnemyScoringProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ScoringQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	ScoringQuery.AddRequirement<FLNPEnemyTargetingCandidateFragment>(EMassFragmentAccess::ReadWrite);
	ScoringQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	ScoringQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
}

void ULNPEnemyScoringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 1. Gather all players
	struct FPlayerData { FMassEntityHandle Handle; FVector Location; };
	TArray<FPlayerData> Players;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& PlayerContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < PlayerContext.GetNumEntities(); ++i)
		{
			Players.Add({ PlayerContext.GetEntity(i), Transforms[i].GetTransform().GetLocation() });
		}
	});

	if (Players.Num() == 0)
	{
		return;
	}

	// 2. Process Enemies
	ScoringQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyFragment> EnemyFragments = EnemyContext.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TArrayView<FLNPEnemyTargetingCandidateFragment> CandidateFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyTargetingCandidateFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const FLNPEnemyTargetingConfig& TConfig = SharedFragment.Config->TargetingConfig;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FTransform& EnemyTransform = Transforms[i].GetTransform();
			const FVector EnemyLoc = EnemyTransform.GetLocation();
			const FVector EnemyForward = EnemyTransform.GetUnitAxis(EAxis::X);
			const FLNPEnemyFragment& EnemyData = EnemyFragments[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			FLNPEnemyTargetingCandidateFragment& CandidateData = CandidateFragments[i];
			const ELNPTargetingState PreviousState = Targeting.State;

			CandidateData.Reset();

			// Leash penalty: score drops steeply to zero as enemy approaches MaxLeashDistance from pod
			const float DistToLeash = FVector::Dist(EnemyLoc, EnemyData.ParentPodLocation);
			const float LeashFactor = FMath::Square(FMath::Clamp(1.0f - (DistToLeash / TConfig.MaxLeashDistance), 0.0f, 1.0f));

			struct FCandidate { FMassEntityHandle Handle; FVector Location; float DistSq; };
			TArray<FCandidate, TInlineAllocator<8>> VisiblePlayers;

			for (const auto& Player : Players)
			{
				const float DistSq = FVector::DistSquared(EnemyLoc, Player.Location);

				if (FMath::Square(TConfig.MaxTargetingDistance) < DistSq)
					continue;

				bool bVisible = false;
				if (PreviousState == ELNPTargetingState::Confirmed)
				{
					bVisible = true;
				}
				else if (DistSq <= FMath::Square(TConfig.AwarenessDistance))
				{
					bVisible = true;
				}
				else if (DistSq <= FMath::Square(TConfig.VisionDistance))
				{
					const FVector DirToTarget = (Player.Location - EnemyLoc).GetSafeNormal();
					const float DotToTarget = FVector::DotProduct(EnemyForward, DirToTarget);
					const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(DotToTarget));

					if (AngleToTarget <= (TConfig.VisionAngle * 0.5f))
					{
						bVisible = true;
					}
				}

				if (bVisible)
				{
					VisiblePlayers.Add({ Player.Handle, Player.Location, DistSq });
				}
			}

			if (VisiblePlayers.Num() > 0)
			{
				VisiblePlayers.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });

				CandidateData.NumPotentialTargets = FMath::Min(VisiblePlayers.Num(), 4);

				for (int32 TargetIdx = 0; TargetIdx < CandidateData.NumPotentialTargets; ++TargetIdx)
				{
					const auto& Candidate = VisiblePlayers[TargetIdx];
					CandidateData.PotentialTargets[TargetIdx] = Candidate.Handle;

					float Score = 1000000.0f / (FMath::Sqrt(Candidate.DistSq) + 1.0f);
					Score *= LeashFactor;

					if (Score > KINDA_SMALL_NUMBER)
					{
						//bool bIsMelee = SharedFragment.Config->EnemyTypeTag.ToString().Contains(TEXT("Melee"), ESearchCase::IgnoreCase);
						bool bIsMelee = true; // For testing

						FMassEntityHandle EnemyEntity = EnemyContext.GetEntity(i);
						FMassEntityHandle PlayerHandle = Candidate.Handle;

						Context.Defer().PushCommand<FMassDeferredSetCommand>([EnemyEntity, PlayerHandle, Score, bIsMelee](const FMassEntityManager& InOutEntityManager)
						{
							ULNPTargetingSubsystem* TargetingSubsystem = UWorld::GetSubsystem<ULNPTargetingSubsystem>(InOutEntityManager.GetWorld());
							if (TargetingSubsystem != nullptr)
								TargetingSubsystem->RegisterEnemyInterest(EnemyEntity, PlayerHandle, Score, bIsMelee);
						});
					}
				}
			}
		}
	});
}


// --- Targeting Processor ---

ULNPEnemyTargetingProcessor::ULNPEnemyTargetingProcessor()
	: TargetingQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void ULNPEnemyTargetingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	TargetingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	TargetingQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadWrite);
	TargetingQuery.AddRequirement<FLNPEnemyTargetingCandidateFragment>(EMassFragmentAccess::ReadOnly);
	TargetingQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	TargetingQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyTargetingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPTargetingSubsystem* TargetingSubsystem = GetWorld()->GetSubsystem<ULNPTargetingSubsystem>();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	if (nullptr == TargetingSubsystem)
		return;

	// 1. Gather player locations
	TMap<FMassEntityHandle, FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& PlayerContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < PlayerContext.GetNumEntities(); ++i)
		{
			PlayerLocations.Add(PlayerContext.GetEntity(i), Transforms[i].GetTransform().GetLocation());
		}
	});

	// 2. Perform global rebalance
	TargetingSubsystem->RebalanceSlots();

	TArray<FMassEntityHandle> EntitiesToSignal;

	// 3. Sync results and update specific target info
	TargetingQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetMutableFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FLNPEnemyTargetingCandidateFragment> CandidateFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingCandidateFragment>();

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FVector EnemyLocation = Transforms[i].GetTransform().GetLocation();
			FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];
			const FLNPEnemyTargetingCandidateFragment& CandidateData = CandidateFragments[i];
			const ELNPTargetingState OldState = Targeting.State;
			const FMassEntityHandle OldTarget = Targeting.TargetPlayer;
			Targeting.ResetTargeting();

			bool bFoundConfirmed = false;
			
			// Try to find the best potential target that is confirmed
			for (int32 TargetIdx = 0; TargetIdx < CandidateData.NumPotentialTargets; ++TargetIdx)
			{
				FMassEntityHandle PotentialTarget = CandidateData.PotentialTargets[TargetIdx];
				if (TargetingSubsystem->IsSlotConfirmed(EnemyContext.GetEntity(i), PotentialTarget))
				{
					Targeting.TargetPlayer = PotentialTarget;
					Targeting.State = ELNPTargetingState::Confirmed;
					
					// Update precise info for the chosen target
					if (const FVector* PLoc = PlayerLocations.Find(PotentialTarget))
					{
						Targeting.LastKnownTargetLocation = *PLoc;
						Targeting.DistanceToTargetSq = FVector::DistSquared(EnemyLocation, *PLoc);
					}

					bFoundConfirmed = true;
					break;
				}
			}

			if (!bFoundConfirmed)
			{
				// If we have potential targets but none confirmed, enter Alert state
				if (CandidateData.NumPotentialTargets > 0)
				{
					Targeting.TargetPlayer = CandidateData.PotentialTargets[0];
					Targeting.State = ELNPTargetingState::Alert;

					if (const FVector* PLoc = PlayerLocations.Find(Targeting.TargetPlayer))
					{
						Targeting.LastKnownTargetLocation = *PLoc;
						Targeting.DistanceToTargetSq = FVector::DistSquared(EnemyLocation, *PLoc);
					}

				}
				else
				{
					Targeting.State = ELNPTargetingState::None;
					Targeting.ResetTargeting();
				}
			}

			if (OldState != Targeting.State)
			{
				UE_LOG(LogTemp, Log, TEXT("Entity %d changed state from %s to %s"), EnemyContext.GetEntity(i).Index, *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(Targeting.State));
			}

			if (OldState != Targeting.State || OldTarget != Targeting.TargetPlayer)
			{
				EntitiesToSignal.Add(EnemyContext.GetEntity(i));
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

// --- Target Follow Processor ---

ULNPEnemyTargetFollowProcessor::ULNPEnemyTargetFollowProcessor()
	: FollowQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// Process intent in Behavior phase after targeting
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetingProcessor::StaticClass()->GetFName());
}

void ULNPEnemyTargetFollowProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	FollowQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FollowQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	FollowQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	FollowQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	FollowQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	
	FollowQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyTargetFollowProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	TArray<FMassEntityHandle> EntitiesToSignal;

	FollowQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FVector EntityLocation = Transforms[i].GetTransform().GetLocation();
			FMassMoveTargetFragment& MoveTarget = MoveTargets[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			// 1. Sync MoveTarget with Targeting data
			if (Targeting.TargetPlayer.IsValid())
			{
				MoveTarget.Center = Targeting.LastKnownTargetLocation;
				
				const float ActualDistance = FMath::Sqrt(Targeting.DistanceToTargetSq);
				MoveTarget.DistanceToGoal = ActualDistance - AttackRange;

				// 2. Distance-based Signaling for StateTree
				if (Targeting.State == ELNPTargetingState::Confirmed)
				{
					const float ExitBuffer = AttackRange * 0.1f;

					if (MoveTarget.DistanceToGoal <= 0.0f)
					{
						EntitiesToSignal.Add(EnemyContext.GetEntity(i));
					}
					else if (ActualDistance > AttackRange + ExitBuffer)
					{
						EntitiesToSignal.Add(EnemyContext.GetEntity(i));
					}
				}
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

// --- Movement Processor ---

ULNPEnemyMovementProcessor::ULNPEnemyMovementProcessor()
	: MovementQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void ULNPEnemyMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	MovementQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	MovementQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	MovementQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	MovementQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	MovementQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	MovementQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	ULNPSurfaceCacheSubsystem* SurfaceCache = GetWorld()->GetSubsystem<ULNPSurfaceCacheSubsystem>();
	TArray<FMassEntityHandle> EntitiesToSignal;

	MovementQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TArrayView<FMassActorFragment> ActorFragments = EnemyContext.GetMutableFragmentView<FMassActorFragment>();
		const TArrayView<FTransformFragment> Transforms = EnemyContext.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float RotationRate = SharedFragment.Config->MovementConfig.RotationRate;
		const FVector GravityOrigin = SharedFragment.Config->MovementConfig.GravityOrigin;
		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;
		const float BaseMoveSpeed = SharedFragment.Config->MovementConfig.MoveSpeed;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			FTransform& EntityTransform = Transforms[i].GetMutableTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();
			const FMassMoveTargetFragment& MoveTarget = MoveTargets[i];
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			const FVector UpDir = (GravityOrigin - EntityLocation).GetSafeNormal(); // inward toward center = up for inner-sphere world
			const FQuat CurrentRotation = EntityTransform.GetRotation();

			const FVector TargetPos = MoveTarget.Center;
			const float DistSq = FVector::DistSquared(TargetPos, EntityLocation);
			const FVector DirToTarget = (TargetPos - EntityLocation).GetSafeNormal();
			const FVector TargetDirOnPlane = FVector::VectorPlaneProject(DirToTarget, UpDir).GetSafeNormal();

			float EffectiveSpeed = 0.0f;
			FVector OrientationIntent = FVector::ZeroVector;

			switch (Targeting.State)
			{
			case ELNPTargetingState::None:
				// Idle movement: Walk slowly to the target point
				EffectiveSpeed = BaseMoveSpeed * 0.3f; // Slow walk
				OrientationIntent = TargetDirOnPlane;
				break;

			case ELNPTargetingState::Alert:
				// Alert: Face the player but don't move unless pushed by ST tasks
				EffectiveSpeed = 0.0f;
				OrientationIntent = TargetDirOnPlane;
				break;

			case ELNPTargetingState::Confirmed:
				// Aggressive: Run to target
				const float ActualDistance = Targeting.TargetPlayer.IsValid() ? FMath::Sqrt(Targeting.DistanceToTargetSq) : 0.0f;
				EffectiveSpeed = (ActualDistance <= AttackRange) ? 0.0f : BaseMoveSpeed;
				OrientationIntent = TargetDirOnPlane;
				break;
			}

			// Signal StateTree if reached destination (for None/Confirmed states)
			if (EffectiveSpeed > 0.0f && DistSq < FMath::Square(100.0f)) // Arrival threshold
			{
				UE_LOG(LogTemp, Log, TEXT("Entity %d reached destination"), EnemyContext.GetEntity(i).Index);
				EntitiesToSignal.Add(EnemyContext.GetEntity(i));
				EffectiveSpeed = 0.0f;
			}

			// Override speed if explicitly set by StateTree (e.g., SteeringTask)
			// Alert is always stopped regardless of pending StateTree speed
			if (MoveTarget.DesiredSpeed.Get() > 0.0f && Targeting.State != ELNPTargetingState::Alert)
			{
				EffectiveSpeed = MoveTarget.DesiredSpeed.Get();
			}

			if (AActor* Actor = ActorFragments[i].GetMutable())
			{
				if (ALNPCharacterBase* LNPCharacter = Cast<ALNPCharacterBase>(Actor))
				{
					LNPCharacter->SetAIOrientationIntent(OrientationIntent);
					LNPCharacter->SetAIMoveInput(EffectiveSpeed > 0.0f ? OrientationIntent : FVector::ZeroVector);
				}
			}
			else
			{
				FVector Velocity = FVector::ZeroVector;

				if (!OrientationIntent.IsNearlyZero())
				{
					const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(OrientationIntent, UpDir).ToQuat();
					const FQuat NewRotation = FMath::QInterpConstantTo(CurrentRotation, TargetQuat, DeltaTime, FMath::DegreesToRadians(RotationRate));
					EntityTransform.SetRotation(NewRotation);
					
					Velocity = (EffectiveSpeed > 0.0f) ? (OrientationIntent * EffectiveSpeed) : FVector::ZeroVector;
				}
				else
				{
					Velocity = FVector::ZeroVector;
					const FVector Forward = CurrentRotation.GetForwardVector();
					const FQuat TargetQuat = FRotationMatrix::MakeFromXZ(Forward, UpDir).ToQuat();
					EntityTransform.SetRotation(TargetQuat);
				}

				const FVector DesiredPos = EntityTransform.GetLocation() + Velocity * DeltaTime;
				const FVector DirToSurface = (DesiredPos - GravityOrigin).GetSafeNormal();

				FVector FinalPos = DesiredPos;
				if (SurfaceCache != nullptr)
				{
					FVector SurfacePoint;
					if (SurfaceCache->GetSurfacePoint(DirToSurface, SurfacePoint))
					{
						// Use cache only for surface radius; angular position comes from continuous DesiredPos
						const float SurfaceRadius = FVector::Dist(GravityOrigin, SurfacePoint);
						FinalPos = GravityOrigin + DirToSurface * SurfaceRadius;
					}
				}
				EntityTransform.SetLocation(FinalPos);
			}
		}
	});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

#if WITH_EDITOR
// --- Debug Draw Processor ---

ULNPEnemyDebugDrawProcessor::ULNPEnemyDebugDrawProcessor()
	: DebugQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetingProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(ULNPEnemyScoringProcessor::StaticClass()->GetFName());
}

void ULNPEnemyDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DebugQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	DebugQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	DebugQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
}

void ULNPEnemyDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	UE::Mass::Debug::FLineBatcher LineBatcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

	DebugQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = EnemyContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = EnemyContext.GetFragmentView<FMassMoveTargetFragment>();
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (SharedFragment.Config == nullptr)
			return;

		const float AttackRange = SharedFragment.Config->MovementConfig.AttackRange;

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FVector EntityLocation = EntityTransform.GetLocation();
			const FLNPEnemyTargetingFragment& Targeting = TargetingFragments[i];

			FColor StateColor = FColor::Green; // Idle

			if (Targeting.State == ELNPTargetingState::Confirmed)
			{
				const float ActualDistance = FMath::Sqrt(Targeting.DistanceToTargetSq);
				if (ActualDistance <= AttackRange)
				{
					StateColor = FColor::Red; // Attack
				}
				else
				{
					StateColor = FColor::Blue; // Chase
				}
			}
			else if (Targeting.State == ELNPTargetingState::Alert)
			{
				StateColor = FColor::Yellow; // Alert
			}
			else
			{
				StateColor = FColor::Green; // Idle
			}

			const FVector Offset = (FVector::ZeroVector - EntityLocation).GetSafeNormal() * 50.0f;
			LineBatcher.DrawSolidBox(EntityLocation + Offset, FVector(25.0), StateColor);
			LineBatcher.DrawArrow(EntityTransform, 75.0f, FColor::Black);
		}
	});
}
#endif

// --- Health Processor ---

ULNPHealthProcessor::ULNPHealthProcessor()
	: HealthQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void ULNPHealthProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	HealthQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	HealthQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
}

void ULNPHealthProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToDestroy;

	HealthQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const TArrayView<FLNPEnemyFragment> Enemies = EnemyContext.GetMutableFragmentView<FLNPEnemyFragment>();

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			if (Enemies[i].Health <= 0.0f)
			{
				EntitiesToDestroy.Add(EnemyContext.GetEntity(i));
			}
		}
	});

	if (EntitiesToDestroy.Num() > 0)
	{
		Context.Defer().DestroyEntities(EntitiesToDestroy);
	}
}

// --- LOD Override Processor ---

ULNPEnemyLODOverrideProcessor::ULNPEnemyLODOverrideProcessor()
	: LODOverrideQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassDistanceLODProcessor"));
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void ULNPEnemyLODOverrideProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	LODOverrideQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	LODOverrideQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	LODOverrideQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
}

void ULNPEnemyLODOverrideProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	LODOverrideQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& LODContext)
	{
		const TArrayView<FMassRepresentationFragment> Representations = LODContext.GetMutableFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = LODContext.GetFragmentView<FLNPEnemyTargetingFragment>();

		for (int32 i = 0; i < LODContext.GetNumEntities(); ++i)
		{
			if (TargetingFragments[i].State == ELNPTargetingState::Confirmed)
			{
				//Representations[i].CurrentRepresentation = EMassRepresentationType::HighResSpawnedActor;
			}
		}
	});
}

// --- Actor Initializer Processor ---

ULNPEnemyActorInitializerProcessor::ULNPEnemyActorInitializerProcessor()
	: InitializerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void ULNPEnemyActorInitializerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	InitializerQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	InitializerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	InitializerQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	InitializerQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	InitializerQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	InitializerQuery.AddTagRequirement<FLNPEnemyActorInitializedTag>(EMassFragmentPresence::None);
}

void ULNPEnemyActorInitializerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	InitializerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& EnemyContext)
	{
		const FLNPEnemySharedFragment& SharedFragment = EnemyContext.GetConstSharedFragment<FLNPEnemySharedFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = EnemyContext.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPEnemyFragment> EnemyFragments = EnemyContext.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFragments = EnemyContext.GetFragmentView<FLNPEnemyTargetingFragment>();

		for (int32 i = 0; i < EnemyContext.GetNumEntities(); ++i)
		{
			if (AActor* RawActor = ActorFragments[i].GetMutable())
			{
				FMassEntityHandle Entity = EnemyContext.GetEntity(i);
				ULNPEnemyConfig* Config = SharedFragment.Config;
				float Health = EnemyFragments[i].Health;
				ELNPTargetingState TState = TargetingFragments[i].State;

				Context.Defer().PushCommand<FMassDeferredAddCommand>([Entity, RawActor, Config, Health, TState](FMassEntityManager& InEntityManager)
				{
					if (ALNPEnemyCharacter* EnemyActor = Cast<ALNPEnemyCharacter>(RawActor))
					{
						EnemyActor->InitializeFromConfig(Config);
						EnemyActor->SyncFromEntity(Entity, Health, TState);
						InEntityManager.AddTagToEntity(Entity, FLNPEnemyActorInitializedTag::StaticStruct());
					}
				});
			}
		}
	});
}
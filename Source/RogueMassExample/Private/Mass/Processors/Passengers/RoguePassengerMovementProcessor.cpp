// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Processors/Passengers/RoguePassengerMovementProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Subsystems/RogueTrainWorldSubsystem.h"
#include "Utilities/RoguePassengerUtility.h"

URoguePassengerMovementProcessor::URoguePassengerMovementProcessor(): EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void URoguePassengerMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{	
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);	
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FRoguePassengerFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);	
	EntityQuery.RegisterWithProcessor(*this);	
}

void URoguePassengerMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	auto* TrainSubsystem = Context.GetWorld()->GetSubsystem<URogueTrainWorldSubsystem>();
	if (!TrainSubsystem) return;
	
	const FRogueTrackSharedFragment& TrackSharedFragment = TrainSubsystem->GetTrackShared();
	if (!TrackSharedFragment.IsValid()) return;

	const float Time = Context.GetWorld()->GetTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& SubContext)
	{
		const TConstArrayView<FTransformFragment> TransformFragments = SubContext.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassMoveTargetFragment> NavTargetList = SubContext.GetMutableFragmentView<FMassMoveTargetFragment>();
		const FMassMovementParameters& MoveParams = SubContext.GetConstSharedFragment<FMassMovementParameters>();
		const TArrayView<FRoguePassengerFragment> PassengerFragments = SubContext.GetMutableFragmentView<FRoguePassengerFragment>();
		const int32 NumEntities = SubContext.GetNumEntities();
		
		for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
		{
			const FTransform& PTransform = TransformFragments[EntityIndex].GetTransform();
			FMassMoveTargetFragment& MoveTarget = NavTargetList[EntityIndex];
			const FMassEntityHandle PassengerHandle = SubContext.GetEntity(EntityIndex);
			FRoguePassengerFragment& PassengerFragment = PassengerFragments[EntityIndex];

			// Check we have a valid station and no target has been assigned yet
			if (PassengerFragment.OriginStation.IsValid() && PassengerFragment.WaitingPointIdx == INDEX_NONE)
			{				
				// Assign a waiting point 
				AssignWaitingPoint(EntityManager, PassengerFragment);	
			}

			// If we have a move target, update movement towards it
			if (!PassengerFragment.Target.IsNearlyZero())
			{
				MoveToTarget(MoveTarget, MoveParams, PTransform, PassengerFragment.Target);
			}			

			// Handle phase-specific logic, destination arrival, boarding, departing and waiting
			switch (PassengerFragment.Phase)
			{
				case ERoguePassengerPhase::ToStationWaitingPoint: ToStationWaitingPoint(EntityManager, PassengerFragment, PTransform, PassengerHandle, Time); break;
				case ERoguePassengerPhase::ToAssignedCarriage: ToAssignedCarriage(EntityManager, SubContext, PassengerFragment, PTransform, PassengerHandle); break;
				case ERoguePassengerPhase::RideOnTrain: break; // Riding, do nothing
				case ERoguePassengerPhase::UnloadAtStation: UnloadAtStation(EntityManager, PassengerFragment, PTransform); break;
				case ERoguePassengerPhase::ToPostUnloadWaitingPoint: ToPostUnloadWaitingPoint(EntityManager, PassengerFragment, PTransform); break;
				case ERoguePassengerPhase::ToExitSpawn: ToExitSpawn(TrainSubsystem, SubContext, PassengerFragment, PTransform, PassengerHandle); break;
				default:
					break;
			}
		}
	});
}

void URoguePassengerMovementProcessor::AssignWaitingPoint(const FMassEntityManager& EntityManager, FRoguePassengerFragment& PassengerFragment)
{
	if (const auto* StationQueueFragment = EntityManager.GetFragmentDataPtr<FRogueStationQueueFragment>(PassengerFragment.OriginStation))
	{
		// Choose a random waiting point at that station
		PassengerFragment.WaitingPointIdx = (StationQueueFragment->WaitingPoints.Num() > 0)
			? FMath::RandRange(0, StationQueueFragment->WaitingPoints.Num() - 1)
			: INDEX_NONE;
		if (PassengerFragment.WaitingPointIdx == INDEX_NONE) return;
		
		// Assign target waiting point
		if (StationQueueFragment->WaitingPoints.IsValidIndex(PassengerFragment.WaitingPointIdx))
		{
			PassengerFragment.Target = StationQueueFragment->WaitingPoints[PassengerFragment.WaitingPointIdx];
		}
	}
}

void URoguePassengerMovementProcessor::MoveToTarget(FMassMoveTargetFragment& MoveTarget, const FMassMovementParameters& MoveParams, const FTransform& PTransform, const FVector& TargetDestination)
{
	MoveTarget.Center = TargetDestination;
	FVector TargetVector = TargetDestination - PTransform.GetLocation();
	TargetVector.Z = 0.0f; // Ignore Z for horizontal movement
	MoveTarget.DistanceToGoal = TargetVector.Size();
	MoveTarget.Forward = TargetVector.GetSafeNormal();
	const bool bHasArrived = MoveTarget.DistanceToGoal <= 20.f;
	MoveTarget.DesiredSpeed = bHasArrived ? FMassInt16Real(0.f) : FMassInt16Real(MoveParams.DefaultDesiredSpeed);
}

void URoguePassengerMovementProcessor::ToStationWaitingPoint(const FMassEntityManager& EntityManager, FRoguePassengerFragment& PassengerFragment,
	const FTransform& PTransform, const FMassEntityHandle PassengerHandle, const float Time)
{
	// Arrived? enqueue into that waiting-point queue if not already queued, idle until boarding - boarding handled by station ops processor
	if (FVector::DistSquared(PTransform.GetLocation(), PassengerFragment.Target) <= FMath::Square(PassengerFragment.AcceptanceRadius) && !PassengerFragment.bWaiting)
	{
		if (!PassengerFragment.OriginStation.IsValid()) return;
		
		if (auto* StationQueueFragment = EntityManager.GetFragmentDataPtr<FRogueStationQueueFragment>(PassengerFragment.OriginStation))
		{
			RoguePassengerQueueUtility::EnqueueAtWaitingPoint(*StationQueueFragment, PassengerFragment.WaitingPointIdx, PassengerHandle, PassengerFragment.DestinationStation, Time, /*prio*/0);
			PassengerFragment.bWaiting = true;
			PassengerFragment.Target = PTransform.GetLocation();
		}		
	}
}

void URoguePassengerMovementProcessor::ToAssignedCarriage(const FMassEntityManager& EntityManager, const FMassExecutionContext& Context, FRoguePassengerFragment& PassengerFragment,
	 const FTransform& PTransform, const FMassEntityHandle PassengerHandle)
{
	// Reset waiting flag
	PassengerFragment.bWaiting = false;
	
	// If we were boarded already, VehicleHandle is set, head to the carriage door (carriage transform)
	if (PassengerFragment.VehicleHandle.IsSet() && EntityManager.IsEntityValid(PassengerFragment.VehicleHandle))
	{
		if (const auto* CarriageTransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(PassengerFragment.VehicleHandle))
		{
			PassengerFragment.Target = CarriageTransformFragment->GetTransform().GetLocation();

			if (FVector::DistSquared(PTransform.GetLocation(), PassengerFragment.Target) <= FMath::Square(PassengerFragment.AcceptanceRadius))
			{				
				RoguePassengerUtility::HidePassenger(EntityManager, PassengerHandle);
				Context.Defer().PushCommand<FMassCommandAddTag<FRoguePassengerOnTrainTag>>(PassengerHandle);
				PassengerFragment.Phase = ERoguePassengerPhase::RideOnTrain;
				PassengerFragment.Target = PTransform.GetLocation();
			}
		}
	}
}

void URoguePassengerMovementProcessor::UnloadAtStation(const FMassEntityManager& EntityManager, FRoguePassengerFragment& PassengerFragment, const FTransform& PTransform)
{
	if (!PassengerFragment.DestinationStation.IsValid()) return;

	if (const auto* StationQueueFragment = EntityManager.GetFragmentDataPtr<FRogueStationQueueFragment>(PassengerFragment.DestinationStation))
	{
		const int32 WaitingPoint = RoguePassengerUtility::FindNearestIndex(StationQueueFragment->WaitingPoints, PTransform.GetLocation());				
		if (StationQueueFragment->WaitingPoints.IsValidIndex(WaitingPoint))
		{
			PassengerFragment.WaitingPointIdx = WaitingPoint;
			PassengerFragment.Target = StationQueueFragment->WaitingPoints[WaitingPoint];
			PassengerFragment.Phase = ERoguePassengerPhase::ToPostUnloadWaitingPoint;
		}
	}
}

void URoguePassengerMovementProcessor::ToPostUnloadWaitingPoint(const FMassEntityManager& EntityManager, FRoguePassengerFragment& PassengerFragment, const FTransform& PTransform)
{
	if (FVector::DistSquared(PTransform.GetLocation(), PassengerFragment.Target) <= FMath::Square(PassengerFragment.AcceptanceRadius))
	{
		// Immediately head to nearest exit spawn to leave the world
		if (const auto* StationQueueFragment = EntityManager.GetFragmentDataPtr<FRogueStationQueueFragment>(PassengerFragment.DestinationStation))
		{
			const int32 ExitIdx = RoguePassengerUtility::FindNearestIndex(StationQueueFragment->SpawnPoints, PTransform.GetLocation());			
			if (StationQueueFragment->SpawnPoints.IsValidIndex(ExitIdx))
			{
				PassengerFragment.Target = StationQueueFragment->SpawnPoints[ExitIdx];
			}
		}
		
		PassengerFragment.Phase = ERoguePassengerPhase::ToExitSpawn;
	}
}

void URoguePassengerMovementProcessor::ToExitSpawn(URogueTrainWorldSubsystem* TrainSubsystem, const FMassExecutionContext& Context, FRoguePassengerFragment& PassengerFragment,
	 const FTransform& PTransform, const FMassEntityHandle PassengerHandle)
{
	if (FVector::DistSquared(PTransform.GetLocation(), PassengerFragment.Target) <= FMath::Square(PassengerFragment.AcceptanceRadius))
	{
		// Return to pool / mark for destruction
		PassengerFragment.Phase = ERoguePassengerPhase::Pool;
		TrainSubsystem->EnqueueEntityToPool(PassengerHandle, Context, ERogueEntityType::Passenger);
		PassengerFragment.Target = PTransform.GetLocation();
	}
}

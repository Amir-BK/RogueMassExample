// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Processors/Stations/RogueTrainStationOpsProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Data/RogueDeveloperSettings.h"
#include "Mass/Processors/Stations/RogueTrainStationDetectProcessor.h"
#include "Subsystems/RogueTrainWorldSubsystem.h"
#include "Utilities/RoguePassengerUtility.h"


URogueTrainStationOpsProcessor::URogueTrainStationOpsProcessor(): EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
    ExecutionOrder.ExecuteAfter.Add(URogueTrainStationDetectProcessor::StaticClass()->GetFName());
}

void URogueTrainStationOpsProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRogueTrainStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FRogueTrainEngineTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void URogueTrainStationOpsProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	auto* TrainSubsystem = Context.GetWorld()->GetSubsystem<URogueTrainWorldSubsystem>();
	if (!TrainSubsystem) return;

	const FRogueTrackSharedFragment& TrackSharedFragment = TrainSubsystem->GetTrackShared();
	if (!TrackSharedFragment.IsValid()) return;

	const auto* Settings = GetDefault<URogueDeveloperSettings>();
	if (!Settings) return;

	const float DepartureTime = Settings->DepartureTimeSeconds;
	const int32 MaxLoadPerTickPerCar = Settings->MaxLoadPerTickPerCarriage;
	const int32 MaxUnloadPerTickPerCar = Settings->MaxUnLoadPerTickPerCarriage;
	const float StationStateSwitchTime = (Settings->MaxDwellTimeSeconds * 0.5f) + (DepartureTime * 0.5f);

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& SubContext)
	{
		const TArrayView<FRogueTrainStateFragment> StateView = SubContext.GetMutableFragmentView<FRogueTrainStateFragment>();

		for (int32 i = 0; i < SubContext.GetNumEntities(); ++i)
		{
			auto& State = StateView[i];
			
			// Skip if not at station
            if (!State.bAtStation || State.TargetStationIdx == INDEX_NONE) continue;

			switch (State.StationTrainPhase)
			{
				case ERogueStationTrainPhase::NotStopped: State.StationTrainPhase = ERogueStationTrainPhase::Arriving;
					break;
				case ERogueStationTrainPhase::Arriving:
				{
					// Unload passengers on first half of dwell time
					if (State.StationTimeRemaining >= StationStateSwitchTime)
					{
						State.StationTrainPhase = ERogueStationTrainPhase::Unloading;
					}
				}
				break;
				case ERogueStationTrainPhase::Unloading:
				{
					// Load passengers on second half of dwell time
				   if (State.StationTimeRemaining < StationStateSwitchTime)
				   {
					   State.StationTrainPhase = ERogueStationTrainPhase::Loading;
				   }
				}
				break;
				case ERogueStationTrainPhase::Loading:
				{
					// Clear flags when dwell time is almost up
					if (State.StationTimeRemaining < DepartureTime)
					{
						State.StationTrainPhase = ERogueStationTrainPhase::Departing;
					}
				}
				break;
				case ERogueStationTrainPhase::Departing:
					break;
			}			

			// Only proceed if loading or unloading
			if (State.StationTrainPhase != ERogueStationTrainPhase::Loading && State.StationTrainPhase != ERogueStationTrainPhase::Unloading) continue;

            // Resolve current station entity
            if (!TrackSharedFragment.StationEntities.IsValidIndex(State.TargetStationIdx)) continue;			
            const FMassEntityHandle CurrentStationEntity = TrackSharedFragment.StationEntities[State.TargetStationIdx];

			// Get station queue fragment
            FRogueStationQueueFragment* StationQueueFragment = EntityManager.GetFragmentDataPtr<FRogueStationQueueFragment>(CurrentStationEntity);
            if (!StationQueueFragment) continue;

            // Gather carriages for this engine
            const FMassEntityHandle EngineEntity = SubContext.GetEntity(i);
            const TArray<FMassEntityHandle>* CarriageList = TrainSubsystem->LeadToCarriages.Find(EngineEntity);
            if (!CarriageList) continue;

            // UNLOAD passengers whose Dest == current station (per carriage)
			if (State.StationTrainPhase == ERogueStationTrainPhase::Unloading)
			{
				for (const FMassEntityHandle CarriageEntity : *CarriageList)
				{
					auto* CarriageFragment = EntityManager.GetFragmentDataPtr<FRogueCarriageFragment>(CarriageEntity);
					if (!CarriageFragment || CarriageFragment->Occupants.Num() <= 0) continue;

					auto* CarriageTransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(CarriageEntity);
					if (!CarriageTransformFragment) continue;

					const FVector CarriageLocation = CarriageTransformFragment->GetTransform().GetLocation();

					int32 Budget = MaxUnloadPerTickPerCar;
					for (int32 Idx = CarriageFragment->Occupants.Num()-1; Idx >= 0 && Budget > 0; --Idx)
					{
						const FMassEntityHandle P = CarriageFragment->Occupants[Idx];
						if (!RoguePassengerUtility::IsHandleValid(EntityManager, P)) { CarriageFragment->Occupants.RemoveAtSwap(Idx); continue; }

						const FRoguePassengerFragment* PassengerFragment = EntityManager.GetFragmentDataPtr<FRoguePassengerFragment>(P);
						if (!PassengerFragment)
						{
							CarriageFragment->Occupants.RemoveAtSwap(Idx); continue;
						}

						// unload only if this is their destination
						if (PassengerFragment->DestinationStation == CurrentStationEntity)
						{
							RoguePassengerUtility::Disembark(EntityManager, SubContext, *CarriageFragment, Idx, CarriageLocation);
							--Budget;
						}
					}				
				}				
			}

            // LOAD passengers whose dest != current station (per carriage)
			if (State.StationTrainPhase == ERogueStationTrainPhase::Loading)
			{
				for (const FMassEntityHandle CarriageEntity : *CarriageList)
				{
					auto* CarriageFragment = EntityManager.GetFragmentDataPtr<FRogueCarriageFragment>(CarriageEntity);
					const FTransformFragment* CarriageTransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(CarriageEntity);
					if (!CarriageFragment || !CarriageTransformFragment) continue;

					const int32 FreeSlots = CarriageFragment->Capacity - CarriageFragment->Occupants.Num();
					if (FreeSlots <= 0) continue;

					int32 BoardingBudget = FMath::Min(FreeSlots, MaxLoadPerTickPerCar);
					if (BoardingBudget <= 0) continue;

					// Build a list of WP indices from the TMap
					TArray<int32> WaitingPointIndices;
					WaitingPointIndices.Reserve(StationQueueFragment->QueuesByWP.Num());
					for (const auto& Pair : StationQueueFragment->QueuesByWP)
					{
						WaitingPointIndices.Add(Pair.Key);
					}

					// Sort by distance to carriage
					const FVector CarriageLocation = CarriageTransformFragment->GetTransform().GetLocation();
					WaitingPointIndices.Sort([&](const int32 A, const int32 B)
					{
						const FVector& PositionA = StationQueueFragment->WaitingPoints[A];
						const FVector& PositionB = StationQueueFragment->WaitingPoints[B];
						return FVector::DistSquared(PositionA, CarriageLocation) < FVector::DistSquared(PositionB, CarriageLocation);
					});

					// Drain queues in waiting point distance order
					for (int32 j = 0; j < WaitingPointIndices.Num() && BoardingBudget > 0; ++j)
					{
						const int32 WaitingPointIdx = WaitingPointIndices[j];

						FRoguePassengerQueueEntry Entry;
						while (BoardingBudget > 0 && RoguePassengerQueueUtility::DequeueFromWaitingPoint(*StationQueueFragment, WaitingPointIdx, Entry))
						{
							// Skip arrivals for this station (they should exit)
							if (Entry.DestStation == CurrentStationEntity) continue;

							// Try to board
							if (RoguePassengerUtility::TryBoard(EntityManager, SubContext, Entry.Passenger, CarriageEntity, *CarriageFragment))
							{
								--BoardingBudget;
							}
						}
					}
				}
			}
        }
    });
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityManager.h"
#include "Mass/Fragments/RogueFragments.h"


class URogueTrainWorldSubsystem;

namespace RoguePassengerQueueUtility
{
	void EnqueueAtWaitingPoint(FRogueStationQueueFragment& StationQueueFragment, const int32 WaitingPointIdx, const FMassEntityHandle Passenger,
		const FMassEntityHandle DestStation, const float Time, const int32 Priority = 0);
	bool DequeueFromWaitingPoint(FRogueStationQueueFragment& StationQueueFragment, const int32 WaitingPointIdx, FRoguePassengerQueueEntry& Out);	
}

namespace RoguePassengerUtility
{
    inline bool IsHandleValid(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle) { return EntityHandle.IsSet() && EntityManager.IsEntityValid(EntityHandle); }

    // Remove passenger at index (swap & pop), clear their tags/vehicle
    void Disembark(const FMassEntityManager& EntityManager, const FMassExecutionContext& Context, FRogueCarriageFragment& CarriageFragment, const int32 Index, const FVector& Location);
    bool TryBoard(const FMassEntityManager& EntityManager, const FMassExecutionContext& Context, const FMassEntityHandle Passenger, const FMassEntityHandle CarriageEntity, FRogueCarriageFragment& CarriageFragment);
	void HidePassenger(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);
	void ShowPassenger(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle, const FVector& ShowLocation);
	int32 FindNearestIndex(const TArray<FVector>& Points, const FVector& From);
	bool SnapToPlatform(const UWorld* WorldContext, FVector& InOutPos, float MaxStepUp = 60.f, float MaxDrop = 200.f);
}
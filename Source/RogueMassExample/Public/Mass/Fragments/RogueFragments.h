// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "RogueFragments.generated.h"

class USplineComponent;

USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainEngineTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainCarriageTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainStationTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainPassengerTag : public FMassTag { GENERATED_BODY() };

USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainAtStationTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainDoorsOpenTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRogueTrainReadyDepartTag : public FMassTag { GENERATED_BODY() };

USTRUCT() struct ROGUEMASSEXAMPLE_API FRoguePassengerWaitingTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRoguePassengerOnTrainTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct ROGUEMASSEXAMPLE_API FRoguePassengerDespawnTag : public FMassTag { GENERATED_BODY() };

USTRUCT() struct ROGUEMASSEXAMPLE_API FRoguePooledEntityTag : public FMassTag { GENERATED_BODY() };

UENUM()
enum class ERoguePassengerPhase : uint8
{
	ToStationWaitingPoint,		// 1) spawn -> nearest station waiting point
	ToAssignedCarriage,					// 2) waiting -> carriage
	RideOnTrain,						// 
	UnloadAtStation,					// 
	ToPostUnloadWaitingPoint,			// 4) after unload -> nearest waiting point (brief settle)
	ToExitSpawn,						// 5) waiting -> nearest station spawn
	Pool								// 6) ready to destroy/return to pool
};

UENUM()
enum class ERogueStationTrainPhase : uint8
{
	NotStopped,
	Arriving,
	Unloading,
	Loading,
	Departing,
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueSplineFollowFragment : public FMassFragment
{
	GENERATED_BODY()
	
	float Alpha = 0.f; // [0..1] along spline
	float Speed = 0.f;  // cm/s
	FVector WorldPos = FVector::ZeroVector;
	FVector WorldFwd = FVector::ForwardVector;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueStationFragment : public FMassFragment
{
	GENERATED_BODY()
	
	int32 StationIndex = INDEX_NONE;
	float StationAlpha = 0.f; // [0..1] along track
	FVector WorldPosition = FVector::ZeroVector;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueTrainStateFragment : public FMassFragment
{
	GENERATED_BODY()
	
	bool bIsStopping = false;
	bool bAtStation = false;
	bool bHasArrived = false;
	ERogueStationTrainPhase StationTrainPhase = ERogueStationTrainPhase::NotStopped;
	float HeadwaySpeedScale = 1.f;
	float StationTimeRemaining = 0.f;  
	float PrevAlpha = 0.f;  
	FMassEntityHandle TargetStation = FMassEntityHandle();
	int32 TargetStationIdx = INDEX_NONE;
	int32 PreviousStationIndex = INDEX_NONE;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueTrainLinkFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FMassEntityHandle LeadHandle;
	int32 CarriageIndex = 0; // 0 reserved for lead
	float SpacingMeters = 8.f;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueCarriageFragment : public FMassFragment
{
	GENERATED_BODY()
	
	int32 Capacity = 100;
	TArray<FMassEntityHandle> Occupants;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRoguePassengerFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FMassEntityHandle OriginStation = FMassEntityHandle();       
	FMassEntityHandle DestinationStation = FMassEntityHandle();
	int32 WaitingPointIdx = INDEX_NONE;
	int32 CarriageIndex = INDEX_NONE;
	FMassEntityHandle VehicleHandle;
	ERoguePassengerPhase Phase = ERoguePassengerPhase::ToStationWaitingPoint;
	FVector Target = FVector::ZeroVector;
	float AcceptanceRadius = 20.f;
	float MaxSpeed = 200.f;
	bool bWaiting = false;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRoguePassengerHandleFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FMassEntityHandle RidingTrain;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRoguePassengerQueueEntry
{
	GENERATED_BODY()

	FMassEntityHandle Passenger = FMassEntityHandle();
	FMassEntityHandle DestStation = FMassEntityHandle();
	int32 WaitingPointIdx = INDEX_NONE;
	float EnqueuedGameTime = 0.f;
	int32 Priority = 0;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueStationQueueFragment : public FMassFragment
{
	GENERATED_BODY()
	TMap<int32, TArray<FRoguePassengerQueueEntry>> QueuesByWP;
	TArray<FVector> WaitingPoints;
	TArray<FVector> SpawnPoints;   
};

/** Shared fragments used in the Mass Train Example */
USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueTrackSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()
	
	TWeakObjectPtr<USplineComponent> Spline;
	TArray<float> StationTrackAlphas; // sorted [0..1]
	TArray<FMassEntityHandle> StationEntities;
	TArray<FVector> StationWorldPositions;
	float TrackLength = 100000.f;  

	FORCEINLINE bool IsValid() const { return Spline.IsValid() && TrackLength > 0.f && StationTrackAlphas.Num() >= 2; }
	FORCEINLINE FMassEntityHandle GetStationEntityByIndex(const int32 Index) const
	{
		return StationEntities.IsValidIndex(Index) ? StationEntities[Index] : FMassEntityHandle();
	}
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueTrainLineSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()
	
	float CruiseSpeed = 1200.f;
	float ApproachSpeed = 500.f;
	float StopRadius = 600.f;
	float CarriageSpacingMeters = 8.f;
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueDemoLimitsSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()
	
	int32 MaxPassengersOverall = 500;
	float MaxDwellTimeSeconds = 12.f;
	int32 MaxPerCarriage = 20;
};
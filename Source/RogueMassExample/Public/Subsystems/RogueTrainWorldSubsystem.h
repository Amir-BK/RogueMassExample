// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTemplate.h"
#include "Mass/Fragments/RogueFragments.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueTrainWorldSubsystem.generated.h"

class UMassEntityConfigAsset;
class USplineComponent;

UENUM()
enum class ERogueEntityType : uint8
{
	Station,
	TrainEngine,
	TrainCarriage,
	Passenger
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueStationData
{
	GENERATED_BODY()
	
	float Alpha = 0.f;
	FVector WorldPos = FVector::ZeroVector;
	TArray<FVector> WaitingPoints;
	TArray<FVector> SpawnPoints;
	FMassEntityHandle StationHandle = FMassEntityHandle();
};

USTRUCT()
struct ROGUEMASSEXAMPLE_API FRogueSpawnRequest
{
	GENERATED_BODY()

	ERogueEntityType Type = ERogueEntityType::Passenger;
	const FMassEntityTemplate* EntityTemplate = nullptr;
	int32 RemainingCount = 0;

	// Any
	FVector SpawnLocation = FVector::ZeroVector;
	float StartAlpha = 0.f; 

	// Station
	FRogueStationData StationData;
	int32 StationIdx = INDEX_NONE;

	// Carriage
	FMassEntityHandle LeadHandle; 
	int32 CarriageIndex = 1;      
	float SpacingMeters = 8.f;                  
	int32 CarriageCapacity = 20;                 

	// Passenger
	FMassEntityHandle OriginStation = FMassEntityHandle();       
	FMassEntityHandle DestinationStation = FMassEntityHandle();
	int32 WaitingPointIdx = INDEX_NONE;
	float AcceptanceRadius = 20.f;
	float MaxSpeed = 200.f;

	// Completion callback
	TFunction<void(const TArray<FMassEntityHandle>& /*Spawned*/)> OnSpawned = nullptr;
};

/**
 * 
 */
UCLASS()
class ROGUEMASSEXAMPLE_API URogueTrainWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USplineComponent* GetSpline() const { return TrackSpline.Get(); }
	const TArray<FRogueStationData>& GetStations() const { return StationActorData; }

	// Build shared track fragment
	void BuildTrackShared();
	void InvalidateTrackShared() { bTrackDirty = true; }
	const FRogueTrackSharedFragment& GetTrackShared();
	int32 GetTrackRevision() const { return TrackRevision; }
	
	// Queue a spawn using the template you created from Dev Settings
	void EnqueueSpawns(const FRogueSpawnRequest& Request);

	// Pooling (generic)
	void EnqueueEntityToPool(const FMassEntityHandle Entity, const FMassExecutionContext& Context, const ERogueEntityType Type);
	int32 RetrievePooledEntities(const ERogueEntityType Type, const int32 Count, TArray<FMassEntityHandle>& Out);

	// Template accessors
	const FMassEntityTemplate* GetStationTemplate() const;
	const FMassEntityTemplate* GetTrainTemplate() const;
	const FMassEntityTemplate* GetCarriageTemplate() const;
	const FMassEntityTemplate* GetPassengerTemplate() const; 
	
	TMap<FMassEntityHandle, int32> CarriageCounts;
	TMap<FMassEntityHandle, TArray<FMassEntityHandle>> LeadToCarriages;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	void ProcessPendingSpawns();

private:
	UPROPERTY() TWeakObjectPtr<USplineComponent> TrackSpline;
	UPROPERTY() TArray<FRogueStationData> StationActorData;
	UPROPERTY() TArray<FMassEntityHandle> StationEntities;
	UPROPERTY() TArray<FRogueSpawnRequest> PendingSpawns;
	UPROPERTY() FRogueTrackSharedFragment CachedTrack;   // cached once, reused everywhere
	UPROPERTY() int32 TrackRevision = 0;
	UPROPERTY() bool bTrackDirty = true;
	TMap<ERogueEntityType, TArray<FMassEntityHandle>> EntityPool;
	TMap<ERogueEntityType, TArray<FMassEntityHandle>> WorldEntities;
	TArray<TPair<float, FMassEntityHandle>> PendingStations;
	UPROPERTY() UMassEntityConfigAsset* StationConfig = nullptr;
	UPROPERTY() UMassEntityConfigAsset* TrainConfig = nullptr;
	UPROPERTY() UMassEntityConfigAsset* CarriageConfig = nullptr;
	UPROPERTY() UMassEntityConfigAsset* PassengerConfig = nullptr;
	FMassEntityTemplate StationTemplate;  
	FMassEntityTemplate TrainTemplate;  
	FMassEntityTemplate CarriageTemplate;  
	FMassEntityTemplate PassengerTemplate;  

	void InitTemplateConfigs();
	void InitConfigTemplates(const UWorld& InWorld);
	void StartSpawnManager();
	void SpawnManager();
	void StopSpawnManager();
	void InitEntityManagement();
	void DiscoverSplineFromSettings();
	void GatherStationActors();
	void CreateStations();
	void CreateTrains();

	// Cache
	FMassEntityManager* EntityManager = nullptr;

	// Helpers
	void ConfigureSpawnedEntity(const FRogueSpawnRequest& Request, const FMassEntityHandle Entity);
	void RegisterEntity(const ERogueEntityType Type, const FMassEntityHandle Entity);
	void UnregisterEntity(const ERogueEntityType Type, const FMassEntityHandle Entity);
	
	TArray<FMassEntityHandle>& GetEntitiesFromPoolByType(const ERogueEntityType Type) {	return EntityPool.FindOrAdd(Type); }
	TArray<FMassEntityHandle>& GetEntitiesFromWorldByType(const ERogueEntityType Type) { return WorldEntities.FindOrAdd(Type); }

	FTimerHandle SpawnTimerHandle;

public:
	// Read-only accessors
	const TArray<FMassEntityHandle>& GetLiveEntities(const ERogueEntityType Type) const { return WorldEntities.FindChecked(Type); }
	int32 GetLiveCount(const ERogueEntityType Type) const { if (const auto* A = WorldEntities.Find(Type)) return A->Num(); return 0; }
	int32 GetPoolCount(const ERogueEntityType Type) const { if (const auto* A = EntityPool.Find(Type)) return A->Num(); return 0; }
	int32 GetTotalLiveCount() const;
	int32 GetTotalPoolCount() const;
};

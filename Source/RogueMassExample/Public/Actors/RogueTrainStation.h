// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueTrainStation.generated.h"

UCLASS()
class ROGUEMASSEXAMPLE_API ARogueTrainStation : public AActor
{
	GENERATED_BODY()
	
public:
	ARogueTrainStation();

	UPROPERTY(EditAnywhere, Category="Station", meta=(MakeEditWidget))
	TArray<FTransform> WaitingPointWidgets;

	UPROPERTY(EditAnywhere, Category="Station", meta=(MakeEditWidget))
	TArray<FTransform> SpawnPointWidgets;

	UFUNCTION(BlueprintCallable, Category="Station")
	float GetStationAlpha() const { return StationAlpha; }

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

private:
	UPROPERTY(EditAnywhere, Category="Station")
	float StationAlpha = 0.f; 
	void ComputeStationAlpha();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "RogueTrainHeadwayProcessor.generated.h"

/**
 * 
 */
UCLASS()
class ROGUEMASSEXAMPLE_API URogueTrainHeadwayProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URogueTrainHeadwayProcessor();
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoguePassenger.generated.h"

UCLASS()
class ROGUEMASSEXAMPLE_API ARoguePassenger : public AActor
{
	GENERATED_BODY()

public:
	ARoguePassenger();

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess=true))
	UStaticMeshComponent* MeshComponent;
};

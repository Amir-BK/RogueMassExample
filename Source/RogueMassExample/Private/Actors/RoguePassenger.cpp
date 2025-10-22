// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/RoguePassenger.h"


// Sets default values
ARoguePassenger::ARoguePassenger()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
}

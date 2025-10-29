// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Fragments/RogueFragments.h"
#include "Components/SplineComponent.h"

float FRogueTrackSharedFragment::GetStationAlphaByIndex(const int32 Index) const
{
	const USplineComponent& TrackSpline = *Spline.Get();
	const float SplineLength = FMath::Max(1.f, TrackSpline.GetSplineLength());
	const FRoguePlatformData& Platform = Platforms.IsValidIndex(Index) ? Platforms[Index] : FRoguePlatformData();
	const float Dist = Spline->GetDistanceAlongSplineAtLocation(Platform.Center, ESplineCoordinateSpace::World);

	return FMath::Frac(Dist / SplineLength);
}

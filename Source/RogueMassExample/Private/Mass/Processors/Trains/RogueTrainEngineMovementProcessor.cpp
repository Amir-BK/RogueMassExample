// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Processors/Trains/RogueTrainEngineMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Data/RogueDeveloperSettings.h"
#include "Mass/Fragments/RogueFragments.h"
#include "Subsystems/RogueTrainWorldSubsystem.h"
#include "Utilities/RogueTrainUtility.h"

URogueTrainEngineMovementProcessor::URogueTrainEngineMovementProcessor() : EntityQuery(*this)
{	
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void URogueTrainEngineMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FRogueTrainTrackFollowFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FRogueTrainStateFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);	
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FRogueTrainEngineTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);	
}

void URogueTrainEngineMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	auto* TrainSubsystem = Context.GetWorld()->GetSubsystem<URogueTrainWorldSubsystem>();
	if (!TrainSubsystem) return;

	const FRogueTrackSharedFragment& TrackSharedFragment = TrainSubsystem->GetTrackShared();
	if (!TrackSharedFragment.IsValid()) return;

	const auto* Settings = GetDefault<URogueDeveloperSettings>();
	if (!Settings) return;
	const float RideHeight = Settings ? Settings->CarriageRideHeight : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& SubContext)
	{
		const auto TrackFollowFragments = SubContext.GetMutableFragmentView<FRogueTrainTrackFollowFragment>();
		const auto StateView  = SubContext.GetMutableFragmentView<FRogueTrainStateFragment>();
		const auto TransformView = SubContext.GetMutableFragmentView<FTransformFragment>();
		const int32 NumEntities = SubContext.GetNumEntities();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = SubContext.GetEntity(i);
			auto& TrackFollowFragment = TrackFollowFragments[i];
			const auto& State  = StateView[i];
			FTransform& TrainTransform = TransformView[i].GetMutableTransform();
			if (!TrackSharedFragment.StationEntities.IsValidIndex(State.TargetStationIdx)) continue;

			float TargetSpeed = Settings->LeadCruiseSpeed;
			TargetSpeed *= State.HeadwaySpeedScale;

			if (State.bAtStation)
			{
				TargetSpeed = 0.f;
			}
			else if (State.bIsStopping)
			{
				TargetSpeed = FMath::Min(TargetSpeed, Settings->StationApproachSpeed);
			}

			// Use 'target' for your acceleration model
			TrackFollowFragment.Speed = FMath::FInterpTo(TrackFollowFragment.Speed, TargetSpeed, SubContext.GetDeltaTimeSeconds(), 2.f);
			TrackFollowFragment.Alpha = RogueTrainUtility::WrapTrackAlpha(TrackFollowFragment.Alpha + (TrackFollowFragment.Speed * SubContext.GetDeltaTimeSeconds()) / TrackSharedFragment.TrackLength );

			RogueTrainUtility::FSplineStationSample SplineSample;
			if (!RogueTrainUtility::GetSplineSample(TrackSharedFragment, TrackFollowFragment.Alpha, 0, 0.f, RideHeight, SplineSample))
				continue;

			TrackFollowFragment.Alpha = SplineSample.Alpha;
			TrackFollowFragment.WorldPos = SplineSample.Location;
			TrackFollowFragment.WorldFwd = SplineSample.Forward;
			
			TrainTransform = SplineSample.World;
			const FQuat Rot = FRotationMatrix::MakeFromXZ(SplineSample.Forward, FVector::UpVector).ToQuat();
			TrainTransform.SetRotation(Rot);
		}
	});
}

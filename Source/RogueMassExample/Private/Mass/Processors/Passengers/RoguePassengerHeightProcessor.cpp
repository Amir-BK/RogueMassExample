// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Processors/Passengers/RoguePassengerHeightProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/Processors/Passengers/RoguePassengerMovementProcessor.h"
#include "Utilities/RoguePassengerUtility.h"

URoguePassengerHeightProcessor::URoguePassengerHeightProcessor(): EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(URoguePassengerMovementProcessor::StaticClass()->GetFName());
}

void URoguePassengerHeightProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FRoguePassengerFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FRogueTrainPassengerTag>(EMassFragmentPresence::All);
}

void URoguePassengerHeightProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* WorldContext = Context.GetWorld();
	if (!WorldContext) return;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& SubContext)
	{
		const TArrayView<FTransformFragment> TransformFragments = SubContext.GetMutableFragmentView<FTransformFragment>();		
		const int32 NumEntities = SubContext.GetNumEntities();
		
		for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
		{
			FTransform& PTransform = TransformFragments[EntityIndex].GetMutableTransform();
			FVector PLocation = PTransform.GetLocation();
			RoguePassengerUtility::SnapToPlatform(WorldContext, PLocation);
			PTransform.SetLocation(PLocation);
		}
	});
}

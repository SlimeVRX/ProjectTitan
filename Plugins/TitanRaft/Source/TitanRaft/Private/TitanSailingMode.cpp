// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanSailingMode.h"
#include "Components/SceneComponent.h"
#include "TitanRaftActor.h"
#include "TitanMoverTypes.h"
#include "TitanRaftLogging.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "../../../../../../../Source/Runtime/Engine/Public/VisualLogger/VisualLogger.h"

bool UTitanSailingMode::CheckIfMovementIsDisabled()
{
	return false;
}

void UTitanSailingMode::ApplyMovement(FMoverTickEndData& OutputState)
{
	// invalidate the previous floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	// default to the component's location/rotation
	FVector OutLoc = MovingComponentSet.UpdatedComponent->GetComponentLocation();
	FRotator OutRot = MovingComponentSet.UpdatedComponent->GetComponentRotation();
	FVector OutVel = FVector::ZeroVector;

	ATitanRaft* LastRaft;
	
	// get the raft
	if (SimBlackboard->TryGet(TitanBlackboard::LastRaft, LastRaft))
	{
		if (IsValid(LastRaft))
		{
			OutLoc = LastRaft->GetPilotLocation();
			OutRot = LastRaft->GetPilotRotation();
			OutVel = LastRaft->GetPilotVelocity();
		}
	}

	// update the component's location and rotation
	MovingComponentSet.UpdatedComponent->SetWorldLocationAndRotation(OutLoc, OutRot);

	// set the default sync state's final transforms
	OutDefaultSyncState->SetTransforms_WorldSpace(OutLoc,
		OutRot,
		FVector::ZeroVector,
		nullptr);

	// set the updated component's velocity
	MovingComponentSet.UpdatedComponent->ComponentVelocity = OutVel;

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = MovingComponentSet.UpdatedComponent->GetComponentLocation();
		const FVector ArrowEnd = ArrowStart + OutVel;
		const FColor ArrowColor = FColor::Green;

		UE_VLOG_ARROW(this, VLogTitanRaft, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Sailing\nVel[%s]\nSpd[%f]"), *OutVel.ToCompactString(), OutVel.Size());
	}

#endif

}

void UTitanSailingMode::PostMove(FMoverTickEndData& OutputState)
{
	// add the mode tags
	Super::PostMove(OutputState);

	// calculate the stamina delta for this step
	float StaminaUse = TitanSettings->StaminaRegeneration * (DeltaMs - OutputState.MovementEndState.RemainingMs) * 0.001f;

	// update the stamina
	UpdateStamina(StaminaUse);

	// are we switching to falling state?
	if (OutputState.MovementEndState.NextModeName == CommonLegacySettings->AirMovementModeName)
	{
		// save the last fall time to the blackboard
		SimBlackboard->Set(TitanBlackboard::LastFallTime, CurrentSimulationTime);
	}
}

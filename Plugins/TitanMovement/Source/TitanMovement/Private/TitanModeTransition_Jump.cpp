// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanModeTransition_Jump.h"
#include "TitanMoverTypes.h"
#include "TitanLayeredMove_Jump.h"
#include "TitanMovementLogging.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoverDataModelTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "VisualLogger/VisualLogger.h"


UTitanModeTransition_Jump::UTitanModeTransition_Jump(const FObjectInitializer& ObjectInitializer)
{
	bJumpWhenButtonPressed = true;
	bRequireGround = true;
	bTruncateOnJumpRelease = true;
	bOverrideMovementPlaneVelocity = false;
	bOverrideVerticalVelocity = true;
	bAddFloorVelocity = true;
	bKeepPreviousVelocity = true;
	bKeepPreviousVerticalVelocity = true;
}

FTransitionEvalResult UTitanModeTransition_Jump::OnEvaluate(const FSimulationTickParams& Params) const
{
	// get the default kinematic inputs
	const FCharacterDefaultInputs* KinematicInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	// do we care about jump button state?
	if (bJumpWhenButtonPressed && KinematicInputs)
	{
		// check if jump was just pressed
		if (!KinematicInputs->bIsJumpJustPressed)
		{
			return FTransitionEvalResult::NoTransition;
		}
	}

	// get the sync state tags
	const FTitanTagsSyncState* TagsState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();

	// do we have tags to match?
	if (TagsState && !JumpRequiredTags.IsEmpty())
	{
		// check if the required tags match
		if (!TagsState->GetMovementTags().HasAllExact(JumpRequiredTags))
		{
			return FTransitionEvalResult::NoTransition;
		}
	}

	// get the blackboard
	const UMoverBlackboard* SimBlackboard = Params.MovingComps.MoverComponent->GetSimBlackboard();

	if (SimBlackboard)
	{
		float LastJumpTime = 0.0f;

		if (SimBlackboard->TryGet(TitanBlackboard::LastJumpTime, LastJumpTime))
		{
			const float TimeSinceLastJump = Params.TimeStep.BaseSimTimeMs - LastJumpTime; 
			if (TimeSinceLastJump < MinTimeBetweenJumps * 1000.0f)
			{
				//UE_LOG(LogTitanMover, Warning, TEXT("Too soon since last jump [%f]"), TimeSinceLastJump);
				return FTransitionEvalResult::NoTransition;
			}
		}

		if (bRequireGround)
		{
			// get the current floor
			FFloorCheckResult CurrentFloor;

			// do we have a valid floor?
			bool bValidFloor = SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor);
			
			if (!bValidFloor || !CurrentFloor.IsWalkableFloor())
			{
				if (CoyoteTime > 0.0f)
				{
					float LastFallTime = 0.0f;
					if (SimBlackboard->TryGet(TitanBlackboard::LastFallTime, LastFallTime))
					{
						const float TimeSinceFall = Params.TimeStep.BaseSimTimeMs - LastFallTime;
						if (TimeSinceFall > CoyoteTime * 1000.0f)
						{
							return FTransitionEvalResult::NoTransition;
						}
					}
					else {

						return FTransitionEvalResult::NoTransition;
					}

				}
				else {

					return FTransitionEvalResult::NoTransition;

				}
			}

		}
	}

	// valid jump
	return FTransitionEvalResult(JumpMovementMode);
}

void UTitanModeTransition_Jump::OnTrigger(const FSimulationTickParams& Params)
{
	// get the movement settings
	const UCommonLegacyMovementSettings* CommonLegacySettings = Params.MovingComps.MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	check(CommonLegacySettings);

	// get the sync state
	const FTitanTagsSyncState* TagsSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();
	check(TagsSyncState);

	// get the blackboard
	UMoverBlackboard* SimBlackboard = Params.MovingComps.MoverComponent->GetSimBlackboard_Mutable();

	// compute the inherited velocity
	FVector InheritedVelocity = FVector::ZeroVector;

	// get the current floor
	FFloorCheckResult CurrentFloor;
	if (SimBlackboard)
	{
		if (SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor))
		{
			if (CurrentFloor.IsWalkableFloor())
			{
				// we're jumping off of a walkable floor, so save the last falling time to the blackboard
				SimBlackboard->Set(TitanBlackboard::LastFallTime, Params.TimeStep.BaseSimTimeMs);

				if (bAddFloorVelocity && CurrentFloor.HitResult.GetActor())
				{
					// add the floor inherited velocity
					InheritedVelocity = CurrentFloor.HitResult.GetActor()->GetVelocity();
				}
			}
		}

		// set the last jumping time
		SimBlackboard->Set(TitanBlackboard::LastJumpTime, Params.TimeStep.BaseSimTimeMs);
	}	

	// preserve any momentum from our current base
	FVector ClampedVelocity = bKeepPreviousVelocity ? Params.MovingComps.UpdatedComponent->GetComponentVelocity() : FVector::ZeroVector;
	
	// should we reset the vertical velocity?
	if (!bKeepPreviousVerticalVelocity)
	{
		ClampedVelocity.Z = 0.0f;
	}

	if (MaxPreviousVelocity >= 0.0f)
	{
		ClampedVelocity = ClampedVelocity.GetClampedToMaxSize(MaxPreviousVelocity);
	}

	InheritedVelocity += ClampedVelocity;
	
	// choose the vertical impulse to use
	const float UpwardsSpeed = VerticalImpulse + (TagsSyncState->HasTagExact(ExtraVerticalImpulseTag) ? ExtraVerticalImpulse : 0.0f);

	// create the jump impulse layered move
	TSharedPtr<FTitanLayeredMove_Jump> JumpMove = MakeShared<FTitanLayeredMove_Jump>();
	JumpMove->UpwardsSpeed = UpwardsSpeed;
	JumpMove->Momentum = InheritedVelocity;
	JumpMove->AirControl = AirControl;
	JumpMove->DurationMs = HoldTime * 1000.0f;
	JumpMove->bTruncateOnJumpRelease = bTruncateOnJumpRelease;
	JumpMove->bOverrideHorizontalMomentum = bOverrideMovementPlaneVelocity;
	JumpMove->bOverrideVerticalMomentum = bOverrideVerticalVelocity;

	// queue the layered move
	Params.MovingComps.MoverComponent->QueueLayeredMove(JumpMove);

	// check if we should log the jump time in an additional blackboard key
	if (BlackboardTimeLoggingKey != NAME_None)
	{
		SimBlackboard->Set(BlackboardTimeLoggingKey, Params.TimeStep.BaseSimTimeMs);
	}

	// do we want to send a trigger event?
	if (TriggerEvent != FGameplayTag::EmptyTag)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Params.MovingComps.MoverComponent->GetOwner(), TriggerEvent, FGameplayEventData());
	}

#if ENABLE_VISUAL_LOG

	{
		const FString Truncate = bTruncateOnJumpRelease ? "true" : "false";
		const FString OverrideH = bOverrideMovementPlaneVelocity ? "true" : "false";
		const FString OverrideV = bOverrideVerticalVelocity ? "true" : "false";

		UE_VLOG(Params.MovingComps.MoverComponent->GetOwner(), VLogTitanMover, Log, TEXT("Jump Transition\nUpwards[%f]\nMomentum[%s]\nAirControl[%f]\nHold Time[%f]\nTruncate[%s]\nOverride H[%s] B[%s]"), UpwardsSpeed, *InheritedVelocity.ToCompactString(), AirControl, HoldTime, *Truncate, *OverrideH, *OverrideV);
	}

#endif

}
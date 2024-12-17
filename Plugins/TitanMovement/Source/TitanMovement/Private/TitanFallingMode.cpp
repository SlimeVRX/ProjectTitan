// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanFallingMode.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"

#include "MoverComponent.h"
#include "TitanMoverComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "VisualLogger/VisualLogger.h"

// Gameplay tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Gliding, "Titan.Movement.Falling.Gliding");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_SoftLanding, "Titan.Movement.Falling.SoftLanding");

constexpr float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.

void UTitanFallingMode::OnDeactivate()
{
	// send the glide ended event
	if (GlideEndEvent != FGameplayTag::EmptyTag)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetMoverComponent()->GetOwner(), GlideEndEvent, FGameplayEventData());
	}

	// invalidate the Blackboard keys
	UMoverBlackboard* BB = GetMoverComponent()->GetSimBlackboard_Mutable();

	if (BB)
	{
		
		SimBlackboard->Invalidate(TitanBlackboard::LastFallTime);
		SimBlackboard->Invalidate(TitanBlackboard::LastGrappleTime);
	}

}

void UTitanFallingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{	
	// get the inputs
	const FCharacterDefaultInputs* MoveKinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FTitanMovementInputs* MoveTitanInputs = StartState.InputCmd.InputCollection.FindDataByType<FTitanMovementInputs>();

	// get the sync states
	const FMoverDefaultSyncState* MoveDefaultSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(MoveDefaultSyncState);

	const FTitanStaminaSyncState* MoveStaminaSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FTitanStaminaSyncState>();
	const FTitanTagsSyncState* MoveTagsSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();

	// get the blackboard
	UMoverBlackboard* MoveBlackboard = GetMoverComponent()->GetSimBlackboard_Mutable();
	check(MoveBlackboard);

	// get the Mover Comp
	const UTitanMoverComponent* TitanComp = Cast<UTitanMoverComponent>(GetMoverComponent());
	check(TitanComp);

	// if movement is disabled, return a zero move
	if (TitanComp->IsMovementDisabled())
	{
		OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
		OutProposedMove.LinearVelocity = FVector::ZeroVector;

		return;
	}

	// convert the time step to delta seconds
	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// get the time
	float LastFallTime = 0.0f;
	float TimeFalling = 1000.0f;

	if (MoveBlackboard)
	{
		if (MoveBlackboard->TryGet(TitanBlackboard::LastFallTime, LastFallTime))
		{
			TimeFalling = (TimeStep.BaseSimTimeMs - LastFallTime) * 0.001f;
		}
	}

	// We don't want velocity limits to take the falling velocity component into account, since it is handled 
	// separately by the terminal velocity of the environment.
	const FVector StartVelocity = MoveDefaultSyncState->GetVelocity_WorldSpace();
	const FVector StartHorizontalVelocity = StartVelocity * FVector(1.0f, 1.0f, 0.0f);

	// check if we're gliding
	const bool bGliding = MoveTagsSyncState && MoveTagsSyncState->HasTagExact(GlidingTag)	// have we been gliding since last sim step?
			//&& MoveTitanInputs && MoveTitanInputs->bIsGlidePressed				// is the glide input pressed?
			&& MoveStaminaSyncState && !MoveStaminaSyncState->IsExhausted()			// are we in exhaustion recovery?
			&& MoveStaminaSyncState->GetStamina() > 0.0f							// do we have enough stamina?
			&& (StartVelocity.Z < 0.0f || TimeFalling > GlideMinFallingTime);		// are we falling, or have we been jumping for long enough?

	// check if we're soft landing
	const bool bSoftLanding = !bGliding && MoveTagsSyncState && MoveTagsSyncState->HasTagExact(SoftLandingTag);

	// build the proposed move
	////////////////////////////////////////////////////////////////////////

	FFreeMoveParams Params;

	// set the input type
	if (MoveKinematicInputs)
	{
		Params.MoveInputType = MoveKinematicInputs->GetMoveInputType();
		Params.MoveInput = MoveKinematicInputs->GetMoveInput();
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}

	FRotator IntendedOrientation_WorldSpace;
	
	// Zero out the vertical input, vertical movement will be determined by gravity
	Params.MoveInput.Z = 0;

	// do we have an orientation intent?
	if (!MoveKinematicInputs || MoveKinematicInputs->OrientationIntent.IsNearlyZero())
	{
		// default to the previous frame orientation
		IntendedOrientation_WorldSpace = MoveDefaultSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		// use the input's orientation intent
		IntendedOrientation_WorldSpace = MoveKinematicInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
		
	}

	// set the move params that don't change depending on whether we're gliding or not
	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = StartHorizontalVelocity;
	Params.PriorOrientation = MoveDefaultSyncState->GetOrientation_WorldSpace();
	Params.DeltaSeconds = DeltaSeconds;

	// default to regular falling params
	float AirControl = AirControlPercentage;

	if (bGliding)
	{
		// apply glide overrides
		AirControl = GlideAirControl;

		Params.TurningRate = GlideTurningRate;
		Params.TurningBoost = GlideTurningBoost;
		Params.MaxSpeed = GlideMaxSpeed;
		Params.Acceleration = GlideAcceleration;
		Params.Deceleration = GlideDeceleration;

	} else {

		// default to regular falling params
		Params.TurningRate = CommonLegacySettings->TurningRate;
		Params.TurningBoost = CommonLegacySettings->TurningBoost;
		Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
		Params.Acceleration = CommonLegacySettings->Acceleration;
		Params.Deceleration = FallingDeceleration;
	}

	// apply the air control percentage
	Params.MoveInput *= AirControl;

	if (!bGliding)
	{
		// do we want to move towards our velocity while over horizontal terminal velocity?
		if (Params.MoveInput.Dot(StartVelocity) > 0 && StartVelocity.Size2D() >= TerminalMovementPlaneSpeed)
		{
			// project the input into the movement plane defined by the velocity
			const FPlane MovementNormalPlane(StartVelocity, StartVelocity.GetSafeNormal());
			Params.MoveInput = Params.MoveInput.ProjectOnTo(MovementNormalPlane);

			// use the horizontal terminal velocity deceleration so we break faster
			Params.Deceleration = OverTerminalSpeedFallingDeceleration;
		}
	}

	FFloorCheckResult LastFloorResult;

	// do we have a valid floor hit result?
	if (MoveBlackboard && MoveBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult))
	{
		// are we sliding along a vertical slope?
		if (LastFloorResult.HitResult.IsValidBlockingHit() && LastFloorResult.HitResult.Normal.Z > VERTICAL_SLOPE_NORMAL_Z && !LastFloorResult.IsWalkableFloor())
		{
			// Are we trying to accelerate into the wall?
			if (FVector::DotProduct(Params.MoveInput, LastFloorResult.HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector Normal2D = LastFloorResult.HitResult.Normal.GetSafeNormal2D();
				Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, Normal2D);
			}
		}
	}

	// compute the free move
	OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);

	float SoftLandDistance = 0.0f;

	if (bSoftLanding && MoveBlackboard->TryGet<float>(TitanBlackboard::SoftLandDistance, SoftLandDistance))
	{

		// get the exact deceleration we need to hit the ground at a velocity of zero
		float Deceleration = (StartVelocity.Z * StartVelocity.Z) / (SoftLandDistance * 2.0f) * DeltaSeconds;

		// clamp to our vertical speed to ensure we don't overshoot and move upwards
		Deceleration = FMath::Min(Deceleration, FMath::Abs(StartVelocity.Z));

		// sign the speed correctly
		Deceleration *= FMath::Sign(StartVelocity.Z);

		OutProposedMove.LinearVelocity.Z = StartVelocity.Z - Deceleration;

	} else {

		// switch our terminal velocity if we're gliding and moving downwards
		const bool bUseGlideTerminalVelocity = bGliding;

		// choose the vertical terminal speed params
		const float TerminalSpeed = bUseGlideTerminalVelocity ? GlideTerminalVerticalSpeed : TerminalVerticalSpeed;
		const float TerminalUpwardsSpeed = bUseGlideTerminalVelocity ? GlideTerminalUpwardsSpeed : TerminalVerticalSpeed;
		const float TerminalDeceleration = bUseGlideTerminalVelocity ? GlideVerticalFallingDeceleration : VerticalFallingDeceleration;

		// Are we falling faster than our terminal speed?
		if (StartVelocity.Z < -TerminalSpeed)
		{
			// Should we clamp to terminal speed?
			if (bShouldClampTerminalVerticalSpeed)
			{
				OutProposedMove.LinearVelocity.Z = FMath::Sign(StartVelocity.Z) * TerminalSpeed;
			}
			else
			{
				const float DesiredSpeedDelta = FMath::Abs(TerminalSpeed - FMath::Abs(StartVelocity.Z)) / DeltaSeconds;

				float Deceleration = FMath::Min(DesiredSpeedDelta, TerminalDeceleration);
				Deceleration = FMath::Sign(StartVelocity.Z) * Deceleration * DeltaSeconds;

				OutProposedMove.LinearVelocity.Z = StartVelocity.Z - Deceleration;
			}

		}
		// Are we moving upwards faster than our terminal upwards speed?
		else if (StartVelocity.Z > TerminalUpwardsSpeed)
		{
			// Should we clamp to terminal speed?
			if (bShouldClampTerminalVerticalSpeed)
			{
				OutProposedMove.LinearVelocity.Z = FMath::Sign(StartVelocity.Z) * TerminalUpwardsSpeed;
			}
			else
			{
				const float DesiredSpeedDelta = FMath::Abs(TerminalUpwardsSpeed - FMath::Abs(StartVelocity.Z)) / DeltaSeconds;

				float Deceleration = FMath::Min(DesiredSpeedDelta, TerminalDeceleration);
				Deceleration = FMath::Sign(StartVelocity.Z) * Deceleration * DeltaSeconds;

				OutProposedMove.LinearVelocity.Z = StartVelocity.Z - Deceleration;
			}
		}
		else
		{
			// restore original vertical velocity
			OutProposedMove.LinearVelocity.Z = StartVelocity.Z;	
		}

	}

	// Add the wind acceleration if we're gliding
	if (bGliding)
	{
		OutProposedMove.LinearVelocity += MoveTitanInputs->Wind * DeltaSeconds;
	}

	// Add velocity change due to gravity
	OutProposedMove.LinearVelocity += UMovementUtils::ComputeVelocityFromGravity(GetMoverComponent()->GetGravityAcceleration(), DeltaSeconds);

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = MoveDefaultSyncState->GetLocation_WorldSpace();
		const FVector ArrowEnd = ArrowStart + OutProposedMove.LinearVelocity;
		const FColor ArrowColor = bGliding ? FColor::Orange : FColor::Yellow;
		const FString Gliding = bGliding ? "true" : "false";

		UE_VLOG_ARROW(this, VLogTitanMoverGenerateMove, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Fall Move\nVel[%s]\nAng[%s]\nGlide[%s]"), *OutProposedMove.LinearVelocity.ToCompactString(), *OutProposedMove.AngularVelocity.ToCompactString(), *Gliding);
	}

#endif
}

bool UTitanFallingMode::PrepareSimulationData(const FSimulationTickParams& Params)
{
	if (Super::PrepareSimulationData(Params))
	{
		// get the time
		float LastGrappleTime = 0.0f;
		TimeSinceGrappleJump = 1000.0f;

		if (SimBlackboard->TryGet<float>(TitanBlackboard::LastGrappleTime, LastGrappleTime))
		{
			TimeSinceGrappleJump = (Params.TimeStep.BaseSimTimeMs - LastGrappleTime) * 0.001f;
		}

		return true;
	}

	return false;
}

void UTitanFallingMode::ApplyMovement(FMoverTickEndData& OutputState)
{
	// initialize the fall data
	FTitanMoveData FallData;

	FallData.MoveRecord.SetDeltaSeconds(DeltaTime);
	FallData.OriginalMoveDelta = ProposedMove->LinearVelocity * DeltaTime;
	FallData.CurrentMoveDelta = FallData.OriginalMoveDelta;
	
	// cache the prior velocity
	const FVector PriorFallingVelocity = StartingSyncState->GetVelocity_WorldSpace();

	// invalidate the previous floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	// calculate the new orientation
	bool bOrientationChanged = CalculateOrientationChange(FallData.TargetOrientQuat);

	// move the component
	UMovementUtils::TrySafeMoveUpdatedComponent(MovingComponentSet, FallData.CurrentMoveDelta, FallData.TargetOrientQuat, true, FallData.MoveHitResult, ETeleportType::None, FallData.MoveRecord);

	// Compute final velocity based on how long we actually go until we get a hit.
	FVector NewFallingVelocity = StartingSyncState->GetVelocity_WorldSpace();
	NewFallingVelocity.Z = ProposedMove->LinearVelocity.Z * FallData.MoveHitResult.Time;

	// Handle collisions against floors or walls

	FFloorCheckResult LandingFloor;

	// Have we hit something?
	if (FallData.MoveHitResult.IsValidBlockingHit() && MovingComponentSet.UpdatedPrimitive.IsValid())
	{
		// update the time applied so far
		FallData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(FallData.PercentTimeAppliedSoFar, FallData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

		{
			const FVector ArrowEnd = FallData.MoveHitResult.bBlockingHit ? FallData.MoveHitResult.Location : FallData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = FColor::Red;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, FallData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Fall\nStart[%s]\nEnd[%s]\nPct[%f]"), *FallData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), FallData.PercentTimeAppliedSoFar);
		}

#endif

		// Have we hit a landing surface?
		if (UAirMovementUtils::IsValidLandingSpot(MovingComponentSet, MovingComponentSet.UpdatedPrimitive->GetComponentLocation(),
			FallData.MoveHitResult, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, OUT LandingFloor))
		{
			// have we spent the grapple boost forced air time?
			if (TimeSinceGrappleJump > TitanSettings->GrappleBoostForcedAirModeDuration)
			{
				CaptureFinalState(LandingFloor, DeltaTime * FallData.PercentTimeAppliedSoFar, OutputState, FallData.MoveRecord);
				return;
			}
		}

		// update the last floor result on the blackboard
		LandingFloor.HitResult = FallData.MoveHitResult;
		SimBlackboard->Set(CommonBlackboard::LastFloorResult, LandingFloor);

		// tell the mover component to handle a wall impact
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Falling, FallData.MoveHitResult, FallData.CurrentMoveDelta);
		MutableMoverComponent->HandleImpact(ImpactParams);

		// we didn't land on a walkable surface, so let's try to slide along it
		UAirMovementUtils::TryMoveToFallAlongSurface(MovingComponentSet, FallData.CurrentMoveDelta,
			(1.f - FallData.MoveHitResult.Time), FallData.TargetOrientQuat, FallData.MoveHitResult.Normal, FallData.MoveHitResult, true,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, LandingFloor, FallData.MoveRecord);

		// update the time applied so far
		FallData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(FallData.PercentTimeAppliedSoFar, FallData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

		{
			const FVector ArrowEnd = FallData.MoveHitResult.bBlockingHit ? FallData.MoveHitResult.Location : FallData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = FallData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, FallData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("FallAlongSurface\nStart[%s]\nEnd[%s]\nPct[%f]"), *FallData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), FallData.PercentTimeAppliedSoFar);
		}

#endif

		// have we landed on a floor?
		if (LandingFloor.IsWalkableFloor())
		{
			// have we exhausted the grapple boost forced air time?
			if (TimeSinceGrappleJump > TitanSettings->GrappleBoostForcedAirModeDuration)
			{
				// capture the final state and handle landing
				CaptureFinalState(LandingFloor, DeltaTime * FallData.PercentTimeAppliedSoFar, OutputState, FallData.MoveRecord);
				return;
			}
		}
	}
	else
	{
		// this indicates an unimpeded full move
		FallData.PercentTimeAppliedSoFar = 1.0f;

#if ENABLE_VISUAL_LOG
		
		{
			const FVector ArrowEnd = FallData.MoveHitResult.bBlockingHit ? FallData.MoveHitResult.Location : FallData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = FColor::Green;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, FallData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Fall\nStart[%s]\nEnd[%s]\nPct[%f]"), *FallData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), FallData.PercentTimeAppliedSoFar);
		}

#endif

	}

	// capture the final state
	CaptureFinalState(LandingFloor, DeltaTime * FallData.PercentTimeAppliedSoFar, OutputState, FallData.MoveRecord);
}

void UTitanFallingMode::PostMove(FMoverTickEndData& OutputState)
{
	// add the mode tags
	Super::PostMove(OutputState);

	// check if we've been soft landing since the last frame
	bool bSoftLanding = TagsSyncState->HasTagExact(SoftLandingTag);

	// get the time
	float LastFallTime = 0.0f;
	float LastJumpTime = 0.0f;
	float TimeFalling = 1000000.0f;
	float MinFallingTime = GlideMinFallingTime;

	if (SimBlackboard)
	{
		if (SimBlackboard->TryGet<float>(TitanBlackboard::LastFallTime, LastFallTime))
		{
			TimeFalling = CurrentSimulationTime - LastFallTime;
		}

		if (SimBlackboard->TryGet<float>(TitanBlackboard::LastJumpTime, LastJumpTime))
		{
			if (FMath::Abs(LastFallTime - LastJumpTime) < GlideJumpTimeTolerance * 1000.0f)
			{
				MinFallingTime = GlideMinJumpTime;
			}
		}
	}

	// check if we're gliding
	if (bSoftLanding																					// are we soft landing?
		|| ((TitanInputs && ((TagsSyncState->HasTagExact(GlidingTag) && TitanInputs->bIsGlidePressed)	// were we gliding last frame and is the glide input pressed?
			|| TitanInputs->bIsGlideJustPressed))														// is this the first frame we've pressed the glide input?
			&& OutStaminaSyncState && !OutStaminaSyncState->IsExhausted()								// are we in exhaustion recovery?
			&& OutStaminaSyncState->GetStamina() > 0.0f													// do we have enough stamina?
			&& TimeFalling > MinFallingTime	* 1000.0f													// have we been falling for long enough?
			)
		)
	{
		
		// only update stamina if we're not soft landing and are actively falling
		if (!bSoftLanding && MovingComponentSet.UpdatedComponent->ComponentVelocity.Z < 0.0f)
		{
			// calculate the stamina delta for this step
			float StaminaDelta = -GlideStaminaCostPerSecond * (DeltaMs - OutputState.MovementEndState.RemainingMs) * 0.001f;

			// update the stamina
			UpdateStamina(StaminaDelta);
		}
		else {

			// we were soft landing last frame, so run another trace to check if we're still projected to hit ground
			FHitResult SoftLandingHit(1.0f);
			if (DoSoftLandingTrace(SoftLandingHit))
			{
				// add the soft landing tag to signal GenerateMove() to slow us down
				OutTagsSyncState->AddTag(SoftLandingTag);
			}

		}

		// add the gliding tag if necessary
		if (!OutStaminaSyncState->IsExhausted() || bSoftLanding)
		{
			OutTagsSyncState->AddTag(GlidingTag);
		}
	}
	// check for soft landing
	else if(CheckForSoftLanding())
	{
		// add the soft landing tag
		OutTagsSyncState->AddTag(SoftLandingTag);

	}

	// have we started soft landing?
	if (OutTagsSyncState->HasTagExact(SoftLandingTag) && !TagsSyncState->HasTagExact(SoftLandingTag))
	{
		// send the glide start event
		if (SoftLandingStartEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), SoftLandingStartEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanFallingMode: Soft Land Start"));
		}

#endif
	}
	// are we done soft landing?
	else if (!OutTagsSyncState->HasTagExact(SoftLandingTag) && TagsSyncState->HasTagExact(SoftLandingTag))
	{
		// send the glide start event
		if (SoftLandingEndEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), SoftLandingEndEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanFallingMode: Soft Land End"));
		}

#endif
	}

	// have we started gliding?
	if (OutTagsSyncState->HasTagExact(GlidingTag) && !TagsSyncState->HasTagExact(GlidingTag))
	{
		// send the glide start event
		if (GlideStartEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), GlideStartEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanFallingMode: Glide Start"));
		}

#endif

	}
	// are we done gliding?
	else if (!OutTagsSyncState->HasTagExact(GlidingTag) && TagsSyncState->HasTagExact(GlidingTag))
	{
		// send the glide end event
		if (GlideEndEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), GlideEndEvent, FGameplayEventData());
		}

		// update the last fall and jump time on the blackboard
		SimBlackboard->Set(TitanBlackboard::LastFallTime, CurrentSimulationTime);
		SimBlackboard->Set(TitanBlackboard::LastJumpTime, CurrentSimulationTime);

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanFallingMode: Glide End"));
		}

#endif

	}
}

void UTitanFallingMode::CaptureFinalState(const FFloorCheckResult& FloorResult, float DeltaSecondsUsed, FMoverTickEndData& TickEndData, FMovementRecord& Record)
{
	const FVector FinalLocation = MovingComponentSet.UpdatedComponent->GetComponentLocation();

	// Check for time refunds
	// If we have this amount of time (or more) remaining, give it to the next simulation step.
	constexpr float MinRemainingSecondsToRefund = 0.0001f;	

	if ((DeltaTime - DeltaSecondsUsed) >= MinRemainingSecondsToRefund)
	{
		const float PctOfTimeRemaining = (1.0f - (DeltaSecondsUsed / DeltaTime));
		TickEndData.MovementEndState.RemainingMs = PctOfTimeRemaining * DeltaTime * 1000.f;
	}
	else
	{
		TickEndData.MovementEndState.RemainingMs = 0.f;
	}

	Record.SetDeltaSeconds(DeltaSecondsUsed);

	EffectiveVelocity = Record.GetRelevantVelocity();
	// TODO: Update Main/large movement record with substeps from our local record

	FRelativeBaseInfo MovementBaseInfo;
	ProcessLanded(FloorResult, EffectiveVelocity, MovementBaseInfo, TickEndData);

	if (MovementBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);

		OutDefaultSyncState->SetTransforms_WorldSpace(FinalLocation,
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			EffectiveVelocity,
			MovementBaseInfo.MovementBase.Get(), MovementBaseInfo.BoneName);
	}
	else
	{
		OutDefaultSyncState->SetTransforms_WorldSpace(FinalLocation,
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			EffectiveVelocity,
			nullptr); // no movement base
	}

	// set the component's velocity
	MovingComponentSet.UpdatedComponent->ComponentVelocity = EffectiveVelocity;
}

void UTitanFallingMode::ProcessLanded(const FFloorCheckResult& FloorResult, FVector& Velocity, FRelativeBaseInfo& BaseInfo, FMoverTickEndData& TickEndData) const
{
	FName NextMovementMode = NAME_None;

	// if we can walk on the floor we landed on
	if (FloorResult.IsWalkableFloor())
	{
		// send the landing event to the owning actor
		if (LandingEndEvent != FGameplayTag::EmptyTag)
		{
			// encode the velocity as the event magnitude
			FGameplayEventData LandingData = FGameplayEventData();
			LandingData.EventMagnitude = Velocity.Z;

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), LandingEndEvent, LandingData);
		}

		// Switch to ground movement mode and cache any floor / movement base info
		Velocity.Z = 0.0;
		NextMovementMode = CommonLegacySettings->GroundMovementModeName;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

		if (UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
		{
			BaseInfo.SetFromFloorResult(FloorResult);
		}
	}

	// we could check for other surfaces here (i.e. when swimming is implemented we can check the floor hit here and see if we need to go into swimming)

	// This would also be a good spot for implementing some falling physics interactions (i.e. falling into a movable object and pushing it based off of this actors velocity)

	// if a new mode was set go ahead and switch to it after this tick and broadcast we landed
	if (!NextMovementMode.IsNone())
	{
		TickEndData.MovementEndState.NextModeName = NextMovementMode;
		MutableMoverComponent->OnLanded(NextMovementMode, FloorResult.HitResult);

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanFallingMode: Switching to Landed"));
		}

#endif
	}
}

bool UTitanFallingMode::CheckForSoftLanding()
{
	// have we been falling for long enough to check for soft landing?
	float LastFallTime = 0.0f;

	FHitResult Hit;

	// run the soft landing trace
	bool bHasSoftLandingTrace = DoSoftLandingTrace(Hit);

	if (bHasSoftLandingTrace)
	{
		if (SimBlackboard->TryGet(TitanBlackboard::LastFallTime, LastFallTime)
			&& (CurrentSimulationTime - LastFallTime) * 0.001f > MinTimeForSoftLanding)
		{
			// are we falling fast enough to check for soft landing?
			const float FallSpeed = EffectiveVelocity.Z;

			return FallSpeed < -SoftLandingTerminalVerticalSpeed;

		}
	}
	

	// invalidate the soft landing distance on the blackboard
	SimBlackboard->Invalidate(TitanBlackboard::SoftLandDistance);

	// we shouldn't soft land
	return false;
}

bool UTitanFallingMode::DoSoftLandingTrace(FHitResult& OutHit)
{
	// assume a trace multiplier of 1/4 if we got no curve
	float TraceMultiplier = 0.25f;

	// query the trace multiplier curve based on our falling speed
	if (SoftLandingTraceMultiplierCurve)
	{
		TraceMultiplier = SoftLandingTraceMultiplierCurve->GetFloatValue(EffectiveVelocity.Z);
	}

	// sweep in the direction of our fall velocity to check if we're projected to hit ground beneath us
	FVector Start = MovingComponentSet.UpdatedPrimitive->GetComponentLocation();
	FVector End = Start + EffectiveVelocity * TraceMultiplier;

	const ECollisionChannel GroundProbeChannel = SoftLandingTraceChannel;

	float Radius, HalfHeight = 0.0f;
	MovingComponentSet.UpdatedPrimitive->CalcBoundingCylinder(Radius, HalfHeight);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams QueryParams;
	FCollisionResponseParams ResponseParams;
	MovingComponentSet.UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	QueryParams.AddIgnoredActor(MovingComponentSet.UpdatedComponent->GetOwner());
	GetWorld()->SweepSingleByChannel(OutHit, Start, End, MovingComponentSet.UpdatedPrimitive->GetComponentQuat(), GroundProbeChannel, Capsule, QueryParams, ResponseParams);

	if (OutHit.bBlockingHit || OutHit.bStartPenetrating)
	{
		// is the projected landing point too close?
		const float ContactDistance = (OutHit.Location - Start).Size();

		// is the surface we're about to hit walkable?
		if (UFloorQueryUtils::IsHitSurfaceWalkable(OutHit, CommonLegacySettings->MaxWalkSlopeCosine))
		{
			// save the soft landing distance to the blackboard so we can use it in the next move generation
			SimBlackboard->Set(TitanBlackboard::SoftLandDistance, ContactDistance);

#if ENABLE_VISUAL_LOG

			{
				const FVector ArrowEnd = OutHit.bBlockingHit ? OutHit.Location : OutHit.TraceEnd;
				const FColor ArrowColor = FColor::Blue;

				UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, OutHit.TraceStart, ArrowEnd, ArrowColor, TEXT("UTitanFallingMode - Soft Landing - Ground\nStart[%s]\nEnd[%s]\nDist[%f]"), *OutHit.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), ContactDistance);
			}

#endif

			// we should soft land
			return true;
		}

		// have we hit a water body?
		AWaterBody* WaterBody = Cast<AWaterBody>(OutHit.GetActor());

		if (WaterBody != nullptr)
		{
			EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeDepth;

			FWaterBodyQueryResult QueryResult = WaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(OutHit.ImpactPoint, QueryFlags);

			const float WaterDepth = QueryResult.GetWaterSurfaceDepth();

			if (WaterDepth > WaterSoftLandingMinDepth)
			{
				// save the soft landing distance to the blackboard so we can use it in the next move generation
				SimBlackboard->Set(TitanBlackboard::SoftLandDistance, ContactDistance);

#if ENABLE_VISUAL_LOG

				{
					const FVector ArrowEnd = OutHit.bBlockingHit ? OutHit.Location : OutHit.TraceEnd;
					const FColor ArrowColor = FColor::Blue;

					UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, OutHit.TraceStart, ArrowEnd, ArrowColor, TEXT("UTitanFallingMode: Soft Landing - Water\nStart[%s]\nEnd[%s]\nDist[%f]"), *OutHit.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), ContactDistance);
				}

#endif

				//UE_LOG(LogTitanMover, Warning, TEXT("Water Soft Landing Hit"));
				return true;
			}

		}
	}

	// our trace hit nothing relevant
	return false;
}

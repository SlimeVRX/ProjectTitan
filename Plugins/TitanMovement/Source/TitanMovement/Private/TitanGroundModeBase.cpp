// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanGroundModeBase.h"

#include "TitanMoverComponent.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "MoverComponent.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"
#include "GameFramework/Pawn.h"

void UTitanGroundModeBase::ApplyMovement(FMoverTickEndData& OutputState)
{
	// ensure we have cached floor information before moving
	ValidateFloor();

	// initialize the move data
	FTitanMoveData WalkData;

	// initialize the move record
	WalkData.MoveRecord.SetDeltaSeconds(DeltaTime);

	// apply any movement from a dynamic base
	bool bDidMoveAlongWithBase = ApplyDynamicFloorMovement(OutputState, WalkData.MoveRecord);

	// after handling dynamic base, check for disabled movement
	if (MutableMoverComponent->IsMovementDisabled())
	{
		CaptureFinalState(CurrentFloor, WalkData.MoveRecord);
		return;
	}

	// calculate the target orientation Quat for the following moves
	bool bIsOrientationChanging = CalculateOrientationChange(WalkData.TargetOrientQuat);

	// calculate the move delta
	WalkData.OriginalMoveDelta = ProposedMove->LinearVelocity * DeltaTime;
	WalkData.CurrentMoveDelta = WalkData.OriginalMoveDelta;

	// floor check result passed to step-up sub-operations 
	// so we can use their final floor results if they did a test
	FOptionalFloorCheckResult StepUpFloorResult;

	// are we moving or reorienting?
	if (!WalkData.CurrentMoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		// apply the first move. This will catch any potential collisions or initial penetration
		bool bMovedFreely = ApplyFirstMove(WalkData);

		// apply any depenetration in case we started the frame stuck. This will include any catch-up from first move
		bool bDepenetration = ApplyDepenetrationOnFirstMove(WalkData);

		if (!bDepenetration)
		{
			// check if we've hit a ramp
			bool bMovedUpRamp = ApplyRampMove(WalkData);

			// attempt to move up any climbable obstacles
			bool bSteppedUp = ApplyStepUpMove(WalkData, StepUpFloorResult);

			bool bSlidAlongWall = false;

			// did we fail to step up?
			if (bSteppedUp)
			{
				// attempt to slide along an unclimbable obstacle
				bSlidAlongWall = ApplySlideAlongWall(WalkData);
			}

			// search for the floor we've ended up on
			UFloorQueryUtils::FindFloor(MovingComponentSet.UpdatedComponent.Get(), MovingComponentSet.UpdatedPrimitive.Get(),
				CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
				MovingComponentSet.UpdatedPrimitive->GetComponentLocation(), CurrentFloor);


			// adjust vertically so we remain in contact with the floor
			bool bAdjustedToFloor = ApplyFloorHeightAdjustment(WalkData);

			// check if we're falling
			if (HandleFalling(OutputState, WalkData.MoveRecord, CurrentFloor.HitResult, DeltaMs * WalkData.PercentTimeAppliedSoFar))
			{
				// our output state was captured by handle falling, so we can return
				return;
			}
		}

	}
	else {

		// we don't need to move this frame, but we may still need to adjust to the floor

		// search for the floor we're standing on
		UFloorQueryUtils::FindFloor(MovingComponentSet.UpdatedComponent.Get(), MovingComponentSet.UpdatedPrimitive.Get(),
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
			MovingComponentSet.UpdatedPrimitive->GetComponentLocation(), CurrentFloor);

		// copy the current floor hit result
		WalkData.MoveHitResult = CurrentFloor.HitResult;

		// check if we need to adjust to depenetrate from the floor
		bool bAdjustedToFloor = ApplyIdleCorrections(WalkData);

		// check if we're falling
		if (HandleFalling(OutputState, WalkData.MoveRecord, WalkData.MoveHitResult, 0.0f))
		{
			// our output state was captured by handle falling, so we can return
			return;
		}

	}

	// capture the final movement state
	CaptureFinalState(CurrentFloor, WalkData.MoveRecord);
}

void UTitanGroundModeBase::ValidateFloor()
{
	// check if we have cached floor data
	if (!SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor))
	{
		// search for the floor data again
		UFloorQueryUtils::FindFloor(MovingComponentSet.UpdatedComponent.Get(), MovingComponentSet.UpdatedPrimitive.Get(),
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
			MovingComponentSet.UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
	}

	// check if we have a cached relative base
	if (!SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, OldRelativeBase))
	{
		// update the floor and base info
		OldRelativeBase = UpdateFloorAndBaseInfo(CurrentFloor);
	}
}

bool UTitanGroundModeBase::ApplyDynamicFloorMovement(FMoverTickEndData& OutputState, FMovementRecord& MoveRecord)
{
	// If we're on a dynamic movement base, attempt to move along with whatever motion is has performed since we last ticked
	//if (OldRelativeBase.UsesSameBase(StartingSyncState->GetMovementBase(), StartingSyncState->GetMovementBaseBoneName()))
	//{
	//	return UBasedMovementUtils::TryMoveToStayWithBase(UpdatedComponent, UpdatedPrimitive, OldRelativeBase, MoveRecord, false);
	//}

	return false;
}

bool UTitanGroundModeBase::ApplyFirstMove(FTitanMoveData& WalkData)
{
	// attempt to move the full amount first
	bool bMoved = UMovementUtils::TrySafeMoveUpdatedComponent(MovingComponentSet, WalkData.CurrentMoveDelta, WalkData.TargetOrientQuat, true, WalkData.MoveHitResult, ETeleportType::None, WalkData.MoveRecord);

	// update the time percentage applied
	WalkData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(WalkData.PercentTimeAppliedSoFar, WalkData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

	const FVector ArrowEnd = WalkData.MoveHitResult.bBlockingHit ? WalkData.MoveHitResult.Location : WalkData.MoveHitResult.TraceEnd;
	const FColor ArrowColor = WalkData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

	UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("First\nStart[%s]\nEnd[%s]\nPct[%f]"), *WalkData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), WalkData.PercentTimeAppliedSoFar);

#endif
	return bMoved;
}

bool UTitanGroundModeBase::ApplyDepenetrationOnFirstMove(FTitanMoveData& WalkData)
{
	// were we immediately blocked?
	if (WalkData.MoveHitResult.bStartPenetrating)
	{
		// TODO: handle de-penetration, try moving again, etc.

		return true;
	}

	return false;
}

bool UTitanGroundModeBase::ApplyRampMove(FTitanMoveData& WalkData)
{
	// have we hit something that we suspect is a ramp?
	if (WalkData.MoveHitResult.IsValidBlockingHit())
	{
		// check if we're hitting a walkable ramp
		if ((WalkData.MoveHitResult.Time > 0.0f)
			&& (WalkData.MoveHitResult.Normal.Z > UE_KINDA_SMALL_NUMBER)
			&& UFloorQueryUtils::IsHitSurfaceWalkable(WalkData.MoveHitResult, CommonLegacySettings->MaxWalkSlopeCosine)
			)
		{
			// Compute the deflected move onto the ramp and update the move delta. We apply only the time remaining (1 - time applied)
			WalkData.CurrentMoveDelta = UGroundMovementUtils::ComputeDeflectedMoveOntoRamp(WalkData.CurrentMoveDelta * (1.0f - WalkData.PercentTimeAppliedSoFar), WalkData.MoveHitResult, CommonLegacySettings->MaxWalkSlopeCosine, CurrentFloor.bLineTrace);

			// Move again onto the ramp
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComponentSet, WalkData.CurrentMoveDelta, WalkData.TargetOrientQuat, true, WalkData.MoveHitResult, ETeleportType::None, WalkData.MoveRecord);

			// Update the time percentage applied
			WalkData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(WalkData.PercentTimeAppliedSoFar, WalkData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

			const FVector ArrowEnd = WalkData.MoveHitResult.bBlockingHit ? WalkData.MoveHitResult.Location : WalkData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = WalkData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Ramp\nStart[%s]\nEnd[%s]\nPct[%f]"), *WalkData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), WalkData.PercentTimeAppliedSoFar);

#endif

			return true;
		}
	}

	// no blocking hit, or no walkable ramp
	return false;
}

bool UTitanGroundModeBase::ApplyStepUpMove(FTitanMoveData& WalkData, FOptionalFloorCheckResult& StepUpFloorResult)
{
	// are we hitting something?
	if (WalkData.MoveHitResult.IsValidBlockingHit())
	{
		// is this a surface we can step up on?
		if (UGroundMovementUtils::CanStepUpOnHitSurface(WalkData.MoveHitResult))
		{
			// hit a barrier or unwalkable surface, try to step up and onto it
			const FVector PreStepUpLocation = MovingComponentSet.UpdatedComponent->GetComponentLocation();
			const FVector DownwardDir = -MutableMoverComponent->GetUpDirection();

			if (!UGroundMovementUtils::TryMoveToStepUp(MovingComponentSet, DownwardDir, CommonLegacySettings->MaxStepHeight, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->FloorSweepDistance, WalkData.OriginalMoveDelta * (1.f - WalkData.PercentTimeAppliedSoFar), WalkData.MoveHitResult, CurrentFloor, false, &StepUpFloorResult, WalkData.MoveRecord))
			{
				// update the time percentage
				// WalkData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(WalkData.PercentTimeAppliedSoFar, WalkData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

				const FVector ArrowEnd = WalkData.MoveHitResult.bBlockingHit ? WalkData.MoveHitResult.Location : WalkData.MoveHitResult.TraceEnd;
				const FColor ArrowColor = WalkData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

				UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Step Up\nStart[%s]\nEnd[%s]\nPct[%f]"), *WalkData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), WalkData.PercentTimeAppliedSoFar);

#endif

				return true;
			}
		}
		else if (WalkData.MoveHitResult.Component.IsValid() && !WalkData.MoveHitResult.Component.Get()->CanCharacterStepUp(Cast<APawn>(WalkData.MoveHitResult.GetActor())))
		{
			return true;
		}
	}

	// either not a blocking obstacle or not a climbable obstacle
	return false;
}

bool UTitanGroundModeBase::ApplySlideAlongWall(FTitanMoveData& WalkData)
{
	// are we hitting something?
	if (WalkData.MoveHitResult.IsValidBlockingHit())
	{
		// tell the mover component to handle the impact
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Walking, WalkData.MoveHitResult, WalkData.OriginalMoveDelta);
		MutableMoverComponent->HandleImpact(ImpactParams);

		// slide along the wall
		float SlidePct = 1.0f - WalkData.PercentTimeAppliedSoFar;

		float SlideAmount = UGroundMovementUtils::TryWalkToSlideAlongSurface(MovingComponentSet, WalkData.OriginalMoveDelta, SlidePct, WalkData.TargetOrientQuat, WalkData.MoveHitResult.Normal, WalkData.MoveHitResult, true, WalkData.MoveRecord, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->MaxStepHeight);

		// update the time percentage
		WalkData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(WalkData.PercentTimeAppliedSoFar, SlideAmount);

#if ENABLE_VISUAL_LOG

		const FVector ArrowEnd = WalkData.MoveHitResult.bBlockingHit ? WalkData.MoveHitResult.Location : WalkData.MoveHitResult.TraceEnd;
		const FColor ArrowColor = WalkData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

		UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Slide\nStart[%s]\nEnd[%s]\nPct[%f]"), *WalkData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), WalkData.PercentTimeAppliedSoFar);

#endif

		return true;
	}

	// not a blocking obstacle, can't slide along
	return false;
}

bool UTitanGroundModeBase::ApplyFloorHeightAdjustment(FTitanMoveData& WalkData)
{
	// ensure we're standing on a walkable floor
	if (CurrentFloor.IsWalkableFloor())
	{

#if ENABLE_VISUAL_LOG
		const FVector ArrowStart = MovingComponentSet.UpdatedPrimitive->GetComponentLocation();
#endif

		// adjust our height to match the floor
		UGroundMovementUtils::TryMoveToAdjustHeightAboveFloor(MovingComponentSet, CurrentFloor, CommonLegacySettings->MaxWalkSlopeCosine, WalkData.MoveRecord);

#if ENABLE_VISUAL_LOG
		
		const FVector ArrowEnd = MovingComponentSet.UpdatedPrimitive->GetComponentLocation();
		const FColor ArrowColor = FColor::Blue;

		UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Floor Adjust\nStart[%s]\nEnd[%s]"), *ArrowStart.ToCompactString(), *ArrowEnd.ToCompactString());

#endif

		return true;
	}

	// couldn't adjust, the floor is not walkable
	return false;
}

bool UTitanGroundModeBase::ApplyIdleCorrections(FTitanMoveData& WalkData)
{
	if (WalkData.MoveHitResult.bStartPenetrating)
	{
		// The floor check failed because it started in penetration
		// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
		WalkData.MoveHitResult.TraceEnd = WalkData.MoveHitResult.TraceStart + FVector(0.f, 0.f, 2.4f);

		// compute the depenetration adjustment vector
		FVector RequestedAdjustment = UMovementUtils::ComputePenetrationAdjustment(WalkData.MoveHitResult);

		// set up the component flags for the depenetration test
		const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
		EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;
		MoveComponentFlags = (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents);

		// move the component to resolve the penetration
		UMovementUtils::TryMoveToResolvePenetration(MovingComponentSet, MoveComponentFlags, RequestedAdjustment, WalkData.MoveHitResult, MovingComponentSet.UpdatedComponent->GetComponentQuat(), WalkData.MoveRecord);

#if ENABLE_VISUAL_LOG

		const FVector ArrowEnd = WalkData.MoveHitResult.bBlockingHit ? WalkData.MoveHitResult.Location : WalkData.MoveHitResult.TraceEnd;
		const FColor ArrowColor = WalkData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

		UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, WalkData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Resolve Penetration\nStart[%s]\nEnd[%s]\nPct[%f]"), *WalkData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), WalkData.PercentTimeAppliedSoFar);

#endif

		return true;
	}

	// no initial penetration
	return false;
}

bool UTitanGroundModeBase::HandleFalling(FMoverTickEndData& OutputState, FMovementRecord& MoveRecord, FHitResult& Hit, float TimeAppliedSoFar)
{
	if (!CurrentFloor.IsWalkableFloor() && !Hit.bStartPenetrating)
	{
		// no floor or not walkable, so let's let the airborne movement mode deal with it
		OutputState.MovementEndState.NextModeName = GetFallingModeName();

		// set the remaining time
		OutputState.MovementEndState.RemainingMs = DeltaMs - TimeAppliedSoFar;

		// update the move record's delta seconds
		MoveRecord.SetDeltaSeconds((DeltaMs - OutputState.MovementEndState.RemainingMs) * 0.001f);

		// capture the final movement state
		CaptureFinalState(CurrentFloor, MoveRecord);

		// update the last fall time on the blackboard
		SimBlackboard->Set(TitanBlackboard::LastFallTime, CurrentSimulationTime);

#if ENABLE_VISUAL_LOG

		UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanGroundModeBase: Switching to Falling"));

#endif

		return true;
	}

	return false;
}

void UTitanGroundModeBase::CaptureFinalState(const FFloorCheckResult& FloorResult, const FMovementRecord& Record) const
{
	FRelativeBaseInfo BaseInfo = UpdateFloorAndBaseInfo(FloorResult);

	// TODO: Update Main/large movement record with substeps from our local record

	if (BaseInfo.HasRelativeInfo())
	{
		OutDefaultSyncState->SetTransforms_WorldSpace(MovingComponentSet.UpdatedComponent->GetComponentLocation(),
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			Record.GetRelevantVelocity(),
			BaseInfo.MovementBase.Get(), BaseInfo.BoneName);
	}
	else
	{
		OutDefaultSyncState->SetTransforms_WorldSpace(MovingComponentSet.UpdatedComponent->GetComponentLocation(),
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			Record.GetRelevantVelocity(),
			nullptr);	// no movement base
	}

	MovingComponentSet.UpdatedComponent->ComponentVelocity = OutDefaultSyncState->GetVelocity_WorldSpace();
}

FRelativeBaseInfo UTitanGroundModeBase::UpdateFloorAndBaseInfo(const FFloorCheckResult& FloorResult) const
{
	FRelativeBaseInfo ReturnBaseInfo;

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

	if (FloorResult.IsWalkableFloor() && UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
	{
		ReturnBaseInfo.SetFromFloorResult(FloorResult);

		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, ReturnBaseInfo);
	}
	else
	{

		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	return ReturnBaseInfo;
}

const FName& UTitanGroundModeBase::GetFallingModeName() const
{
	return CommonLegacySettings->AirMovementModeName;
}
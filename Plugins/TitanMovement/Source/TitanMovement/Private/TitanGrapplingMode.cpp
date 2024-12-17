// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanGrapplingMode.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"

#include "Curves/CurveFloat.h"

#include "MoverComponent.h"
#include "TitanMoverComponent.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "VisualLogger/VisualLogger.h"

// Gameplay Tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_GrappleBoost, "Titan.Movement.Grappling.Boost");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_GrappleArrival, "Titan.Movement.Grappling.Arrival");

UTitanGrapplingMode::UTitanGrapplingMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	BoostingTag = TAG_Titan_Movement_GrappleBoost;
	ArrivalTag = TAG_Titan_Movement_GrappleArrival;
}

void UTitanGrapplingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FMoverDefaultSyncState* MoveSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FTitanTagsSyncState* MoveTagsState = StartState.SyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();
	check(MoveSyncState);
	check(MoveTagsState);

	const UTitanMoverComponent* TitanComp = Cast<UTitanMoverComponent>(GetMoverComponent());
	check(TitanComp);

	// return a zero move if movement is disabled
	if (TitanComp->IsMovementDisabled())
	{

		OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
		OutProposedMove.LinearVelocity = FVector::ZeroVector;

		return;
	}

	// convert the time step to delta seconds
	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// calculate the difference towards the goal
	FVector MoveGrappleGoal = FVector::ZeroVector;
	TitanComp->GetSimBlackboard()->TryGet(TitanBlackboard::GrappleGoal, MoveGrappleGoal);

	FVector MoveDir = MoveGrappleGoal - TitanComp->GetUpdatedComponentTransform().GetLocation();

	// get the grapple goal impact normal
	FVector MoveGrappleNormal = FVector::ZeroVector;
	TitanComp->GetSimBlackboard()->TryGet(TitanBlackboard::GrappleNormal, MoveGrappleNormal);
	
	MoveGrappleNormal = MoveGrappleNormal.GetSafeNormal2D();

	// get the distance to the grapple goal
	const float DistanceToGoal = MoveDir.Size();

	// normalize the difference to get the movement direction
	MoveDir = MoveDir.GetSafeNormal();

	// check if we've arrived at the goal
	const bool bArrived = DistanceToGoal < ArrivalTolerance;

	// check if we're grapple boosting
	bool bBoosting = MoveTagsState->HasTagExact(BoostingTag);

	// if we've arrived, try to get as close as we can to the goal, then skip
	if (bArrived)
	{
		OutProposedMove.LinearVelocity = MoveDir * DistanceToGoal / DeltaSeconds;

		const FRotator TargetRot = UKismetMathLibrary::MakeRotFromXZ(-MoveGrappleNormal, FVector::UpVector);

		OutProposedMove.AngularVelocity = UMovementUtils::ComputeAngularVelocity(MoveSyncState->GetOrientation_WorldSpace(), TargetRot, DeltaSeconds, TurningRate);

		return;
	}

	// calculate the time spent in grapple mode
	float StartTime = 0.0f;
	TitanComp->GetSimBlackboard()->TryGet(TitanBlackboard::GrappleStartTime, StartTime);

	const float TimeGrappling = TimeStep.BaseSimTimeMs - StartTime;

	// calculate the move

	// set the orientation to the move dir
	const FVector FlatDir = MoveDir.GetSafeNormal2D();

	// apply the optional scaling curves to the speed
	float SpeedScaling = 1.0f;

	if (SpeedScalingOverTime)
	{
		SpeedScaling = SpeedScalingOverTime->GetFloatValue(TimeGrappling * 0.001f);
	}

	float ApproachScaling = 1.0f;
	const float SlowdownPct = DistanceToGoal < SlowdownDistance ? DistanceToGoal / SlowdownDistance : 1.0f;

	if (ApproachSpeedScaling)
	{
		ApproachScaling = ApproachSpeedScaling->GetFloatValue(SlowdownPct);

	}

	// choose the speed based on whether we're grapple boosting
	float Speed = bBoosting ? BoostMaxSpeed : MaxSpeed;

	Speed *= SpeedScaling;
	Speed *= ApproachScaling;

	// calculate the velocity with a Seek behavior
	const FVector DesiredVelocity = MoveDir * Speed;
	FVector SteeringAccel = DesiredVelocity - MoveSyncState->GetVelocity_WorldSpace();

	const float AccelMagnitude = FMath::Min(SteeringAccel.Size() / DeltaSeconds, Acceleration);

	SteeringAccel = SteeringAccel.GetSafeNormal() * AccelMagnitude;

	OutProposedMove.LinearVelocity = MoveSyncState->GetVelocity_WorldSpace() + (SteeringAccel * DeltaSeconds);

	// calculate the angular velocity
	OutProposedMove.AngularVelocity = UMovementUtils::ComputeAngularVelocity(MoveSyncState->GetOrientation_WorldSpace(), FlatDir.ToOrientationRotator(), DeltaSeconds, TurningRate);

	// check the expected distance to see if we're about to overshoot the goal
	float EstimatedCoveredDistance = (OutProposedMove.LinearVelocity * DeltaSeconds).Size();

	if (EstimatedCoveredDistance > DistanceToGoal)
	{
		// clamp the speed to the distance to goal so we hit it exactly
		OutProposedMove.LinearVelocity = OutProposedMove.LinearVelocity.GetSafeNormal() * (DistanceToGoal / DeltaSeconds);
	}

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = MoveSyncState->GetLocation_WorldSpace();
		const FVector ArrowEnd = ArrowStart + OutProposedMove.LinearVelocity;
		const FColor ArrowColor = bBoosting ? FColor::Orange : FColor::Yellow;
		const FString Arrived = bArrived ? "true" : "false";
		const FString Boost = bBoosting ? "true" : "false";

		UE_VLOG_ARROW(this, VLogTitanMoverGenerateMove, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Grapple Move\nVel[%s]\nAng[%s]\nArrived[%s]\nBoost[%s]"), *OutProposedMove.LinearVelocity.ToCompactString(), *OutProposedMove.AngularVelocity.ToCompactString(), *Arrived, *Boost);
	}

#endif
}

bool UTitanGrapplingMode::PrepareSimulationData(const FSimulationTickParams& Params)
{
	if (Super::PrepareSimulationData(Params))
	{
		if (!SimBlackboard->TryGet(TitanBlackboard::GrappleGoal, GrappleGoal))
		{
			UE_LOG(LogTitanMover, Error, TEXT("Grapple Error: No Grapple Goal in the Blackboard"));
			return false;
		}

		if (!SimBlackboard->TryGet(TitanBlackboard::GrappleStartTime, GrappleStartTime))
		{
			UE_LOG(LogTitanMover, Error, TEXT("Grapple Error: No Grapple Start Time in the Blackboard"));
			return false;
		}

		// cache the starting location so we can compare it later and determine if we're stuck
		StartingLocation = MovingComponentSet.UpdatedComponent->GetComponentLocation();

		// get the distance to the grapple goal
		const float DistanceToGoal = (GrappleGoal - StartingLocation).Size();

		// have we arrived at the grapple point?
		bWasArrived = TagsSyncState->HasTagExact(ArrivalTag);

		// were we boosting during the last sim step?
		bWasBoosting = TagsSyncState->HasTagExact(BoostingTag);

		// assume we haven't collided against anything yet
		bAbortOnCollision = false;

		return true;
	}

	// data prep failed
	return false;
}

void UTitanGrapplingMode::ApplyMovement(FMoverTickEndData& OutputState)
{
	// initialize the move data
	FTitanMoveData GrappleData;
	GrappleData.MoveRecord.SetDeltaSeconds(DeltaTime);

	GrappleData.OriginalMoveDelta = ProposedMove->LinearVelocity * DeltaTime;
	GrappleData.CurrentMoveDelta = GrappleData.OriginalMoveDelta;

	// invalidate the floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	// calculate the orientation quaternion
	bool bIsOrientationChanging = CalculateOrientationChange(GrappleData.TargetOrientQuat);

	if (!GrappleData.CurrentMoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		// attempt a free move
		UMovementUtils::TrySafeMoveUpdatedComponent(MovingComponentSet, GrappleData.CurrentMoveDelta, GrappleData.TargetOrientQuat, true, GrappleData.MoveHitResult, ETeleportType::None, GrappleData.MoveRecord);

		// update the time percentage applied so far
		GrappleData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(GrappleData.PercentTimeAppliedSoFar, GrappleData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

		{
			const FVector ArrowEnd = GrappleData.MoveHitResult.bBlockingHit ? GrappleData.MoveHitResult.Location : GrappleData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = GrappleData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, GrappleData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Grapple First\nStart[% s]\nEnd[% s]\nPct[% f]"), *GrappleData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), GrappleData.PercentTimeAppliedSoFar);
		}

#endif
	}

	if (GrappleData.MoveHitResult.IsValidBlockingHit())
	{
		// tell the mover component to handle the impact
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Flying, GrappleData.MoveHitResult, GrappleData.CurrentMoveDelta);
		MutableMoverComponent->HandleImpact(ImpactParams);

		// have we hit a surface we can't slide off of?
		float HitDot = FVector::DotProduct(GrappleData.MoveHitResult.ImpactNormal, GrappleData.CurrentMoveDelta.GetSafeNormal2D());

		// run a secondary dot product against the up direction to rule out cases where we're sliding against a floor 
		float VerticalDot = FMath::Abs(FVector::DotProduct(GrappleData.MoveHitResult.ImpactNormal, MutableMoverComponent->GetUpDirection()));

		if (HitDot < MinCollisionDot && VerticalDot < MinCollisionDot)
		{
#if ENABLE_VISUAL_LOG

			{
				UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanGrapplingMode: Aborting Grapple due to collision. Dot[%f] VerticalDot[%f]"), HitDot, VerticalDot);
			}

#endif

			// raise the collision flag
			bAbortOnCollision = true;

			// if we're grapple boosting, we want the boost jump to take us out of grapple mode on the next transition check
			if (!bWasBoosting)
			{
				// abort the grapple and set the next movement mode
				OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
			}

			// set the remaining time
			OutputState.MovementEndState.RemainingMs = DeltaMs - (DeltaMs * GrappleData.PercentTimeAppliedSoFar);
			GrappleData.MoveRecord.SetDeltaSeconds(DeltaTime * GrappleData.PercentTimeAppliedSoFar);

			// capture the final state and return. We signal that we want to zero out the velocity
			CaptureFinalState(GrappleData.MoveRecord, true);
			return;
		}

		// try to slide the remaining distance along the surface.
		UMovementUtils::TryMoveToSlideAlongSurface(MovingComponentSet, GrappleData.CurrentMoveDelta, 1.f - GrappleData.PercentTimeAppliedSoFar, GrappleData.TargetOrientQuat, GrappleData.MoveHitResult.Normal, GrappleData.MoveHitResult, true, GrappleData.MoveRecord);

		// update the time percentage applied so far
		GrappleData.PercentTimeAppliedSoFar = UpdateTimePercentAppliedSoFar(GrappleData.PercentTimeAppliedSoFar, GrappleData.MoveHitResult.Time);

#if ENABLE_VISUAL_LOG

		{
			const FVector ArrowEnd = GrappleData.MoveHitResult.bBlockingHit ? GrappleData.MoveHitResult.Location : GrappleData.MoveHitResult.TraceEnd;
			const FColor ArrowColor = GrappleData.MoveHitResult.bBlockingHit ? FColor::Red : FColor::Green;

			UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, GrappleData.MoveHitResult.TraceStart, ArrowEnd, ArrowColor, TEXT("Grapple Slide\nStart[%s]\nEnd[%s]\nPct[%f]"), *GrappleData.MoveHitResult.TraceStart.ToCompactString(), *ArrowEnd.ToCompactString(), GrappleData.PercentTimeAppliedSoFar);
		}

#endif

	}

	// capture the final state
	CaptureFinalState(GrappleData.MoveRecord);
}

void UTitanGrapplingMode::PostMove(FMoverTickEndData& OutputState)
{
	// add the mode tags
	Super::PostMove(OutputState);

	// calculate the stamina delta for this step
	float StaminaUse = TitanSettings->StaminaRegeneration * (DeltaMs - OutputState.MovementEndState.RemainingMs) * 0.001f;

	// update the stamina
	UpdateStamina(StaminaUse);

	// check if we've arrived at the goal
	bool bArrived = (OutDefaultSyncState->GetLocation_WorldSpace() - GrappleGoal).Size() < ArrivalTolerance;

	// copy over the grapple boost tag
	if (bWasBoosting)
	{
		OutTagsSyncState->AddTag(BoostingTag);

		// if we've been blocked by collision, signal arrival so the grapple boost jump can take us out on the next transition
		if (bAbortOnCollision)
		{
			bArrived = true;
		}
	}

	if (bArrived || TagsSyncState->HasTagExact(ArrivalTag))
	{
		OutTagsSyncState->AddTag(ArrivalTag);
	}

	// have we just arrived at the goal this sim step?
	if (bArrived && !bWasArrived)
	{
		// send the grapple arrival event
		if (ArrivalEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), ArrivalEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanGrapplingMode: Arrived at Goal"));
		}

#endif
	}

	// check if we should grapple boost
	if (KinematicInputs
		&& KinematicInputs->bIsJumpJustPressed
		&& !bWasBoosting
		&& !bArrived
		&& !bWasArrived)
	{
		// not at the destination, so add the grapple boost tag
		OutTagsSyncState->AddTag(BoostingTag);

		// send the grapple boost event
		if (BoostEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), BoostEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanGrapplingMode: Grapple Boost Start"));
		}

#endif
	}

	// check how long we've been grappling
	const float TimeGrappling = CurrentSimulationTime - GrappleStartTime;

	// skip stuck check for a set amount of time.
	// This prevents us from canceling out of grapple when we were supposed to be standing still due to an initial delay by the multiplier curve
	if (!(bArrived || bWasArrived) && TimeGrappling > StuckMinTime * 1000.0f)
	{
		// check how much we've moved this frame
		const float MoveDelta = (OutDefaultSyncState->GetLocation_WorldSpace() - StartingLocation).Size();

		// have we moved so little we should be considered stuck?
		if (MoveDelta < StuckMovementDistance)
		{
#if ENABLE_VISUAL_LOG

			{
				UE_VLOG(this, VLogTitanMover, Log, TEXT("UTitanGrapplingMode: Grapple stuck - aborting."));
			}

#endif
			
			// if we were boosting, signal arrival so that the grapple boost jump takes us out of grapple mode on the next transition check
			if (bWasBoosting)
			{
				OutTagsSyncState->AddTag(ArrivalTag);
				return;
			}

			// end the grapple
			OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;

			// negate the velocity on the out sync state
			OutDefaultSyncState->SetTransforms_WorldSpace(
				OutDefaultSyncState->GetLocation_WorldSpace(),
				MovingComponentSet.UpdatedComponent->GetComponentRotation(),
				FVector::ZeroVector,
				nullptr); // no movement base

			// negate the component velocity
			MovingComponentSet.UpdatedComponent->ComponentVelocity = FVector::ZeroVector;

			// save the last fall time to the blackboard
			SimBlackboard->Set(TitanBlackboard::LastFallTime, CurrentSimulationTime);

			// invalidate the last grapple time
			SimBlackboard->Invalidate(TitanBlackboard::LastGrappleTime);
			
			return;
		}
	}

	// are we switching to falling state?
	if (OutputState.MovementEndState.NextModeName == CommonLegacySettings->AirMovementModeName)
	{
		// save the last fall time to the blackboard
		SimBlackboard->Set(TitanBlackboard::LastFallTime, CurrentSimulationTime);

		// only save grapple time if we haven't collided against something unexpectedly
		if (bAbortOnCollision)
		{
			// invalidate the last grapple time
			SimBlackboard->Invalidate(TitanBlackboard::LastGrappleTime);
		}
		else {
			// save the last grapple time to the blackboard
			SimBlackboard->Set(TitanBlackboard::LastGrappleTime, CurrentSimulationTime);
		}
		
	}

}

void UTitanGrapplingMode::CaptureFinalState(FMovementRecord& Record, bool bOverrideVelocity) const
{	
	const FVector FinalLocation = MovingComponentSet.UpdatedComponent->GetComponentLocation();

	// zero out the velocity in case we bumped into something so we don't bounce off randomly
	const FVector FinalVelocity = bOverrideVelocity ? FVector::ZeroVector : Record.GetRelevantVelocity();

	// update the output sync state
	OutDefaultSyncState->SetTransforms_WorldSpace(
		FinalLocation,
		MovingComponentSet.UpdatedComponent->GetComponentRotation(),
		FinalVelocity,
		nullptr); // no movement base

	// update the component velocity
	MovingComponentSet.UpdatedComponent->ComponentVelocity = FinalVelocity;

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = FinalLocation;
		const FVector ArrowEnd = ArrowStart + FinalVelocity;
		const FColor ArrowColor = FColor::Cyan;

		UE_VLOG_ARROW(this, VLogTitanMoverSimulation, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Grapple Final State\nLoc[%s]\nRot[%s]\nVel[%s]"), *FinalLocation.ToCompactString(), *MovingComponentSet.UpdatedComponent->GetComponentRotation().ToCompactString(), *FinalVelocity.ToCompactString());
	}

#endif
}

FTitanGrappleEffect::FTitanGrappleEffect()
{
	GrappleGoal = FVector::ZeroVector;
	GrappleNormal = FVector::ZeroVector;
}

bool FTitanGrappleEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	// get the movement settings
	const UTitanMovementSettings* TitanSettings = ApplyEffectParams.MoverComp->FindSharedSettings<UTitanMovementSettings>();

	// get the blackboard
	UMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard_Mutable();

	if (TitanSettings && SimBlackboard && ApplyEffectParams.TimeStep)
	{
		// set the fall time in the blackboard
		SimBlackboard->Set(TitanBlackboard::GrappleStartTime, ApplyEffectParams.TimeStep->BaseSimTimeMs);

		// set the grapple goal in the blackboard
		SimBlackboard->Set(TitanBlackboard::GrappleGoal, GrappleGoal);
		SimBlackboard->Set(TitanBlackboard::GrappleNormal, GrappleNormal);

		// set up the movement mode transition
		OutputState.MovementMode = TitanSettings->GrapplingMovementModeName;

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(ApplyEffectParams.MoverComp->GetOwner(), VLogTitanMover, Log, TEXT("Grapple Effect\nGoal[%s]\nNormal[%s]"), *GrappleGoal.ToCompactString(), *GrappleNormal.ToCompactString());
		}

#endif

		return true;
	}

	return false;
}

FInstantMovementEffect* FTitanGrappleEffect::Clone() const
{
	FTitanGrappleEffect* CopyPtr = new FTitanGrappleEffect(*this);
	return CopyPtr;
}

void FTitanGrappleEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << GrappleGoal;
}

UScriptStruct* FTitanGrappleEffect::GetScriptStruct() const
{
	return FTitanGrappleEffect::StaticStruct();
}

FString FTitanGrappleEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Titan Grapple"));
}

void FTitanGrappleEffect::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

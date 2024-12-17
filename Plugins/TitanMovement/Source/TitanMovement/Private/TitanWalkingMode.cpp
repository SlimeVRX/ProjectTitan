// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanWalkingMode.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"

#include "MoverComponent.h"
#include "TitanMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/ModularMovement.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "VisualLogger/VisualLogger.h"

// Gameplay Tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Sprinting, "Titan.Movement.Walking.Sprinting");

void UTitanWalkingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// get the mover component
	const UMoverComponent* MoverComp = GetMoverComponent();
	check(MoverComp);

	// get the inputs
	const FCharacterDefaultInputs* MoveKinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FTitanMovementInputs* MoveTitanInputs = StartState.InputCmd.InputCollection.FindDataByType<FTitanMovementInputs>();

	// get the sync states
	const FMoverDefaultSyncState* DefaultSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(DefaultSyncState);

	const FTitanStaminaSyncState* MoveStaminaSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FTitanStaminaSyncState>();
	check(MoveStaminaSyncState);

	// get the blackboard
	UMoverBlackboard* MoveBlackboard = nullptr;
	if (MoverComp)
	{
		MoveBlackboard = MoverComp->GetSimBlackboard_Mutable();
	}

	// convert the time step to seconds
	float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// build the proposed move
	////////////////////////////////////////////////////////////////////////

	FFloorCheckResult LastFloorResult;
	FVector MovementNormal;

	// Look for a walkable floor on the blackboard so we can walk along slopes
	if (MoveBlackboard && MoveBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		// use the floor result normal
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}
	else
	{
		// no floor, so default to the up direction
		MovementNormal = MoverComp->GetUpDirection();
	}

	FRotator IntendedOrientation_WorldSpace;

	// do we have an orientation intent?
	if (!MoveKinematicInputs || MoveKinematicInputs->OrientationIntent.IsNearlyZero())
	{
		// no orientation intent, default to last frame's orientation
		IntendedOrientation_WorldSpace = DefaultSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		// use the input orientation 
		IntendedOrientation_WorldSpace = MoveKinematicInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	// calculate a speed boost scaling so we better retain movement plane speed on steep slopes (up to a point)
	float SlopeBoost = 1.0f / FMath::Max(FMath::Abs(MovementNormal.Dot(FVector::UpVector)), 0.75f);

	// build the ground move parameters
	FGroundMoveParams Params;

	// do we have an input?
	if (MoveKinematicInputs)
	{
		// use the move input
		Params.MoveInputType = MoveKinematicInputs->GetMoveInputType();
		Params.MoveInput = MoveKinematicInputs->GetMoveInput_WorldSpace();
	}
	else
	{
		// default to a null input
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}

	// set the rest of the ground move params
	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = DefaultSyncState->GetVelocity_WorldSpace();
	Params.PriorOrientation = DefaultSyncState->GetOrientation_WorldSpace();
	Params.GroundNormal = MovementNormal;
	Params.DeltaSeconds = DeltaSeconds;

	const bool bExhausted = MoveStaminaSyncState->IsExhausted();
	const bool bSprinting = MoveTitanInputs && MoveTitanInputs->bIsSprintPressed && MoveStaminaSyncState->GetStamina() > 0.0f;


	// are we exhausted?
	if (bExhausted)
	{

		// use exhausted walk params
		Params.TurningRate = ExhaustedTurningRate;
		Params.TurningBoost = ExhaustedTurningBoost;
		Params.MaxSpeed = ExhaustedMaxSpeed * SlopeBoost;
		Params.Acceleration = ExhaustedAcceleration * SlopeBoost;
		Params.Deceleration = ExhaustedDeceleration;

		// do we want to sprint and have enough stamina?
	}
	else if (bSprinting) {

		// use sprint walk params
		Params.TurningRate = SprintTurningRate;
		Params.TurningBoost = SprintTurningBoost;
		Params.MaxSpeed = SprintMaxSpeed * SlopeBoost;
		Params.Acceleration = SprintAcceleration * SlopeBoost;
		Params.Deceleration = SprintDeceleration;

	}
	else {

		// use the default legacy walk params
		Params.TurningRate = CommonLegacySettings->TurningRate;
		Params.TurningBoost = CommonLegacySettings->TurningBoost;
		Params.MaxSpeed = CommonLegacySettings->MaxSpeed * SlopeBoost;
		Params.Acceleration = CommonLegacySettings->Acceleration * SlopeBoost;
		Params.Deceleration = CommonLegacySettings->Deceleration;

	}

	// are we moving faster than max speed?
	if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, Params.MaxSpeed))
	{
		// default to regular friction
		Params.Friction = CommonLegacySettings->GroundFriction;
	}
	else
	{
		// use the braking friction multiplier
		Params.Friction = CommonLegacySettings->bUseSeparateBrakingFriction ? CommonLegacySettings->BrakingFriction : CommonLegacySettings->GroundFriction;
		Params.Friction *= CommonLegacySettings->BrakingFrictionFactor;
	}

	// output the proposed move
	OutProposedMove = UGroundMovementUtils::ComputeControlledGroundMove(Params);

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = DefaultSyncState->GetLocation_WorldSpace();
		const FVector ArrowEnd = ArrowStart + OutProposedMove.LinearVelocity;
		const FColor ArrowColor = bExhausted ? FColor::Magenta : bSprinting ? FColor::Orange : FColor::Yellow;
		const FString Sprinting = bSprinting ? "true" : "false";
		const FString Exhausted = bExhausted ? "true" : "false";

		UE_VLOG_ARROW(this, VLogTitanMoverGenerateMove, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Walk Move\nVel[%s]\nAng[%s]\nSprint[%s]\nExhausted[%s]"), *OutProposedMove.LinearVelocity.ToCompactString(), *OutProposedMove.AngularVelocity.ToCompactString(), *Sprinting, *Exhausted);
	}

#endif
}

bool UTitanWalkingMode::CheckIfMovementIsDisabled()
{
	return false;
}

void UTitanWalkingMode::PostMove(FMoverTickEndData& OutputState)
{
	// super handles mode-specific tags
	Super::PostMove(OutputState);

	// calculate the stamina use for this sim frame
	float StaminaUse = 0.0f;

	if (SprintStaminaConsumptionCurve)
	{
		// get the linear velocity
		float CurrentSpeed = MovingComponentSet.UpdatedComponent->ComponentVelocity.Size();

		StaminaUse = SprintStaminaConsumptionCurve->GetFloatValue(CurrentSpeed);
	}

	// calculate the stamina delta for this step
	StaminaUse *= (DeltaMs - OutputState.MovementEndState.RemainingMs) * 0.001f;

	// update the stamina
	UpdateStamina(StaminaUse);

	// add the sprinting tag if necessary
	if (!OutStaminaSyncState->IsExhausted() && StaminaUse < 0.0f)
	{
		OutTagsSyncState->AddTag(SprintingTag);
	}

	// have we started sprinting?
	if (OutTagsSyncState->HasTagExact(SprintingTag) && !TagsSyncState->HasTagExact(SprintingTag))
	{
		// send the sprint start event
		if (SprintStartEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), SprintStartEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanWalkingMode: Sprint Start"));
		}

#endif

	}
	// are we done sprinting?
	else if (!OutTagsSyncState->HasTagExact(SprintingTag) && TagsSyncState->HasTagExact(SprintingTag))
	{
		// send the sprint end event
		if (SprintEndEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), SprintEndEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanWalkingMode: Sprint End"));
		}

#endif
	}
}

bool UTitanWalkingMode::HandleFalling(FMoverTickEndData& OutputState, FMovementRecord& MoveRecord, FHitResult& Hit, float TimeAppliedSoFar)
{
	if (Super::HandleFalling(OutputState, MoveRecord, Hit, TimeAppliedSoFar))
	{
		// send the sprint end event
		if (SprintEndEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), SprintEndEvent, FGameplayEventData());
		}

		return true;
	}

	return false;
}
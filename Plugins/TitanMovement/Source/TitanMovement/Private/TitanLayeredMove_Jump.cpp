// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanLayeredMove_Jump.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"
#include "TitanMoverComponent.h"

#include "MoverSimulationTypes.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "VisualLogger/VisualLogger.h"


FTitanLayeredMove_Jump::FTitanLayeredMove_Jump()
{
	DurationMs = 0.0f;
	MixMode = EMoveMixMode::OverrideVelocity;

	UpwardsSpeed = 0.0f;

	Momentum = FVector::ZeroVector;

	AirControl = 1.0f;

	bTruncateOnJumpRelease = true;
	bOverrideHorizontalMomentum = false;
	bOverrideVerticalMomentum = false;
}

bool FTitanLayeredMove_Jump::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const UCommonLegacyMovementSettings* CommonLegacySettings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
	check(CommonLegacySettings);

	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(SyncState);

	// get the inputs
	const FCharacterDefaultInputs* MoveKinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	
	// if we're no longer falling, set the duration to zero
	if (TimeStep.BaseSimTimeMs != StartSimTimeMs && StartState.SyncState.MovementMode != CommonLegacySettings->AirMovementModeName)
	{
		DurationMs = 0.0f;
	}

	bool bJumping = false;

	if (MoveKinematicInputs)
	{
		bJumping = MoveKinematicInputs->bIsJumpPressed;
	}

	// if we're no longer pressing jump, set the duration to zero to end the upwards impulse
	if (bTruncateOnJumpRelease && !bJumping)
	{
		DurationMs = 0.0f;
	}

	const UTitanMoverComponent* TitanComp = Cast<UTitanMoverComponent>(MoverComp);
	check(TitanComp);

	// return a zero move if movement is disabled
	if (TitanComp->IsMovementDisabled())
	{
		OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
		OutProposedMove.LinearVelocity = FVector::ZeroVector;

		return true;
	}

	const FVector UpDir = MoverComp->GetUpDirection();

	// We can either override vertical velocity with the provided momentum, or grab it from the sync state
	FVector UpwardsVelocity = bOverrideVerticalMomentum ? Momentum.ProjectOnToNormal(UpDir) : SyncState->GetVelocity_WorldSpace().ProjectOnToNormal(UpDir);

	// We can either override move plane velocity with the provided momentum, or grab it from the sync state
	const FVector NonUpwardsVelocity = bOverrideHorizontalMomentum ? Momentum - UpwardsVelocity : SyncState->GetVelocity_WorldSpace() - UpwardsVelocity;
	
	// apply jump upwards speed
	UpwardsVelocity += UpDir * UpwardsSpeed;

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

	Params.MoveInput *= AirControl;

	FRotator IntendedOrientation_WorldSpace;

	// do we have an orientation intent?
	if (!MoveKinematicInputs || MoveKinematicInputs->OrientationIntent.IsNearlyZero())
	{
		// default to the previous frame orientation
		IntendedOrientation_WorldSpace = SyncState->GetOrientation_WorldSpace();
	}
	else
	{
		// use the input's orientation intent
		IntendedOrientation_WorldSpace = MoveKinematicInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	// Zero out the vertical input, vertical movement will be determined by gravity
	Params.MoveInput.Z = 0;

	// set the rest of the move params
	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = NonUpwardsVelocity + UpwardsVelocity;
	Params.PriorOrientation = SyncState->GetOrientation_WorldSpace();
	Params.DeltaSeconds = TimeStep.StepMs * 0.001f;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = 0.0f;

	if (MixMode == EMoveMixMode::OverrideVelocity)
	{
		// calculate the out proposed move
		OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);

		// Add velocity change due to gravity
		OutProposedMove.LinearVelocity += UMovementUtils::ComputeVelocityFromGravity(MoverComp->GetGravityAcceleration(), Params.DeltaSeconds);

		// save the fall time to the blackboard
		if (TimeStep.BaseSimTimeMs == StartSimTimeMs)
		{
			SimBlackboard->Set(TitanBlackboard::LastFallTime, StartSimTimeMs);
		}
	}
	else
	{
		// we do not support other mix modes
		ensure(0);
		return false;
	}

#if ENABLE_VISUAL_LOG

	{
		const FVector ArrowStart = SyncState->GetLocation_WorldSpace();
		const FVector ArrowEnd = ArrowStart + OutProposedMove.LinearVelocity;
		const FColor ArrowColor = FColor::Yellow;

		UE_VLOG_ARROW(MoverComp->GetOwner(), VLogTitanMoverGenerateMove, Log, ArrowStart, ArrowEnd, ArrowColor, TEXT("Jump Move\nVel[%s]\nAng[%s]"), *OutProposedMove.LinearVelocity.ToCompactString(), *OutProposedMove.AngularVelocity.ToCompactString());
	}

#endif

	return true;
}

FLayeredMoveBase* FTitanLayeredMove_Jump::Clone() const
{
	FTitanLayeredMove_Jump* CopyPtr = new FTitanLayeredMove_Jump(*this);
	return CopyPtr;
}

void FTitanLayeredMove_Jump::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << UpwardsSpeed;
	Ar << Momentum;
	Ar << AirControl;
	Ar << bTruncateOnJumpRelease;
}

UScriptStruct* FTitanLayeredMove_Jump::GetScriptStruct() const
{
	return FTitanLayeredMove_Jump::StaticStruct();
}

FString FTitanLayeredMove_Jump::ToSimpleString() const
{
	return FString::Printf(TEXT("Titan Jump"));
}

void FTitanLayeredMove_Jump::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
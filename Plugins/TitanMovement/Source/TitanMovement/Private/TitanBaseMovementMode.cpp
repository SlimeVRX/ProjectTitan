// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanBaseMovementMode.h"
#include "TitanMovementLogging.h"
#include "TitanMoverTypes.h"
#include "MoverComponent.h"
#include "TitanMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "TitanLayeredMove_Jump.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "VisualLogger/VisualLogger.h"

UTitanBaseMovementMode::UTitanBaseMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(UCommonLegacyMovementSettings::StaticClass());
	SharedSettingsClasses.Add(UTitanMovementSettings::StaticClass());
}
void UTitanBaseMovementMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// stub
}

void UTitanBaseMovementMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	// prepare the simulation data
	if (!PrepareSimulationData(Params))
	{
		UE_LOG(LogTitanMover, Error, TEXT("Couldn't prepare move simulation data for [%s]"), *GetNameSafe(this));
		return;
	}

	// build the output states
	BuildSimulationOutputStates(OutputState);

	// has movement been disabled?
	if (CheckIfMovementIsDisabled())
	{
		// update the output sync state
		OutDefaultSyncState->SetTransforms_WorldSpace(
			MovingComponentSet.UpdatedComponent->GetComponentLocation(),
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			FVector::ZeroVector,
			nullptr);

		// update the component velocity
		MovingComponentSet.UpdatedComponent->ComponentVelocity = FVector::ZeroVector;

		// give back all the time to the next state
		OutputState.MovementEndState.RemainingMs = 0.0f;
		return;
	}

	// handle anything else that needs to happen before we start moving
	PreMove(OutputState);

	// move the updated component
	ApplyMovement(OutputState);

	// handle anything else after the final location and velocity has been computed
	PostMove(OutputState);

	// log the final state of the updated comp
#if ENABLE_VISUAL_LOG
	
	{	
		const FVector LogLoc = MovingComponentSet.UpdatedComponent->GetComponentLocation();
		const FRotator LogRot = MovingComponentSet.UpdatedComponent->GetComponentRotation();
		const FVector LogVel = MovingComponentSet.UpdatedComponent->GetComponentVelocity();
		const float LogSpeed = LogVel.Size();

		UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("Final State:\nCurrent:[%s]\nNext[%s]\nLoc[%s]\nRot[%s]\nVel[%s]\nSpd[%f]"), *Params.StartState.SyncState.MovementMode.ToString(), *OutputState.MovementEndState.NextModeName.ToString(), *LogLoc.ToCompactString(), *LogRot.ToCompactString(), *LogVel.ToCompactString(), LogSpeed);
	}

#endif
}

bool UTitanBaseMovementMode::PrepareSimulationData(const FSimulationTickParams& Params)
{
	// get the Mover component
	MutableMoverComponent = Cast<UTitanMoverComponent>(GetMoverComponent());

	if (!MutableMoverComponent)
	{
		UE_LOG(LogTitanMover, Error, TEXT("PrepareSimulationData: Mutable Mover Component not valid."));
		return false;
	}

	// get the updated component set
	MovingComponentSet = Params.MovingComps;

	// if the updated component is not valid, abort the simulation
	if (!MovingComponentSet.UpdatedComponent.IsValid() || !MovingComponentSet.UpdatedPrimitive.IsValid())
	{
		UE_LOG(LogTitanMover, Error, TEXT("PrepareSimulationData: UpdatedComponent not valid."));
		return false;
	}

	// get the sync states
	StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	//check(StartingSyncState);

	StaminaSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FTitanStaminaSyncState>();
	//check(StaminaSyncState);

	TagsSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();
	//check(StaminaSyncState);

	// get the input structs
	KinematicInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	TitanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FTitanMovementInputs>();

	// get the proposed move
	ProposedMove = &Params.ProposedMove;

	// get the blackboard
	if (UMoverComponent* OwnerComponent = GetMoverComponent())
	{
		SimBlackboard = OwnerComponent->GetSimBlackboard_Mutable();
	}

	// get the velocity
	StartingVelocity = StartingSyncState->GetVelocity_WorldSpace();

	// get the time deltas
	DeltaMs = Params.TimeStep.StepMs;
	DeltaTime = Params.TimeStep.StepMs * 0.001f;
	CurrentSimulationTime = Params.TimeStep.BaseSimTimeMs;

	return true;
}

void UTitanBaseMovementMode::BuildSimulationOutputStates(FMoverTickEndData& OutputState)
{
	// create the output default sync state
	OutDefaultSyncState = &OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	// create the output stamina sync state
	OutStaminaSyncState = &OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FTitanStaminaSyncState>();

	// create the output tags sync state
	OutTagsSyncState = &OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FTitanTagsSyncState>();
	OutTagsSyncState->ClearTags();
}

bool UTitanBaseMovementMode::CheckIfMovementIsDisabled()
{
	return MutableMoverComponent->IsMovementDisabled();
}

void UTitanBaseMovementMode::PreMove(FMoverTickEndData& OutputState)
{
	// stub
}

void UTitanBaseMovementMode::ApplyMovement(FMoverTickEndData& OutputState)
{
	// stub
}

void UTitanBaseMovementMode::PostMove(FMoverTickEndData& OutputState)
{
	// add the movement mode tag
	OutTagsSyncState->AddTag(ModeTag);
}

bool UTitanBaseMovementMode::AttemptTeleport(const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity)
{
	if (MovingComponentSet.UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		OutDefaultSyncState->SetTransforms_WorldSpace(MovingComponentSet.UpdatedComponent->GetComponentLocation(),
			MovingComponentSet.UpdatedComponent->GetComponentRotation(),
			PriorVelocity,
			nullptr); // no movement base

		MovingComponentSet.UpdatedComponent->ComponentVelocity = PriorVelocity;
		return true;
	}

	return false;
}

float UTitanBaseMovementMode::UpdateTimePercentAppliedSoFar(float PreviousTimePct, float LastCollisionTime) const
{
	return PreviousTimePct + ((1.0f - PreviousTimePct) * LastCollisionTime);
}

bool UTitanBaseMovementMode::CalculateOrientationChange(FQuat& TargetOrientQuat)
{
	// set the move direction intent on the output sync state
	OutDefaultSyncState->MoveDirectionIntent = (ProposedMove->bHasDirIntent ? ProposedMove->DirectionIntent : FVector::ZeroVector);

	// get the start orientation
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	FRotator TargetOrient = StartingOrient;

	// apply orientation changes, if any
	if (!ProposedMove->AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove->AngularVelocity * DeltaTime);
	}

	// return the quat and whether orientation changed
	TargetOrientQuat = TargetOrient.Quaternion();
	return TargetOrient != StartingOrient;
}

void UTitanBaseMovementMode::UpdateStamina(float StaminaUse)
{
	// skip stamina deductions if stamina is disabled
	if (StaminaUse < 0.0f && !MutableMoverComponent->IsStaminaEnabled())
	{
		return;
	}

	// check if we've depleted or maxed out the stamina
	bool bStaminaDepleted, bStaminaMaxedOut;
	OutStaminaSyncState->UpdateStamina(StaminaUse, bStaminaDepleted, bStaminaMaxedOut, TitanSettings->bUseExhaustion);

	if (bStaminaDepleted)
	{
		// send the exhaustion event
		if (TitanSettings->ExhaustionEvent != FGameplayTag::EmptyTag)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), TitanSettings->ExhaustionEvent, FGameplayEventData());
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanBaseMovementMode: Stamina Exhausted"));
		}

#endif

	}

	if (bStaminaMaxedOut)
	{
		// were we exhausted?
		if (StaminaSyncState->IsExhausted())
		{
			// send the exhaustion recovery event
			if (TitanSettings->ExhaustionRecoveryEvent != FGameplayTag::EmptyTag)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MutableMoverComponent->GetOwner(), TitanSettings->ExhaustionRecoveryEvent, FGameplayEventData());
			}

#if ENABLE_VISUAL_LOG

			{
				UE_VLOG(this, VLogTitanMoverSimulation, Log, TEXT("UTitanBaseMovementMode: Stamina Maxed Out"));
			}

#endif
		}

	}

	// add the exhausted tag if pertinent
	if (OutStaminaSyncState->IsExhausted())
	{
		OutTagsSyncState->AddTag(TitanSettings->ExhaustedTag);
	}
}

void UTitanBaseMovementMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	// get the common legacy settings
	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));

	// get the titan shared settings
	TitanSettings = GetMoverComponent()->FindSharedSettings<UTitanMovementSettings>();
	ensureMsgf(TitanSettings, TEXT("Failed to find instance of TitanMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));

#if ENABLE_VISUAL_LOG
	// redirect Visual Logger to the owning Actor
	REDIRECT_TO_VLOG(GetMoverComponent()->GetOwner());
#endif

}

void UTitanBaseMovementMode::OnUnregistered()
{
	// release the shared settings pointers
	CommonLegacySettings = nullptr;
	TitanSettings = nullptr;

	Super::OnUnregistered();
}

#if ENABLE_VISUAL_LOG
void UTitanBaseMovementMode::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{

}
#endif
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "EngineDefines.h"
#include "TitanBaseMovementMode.generated.h"

// forward declarations
class USceneComponent;
class UPrimitiveComponent;

class UTitanMoverComponent;
struct FMoverDefaultSyncState;
struct FProposedMove;
struct FCharacterDefaultInputs;
class UCommonLegacyMovementSettings;

struct FTitanStaminaSyncState;
struct FTitanTagsSyncState;
struct FTitanMovementInputs;
class UTitanMovementSettings;
class UMoverBlackboard;

/**
 *  FTitanMoveData
 *  Holds utility data for moving the updated component during simulation ticks
 */
struct FTitanMoveData
{
	/** Original move delta for the simulation frame*/
	FVector OriginalMoveDelta;

	/** Move delta for the current stage of the simulation frame */
	FVector CurrentMoveDelta;

	/** Target orientation that we want the updated component to achieve */
	FQuat TargetOrientQuat;

	/** HitResult to hold any potential collision response data as we move the simulated component */
	FHitResult MoveHitResult;

	/** Record of all the movement we've incurred so far in the frame */
	FMovementRecord MoveRecord;

	/** Percentage of the simulation time slice we've used so far while moving */
	float PercentTimeAppliedSoFar = 0.0f;
};

/**
 *  UTitanBaseMovementMode
 *  Provides a common structure for all Titan Pawn Movement Modes
 */
UCLASS()
class TITANMOVEMENT_API UTitanBaseMovementMode : public UBaseMovementMode,
	public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()

public:

	/** Constructor */
	UTitanBaseMovementMode(const FObjectInitializer& ObjectInitializer);

public:

	/** Generates the movement data that will be consumed by the simulation tick */
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** Runs (or re-runs) the simulation and moves the updated component. */
	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	
	// Simulation stages and utility
	///////////////////////////////////////////////////////////////////////////////////

protected:

	/** Prepares and validates all the data needed for the Simulation Tick and saves it into transient variables */
	virtual bool PrepareSimulationData(const FSimulationTickParams& Params);

	/** Builds the output sync states and saves them into transient variables */
	virtual void BuildSimulationOutputStates(FMoverTickEndData& OutputState);

	/** Checks if character movement has been disabled at the component level, and cancels simulation if needed */
	virtual bool CheckIfMovementIsDisabled();

	/** Handles any additional prerequisite work that needs to be done before the simulation moves the updated component */
	virtual void PreMove(FMoverTickEndData& OutputState);

	/** Handles most of the actual movement, including collision recovery  */
	virtual void ApplyMovement(FMoverTickEndData& OutputState);

	/** Handles any additional behaviors after the updated component's final position and velocity have been computed */
	virtual void PostMove(FMoverTickEndData& OutputState);

	/** Attempts to teleport the updated component */
	virtual bool AttemptTeleport(const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity);

	/** Utility function to help keep track of the percentage of the time slice applied so far during move sub-stages */
	float UpdateTimePercentAppliedSoFar(float PreviousTimePct, float LastCollisionTime) const;

	/** Calculates the target orientation Quat for the movement. Returns true if there's a change in orientation. */
	virtual bool CalculateOrientationChange(FQuat& TargetOrientQuat);

	/** Updates the stamina value on the out sync state and calls the relevant handlers */
	void UpdateStamina(float StaminaUse);

protected:

	/** Tag to add to add while this mode is active */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ModeTag;


	// Transient variables used by the simulation stages
	// Note that these should be considered invalidated outside of OnSimulationTick()
	// and are not meant to persist between simulation frames.
	///////////////////////////////////////////////////////////////////////////////////

	/** Mutable pointer to the Mover component */
	UTitanMoverComponent* MutableMoverComponent;

	/** Pointers to the updated components */
	FMovingComponentSet MovingComponentSet;

	/** Non-mutable pointers to the starting sync states */
	const FMoverDefaultSyncState* StartingSyncState;
	const FTitanStaminaSyncState* StaminaSyncState;
	const FTitanTagsSyncState* TagsSyncState;

	/** Mutable pointer to the blackboard */
	UMoverBlackboard* SimBlackboard;

	/** Non-mutable pointers to the input structs */
	const FCharacterDefaultInputs* KinematicInputs;
	const FTitanMovementInputs* TitanInputs;

	/** Pointer to the proposed move for this simulation step */
	const FProposedMove* ProposedMove;

	/** Mutable pointers to the output sync states */
	FMoverDefaultSyncState* OutDefaultSyncState;
	FTitanStaminaSyncState* OutStaminaSyncState;
	FTitanTagsSyncState* OutTagsSyncState;

	/** Utility velocity values */
	FVector StartingVelocity;

	/** Utility time values */
	float DeltaMs;
	float DeltaTime;
	float CurrentSimulationTime;

protected:

	virtual void OnRegistered(const FName ModeName) override;
	virtual void OnUnregistered() override;

	/** Pointer to the legacy movement settings */
	TObjectPtr<const UCommonLegacyMovementSettings> CommonLegacySettings;

	/** Pointer to the titan movement settings */
	TObjectPtr<const UTitanMovementSettings> TitanSettings;

	// Debug
	/////////////////////////////////////////////////////

#if ENABLE_VISUAL_LOG
	//~ Begin IVisualLoggerDebugSnapshotInterface interface

	/** Visual Logger Debug Snapshot */
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;

	//~ End IVisualLoggerDebugSnapshotInterface interface
#endif
};

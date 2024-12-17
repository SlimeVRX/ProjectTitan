// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TitanBaseMovementMode.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"

#include "TitanGroundModeBase.generated.h"

/**
 *  UTitanGroundModeBase
 *  Base class for all ground movement modes.
 *  Establishes a common simulation structure to handle slopes, stairs and other obstacles.
 */
UCLASS()
class TITANMOVEMENT_API UTitanGroundModeBase : public UTitanBaseMovementMode
{
	GENERATED_BODY()
	
	// Simulation stages
	///////////////////////////////////////////////////////////////////////////////////

protected:

	/** Implements ground movement  */
	virtual void ApplyMovement(FMoverTickEndData& OutputState) override;

	// ApplyMovement sub-stages
	///////////////////////////////////////////////////////////////////////////////////

	/** Validates the floor prior to any movement */
	virtual void ValidateFloor();

	/** Attempts to move the updated comp along any dynamically moving floor it's standing on */
	virtual bool ApplyDynamicFloorMovement(FMoverTickEndData& OutputState, FMovementRecord& MoveRecord);

	/** Applies the first free movement. Returns true if the updated component was successfully moved. */
	virtual bool ApplyFirstMove(FTitanMoveData& WalkData);

	/** Attempts to de-penetrate the updated component prior to its first move */
	virtual bool ApplyDepenetrationOnFirstMove(FTitanMoveData& WalkData);

	/** Calculates ramp deflection and moves the updated component up a ramp */
	virtual bool ApplyRampMove(FTitanMoveData& WalkData);

	/** Attempts to move the updated component over a climbable obstacle */
	virtual bool ApplyStepUpMove(FTitanMoveData& WalkData, FOptionalFloorCheckResult& StepUpFloorResult);

	/** Attempts to slide the updated component along a wall or other blocking, unclimbable obstacle */
	virtual bool ApplySlideAlongWall(FTitanMoveData& WalkData);

	/** Attempts to adjust the character vertically so it contacts the floor */
	virtual bool ApplyFloorHeightAdjustment(FTitanMoveData& WalkData);

	/** Applies corrections to the updated component's position while not moving. */
	virtual bool ApplyIdleCorrections(FTitanMoveData& WalkData);

	/** Handles any movement mode transitions as a result of falling */
	virtual bool HandleFalling(FMoverTickEndData & OutputState, FMovementRecord & MoveRecord, FHitResult & Hit, float TimeAppliedSoFar);

	/** Captures the final movement state for the simulation frame and updates the output default sync state */
	void CaptureFinalState(const FFloorCheckResult& FloorResult, const FMovementRecord& Record) const;

	// Utility
	///////////////////////////////////////////////////////////////////////////////////

	/** Updates and returns the floor and base info data structures */
	FRelativeBaseInfo UpdateFloorAndBaseInfo(const FFloorCheckResult& FloorResult) const;

	/** Returns the name of the movement mode that will handle falling*/
	virtual const FName& GetFallingModeName() const;

	// Transient variables used by the simulation stages
	// Note that these should be considered invalidated outside of OnSimulationTick()
	// and are not meant to persist between simulation frames.
	///////////////////////////////////////////////////////////////////////////////////

	/** Floor info structs */
	FFloorCheckResult CurrentFloor;
	FRelativeBaseInfo OldRelativeBase;
};

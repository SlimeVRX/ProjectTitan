// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "GameplayTagContainer.h"
#include "TitanModeTransition_Jump.generated.h"

/**
 *  UTitanModeTransition_Jump
 *  Handles movement mode transitions due to jump inputs.
 */
UCLASS()
class TITANMOVEMENT_API UTitanModeTransition_Jump : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	UTitanModeTransition_Jump(const FObjectInitializer& ObjectInitializer);

protected:

	/** Name of the movement mode to transition to for the jump */
	UPROPERTY(EditAnywhere, Category="Mode")
	FName JumpMovementMode;

	/** Tags required on the sync state to allow the jump transition */
	UPROPERTY(EditAnywhere, Category="Evaluation")
	FGameplayTagContainer JumpRequiredTags;

	/** Minimum amount of time to elapse between jumps */
	UPROPERTY(EditAnywhere, Category="Evaluation")
	float MinTimeBetweenJumps = 0.1f;

	/** If greater than zero, the character can jump if less than this falling time has elapsed */
	UPROPERTY(EditAnywhere, Category="Evaluation")
	float CoyoteTime = 0.0f;

	/** Time to hold the jump impulse for */
	UPROPERTY(EditAnywhere, Category="Trigger", meta=(ClampMin=0))
	float HoldTime = 0.0f;

	/** Vertical impulse to provide with the jump as long as the button is pressed */
	UPROPERTY(EditAnywhere, Category="Trigger", meta=(ClampMin=0))
	float VerticalImpulse = 0.0f;

	/** Extra vertical impulse to add if the extra impulse tag is present */
	UPROPERTY(EditAnywhere, Category="Trigger", meta=(ClampMin=0))
	float ExtraVerticalImpulse = 0.0f;

	/** Percentage of air control while jump is active */
	UPROPERTY(EditAnywhere, Category="Trigger", meta=(ClampMin=0, ClampMax=1))
	float AirControl = 1.0f;

	/** If this tag is present, an extra impulse will be provided. Allows for higher jumps while sprinting, etc. */
	UPROPERTY(EditAnywhere, Category="Trigger")
	FGameplayTag ExtraVerticalImpulseTag;

	/** If true, the jump transition will happen when the jump button is pressed */
	UPROPERTY(EditAnywhere, Category="Evaluation")
	uint8 bJumpWhenButtonPressed : 1;

	/** If true, the character will only jump if it has a valid walkable floor */
	UPROPERTY(EditAnywhere, Category="Evaluation")
	uint8 bRequireGround : 1;

	/** If true, the character will stop receiving vertical impulse as soon as the jump button is released */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bTruncateOnJumpRelease : 1;

	/** If true, the character's movement plane velocity will be overridden by the provided computed momentum */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bOverrideMovementPlaneVelocity : 1;

	/** If true, the character's vertical velocity will be overridden by the provided computed momentum */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bOverrideVerticalVelocity : 1;

	/** If true, any floor velocity will be added to the overridden velocity */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bAddFloorVelocity : 1;

	/** If true, the character will keep any existing movement plane velocity from before jumping */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bKeepPreviousVelocity : 1;

	/** If true, the character will keep any existing vertical velocity from before jumping */
	UPROPERTY(EditAnywhere, Category="Trigger")
	uint8 bKeepPreviousVerticalVelocity : 1;

	/** If a positive value is provided, any carried over velocity will be clamped to this maximum value */
	UPROPERTY(EditAnywhere, Category="Trigger")
	float MaxPreviousVelocity = -1.0f;

	/** If this transition is triggered, send this gameplay event to the owner */
	UPROPERTY(EditAnywhere, Category="Trigger")
	FGameplayTag TriggerEvent;

	/** If a FName is provided, the simulation time of the jump will be saved to the Blackboard under that key */
	UPROPERTY(EditAnywhere, Category="Trigger")
	FName BlackboardTimeLoggingKey;

	/** Determines if the transition should be triggered */
	virtual FTransitionEvalResult OnEvaluate(const FSimulationTickParams& Params) const;

	/** Handles transition trigger */
	virtual void OnTrigger(const FSimulationTickParams& Params);

};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TitanBaseMovementMode.h"
#include "Engine/EngineTypes.h"
#include "TitanFallingMode.generated.h"

// Gameplay tags
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Gliding);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_SoftLanding);

class UCurveFloat;

/**
 *  UTitanFallingMode
 *  Specialized Falling Mode for Titan Pawns with additional features.
 *  - Stamina-based Gliding
 */
UCLASS()
class TITANMOVEMENT_API UTitanFallingMode : public UTitanBaseMovementMode
{
	GENERATED_BODY()
	
public:

	/** Clears blackboard fields on deactivation */
	virtual void OnDeactivate() override;

	/** Generates the movement data that will be consumed by the simulation tick */
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	// Simulation stages
	///////////////////////////////////////////////////////////////////////////////////

protected:

	/** Gets additional falling data */
	virtual bool PrepareSimulationData(const FSimulationTickParams& Params) override;

	/** Handles most of the actual movement, including collision recovery  */
	virtual void ApplyMovement(FMoverTickEndData& OutputState) override;

	/** Handles any additional behaviors after the updated component's final position and velocity have been computed */
	virtual void PostMove(FMoverTickEndData& OutputState) override;

	/** Captures the final movement values and sends it to the Output Sync State */
	void CaptureFinalState(const FFloorCheckResult& FloorResult, float DeltaSecondsUsed, FMoverTickEndData& TickEndData, FMovementRecord& Record);

	/**
	 * Called at the end of the tick in falling mode. Handles checking any landings that should occur and switching to specific modes
	 * (i.e. landing on a walkable surface would switch to the walking movement mode) 
	 */
	UFUNCTION(BlueprintCallable, Category = Mover)
	virtual void ProcessLanded(const FFloorCheckResult& FloorResult, FVector& Velocity, FRelativeBaseInfo& BaseInfo, FMoverTickEndData& TickEndData) const;

	/** Returns true if the movement state matches the conditions for a soft landing */
	bool CheckForSoftLanding();

	bool DoSoftLandingTrace(FHitResult& OutHit);

protected:

	/** Gameplay Tag to use when gliding */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag GlidingTag = TAG_Titan_Movement_Gliding;

	/** Gameplay Tag to use when soft landing */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SoftLandingTag = TAG_Titan_Movement_SoftLanding;
	
	/**
	 * When falling, amount of movement control available to the actor.
	 * 0 = no control, 1 = full control
	 */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1.0"))
	float AirControlPercentage = 0.4f;
	
	/**
 	 * Deceleration to apply to air movement when falling slower than terminal velocity.
 	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
 	 */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
	float FallingDeceleration = 200.0f;

	/**
     * Deceleration to apply to air movement when falling faster than terminal velocity
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
     */
    UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
    float OverTerminalSpeedFallingDeceleration = 800.0f;
	
	/**
	 * If the actor's movement plane velocity is greater than this speed falling will start applying OverTerminalSpeedFallingDeceleration instead of FallingDeceleration
	 * The expected behavior is to set OverTerminalSpeedFallingDeceleration higher than FallingDeceleration so the actor will slow down faster
	 * when going over TerminalMovementPlaneSpeed.
	 */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalMovementPlaneSpeed = 1500.0f;

	/** When exceeding maximum vertical speed, should it be enforced via a hard clamp? If false, VerticalFallingDeceleration will be used for a smoother transition to the terminal speed limit. */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite)
	bool bShouldClampTerminalVerticalSpeed = true;

	/** Deceleration to apply to vertical velocity when it's greater than TerminalVerticalSpeed. Only used if bShouldClampTerminalVerticalSpeed is false. */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="!bShouldClampTerminalVerticalSpeed", ClampMin="0", ForceUnits = "cm/s^2"))
	float VerticalFallingDeceleration = 4000.0f;
	
	/**
	 * If the actor's vertical s[eed is greater than speed VerticalFallingDeceleration will be applied to vertical velocity
	 */
	UPROPERTY(Category="Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalVerticalSpeed = 2000.0f;

	/** Minimum amount of time the pawn must be in free fall before we can deploy the glider */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideMinFallingTime = 0.75f;

	/** Minimum amount of time the pawn must be in free fall after jumping, before we can deploy the glider */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideMinJumpTime = 0.75f;

	/** Maximum amount of time between a fall and a jump to still be considered a jump */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideJumpTimeTolerance = 0.5f;

	/** Max air speed while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideMaxSpeed = 700.0f;

	/** Acceleration while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideAcceleration = 600.0f;

	/** Deceleration while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideDeceleration = 800.0f;

	/** Turn rate while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideTurningRate = 350.0f;

	/** Turn rate bust while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideTurningBoost = 2.0f;

	/** Turn rate bust while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax=1))
	float GlideAirControl = 1.0f;

	/** Terminal vertical speed while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideTerminalVerticalSpeed = 100.0f;

	/** Terminal vertical speed while gliding upwards */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideTerminalUpwardsSpeed = 100.0f;

	/** Deceleration to apply while over terminal vertical speed while gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideVerticalFallingDeceleration = 4000.0f;

	/** How much stamina per second is consumed by gliding */
	UPROPERTY(Category="Gliding", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GlideStaminaCostPerSecond = 5.0f;

	/** Collision channel to use for soft landing traces */
	UPROPERTY(Category="Soft Landing", EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ECollisionChannel> SoftLandingTraceChannel;

	/** Falling for longer than this time will trigger a soft landing */
	UPROPERTY(Category="Soft Landing", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float MinTimeForSoftLanding = 0.5f;

	/** Falling at a speed greater than this will trigger a soft landing */
	UPROPERTY(Category="Soft Landing", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SoftLandingTerminalVerticalSpeed = 2400.0f;

	/** How deep water needs to be to trigger a soft landing */
	UPROPERTY(Category="Soft Landing", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float WaterSoftLandingMinDepth = 100.0f;

	/** Multiplies the velocity vector during fall probe traces  */
	UPROPERTY(Category="Soft Landing", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> SoftLandingTraceMultiplierCurve;

	/** Gameplay Event to send to the character when you start gliding */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag GlideStartEvent;

	/** Gameplay Event to send to the character when you finish gliding */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag GlideEndEvent;

	/** Gameplay Event to send to the character when we start soft landing */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SoftLandingStartEvent;

	/** Gameplay Event to send to the character when we stop soft landing */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SoftLandingEndEvent;

	/** Gameplay Event to send to the character when we land */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag LandingEndEvent;

	// Transient simulation variables
	///////////////////////////////////////////

	/** Effective Velocity calculated this frame */
	FVector EffectiveVelocity;

	/** Time since the character grapple jumped, in seconds */
	float TimeSinceGrappleJump = 0.0f;
};
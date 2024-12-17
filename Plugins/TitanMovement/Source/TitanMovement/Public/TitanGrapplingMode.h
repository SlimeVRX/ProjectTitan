// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TitanBaseMovementMode.h"
#include "InstantMovementEffect.h"
#include "TitanGrapplingMode.generated.h"

class UCurveFloat;
class UCommonLegacyMovementSettings;
class UTitanMovementSettings;

// Gameplay Tags
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_GrappleBoost);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_GrappleArrival);

/**
 *  FTitanGrappleEffect
 *  Instant Move Effect to set up the Grappling Movement Mode
 */
 USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanGrappleEffect : public FInstantMovementEffect
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor/Destructor */
	FTitanGrappleEffect();

	virtual ~FTitanGrappleEffect() {};

	/** Goal location for the grapple, in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector GrappleGoal;

	/** Goal normal for the grapple, in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector GrappleNormal;

	/** Generate a move */
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;

	/** Layered Move UStruct utility */

	virtual FInstantMovementEffect* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};


/**
 *  UTitanGrapplingMode
 *  This mode moves the character in a straight line towards a goal location.
 *  The goal location is held in the Titan Sync State.
 *  It will slide off walls and obstacles if possible.
 *  If movement is blocked by a collision or the mode takes too long,
 *  it will automatically abort and switch to the specified mode.
 */
UCLASS()
class TITANMOVEMENT_API UTitanGrapplingMode : public UTitanBaseMovementMode
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	UTitanGrapplingMode(const FObjectInitializer& ObjectInitializer);

public:

	/** Generates the movement data that will be consumed by the simulation tick */
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

protected:

	/** Gets additional data regarding grapple arrival and boosting */
	virtual bool PrepareSimulationData(const FSimulationTickParams& Params) override;

	/** Implements grapple movement  */
	virtual void ApplyMovement(FMoverTickEndData& OutputState) override;

	/** Applies movement mode tags */
	virtual void PostMove(FMoverTickEndData& OutputState) override;

protected:

	void CaptureFinalState(FMovementRecord& Record, bool bOverrideVelocity = false) const;

protected:

	/** Tag to apply while grapple boost is active */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag BoostingTag;

	/** Tag to apply while the character has arrived at the destination */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ArrivalTag;

	/** Max speed while grappling */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float MaxSpeed = 4000.0f;

	/** Max speed while grapple boosting */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float BoostMaxSpeed = 24000.0f;

	/** Acceleration while grappling */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float Acceleration = 30000.0f;

	/** Turning rate while grappling */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float TurningRate = 1440.0f;

	/** Distance at which we'll start applying approach speed scaling */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SlowdownDistance = 100.0f;

	/** Optional curve to scale grappling max speed over time */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> SpeedScalingOverTime;

	/** Optional curve to scale grappling max speed as we approach the goal */
	UPROPERTY(Category="Movement", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> ApproachSpeedScaling;

	/** Tolerance distance for considering we've arrived at the goal while grappling */
	UPROPERTY(Category="Simulation", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ArrivalTolerance = 15.0f;

	/** Min dot product from a collision to allow sliding. Anything below this will abort the grapple */
	UPROPERTY(Category="Simulation", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, ClampMax=1))
	float MinCollisionDot = 0.1f;

	/** Min distance the character is allowed to move before being considered stuck */
	UPROPERTY(Category="Stuck Check", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float StuckMovementDistance = 10.0f;

	/** Minimum amount of time the character must be grappling before the stuck test kicks in */
	UPROPERTY(Category="Stuck Check", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float StuckMinTime = 0.5f;

	/** Gameplay Event to send when the character arrives at the grapple point */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ArrivalEvent;

	/** Gameplay Event to send when the character performs a grapple boost */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag BoostEvent;

private:

	/** Transient starting location at the start of the movement, used to determine if we're stuck */
	FVector StartingLocation = FVector::ZeroVector;

	/** Transient grapple goal obtained from the blackboard */
	FVector GrappleGoal = FVector::ZeroVector;

	/** Transient grapple start time obtained from the blackboard */
	float GrappleStartTime = 0.0f;

	/** If true, the character has arrived at the grapple destination */
	bool bWasArrived = false;

	/** If true, the character has been grapple boosting since last update */
	bool bWasBoosting = false;

	/** If true, the grapple is being aborted due to a collision */
	bool bAbortOnCollision = false;
};

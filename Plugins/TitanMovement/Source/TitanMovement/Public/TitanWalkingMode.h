// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "TitanGroundModeBase.h"

#include "TitanWalkingMode.generated.h"

class UMoverComponent;
class USceneComponent;
class UPrimitiveComponent;
struct FMoverDefaultSyncState;
struct FTitanStaminaSyncState;
struct FProposedMove;
class UTitanMovementSettings;
struct FMovementRecord;
struct FTitanTagsSyncState;

// Gameplay Tags
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Sprinting);

/**
 *  UTitanWalkingMode
 *  Advanced walking mode with extra functionality:
 *  Stamina usage and regeneration.
 *  Sprinting.
 *  Exhaustion.
 */
UCLASS()
class TITANMOVEMENT_API UTitanWalkingMode : public UTitanGroundModeBase
{
	GENERATED_BODY()

protected:

	/** Gameplay Tag to use when sprinting */
	UPROPERTY(Category="Tags", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SprintingTag = TAG_Titan_Movement_Sprinting;

	/** Max ground speed while exhausted */
	UPROPERTY(Category="Exhausted", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ExhaustedMaxSpeed = 400.0f;

	/** Max ground acceleration while exhausted */
	UPROPERTY(Category="Exhausted", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ExhaustedAcceleration = 2000.0f;

	/** Max ground deceleration while exhausted */
	UPROPERTY(Category="Exhausted", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ExhaustedDeceleration = 4000.0f;

	/** Max ground turning rate while exhausted */
	UPROPERTY(Category="Exhausted", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ExhaustedTurningRate = 200.0f;

	/** Max ground turn rate boost while exhausted */
	UPROPERTY(Category="Exhausted", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float ExhaustedTurningBoost = 2.0f;

	/** Max ground speed while sprinting */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SprintMaxSpeed = 1050.0f;

	/** Max ground acceleration while sprinting */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SprintAcceleration = 5000.0f;

	/** Max ground deceleration while sprinting */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SprintDeceleration = 400.0f;

	/** Max ground turning rate while sprinting */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SprintTurningRate = 350.0f;

	/** Max ground turn rate boost while sprinting */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float SprintTurningBoost = 3.0f;

	/** Stamina generation and consumption curve based on walking speed. Positive values regenerate, negative values consume. */
	UPROPERTY(Category="Sprinting", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> SprintStaminaConsumptionCurve;

	/** Gameplay Event to send to the character when you start sprinting */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SprintStartEvent;

	/** Gameplay Event to send to the character when you finish sprinting */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SprintEndEvent;

	/** Gameplay event to send to the character when it becomes exhausted */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ExhaustionEvent;

	/** Gameplay event to send to the character when it recovers from exhaustion */
	UPROPERTY(Category="Events", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ExhaustionRecoveryEvent;

public:

	/** Generates the movement data that will be consumed by the simulation tick */
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	// Simulation stages
	///////////////////////////////////////////////////////////////////////////////////

protected:

	/** Walking needs to account for based movement so it overrides the default disabled check */
	virtual bool CheckIfMovementIsDisabled() override;

	/** Updates stamina usage after movement has been performed */
	virtual void PostMove(FMoverTickEndData& OutputState) override;

	// ApplyMovement sub-stages
	///////////////////////////////////////////////////////////////////////////////////
	
	/** Handles any movement mode transitions as a result of falling */
	virtual bool HandleFalling(FMoverTickEndData& OutputState, FMovementRecord& MoveRecord, FHitResult& Hit, float TimeAppliedSoFar) override;

};

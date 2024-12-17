// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "TitanRaftTeleportEffect.generated.h"

// raft interface
class ATitanRaft;

/**
 *  FTitanRaftTeleportEffect
 *	Helper Instant Movement Effect to set up the Raft Movement Mode
 *  Teleports the pawn to the pilot socket location and sets up the Blackboard for the Movement Mode
 */
 USTRUCT(BlueprintType)
struct TITANRAFT_API FTitanRaftTeleportEffect : public FInstantMovementEffect
{
	GENERATED_USTRUCT_BODY()

public:
	/** Constructor/Destructor */
	FTitanRaftTeleportEffect();
	virtual ~FTitanRaftTeleportEffect() {};

	/** Generate a move */
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	//virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	/** Raft that the Pawn will pilot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan")
	TObjectPtr<ATitanRaft> Raft;

public:

	/** UStruct utility */

	virtual FInstantMovementEffect* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "TitanLayeredMove_Teleport.generated.h"

/**
 *  FTitanTeleportEffect
 *  Specialized Teleport Instant Move Effect for Titan
 */
 USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanTeleportEffect : public FTeleportEffect
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor/Destructor */
	FTitanTeleportEffect();

	virtual ~FTitanTeleportEffect() {};

	/** Generate a move */
	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;

	/** Layered Move UStruct utility */

	virtual FInstantMovementEffect* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};
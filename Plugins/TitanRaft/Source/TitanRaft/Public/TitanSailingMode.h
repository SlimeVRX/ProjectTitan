// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TitanBaseMovementMode.h"
#include "TitanSailingMode.generated.h"

/**
 *  UTitanSailingMode
 *  A dummy movement mode used while sailing or driving vehicles.
 *  Simply updates the default sync state without processing any inputs or collision.
 */
UCLASS()
class TITANRAFT_API UTitanSailingMode : public UTitanBaseMovementMode
{
	GENERATED_BODY()
	
protected:

	/** Sailing defers movement to the raft so we skip the check */
	virtual bool CheckIfMovementIsDisabled() override;

	virtual void ApplyMovement(FMoverTickEndData& OutputState) override;

	virtual void PostMove(FMoverTickEndData& OutputState) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LayeredMove.h"
#include "TitanLayeredMove_Jump.generated.h"

/**
 *  FTitanLayeredMove_Jump
 *  Enhanced jump Layered Move. Keeps track of initial jump simulation time through a Blackboard key
 */
 USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanLayeredMove_Jump : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor/Destructor */
	FTitanLayeredMove_Jump();

	virtual ~FTitanLayeredMove_Jump() {};

	/** Upwards impulse in cm/s, to be applied in the direction the target actor considers up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float UpwardsSpeed;

	/** Optional momentum carried on from before the jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Momentum;

	/** Air control percentage during the jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float AirControl;

	/** If true, the layered move will end if the player releases the jump button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bTruncateOnJumpRelease;

	/** If true, the layered move will override movement plane velocity with the provided Momentum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bOverrideHorizontalMomentum;

	/** If true, the layered move will override the vertical velocity with the provided Momentum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bOverrideVerticalMomentum; 

	/** Generates a move */
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	/** Layered move utility */

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

 template<>
 struct TStructOpsTypeTraits< FTitanLayeredMove_Jump > : public TStructOpsTypeTraitsBase2<FTitanLayeredMove_Jump>
 {
	 enum
	 {
		 WithCopy = true
	 };
 };
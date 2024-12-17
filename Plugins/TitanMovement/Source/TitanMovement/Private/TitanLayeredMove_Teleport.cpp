// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanLayeredMove_Teleport.h"
#include "TitanMoverTypes.h"
#include "TitanMovementLogging.h"
#include "MoverComponent.h"
#include "VisualLogger/VisualLogger.h"

FTitanTeleportEffect::FTitanTeleportEffect()
{

}

bool FTitanTeleportEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	// apply the teleport
	if (Super::ApplyMovementEffect(ApplyEffectParams, OutputState))
	{
		// get the blackboard
		UMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard_Mutable();
		
		if (SimBlackboard && ApplyEffectParams.TimeStep)
		{
			// set the fall time in the blackboard
			SimBlackboard->Set(TitanBlackboard::LastFallTime, ApplyEffectParams.TimeStep->BaseSimTimeMs);
		}

#if ENABLE_VISUAL_LOG

		{
			UE_VLOG(ApplyEffectParams.MoverComp->GetOwner(), VLogTitanMover, Log, TEXT("Teleport Effect"));
		}

#endif

		return true;
	}

	return false;
}
FInstantMovementEffect* FTitanTeleportEffect::Clone() const
{
	FTitanTeleportEffect* CopyPtr = new FTitanTeleportEffect(*this);
	return CopyPtr;
}

void FTitanTeleportEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FTitanTeleportEffect::GetScriptStruct() const
{
	return FTitanTeleportEffect::StaticStruct();
}

FString FTitanTeleportEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Titan Teleport"));
}

void FTitanTeleportEffect::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

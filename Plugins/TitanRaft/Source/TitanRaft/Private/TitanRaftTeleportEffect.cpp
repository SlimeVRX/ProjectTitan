// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanRaftTeleportEffect.h"
#include "TitanRaftActor.h"
#include "TitanMoverTypes.h"
#include "TitanRaftLogging.h"

#include "MoverSimulationTypes.h"
#include "MoverComponent.h"
#include "VisualLogger/VisualLogger.h"

FTitanRaftTeleportEffect::FTitanRaftTeleportEffect()
{
}

bool FTitanRaftTeleportEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	// ensure the raft pointer is valid
	check(Raft);

	const UTitanMovementSettings* TitanSettings = ApplyEffectParams.MoverComp->FindSharedSettings<UTitanMovementSettings>();

	// save the blackboard key for the raft
	// get the blackboard
	UMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard_Mutable();
	if(SimBlackboard)
	{
		SimBlackboard->Set(TitanBlackboard::LastRaft, Raft);
	}

	// teleport to the raft's pilot socket location
	if (ApplyEffectParams.UpdatedComponent->GetOwner()->TeleportTo(Raft->GetPilotLocation(), ApplyEffectParams.UpdatedComponent->GetComponentRotation()))
	{
		FMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		if (const FMoverDefaultSyncState* StartingSyncState = ApplyEffectParams.StartState->SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			OutputSyncState.SetTransforms_WorldSpace(ApplyEffectParams.UpdatedComponent->GetComponentLocation(),
				ApplyEffectParams.UpdatedComponent->GetComponentRotation(),
				OutputSyncState.GetVelocity_WorldSpace(),
				nullptr); // no movement base

			// invalidate the floor
			if (SimBlackboard)
			{
				SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
				SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
			}

		}
	}

	// set the proposed move mode
	OutputState.MovementMode = TitanSettings->RaftMovementModeName;

#if ENABLE_VISUAL_LOG

	{
		UE_VLOG(ApplyEffectParams.MoverComp->GetOwner(), VLogTitanRaft, Log, TEXT("Raft Effect"));
	}

#endif

	return true;
}

FInstantMovementEffect* FTitanRaftTeleportEffect::Clone() const
{
	FTitanRaftTeleportEffect* CopyPtr = new FTitanRaftTeleportEffect(*this);
	return CopyPtr;
}

void FTitanRaftTeleportEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << Raft;
}

UScriptStruct* FTitanRaftTeleportEffect::GetScriptStruct() const
{
	return FTitanRaftTeleportEffect::StaticStruct();
}

FString FTitanRaftTeleportEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Raft"));
}

void FTitanRaftTeleportEffect::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

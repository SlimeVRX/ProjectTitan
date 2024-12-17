// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanMoverComponent.h"
#include "TitanMoverTypes.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "TitanLayeredMove_Teleport.h"
#include "TitanMovementLogging.h"
#include "Components/PrimitiveComponent.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"

void UTitanMoverComponent::BeginPlay()
{
	Super::BeginPlay();

#if ENABLE_VISUAL_LOG
	// redirect Visual Logger to the owning Actor
	REDIRECT_TO_VLOG(GetOwner());
#endif
}

void UTitanMoverComponent::OnHandleImpact(const FMoverOnImpactParams& ImpactParams)
{
	// get the hit component
	UPrimitiveComponent* HitComponent = ImpactParams.HitResult.GetComponent();

	// is it valid and simulating physics?
	if (HitComponent && HitComponent->IsSimulatingPhysics())
	{
		FVector ImpactForce = ImpactParams.AttemptedMoveDelta * HitComponent->GetMass() * ImpactPhysicsForceMultiplier;
		HitComponent->AddImpulseAtLocation(ImpactForce, ImpactParams.HitResult.ImpactPoint);

		UE_LOG(LogTitanMover, Warning, TEXT("Applied impact force on dynamic physics object"));
	}
}

bool UTitanMoverComponent::TeleportImmediately(const FVector& Location, const FRotator& Orientation, const FVector& Velocity)
{
	bool bSuccessfullyWrote = false;
	FMoverSyncState PendingSyncState;

	if (BackendLiaisonComp->ReadPendingSyncState(OUT PendingSyncState))
	{
		if (FMoverDefaultSyncState* DefaultSyncState = PendingSyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>())
		{
			// Move the character and reflect this in the official simulation state
			UpdatedComponent->SetWorldLocationAndRotation(Location, Orientation);
			UpdatedComponent->ComponentVelocity = Velocity;
			DefaultSyncState->SetTransforms_WorldSpace(Location, Orientation, FVector::ZeroVector, nullptr);
			bSuccessfullyWrote = (BackendLiaisonComp->WritePendingSyncState(PendingSyncState));
			if (bSuccessfullyWrote)
			{
				FinalizeFrame(&PendingSyncState, &CachedLastAuxState);
			}
		}
	}
	return bSuccessfullyWrote;
}

void UTitanMoverComponent::OnLanded(const FName& NextMovementModeName, const FHitResult& HitResult)
{
	OnLandedDelegate.Broadcast(NextMovementModeName, HitResult);
}

void UTitanMoverComponent::WaitForTeleport()
{
	// raise the long teleport flag
	bIsTeleporting = true;

	// set up the teleport movement mode
	const UTitanMovementSettings* TitanSettings = FindSharedSettings<UTitanMovementSettings>();

	if (TitanSettings)
	{
		QueueNextMode(TitanSettings->TeleportMovementModeName);
	}
}

void UTitanMoverComponent::TeleportAndFall(const FVector& TeleportLocation)
{
	// switch to air movement mode
	const UCommonLegacyMovementSettings* LegacySettings = FindSharedSettings<UCommonLegacyMovementSettings>();

	if (LegacySettings)
	{
		QueueNextMode(LegacySettings->AirMovementModeName);
	}

	// queue the teleport instant movement effect
	TSharedPtr<FTitanTeleportEffect> TeleportEffect = MakeShared<FTitanTeleportEffect>();
	TeleportEffect->TargetLocation = TeleportLocation;

	QueueInstantMovementEffect(TeleportEffect);

	// reset the long teleport flag
	bIsTeleporting = false;
}

void UTitanMoverComponent::SetMovementDisabled(bool bState)
{
	bDisableMovement = bState;
}

bool UTitanMoverComponent::IsFalling() const
{
	const UCommonLegacyMovementSettings* LegacySettings = FindSharedSettings<UCommonLegacyMovementSettings>();

	if (LegacySettings)
	{
		return GetMovementModeName() != LegacySettings->AirMovementModeName;
	}

	return false;
}

FGameplayTagContainer UTitanMoverComponent::GetTagsFromSyncState() const
{
	if (!bHasValidCachedState)
	{
		//UE_LOG(LogMover, Warning, TEXT("Attempting direct access to the last-cached sync state before one has been set. Results will be unreliable. Use the HasValidCachedState function to check if CachedLastSyncState is valid or not."));
		return FGameplayTagContainer();
	}

	// get the tags sync state
	const FTitanTagsSyncState* TagsSyncState = CachedLastSyncState.SyncStateCollection.FindDataByType<FTitanTagsSyncState>();

	// return the tags
	if (TagsSyncState)
	{
		return TagsSyncState->GetMovementTags();
	}

	return FGameplayTagContainer();
}

FVector UTitanMoverComponent::GetGroundNormal() const
{
	FFloorCheckResult CurrentFloor;
	const UMoverBlackboard* SimBB = GetSimBlackboard();

	if (SimBB && SimBB->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor))
	{
		return CurrentFloor.HitResult.ImpactNormal;
	}

	return FVector::ZeroVector;
}

#if ENABLE_VISUAL_LOG
void UTitanMoverComponent::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{

}
#endif
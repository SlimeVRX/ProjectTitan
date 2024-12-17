// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverComponent.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "TitanMoverComponent.generated.h"

// Fired after the actor lands on a valid surface. First param is the name of the mode this actor is in after landing. Second param is the hit result from hitting the floor.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTitanMover_OnLanded, const FName&, NextMovementModeName, const FHitResult&, HitResult);

/**
 *	Specialized Mover Component for Titan
 */
UCLASS()
class TITANMOVEMENT_API UTitanMoverComponent : public UMoverComponent,
	public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()

protected:

	/** Broadcast when this actor lands on a valid surface. */
	UPROPERTY(BlueprintAssignable, Category = "Mover")
	FTitanMover_OnLanded OnLandedDelegate;

	/** Multiplies the impulse applied to physics objects on collisions to push them harder */
	UPROPERTY(EditAnywhere, Category = "Mover", meta=(ClampMin=0))
	float ImpactPhysicsForceMultiplier = 10.0f;

	/** Set to true while the owner waits for a long teleport to complete */
	bool bIsTeleporting = false;

	/** Set to true while movement has been disabled externally */
	bool bDisableMovement = false;

	/** Set to true when stamina usage is enabled */
	bool bEnableStamina = true;

public:

	/** Visual Logger redirect */
	virtual void BeginPlay() override;

	/** Applies forces to physics objects on impact */
	virtual void OnHandleImpact(const FMoverOnImpactParams& ImpactParams);

	/** Override to handle Raft movement copy and work around simulation timing issues */
	bool TeleportImmediately(const FVector& Location, const FRotator& Orientation, const FVector& Velocity);
	
	/** Called from Movement Modes to notify of landed events */
	void OnLanded(const FName& NextMovementModeName, const FHitResult& HitResult);

	/** Sets up a non-immediate teleport */
	void WaitForTeleport();

	/** Teleports the owner to the given location and switches to falling mode */
	void TeleportAndFall(const FVector& TeleportLocation);

	void SetMovementDisabled(bool bState);

	/** Returns true if the owner is currently falling */
	UFUNCTION(BlueprintPure, Category="Mover")
	bool IsFalling() const;

	/** Returns true if the owner is currently waiting for a non-immediate teleport */
	UFUNCTION(BlueprintPure, Category="Mover")
	bool IsTeleporting() const { return bIsTeleporting; };

	/** Returns true if movement for the owner has been disabled */
	UFUNCTION(BlueprintPure, Category="Mover")
	bool IsMovementDisabled() const { return bDisableMovement; };

	/** Toggles stamina usage */
	void ToggleStamina() { bEnableStamina = !bEnableStamina; };

	/** Returns true if stamina usage is enabled */
	bool IsStaminaEnabled() const { return bEnableStamina; };

	/** Returns the Gameplay Tag Container from the Titan Tags Sync State */
	UFUNCTION(BlueprintPure, Category="Mover")
	FGameplayTagContainer GetTagsFromSyncState() const;

	/** Returns the last recorded ground contact normal, or a zero vector if not on the ground */
	UFUNCTION(BlueprintPure, Category="Mover")
	FVector GetGroundNormal() const;

	// Debug
	/////////////////////////////////////////////////////

#if ENABLE_VISUAL_LOG
	//~ Begin IVisualLoggerDebugSnapshotInterface interface

	/** Visual Logger Debug Snapshot */
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;

	//~ End IVisualLoggerDebugSnapshotInterface interface
#endif

};

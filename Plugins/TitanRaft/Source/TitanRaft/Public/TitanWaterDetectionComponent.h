// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "Chaos/ChaosEngineInterface.h"
#include "TitanWaterDetectionComponent.generated.h"

class AWaterBody;

/**
 *  UTitanWaterDetectionComponent
 *  A helper component to manage ground and water body collision checks.
 *  Provides ground and collision state information to consumers.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TITANRAFT_API UTitanWaterDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UTitanWaterDetectionComponent();

protected:

	virtual void BeginPlay() override;

protected:

	/** If true, the component will run a probe to detect if it's overlapping or above water */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection")
	bool bUseWaterProbe = true;

	/** Vertical offset over the monitored primitive's location to determine if it's submerged */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection", meta = (ClampMin = 0))
	float ImmersionDepthOffset = 50.0f;

	/** Length of the downwards probe to determine if the component is in the air above water */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection", meta = (ClampMin = 0))
	float WaterProbeLength = 200.0f;

	/** Radius of the downwards probe to determine if the component is in the air above water */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection", meta = (ClampMin = 0))
	float WaterProbeRadius = 50.0f;

	/** Collision channel to use for the downwards water probe */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection")
	TEnumAsByte<ECollisionChannel> WaterProbeChannel;

	/** Gameplay event to send when the monitored primitive is submerged */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection")
	FGameplayTag ImmersionEvent;

	/** Gameplay event to send when the monitored primitive first overlaps water */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection")
	FGameplayTag WaterBeginOverlapEvent;

	/** Gameplay event to send when the monitored primitive stops overlapping water */
	UPROPERTY(EditAnywhere, Category="Titan|Water Detection")
	FGameplayTag WaterEndOverlapEvent;

	/** If true, the component will run a probe to detect if it's touching the ground */
	UPROPERTY(EditAnywhere, Category="Titan|Ground Detection", meta = (ClampMin = 0))
	bool bUseGroundProbe = true;

	/** Length of the downwards probe to determine if the component is touching the ground */
	UPROPERTY(EditAnywhere, Category="Titan|Ground Detection", meta = (ClampMin = 0))
	float GroundProbeLength = 25.0f;

	/** Radius of the downwards probe to determine if the component is touching the ground */
	UPROPERTY(EditAnywhere, Category="Titan|Ground Detection", meta = (ClampMin = 0))
	float GroundProbeRadius = 25.0f;

	/** Gameplay event to send when the monitored primitive hits ground */
	UPROPERTY(EditAnywhere, Category="Titan|Ground Detection")
	FGameplayTag GroundContactEvent;

	/** Primitive component to use as a basis for the ground and water probes */
	TObjectPtr<UPrimitiveComponent> MonitoringPrimitive;

	/** List of water body actors the component is currently overlapping */
	TArray<AWaterBody*> OverlappingWaterBodies;

	/** Set to true if the component is considered submerged */
	bool bIsSubmerged = false;

	/** Set to true if the component is considered above water */
	bool bIsAboveWater = false;

	/** Last cached water immersion depth */
	float LastImmersionDepth = 0.0f;

	/** Last cached water depth (depth of ground at the bottom of water) */
	float LastWaterDepth = 0.0f;

	/** Set to true if the component is considered in contact with the ground */
	bool bIsOnGround = false;

public:	

	/** Runs the ground and water collision probes */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Returns true if the component is currently overlapping any water bodies */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	bool IsOverlappingWater() const;

	/** Returns true if the component was detected as currently above a water body */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	bool IsAboveWater() const;

	/** Returns true if the component was detected as currently submerged */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	bool IsSubmerged() const;

	/** Returns true if the component was detected as currently in contact with the ground and is not overlapping any water bodies */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	bool IsOnGround() const;
	
	/** Returns true if the component has a blocking collision with the ground */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	bool HasGroundTrace() const;

	/** Returns the immersion depth of the component when under the water surface */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	float GetImmersionDepth() const;

	/** Returns the water depth at the monitored component's location */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	float GetWaterDepth() const;

	/** Returns the water surface location for the sampled vector */
	UFUNCTION(BlueprintPure, Category="Titan Water Detection Component")
	FVector GetWaterSurfaceAtLocation(const FVector& QueryLocation) const;

protected:

	/** Component Begin Overlap handler */
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** End Begin Overlap handler */
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Runs the water collision probes */
	void UpdateWaterProbe();

	/** Runs the ground collision probes */
	void UpdateGroundProbe();

protected:

	/** Last cached HitResult from a ground probe */
	FHitResult LastGroundHit;

public:

	/** Returns the impact point from the last ground probe hit */
	UFUNCTION(BlueprintPure, Category="Titan")
	const FVector& GetGroundImpactPoint() const;

	/** Returns the impact normal from the last ground probe hit */
	UFUNCTION(BlueprintPure, Category="Titan")
	const FVector& GetGroundNormal() const;
	
	/** Returns the surface type of the last ground probe hit. Returns false if there's no valid physical material on the hit object. */
	UFUNCTION(BlueprintPure, Category="Titan")
	bool GetGroundSurfaceType(TEnumAsByte<EPhysicalSurface>& SurfaceType) const;
};

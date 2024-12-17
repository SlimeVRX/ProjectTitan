// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "TitanCameraComponent.generated.h"

class UTitanCameraState;
class UCurveFloat;

/**
 *  ITitanCameraOwnerInterface
 *  Interface to allow the camera to interact with Pawns' properties
 *  Allows optional support of pawn-reliant functionality such as control rotation auto-align
 */
UINTERFACE(MinimalAPI)
class UTitanCameraOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

class TITANCAMERA_API ITitanCameraOwnerInterface
{
	GENERATED_BODY()

public:

	/** Enables or disables camera auto-align */
	virtual void SetCameraAutoAlignState(bool bEnable, float AutoAlignTime, float AutoAlignSpeed) = 0;
};

/**
 *  UTitanCameraComponent
 * 
 *  Custom camera component for Titan
 *  Incorporates some built-in Spring Arm elements
 *  Supports a Camera State stack
 *  Supports spring damper blending of multiple camera and spring arm properties
 *  Does not support Additive Offsets or HMD
 */
UCLASS()
class TITANCAMERA_API UTitanCameraComponent : public UCameraComponent,
	public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	UTitanCameraComponent(const FObjectInitializer& ObjectInitializer);

protected:

	/** Optional owner interface */
	TScriptInterface<ITitanCameraOwnerInterface> CameraOwnerInterface;

	/** Max allowed delta time when updating the camera spring arm */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Update", meta=(ClampMin=0, ClampMax=1))
	float MaxDeltaTimeForCameraUpdate = 0.1f;

	// Current values used for calculating the view
	/////////////////////////////////////////////////////

	/** Current spring arm length */
	float CurrentArmLength;

	/** Spring arm length rate of channge for spring damper smoothing */
	float CurrentArmLengthRate = 0.0f;

	/** Spring arm length damp ratio for spring damper smoothing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm", meta=(ClampMin=0.0, ClampMax=10))
	float ArmLengthDampRatio = 0.75f;

	/** FOV rate of change for spring damper smoothing */
	float FOVRate = 0.0f;

	/** FOV damp ratio for spring damper smoothing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|FOV", meta=(ClampMin=0.0, ClampMax=10))
	float FOVDampRatio = 0.75f;

	/** Current target offset */
	FVector CurrentOffset;

	/** Current target offset rate of change for spring damper smoothing */
	FVector CurrentOffsetRate = FVector::ZeroVector;

	/** Current velocity offset */
	FVector CurrentVelocityOffset;

	/** Velocity offset rate of change for spring damper smoothing */
	FVector CurrentVelocityOffsetRate = FVector::ZeroVector;

	/** Scales the desired spring arm length */
	float CurrentArmLengthMultiplier = 1.0f;

	/** Maximum allowed value for the arm length multiplier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm", meta=(ClampMin=0.1, ClampMax=10))
	float MinArmLengthMultiplier = 1.0f;

	/** Maximum allowed value for the arm length multiplier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm", meta=(ClampMin=1, ClampMax=10))
	float MaxArmLengthMultiplier = 2.0f;

	/** If a curve is provided, the camera pitch will multiply the spring arm length by this curve's value. This is multiplicative with the global arm length multiplier. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	TObjectPtr<UCurveFloat> ArmLengthMultiplierPitchCurve;

	/** Max allowed velocity offset vector length */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	float MaxVelocityOffset = 200.0f;

	/** Radius for the velocity offset capsule collision check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	float VelocityOffsetProbeRadius = 34.0f;

	/** Half height for the velocity offset capsule collision check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	float VelocityOffsetProbeHalfHeight = 88.0f;

	/** Current value for enabling camera lag */
	uint32 bEnableCameraLag : 1;

	/** Current value for enabling camera rotation lag */
	uint32 bEnableCameraRotationLag : 1;

	/** If true, the spring arm will clip against the min camera bounds in the X axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMinX : 1;

	/** If true, the spring arm will clip against the max camera bounds in the X axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMaxX : 1;

	/** If true, the spring arm will clip against the min camera bounds in the Y axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMinY : 1;

	/** If true, the spring arm will clip against the max camera bounds in the Y axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMaxY : 1;

	/** If true, the spring arm will clip against the min camera bounds in the Z axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMinZ : 1;

	/** If true, the spring arm will clip against the max camera bounds in the Z axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bUseCameraBoundsMaxZ : 1;

	/** Camera spring arm bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	FBox CameraBounds;

	/** Current value for camera lag speed */
	float CameraLagSpeed;

	/** Rate of change of the camera lag speed for spring damper smoothing */
	float CameraLagSpeedRate = 0.0f;

	/** Current value for camera rotation lag speed */
	float CameraRotationLagSpeed;

	/** Rotation lag speed rate of change for spring damper smoothing */
	float CameraRotationLagSpeedRate = 0.0f;

	/** Current value for camera max lag distance */
	float CameraMaxLagDistance;

	/** Lag distance rate of change for spring damper smoothing */
	float CameraMaxLagDistanceRate = 0.0f;

	/** Last calculated target location, including offsets */
	FVector LastDesiredTarget;

	/** Desired target rate of change for spring damper smoothing */
	FVector LastDesiredTargetRate = FVector::ZeroVector;

	/** Last calculated camera rotation */
	FRotator LastDesiredRotation;

	/** Desired rotation rate of change for spring damper smoothing */
	FVector DesiredRotationRate = FVector::ZeroVector;

	/** Last calculated view location */
	FVector LastViewLocation;

	/** Last calculated view rotation */
	FRotator LastViewRotation;
	
	/** Last calculated velocity dot */
	float LastVelocityDot = 0.0f;

	/** Velocity dot rate of change for spring damper smoothing */
	float LastVelocityDotRate = 0.0f;

	// Spring arm collision 
	/////////////////////////////////////////////////////

	/** Radius for the spring arm sphere collision check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	float CollisionProbeRadius = 12.0f;

	/** Collision channel for the spring arm collision check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Spring Arm")
	TEnumAsByte<ECollisionChannel> CollisionProbeChannel;

	// Camera state stack
	/////////////////////////////////////////////////////

	/** Pointer to the default camera state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|States")
	UTitanCameraState* DefaultCameraState;

	/** Stack of camera states. The LAST element in the array is considered active */
	TArray<UTitanCameraState*> CameraStateStack;

protected:

	/** BeginPlay camera initialization */
	virtual void BeginPlay() override;

public:

	/** Override to use camera states and spring arm prior to calculating the camera view */
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

public:

	/** Adds a camera state if a state with that tag isn't present in the stack, or updates its values if it does. */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void PushOrReplaceCameraState(UTitanCameraState* CameraState);

	/** Removes a camera state with a given tag if it's present in the stack */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void RemoveCameraState(const FGameplayTag& StateTag);

	/** Returns a camera state if it's present in the stack */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	UTitanCameraState* GetCameraState(const FGameplayTag& StateTag);

	/** Returns the currently active camera state */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	UTitanCameraState* GetActiveCameraState() const;

	/** Returns the last calculated view location */
	UFUNCTION(BlueprintPure, Category="Titan Camera")
	FVector GetViewLocation()
	{
		return LastViewLocation;
	}

	/** Returns the last calculated view rotation */
	UFUNCTION(BlueprintPure, Category="Titan Camera")
	FRotator GetViewRotation()
	{
		return LastViewRotation;
	}

	/** Allows the camera to initialize player camera manager-specific settings */
	void InitializeCameraForPlayer();

	/** Adjusts the spring arm length multiplier by the given delta value. */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void AdjustArmLengthMultiplier(float Delta);

	/** Resets the spring arm length multiplier to 1 */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void ResetArmLengthMultiplier();

	/** Sets all the camera spring arm bounds */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetAllCameraBounds(bool bEnable, const FBox& CamBounds);

	/** Sets the camera spring arm min bounds in the X axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMinX(bool bEnable, float Value);

	/** Sets the camera spring arm max bounds in the X axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMaxX(bool bEnable, float Value);

	/** Sets the camera spring arm min bounds in the Y axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMinY(bool bEnable, float Value);

	/** Sets the camera spring arm max bounds in the Y axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMaxY(bool bEnable, float Value);

	/** Sets the camera spring arm min bounds in the Z axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMinZ(bool bEnable, float Value);

	/** Sets the camera spring arm max bounds in the Z axis */
	UFUNCTION(BlueprintCallable, Category="Titan Camera")
	void SetCameraBoundsMaxZ(bool bEnable, float Value);

public:

	/** Updates the camera pitch limits on the player camera manager */
	void UpdateCameraPitchLimits(float InPitchMin, float InPitchMax);

protected:

	/* name of the camera socket */
	static const FName SocketName;

public:

	/** socket interface */
	virtual bool HasAnySockets() const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;

	// Debug
	/////////////////////////////////////////////////////

#if ENABLE_VISUAL_LOG
	//~ Begin IVisualLoggerDebugSnapshotInterface interface
	
	/** Visual Logger Debug Snapshot */
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;

	//~ End IVisualLoggerDebugSnapshotInterface interface
#endif

	// friend access to camera states
	friend class UTitanCameraState;
};
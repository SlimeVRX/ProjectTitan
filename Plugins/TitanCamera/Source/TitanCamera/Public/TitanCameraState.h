// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "TitanCameraState.generated.h"

class UTitanCameraComponent;

/**
 *  UTitanCameraState
 *	Represents a single state in the camera state machine
 *  Allows blends and transitions between camera states
 */
UCLASS(BlueprintType)
class TITANCAMERA_API UTitanCameraState : public UDataAsset
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	UTitanCameraState(const FObjectInitializer& ObjectInitializer);

	/** Initializes the camera with this state data. Normally called when this is the first state to get applied. */
	virtual void InitializeCamera(UTitanCameraComponent* Camera) const;

	/** Called when this camera state becomes the topmost in the state stack */
	virtual void EnterState(UTitanCameraComponent* Camera) const;

	/** Called when this camera state is removed from the state stack while being topmost */
	virtual void ExitState(UTitanCameraComponent* Camera) const;

	/** Returns the state's tag */
	UFUNCTION(BlueprintPure, Category="Camera State")
	const FGameplayTag& GetTag() const { return Tag; }

protected:

	/** Blends the camera towards this state */
	virtual void BlendCameraState(UTitanCameraComponent* Camera, float DeltaTime) const;

	/** Calculates and returns the camera's desired rotation */
	virtual FRotator CalculateCameraRotation(UTitanCameraComponent* Camera, float DeltaTime) const;

	/** Calculates and returns the camera's desired target location */
	virtual FVector CalculateCameraTarget(UTitanCameraComponent* Camera, const FRotator& DesiredRotation, float DeltaTime) const;
	
	/** Calculates and returns the camera output view location and rotation */
	virtual void CalculateCameraView(UTitanCameraComponent* Camera, float DeltaTime, const FVector& CameraTarget, const FRotator& DesiredRotation, FVector& OutViewLocation, FRotator& OutViewRotation) const;

	/** Gameplay Tag that identifies this camera state on the stack */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera")
	FGameplayTag Tag;

	/** Speed at which camera state should blend to this state's settings */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category ="Camera|Transition", meta = (ClampMin = 0.0, ClampMax = 5.0))
	float BlendTime = 1.0f;

	/** Desired spring arm length */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category ="Camera|Spring Arm", meta = (ClampMin = 0.0))
	float ArmLength = 600.0f;

	/** Component-space offset on the camera focus target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Offset")
	FVector TargetOffset;

	/** Adds the owning actor's velocity multiplied by this factor to the target offset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Offset", meta = (ClampMin = 0.0, ClampMax = 2.0))
	float VelocityOffsetMultiplier = 0.0f;

	/** Desired camera FOV */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|FOV", meta = (ClampMin = 0.0, ClampMax = 180.0))
	float FieldOfView = 80.0f;

	/** If true, this camera state will use the global arm length multiplier */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bAllowGlobalArmLengthMultiplier : 1;

	/** If true, this camera state will use the pitch-based arm length multiplier, as long as the corresponding curve is provided in the camera comp */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category ="Camera|Spring Arm")
	uint32 bAllowPitchArmLengthMultiplier : 1;

	/** If true, enables camera lag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Lag")
	uint32 bEnablePositionLag : 1;

	/** If true, enables camera rotation lag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Lag")
	uint32 bEnableRotationLag : 1;

	/** If true, the camera state will override the pawn's camera auto alignment values */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Auto Align")
	uint32 bOverrideAutoAlign : 1;

	/** Maximum distance the camera can lag behind its target location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Lag", meta = (ClampMin = 0.0))
	float MaxLagDistance = 600.0f;

	/** Speed to blend towards the desired camera location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Lag", meta = (ClampMin = 0.0, ClampMax = 5.0))
	float LagTime = 1.0f;

	/** Speed to blend towards the desired camera rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Lag", meta = (ClampMin = 0.0, ClampMax = 5.0))
	float RotationLagTime = 1.0f;

	/** Min allowed control pitch for the camera */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Pitch", meta = (ClampMin = -90.0, ClampMax = 90.0))
	float MinPitch = -90.0f;

	/** Max allowed control pitch for the camera */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Pitch", meta = (ClampMin = -90.0, ClampMax = 90.0))
	float MaxPitch = 90.0f;

	/** Override auto alignment time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Auto Align", meta = (ClampMin = 0.0, ClampMax = 15.0))
	float AutoAlignTriggerTime = 0.0f;

	/** Override auto alignment blend time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Camera|Auto Align", meta = (ClampMin = 0.0, ClampMax = 5.0))
	float AutoAlignBlendTime = 1.0f;

	// allows camera access to private members
	friend class UTitanCameraComponent;
};

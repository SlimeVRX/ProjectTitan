// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanCameraState.h"
#include "TitanCameraComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TitanCameraMath.h"
#include "Curves/CurveFloat.h"
#include "TitanCameraLogging.h"

UTitanCameraState::UTitanCameraState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowGlobalArmLengthMultiplier = true;
	bAllowPitchArmLengthMultiplier = true;
	bEnablePositionLag = true;
	bEnableRotationLag = true;
	bOverrideAutoAlign = false;
}

void UTitanCameraState::InitializeCamera(UTitanCameraComponent* Camera) const
{
	check(Camera);

	// set the current interp values
	Camera->CurrentArmLength = ArmLength;
	Camera->CurrentOffset = TargetOffset;
	Camera->CurrentVelocityOffset = FVector::ZeroVector;
	Camera->FieldOfView = FieldOfView;
	Camera->CameraLagSpeed = LagTime;
	Camera->CameraRotationLagSpeed = RotationLagTime;
	Camera->CameraMaxLagDistance = MaxLagDistance;

	// update the pitch limits
	Camera->UpdateCameraPitchLimits(MinPitch, MaxPitch);
}

void UTitanCameraState::EnterState(UTitanCameraComponent* Camera) const
{
	check(Camera);

	// set the pitch limits
	Camera->UpdateCameraPitchLimits(MinPitch, MaxPitch);

	// copy the non-interp values
	Camera->bEnableCameraLag = bEnablePositionLag;
	Camera->bEnableCameraRotationLag = bEnableRotationLag;

	// set the auto align state on the owner
	if (IsValid(Camera->CameraOwnerInterface.GetObjectRef()))
	{
		Camera->CameraOwnerInterface->SetCameraAutoAlignState(bOverrideAutoAlign, AutoAlignTriggerTime, AutoAlignBlendTime);
	}
}

void UTitanCameraState::ExitState(UTitanCameraComponent* Camera) const
{
	check(Camera);
}

void UTitanCameraState::BlendCameraState(UTitanCameraComponent* Camera, float DeltaTime) const
{
	check(Camera);

	// interp to the desired values
	float ArmMultiplier = bAllowGlobalArmLengthMultiplier ? Camera->CurrentArmLengthMultiplier : 1.0f;

	const float LastPitch = Camera->LastViewRotation.Pitch;

	if (bAllowGlobalArmLengthMultiplier && Camera->ArmLengthMultiplierPitchCurve)
	{
		ArmMultiplier *= Camera->ArmLengthMultiplierPitchCurve->GetFloatValue(LastPitch);
	}

	// smooth the arm length
	FMath::SpringDamperSmoothing(Camera->CurrentArmLength, Camera->CurrentArmLengthRate, ArmLength * ArmMultiplier, 0.0f, DeltaTime, BlendTime, Camera->ArmLengthDampRatio);

	// smooth the camera offset
	FMath::CriticallyDampedSmoothing(Camera->CurrentOffset, Camera->CurrentOffsetRate, TargetOffset, FVector::ZeroVector, DeltaTime, BlendTime);

	// smooth the FOV
	FMath::SpringDamperSmoothing(Camera->FieldOfView, Camera->FOVRate, FieldOfView, 0.0f, DeltaTime, BlendTime, Camera->FOVDampRatio);

	// smooth the velocity offset
	FVector TargetVelocityOffset = Camera->GetOwner()->GetRootComponent()->GetComponentVelocity() * VelocityOffsetMultiplier;
	TargetVelocityOffset.Z = 0.0f;

	TargetVelocityOffset.GetClampedToMaxSize2D(Camera->MaxVelocityOffset);

	FMath::CriticallyDampedSmoothing(Camera->CurrentVelocityOffset, Camera->CurrentVelocityOffsetRate, TargetVelocityOffset, FVector::ZeroVector, DeltaTime, BlendTime);

	// set the camera lag times
	Camera->CameraLagSpeed = LagTime;
	Camera->CameraRotationLagSpeed = RotationLagTime;

	// smooth the max lag distance
	FMath::CriticallyDampedSmoothing(Camera->CameraMaxLagDistance, Camera->CameraMaxLagDistanceRate, MaxLagDistance, 0.0f, DeltaTime, BlendTime);
}

FRotator UTitanCameraState::CalculateCameraRotation(UTitanCameraComponent* Camera, float DeltaTime) const
{
	FRotator DesiredRotation = Camera->GetComponentRotation();

	// get the pawn owner
	if (APawn* OwningPawn = Cast<APawn>(Camera->GetOwner()))
	{
		// get the pawn's view rotation
		DesiredRotation = OwningPawn->GetViewRotation();
	}

	// apply rotation lag
	if (Camera->bEnableCameraRotationLag)
	{
		FQuat DesiredQuat = FQuat(Camera->LastDesiredRotation);

		UTitanCameraMath::QuatSpringSmoothing(DesiredQuat, Camera->DesiredRotationRate, FQuat(DesiredRotation), Camera->CameraRotationLagSpeed, DeltaTime);

		DesiredRotation = FRotator(DesiredQuat);
	}

	// return the desired rotation
	return DesiredRotation;
}

FVector UTitanCameraState::CalculateCameraTarget(UTitanCameraComponent* Camera, const FRotator& DesiredRotation, float DeltaTime) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpringArm), false, Camera->GetOwner());
	FHitResult Result(ForceInitToZero);

	FCollisionShape ProbeShape = FCollisionShape::MakeCapsule(Camera->VelocityOffsetProbeRadius, Camera->VelocityOffsetProbeHalfHeight);

	// calculate the arm origin, applying the rotated offset
	FVector ArmOrigin = Camera->GetComponentLocation() + Camera->GetComponentRotation().RotateVector(Camera->CurrentOffset);

	// run a sweep from the component location to the offset arm origin
	// this should prevent the arm origin from being stuck inside geometry and causing further sweeps to fail
	Camera->GetWorld()->SweepSingleByChannel(Result, Camera->GetComponentLocation(), ArmOrigin, FQuat::Identity, Camera->CollisionProbeChannel, ProbeShape, QueryParams);

	if (Result.bBlockingHit)
	{
		ArmOrigin = Result.Location;
	}

	// do we need to apply a velocity offset?
	if (Camera->CurrentVelocityOffset.SizeSquared() > 0.0f)
	{
		// run a sweep from the origin to the velocity displaced origin.
		// We may be moving towards geometry that would occlude the spring arm
		Camera->GetWorld()->SweepSingleByChannel(Result, ArmOrigin, ArmOrigin + Camera->CurrentVelocityOffset, FQuat::Identity, Camera->CollisionProbeChannel, ProbeShape, QueryParams);

		// did we hit something?
		if (Result.bBlockingHit)
		{
			// use the hit location as the arm origin
			ArmOrigin = Result.Location;
		}
		else {

			// no obstructions, so just use the full offset
			ArmOrigin += Camera->CurrentVelocityOffset;
		}
	}

	// apply camera lag
	if (Camera->bEnableCameraLag)
	{
		// get the flat dot product of our velocity and the view
		// lag will be applied in proportion to the dot, and minimized for perpendicular movement
		const FVector FlatVelocity = Camera->GetOwner()->GetRootComponent()->GetComponentVelocity().GetSafeNormal2D();
		const FVector FlatView = DesiredRotation.RotateVector(FVector::ForwardVector).GetSafeNormal2D();

		const float VelocityDot = 1.0f - FMath::Clamp(FVector::DotProduct(FlatVelocity, FlatView), 0.0f, 1.0f);

		// calculate the interpolated lag
		FVector LagLocation = Camera->LastDesiredTarget;
		
		FMath::CriticallyDampedSmoothing(LagLocation, Camera->LastDesiredTargetRate, ArmOrigin, FVector::ZeroVector, DeltaTime, Camera->CameraLagSpeed);
		
		FMath::CriticallyDampedSmoothing(Camera->LastVelocityDot, Camera->LastVelocityDotRate, VelocityDot, 0.0f, DeltaTime, Camera->CameraLagSpeed);

		// use the dot to lerp between the lagged and original desired location so that 1 = desired and 0 = lagged
		ArmOrigin = FMath::Lerp(LagLocation, ArmOrigin, Camera->LastVelocityDot);
	}

	// return the arm origin
	return ArmOrigin;
}

void UTitanCameraState::CalculateCameraView(UTitanCameraComponent* Camera, float DeltaTime, const FVector& CameraTarget, const FRotator& DesiredRotation, FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	FVector DesiredLocation = CameraTarget;
	
	// apply max lag distance
	if (Camera->CameraMaxLagDistance > 0.0f)
	{
		const FVector FromOrigin = DesiredLocation - CameraTarget;
		if (FromOrigin.SizeSquared() > FMath::Square(Camera->CameraMaxLagDistance))
		{
			DesiredLocation = CameraTarget + FromOrigin.GetClampedToMaxSize(Camera->CameraMaxLagDistance);
		}
	}

	// apply the spring arm length
	DesiredLocation -= DesiredRotation.Vector() * Camera->CurrentArmLength;

	//  check against any enabled camera bounds
	if (Camera->bUseCameraBoundsMinX && Camera->CameraBounds.Min.X > DesiredLocation.X)
	{
		DesiredLocation.X = Camera->CameraBounds.Min.X;
	}

	if (Camera->bUseCameraBoundsMaxX && Camera->CameraBounds.Max.X < DesiredLocation.X)
	{
		DesiredLocation.X = Camera->CameraBounds.Max.X;
	}

	if (Camera->bUseCameraBoundsMinY && Camera->CameraBounds.Min.Y > DesiredLocation.Y)
	{
		DesiredLocation.Y = Camera->CameraBounds.Min.Y;
	}

	if (Camera->bUseCameraBoundsMaxY && Camera->CameraBounds.Max.Y < DesiredLocation.Y)
	{
		DesiredLocation.Y = Camera->CameraBounds.Max.Y;
	}

	if (Camera->bUseCameraBoundsMinZ && Camera->CameraBounds.Min.Z > DesiredLocation.Z)
	{
		DesiredLocation.Z = Camera->CameraBounds.Min.Z;
	}

	if (Camera->bUseCameraBoundsMaxZ && Camera->CameraBounds.Max.Z < DesiredLocation.Z)
	{
		DesiredLocation.Z = Camera->CameraBounds.Max.Z;
	}

	// run a collision check to adjust the spring arm location against the world geometry
	if (Camera->CurrentArmLength != 0.0f)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpringArm), false, Camera->GetOwner());

		FHitResult Result;
		Camera->GetWorld()->SweepSingleByChannel(Result, CameraTarget, DesiredLocation, FQuat::Identity, Camera->CollisionProbeChannel, FCollisionShape::MakeSphere(Camera->CollisionProbeRadius), QueryParams);

		// did we hit something?
		if (Result.bBlockingHit)
		{
			// blend to the hit location
			DesiredLocation = Result.Location;
		}
	}

	// save the view location and rotation
	OutViewLocation = DesiredLocation;
	OutViewRotation = (CameraTarget - DesiredLocation).ToOrientationRotator();
}

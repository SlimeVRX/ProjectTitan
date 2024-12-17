// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanCameraComponent.h"
#include "Rendering/MotionVectorSimulation.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "TitanCameraLogging.h"
#include "VisualLogger/VisualLogger.h"
#include "TitanCameraState.h"

const FName UTitanCameraComponent::SocketName(TEXT("CameraEndpoint"));

UTitanCameraComponent::UTitanCameraComponent(const FObjectInitializer& ObjectInitializer)
{
	// initialize all camera bound axis
	bUseCameraBoundsMinX = false;
	bUseCameraBoundsMaxX = false;
	bUseCameraBoundsMinY = false;
	bUseCameraBoundsMaxY = false;
	bUseCameraBoundsMinZ = false;
	bUseCameraBoundsMaxZ = false;
}

void UTitanCameraComponent::BeginPlay()
{
	Super::BeginPlay();

#if ENABLE_VISUAL_LOG
	// redirect Visual Logger to the owning Actor
	REDIRECT_TO_VLOG(GetOwner());
#endif

	// cast the owner to the camera interface
	CameraOwnerInterface = GetOwner();

	// initialize and enter the default camera state
	GetActiveCameraState()->InitializeCamera(this);
	GetActiveCameraState()->EnterState(this);

	// initialize the cached target location and rotation from last frame
	LastDesiredRotation = GetComponentRotation();
	LastDesiredTarget = GetComponentLocation() + LastDesiredRotation.RotateVector(CurrentOffset);
}

void UTitanCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	// clamp to max allowed DeltaTime
	const float ClampedDeltaTime = FMath::Min(DeltaTime, MaxDeltaTimeForCameraUpdate);

	// get the active camera state
	UTitanCameraState* CurrentState = GetActiveCameraState();

	// blend properties to the active camera state
	CurrentState->BlendCameraState(this, ClampedDeltaTime);

	// calculate the desired camera rotation
	LastDesiredRotation = CurrentState->CalculateCameraRotation(this, ClampedDeltaTime);

	// calculate the desired camera target
	LastDesiredTarget = CurrentState->CalculateCameraTarget(this, LastDesiredRotation, DeltaTime);

	// calculate the camera view
	CurrentState->CalculateCameraView(this, DeltaTime, LastDesiredTarget, LastDesiredRotation, LastViewLocation, LastViewRotation);

	// set the basic camera values
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;

	if (bOverrideAspectRatioAxisConstraint)
	{
		DesiredView.AspectRatioAxisConstraint = AspectRatioAxisConstraint;
	}

	// See if the camera wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}

	// If this camera component has a motion vector simulation transform, use that for the current view's previous transform
	DesiredView.PreviousViewTransform = FMotionVectorSimulation::Get().GetPreviousTransform(this);

	// set the FOV
	DesiredView.FOV = FieldOfView;

	// set the desired view location and rotation
	DesiredView.Location = LastViewLocation;
	DesiredView.Rotation = LastViewRotation;

	// add debug data to visual logger
#if ENABLE_VISUAL_LOG

	UE_VLOG(this, VLogTitanCamera, Log, TEXT("GetCameraView"));
	
#endif

}

void UTitanCameraComponent::PushOrReplaceCameraState(UTitanCameraState* CameraState)
{
	// search for an existing state on the stack
	for (int32 i = 0; i < CameraStateStack.Num(); ++i)
	{
		UTitanCameraState* CurrentState = CameraStateStack[i];

		// do we have a matching tag?
		if (CurrentState->GetTag() == CameraState->GetTag())
		{
			bool bActiveStateChanged = CurrentState == GetActiveCameraState();

			// exit the state
			if (bActiveStateChanged)
			{
				CurrentState->ExitState(this);
			}

			// do a member-wise copy of the state
			CurrentState = CameraState;

			if (bActiveStateChanged)
			{
				CameraState->EnterState(this);
			}

			return;
		}
	}

	// no matches

	// exit the active state
	GetActiveCameraState()->ExitState(this);

	// add the camera state to the stack
	CameraStateStack.Add(CameraState);

	// enter the new state
	CameraState->EnterState(this);	
}

void UTitanCameraComponent::RemoveCameraState(const FGameplayTag& StateTag)
{
	// iterate through the state stack
	for (int32 i = 0; i < CameraStateStack.Num(); ++i)
	{
		// do we have a matching tag?
		if (CameraStateStack[i]->GetTag() == StateTag)
		{
			bool bActiveStateRemoved = i == CameraStateStack.Num() - 1;

			// have we removed the active state?
			if (bActiveStateRemoved)
			{
				// exit the state
				CameraStateStack[i]->ExitState(this);
			}

			// remove the state at the given index
			CameraStateStack.RemoveAt(i);

			// let the new active state 
			if (bActiveStateRemoved)
			{
				GetActiveCameraState()->EnterState(this);
			}

			return;
		}
	}
}

UTitanCameraState* UTitanCameraComponent::GetCameraState(const FGameplayTag& StateTag)
{
	// iterate through the camera state stack
	for (UTitanCameraState* CurrentState : CameraStateStack)
	{
		// do we have a matching tag?
		if (CurrentState->GetTag() == StateTag)
		{
			// return the camera state
			return CurrentState;
		}
	}

	// state wasn't found
	return nullptr;
}

UTitanCameraState* UTitanCameraComponent::GetActiveCameraState() const
{
	// if we have states in the stack, return the last one
	if (CameraStateStack.Num() > 0)
	{
		return CameraStateStack[CameraStateStack.Num() - 1];
	}

	// otherwise return the default camera state
	return DefaultCameraState;
}

void UTitanCameraComponent::InitializeCameraForPlayer()
{
	// let the active state initialize the camera
	GetActiveCameraState()->InitializeCamera(this);
}

void UTitanCameraComponent::AdjustArmLengthMultiplier(float Delta)
{
	CurrentArmLengthMultiplier = FMath::Clamp(CurrentArmLengthMultiplier + Delta, MinArmLengthMultiplier, MaxArmLengthMultiplier);
}

void UTitanCameraComponent::ResetArmLengthMultiplier()
{
	CurrentArmLengthMultiplier = 1.0f;
}

void UTitanCameraComponent::SetAllCameraBounds(bool bEnable, const FBox& CamBounds)
{
	// set the use flags
	bUseCameraBoundsMinX = bEnable;
	bUseCameraBoundsMaxX = bEnable;
	bUseCameraBoundsMinY = bEnable;
	bUseCameraBoundsMaxY = bEnable;
	bUseCameraBoundsMinZ = bEnable;
	bUseCameraBoundsMaxZ = bEnable;

	// set the bounds
	CameraBounds = CamBounds;
}

void UTitanCameraComponent::SetCameraBoundsMinX(bool bEnable, float Value)
{
	bUseCameraBoundsMinX = bEnable;

	CameraBounds.Min.X = Value;
}

void UTitanCameraComponent::SetCameraBoundsMaxX(bool bEnable, float Value)
{
	bUseCameraBoundsMaxX = bEnable;

	CameraBounds.Max.X = Value;
}

void UTitanCameraComponent::SetCameraBoundsMinY(bool bEnable, float Value)
{
	bUseCameraBoundsMinY = bEnable;

	CameraBounds.Min.Y = Value;
}

void UTitanCameraComponent::SetCameraBoundsMaxY(bool bEnable, float Value)
{
	bUseCameraBoundsMaxY = bEnable;

	CameraBounds.Max.Y = Value;
}

void UTitanCameraComponent::SetCameraBoundsMinZ(bool bEnable, float Value)
{
	bUseCameraBoundsMinZ = bEnable;

	CameraBounds.Min.Z = Value;
}

void UTitanCameraComponent::SetCameraBoundsMaxZ(bool bEnable, float Value)
{
	bUseCameraBoundsMaxZ = bEnable;

	CameraBounds.Max.Z = Value;
}

void UTitanCameraComponent::UpdateCameraPitchLimits(float InPitchMin, float InPitchMax)
{
	// get the player camera manager
	if (APawn* OwningPawn = GetOwner<APawn>())
	{
		if (APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController()))
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				// set the min and max view pitch
				CameraManager->ViewPitchMin = InPitchMin;
				CameraManager->ViewPitchMax = InPitchMax;
			}
		}
	}
}

bool UTitanCameraComponent::HasAnySockets() const
{
	return true;
}

FTransform UTitanCameraComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace /*= RTS_World*/) const
{
	FTransform WorldTransform(LastViewRotation, LastViewLocation);

	switch (TransformSpace)
	{
	case RTS_World:

		return WorldTransform;
		break;
	case RTS_Actor:

		if (const AActor* Actor = GetOwner())
		{
			return WorldTransform.GetRelativeTransform(Actor->GetTransform());
		}
		break;

	case RTS_Component:

		return WorldTransform.GetRelativeTransform(GetComponentTransform());
	}
	return WorldTransform;
}

void UTitanCameraComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	new (OutSockets) FComponentSocketDescription(SocketName, EComponentSocketType::Socket);
}

#if ENABLE_VISUAL_LOG
void UTitanCameraComponent::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	// pre-calculate some values
	const FVector OwnerLoc = GetOwner()->GetActorLocation();
	const FVector TargetLoc = LastDesiredTarget;
	const FVector CamOffset = CurrentOffset + CurrentVelocityOffset;
	
	const float CamYaw = (LastViewLocation - OwnerLoc).GetSafeNormal2D().ToOrientationRotator().Yaw;
	const float ConeLength = 100.0f;
	const UTitanCameraState* CurrentState = GetActiveCameraState();

	const FVector MinPitch = (FRotator(-CurrentState->MinPitch, CamYaw, 0.0f).Vector() * ConeLength) + TargetLoc;
	const FVector MaxPitch = (FRotator(-CurrentState->MaxPitch, CamYaw, 0.0f).Vector() * ConeLength) + TargetLoc;

	// add the camera category
	const int32 CatIndex = Snapshot->Status.AddZeroed();
	FVisualLogStatusCategory& PlaceableCategory = Snapshot->Status[CatIndex];
	PlaceableCategory.Category = TEXT("Titan Camera");

	Snapshot->Location = OwnerLoc;

	// add text data
	PlaceableCategory.Add(TEXT("Current State"), CurrentState->GetTag().ToString());

	PlaceableCategory.Add(TEXT("FOV"), FString::Printf(TEXT("%f"), FieldOfView));
	PlaceableCategory.Add(TEXT("Arm Length"), FString::Printf(TEXT("%f"), CurrentArmLength));
	PlaceableCategory.Add(TEXT("Target Offset"), CurrentOffset.ToCompactString());
	PlaceableCategory.Add(TEXT("Velocity Offset"), CurrentVelocityOffset.ToCompactString());
	PlaceableCategory.Add(TEXT("Last Velocity Dot"), FString::Printf(TEXT("%f"), LastVelocityDot));

	PlaceableCategory.Add(TEXT("Pitch Min"), FString::Printf(TEXT("%f"), CurrentState->MinPitch));
	PlaceableCategory.Add(TEXT("Pitch Max"), FString::Printf(TEXT("%f"), CurrentState->MaxPitch));

	PlaceableCategory.Add(TEXT("Lag"), bEnableCameraLag ? "Enabled" : "Disabled");
	PlaceableCategory.Add(TEXT("Lag Speed"), FString::Printf(TEXT("%f"), CameraLagSpeed));
	PlaceableCategory.Add(TEXT("Max Lag Distance"), FString::Printf(TEXT("%f"), CameraMaxLagDistance));

	PlaceableCategory.Add(TEXT("Rot Lag"), bEnableCameraRotationLag ? "Enabled" : "Disabled");
	PlaceableCategory.Add(TEXT("Rot Lag Speed"), FString::Printf(TEXT("%f"), CameraRotationLagSpeed));
	
	PlaceableCategory.Add(TEXT("Override Auto Align"), CurrentState->bOverrideAutoAlign ? "Enabled" : "Disabled");
	PlaceableCategory.Add(TEXT("Auto Align Trigger Time"), FString::Printf(TEXT("%f"), CurrentState->AutoAlignTriggerTime));
	PlaceableCategory.Add(TEXT("Auto Align Blend Time"), FString::Printf(TEXT("%f"), CurrentState->AutoAlignBlendTime));

	// draw the owner capsule
	Snapshot->AddCapsule(
		OwnerLoc, 40.0f, 20.0f, FQuat::Identity,
		VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Cyan, "", false);

	// draw the pitch limits
	Snapshot->AddArrow(
		TargetLoc, MinPitch,
		VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Orange, FString::Printf(TEXT("Min Pitch[%f]"), CurrentState->MinPitch));

	Snapshot->AddArrow(
		TargetLoc, MaxPitch,
		VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Orange, FString::Printf(TEXT("Max Pitch[%f]"), CurrentState->MaxPitch));

	// draw the target location and boom
	Snapshot->AddArrow(
		LastViewLocation, TargetLoc,
		VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Magenta);

	// draw the camera frustum
	const float FrustumX = ConeLength;
	const float H = ConeLength / FMath::Cos(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float FrustumY = H * FMath::Sin(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float FrustumZ = FrustumY * 0.75f;

	TArray<FVector> FrustumPoints;
	FrustumPoints.Add(LastViewLocation);
	FrustumPoints.Add(LastViewLocation + LastViewRotation.RotateVector(FVector(FrustumX, FrustumY, FrustumZ)));
	FrustumPoints.Add(LastViewLocation + LastViewRotation.RotateVector(FVector(FrustumX, -FrustumY, FrustumZ)));
	FrustumPoints.Add(LastViewLocation + LastViewRotation.RotateVector(FVector(FrustumX, -FrustumY, -FrustumZ)));
	FrustumPoints.Add(LastViewLocation + LastViewRotation.RotateVector(FVector(FrustumX, FrustumY, -FrustumZ)));
	FrustumPoints.Add(LastViewLocation + LastViewRotation.RotateVector(FVector(FrustumX, 0.0f, FrustumZ * 1.5f)));

	TArray<int32> FrustumIndices;
	FrustumIndices.Add(0);
	FrustumIndices.Add(1);
	FrustumIndices.Add(2);
	FrustumIndices.Add(0);
	FrustumIndices.Add(2);
	FrustumIndices.Add(3);
	FrustumIndices.Add(0);
	FrustumIndices.Add(3);
	FrustumIndices.Add(4);
	FrustumIndices.Add(0);
	FrustumIndices.Add(4);
	FrustumIndices.Add(1);
	FrustumIndices.Add(5);
	FrustumIndices.Add(2);
	FrustumIndices.Add(1);

	Snapshot->AddMesh(FrustumPoints, FrustumIndices, VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Magenta, "");

	Snapshot->AddLocation(LastViewLocation, VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Magenta, FString::Printf(TEXT("FOV[% f]"), FieldOfView));

	Snapshot->AddSphere(TargetLoc, 10.0f,
		VLogTitanCamera.GetCategoryName(), ELogVerbosity::Log, FColor::Magenta, FString::Printf(TEXT("TGT[%s]"), *CamOffset.ToCompactString()), 1);
}
#endif
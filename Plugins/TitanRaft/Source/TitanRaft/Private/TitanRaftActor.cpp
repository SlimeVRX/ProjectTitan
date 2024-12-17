// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanRaftActor.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Curves/CurveFloat.h"
#include "BuoyancyComponent.h"
#include "TitanRaftLogging.h"
#include "Components/SkeletalMeshComponent.h"
#include "WaterBodyActor.h"
#include "Engine/World.h"
#include "TitanWaterDetectionComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "TitanMoverComponent.h"
#include "TitanMoverTypes.h"
#include "TitanRaftTeleportEffect.h"
#include "TitanLayeredMove_Jump.h"
#include "TitanCameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "GameFramework/PhysicsVolume.h"



ATitanRaft::ATitanRaft()
{
	PrimaryActorTick.bCanEverTick = true;

	// set up the post physics tick function
	PostPhysicsTickFunction.TickFunction = [this](float DeltaTime) { PostPhysicsTick(DeltaTime); };
	PostPhysicsTickFunction.TickGroup = TG_PostPhysics;
	PostPhysicsTickFunction.EndTickGroup = TG_PostPhysics;
	PostPhysicsTickFunction.DiagnosticMessageString = "ATitanRaft::UpdatePostPhysics";
	PostPhysicsTickFunction.DiagnosticContextString = "ATitanRaft::UpdatePostPhysics";

	// create the sphere
	RootBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Root Box"));
	check(RootBox);

	// configure the sphere
	SetRootComponent(RootBox);

	// create the mesh
	RaftMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Raft Mesh"));
	check(RaftMesh);

	// configure the mesh
	RaftMesh->SetupAttachment(RootBox);
	RaftMesh->SetCollisionProfileName("NoCollision");

	// create the buoyancy component
	Buoyancy = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("Buoyancy"));
	check(Buoyancy);

	// create the water detection component
	WaterDetection = CreateDefaultSubobject<UTitanWaterDetectionComponent>(TEXT("Water Detection"));
	check(WaterDetection);

	// set the water detection comp as a tick requisite.
	// this way we guarantee we have updated info prior to deciding how to move.
	AddTickPrerequisiteComponent(WaterDetection);
}

void ATitanRaft::RegisterActorTickFunctions(bool bRegister)
{
	Super::RegisterActorTickFunctions(bRegister);

	// register the post-physics tick
	if (bRegister && !IsTemplate())
	{
		ULevel* OwnerLevel = GetLevel();

		// Start disabled. Only enable when there's a pilot
		PostPhysicsTickFunction.SetTickFunctionEnable(false);
		PostPhysicsTickFunction.RegisterTickFunction(OwnerLevel);
		PostPhysicsTickFunction.AddPrerequisite(this, this->PrimaryActorTick);
	}
	else
	{
		// unregister the post physics tick
		if (PostPhysicsTickFunction.IsTickFunctionRegistered())
		{
			PostPhysicsTickFunction.UnRegisterTickFunction();
		}
	}
}

void ATitanRaft::BeginPlay()
{
	Super::BeginPlay();

	// register the physics async callback
	FPhysScene_Chaos& PhysicsScene = *GetWorld()->GetPhysicsScene();

	check(AsyncCallback == nullptr);
	AsyncCallback = PhysicsScene.GetSolver()->CreateAndRegisterSimCallbackObject_External<FTitanRaftAsyncCallback>();

	if (AsyncCallback)
	{
		// initialize the callback
		FBodyInstance* BodyInstance = RootBox->GetBodyInstance();

		if (BodyInstance)
		{
			AsyncCallback->InitializeCallback(GetWorld(), this, BodyInstance->ActorHandle);
		}

		// register the PreTick delegate
		PhysicsScene.OnPhysScenePreTick.AddUObject(this, &ATitanRaft::AsyncPhysicsGenerateInput);
	}
}

void ATitanRaft::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// unregister the physics async callback
	if (AsyncCallback)
	{
		FPhysScene_Chaos& PhysicsScene = *GetWorld()->GetPhysicsScene();

		PhysicsScene.OnPhysScenePreTick.Remove(OnPhysScenePreTickHandle);

		PhysicsScene.GetSolver()->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
		AsyncCallback = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ATitanRaft::PostPhysicsTick(float DeltaTime)
{
	if (Pilot)
	{
		Pilot->RaftPostPhysicsTick(DeltaTime, this);

		// check if we have a buoyancy component
		if (WaterDetection->IsOverlappingWater())
		{
			FVector WaterPoint = WaterDetection->GetWaterSurfaceAtLocation(GetActorLocation());

			Pilot->SetWaterPlaneHeight(WaterPoint.Z + WaterPlaneOffset, true);

		}
		else {

			Pilot->SetWaterPlaneHeight(0.0f, false);
		}

		// get the camera view from the pilot's control rotation
		CachedCameraView = Pilot->GetPilotControlRotation();
		CachedCameraView.Roll = CachedCameraView.Pitch = 0.0f;

		// check if we should turn the camera to face towards a turn
		if (FMath::Abs(CachedSteering) > MinSteeringInputForCameraTurn)
		{
			const FVector CurrentFacing = GetActorForwardVector().GetSafeNormal2D();

			const float FacingDot = CurrentFacing.Dot(CachedCameraView.RotateVector(FVector::ForwardVector));

			if (FacingDot < MaxFacingDotForCameraTurn)
			{
				Pilot->AlignCameraToVector(CurrentFacing, DeltaTime, CameraTurnSpeed);
			}
		}
	}
}

FVector ATitanRaft::GetDismountMomentum() const
{
	return RootBox->GetPhysicsLinearVelocity();
}

void ATitanRaft::InitializeRaft(TScriptInterface<ITitanRaftPilotInterface> PilotPawn)
{
	// ensure the pilot is valid
	check(PilotPawn);

	Pilot = PilotPawn;

	// Allow the pilot to initialize the raft
	Pilot->InitializeRaft(this);

	// get the camera view from the pilot's control rotation
	CachedCameraView = Pilot->GetPilotControlRotation();
	CachedCameraView.Roll = CachedCameraView.Pitch = 0.0f;

	// enable the tick function
	PostPhysicsTickFunction.SetTickFunctionEnable(true);

	// call the BP hook
	OnRaftInitialized();
}

void ATitanRaft::OnMoveInput(FVector2D Value)
{
	// ignore if the pilot is not valid
	if (!IsValid(Pilot.GetObjectRef()))
	{
		CachedCameraView = FRotator::ZeroRotator;
		CachedSteering = CachedThrottle = 0.0f;

		return;
	}

	// cache the steering and throttle value from the axis
	CachedSteering = Value.X;
	CachedThrottle = Value.Y;

}

void ATitanRaft::OnJumpInput(bool bPressed)
{
	// cache the key state
	bJumpPressed = bPressed;
}

void ATitanRaft::SetInitialMovementDisabledState(bool bNewDisabledState)
{
	// check if the pawn started out with movement disabled
	bMovementDisabledState = bNewDisabledState;

	if (bMovementDisabledState)
	{

		HandleDisabledMovement();

	}
	else {

		// enable raft physics
		RootBox->SetSimulatePhysics(true);

		// initialize the raft's linear velocity
		RootBox->SetPhysicsLinearVelocity(Pilot->GetPilotVelocity());
	}
}

void ATitanRaft::OnMovementDisabledStateChanged(bool bNewDisabledState)
{
	bMovementDisabledState = bNewDisabledState;

	HandleDisabledMovement();
}

void ATitanRaft::HandleDisabledMovement()
{
	if (bMovementDisabledState)
	{
		// disable raft physics
		RootBox->SetSimulatePhysics(false);
	}
	else {
		// enable raft physics
		RootBox->SetSimulatePhysics(true);
	}
}

void ATitanRaft::DespawnRaft(bool bDismount)
{
	Pilot->DeInitRaft(this, bDismount);

	// disable physics on the raft
	RootBox->SetSimulatePhysics(false);

	// disable collision on the raft
	SetActorEnableCollision(false);

	// disable the tick function
	PostPhysicsTickFunction.SetTickFunctionEnable(false);

	// release the pilot pointer
	Pilot = nullptr;

	// pass control to Blueprint to handle the destruction animation
	BP_DestroyRaft();
}

FVector ATitanRaft::GetPilotLocation()
{
	return GetActorLocation() + PilotOffset;
}

FRotator ATitanRaft::GetPilotRotation()
{
	FRotator FlatRotation = GetActorRotation();
	FlatRotation.Pitch = FlatRotation.Roll = 0.0f;

	return FlatRotation;
}

FVector ATitanRaft::GetPilotVelocity()
{
	FVector SocketLocation = RaftMesh->GetSocketLocation(PilotAttachmentSocket);
	return RootBox->GetPhysicsLinearVelocityAtPoint(SocketLocation);
}

void ATitanRaft::AsyncPhysicsGenerateInput(FPhysScene_Chaos* PhysScene, float DeltaTime)
{
	// get the async input from the callback
	FTitanRaftAsyncInput* CurAsyncInput = AsyncCallback->GetProducerInputData_External();

	// fill the async input
	if (CurAsyncInput)
	{
		const APhysicsVolume* CurPhysVolume = GetPhysicsVolume();

		CurAsyncInput->Inputs.bJumpPressed = bJumpPressed;
		CurAsyncInput->Inputs.CameraRotation = CachedCameraView;
		CurAsyncInput->Inputs.Steering = CachedSteering;
		CurAsyncInput->Inputs.Throttle = CachedThrottle;
		CurAsyncInput->Inputs.Gravity = CurPhysVolume ? CurPhysVolume->GetGravityZ() : -980.0f;
		CurAsyncInput->Inputs.bIsOnGround = WaterDetection->IsOnGround();
		CurAsyncInput->Inputs.GroundNormal = WaterDetection->GetGroundNormal();
		CurAsyncInput->Inputs.bIsOnWater = WaterDetection->IsOverlappingWater();
		CurAsyncInput->Inputs.Wind = WindAcceleration;
		CurAsyncInput->Inputs.bIsAboveWater = WaterDetection->IsAboveWater();
		CurAsyncInput->Inputs.bIsSubmerged = WaterDetection->IsSubmerged();

	}
	
}

FTransform ATitanRaft::GetLeftHandTransform_Implementation() const
{
	return FTransform::Identity;
}

FTransform ATitanRaft::GetRightHandTransform_Implementation() const
{
	return FTransform::Identity;
}

FTransform ATitanRaft::GetLeftFootTransform_Implementation() const
{
	return FTransform::Identity;
}

FTransform ATitanRaft::GetRightFootTransform_Implementation() const
{
	return FTransform::Identity;
}

FTransform ATitanRaft::GetPelvisTransform_Implementation() const
{
	return FTransform::Identity;
}

FVector ATitanRaft::GetMoveIntent() const
{
	// build the movement intent vector from the inputs
	return CachedCameraView.RotateVector(FVector(CachedThrottle, CachedSteering, 0.0f)).GetClampedToMaxSize(1.0f);
}

void ATitanRaft::AsyncPhysicsSimulate(const FTitanRaftAsyncInput* Input, FTitanRaftAsyncOutput& Output, Chaos::FRigidBodyHandle_Internal* PhysicsHandle, const float DeltaSeconds, const float TotalSeconds)
{
	// build the simulation state struct
	FTitanRaftAsyncSimulationState SimState;

	SimState.PhysicsHandle = PhysicsHandle;
	SimState.Inputs = Input->Inputs;
	SimState.SimTime = TotalSeconds;
	SimState.DeltaTime = DeltaSeconds;
	SimState.Mass = PhysicsHandle->M();
	SimState.Inertia = Chaos::Utilities::ComputeWorldSpaceInertia(PhysicsHandle->R() * PhysicsHandle->RotationOfMass(), PhysicsHandle->I());
	SimState.LinearVelocity = PhysicsHandle->V();
	SimState.AngularVelocity = FMath::RadiansToDegrees(PhysicsHandle->W());
	SimState.Forward = PhysicsHandle->R().RotateVector(FVector::ForwardVector);
	SimState.Up = PhysicsHandle->R().RotateVector(FVector::UpVector);
	SimState.Right = PhysicsHandle->R().RotateVector(FVector::RightVector);

	// apply simulation forces
	AsyncApplyDrag(SimState);
	AsyncKeepUpright(SimState);
	AsyncApplySteering(SimState);
	AsyncApplyWind(SimState);
	AsyncApplyJump(SimState);

	// set the output to valid
	Output.bValid = true;
}

void ATitanRaft::AsyncApplyDrag(FTitanRaftAsyncSimulationState& State)
{
	// ignore if we don't have the scaling curves
	if (!GroundDragCurve || !GroundDragSpeedMultiplierCurve)
		return;

	// default to airborne drag
	float LinearDrag = AirborneDrag;

	// are we on ground?
	if (State.Inputs.bIsOnGround)
	{
		// set the drag based on slope
		const float SlopeDot = State.Inputs.GroundNormal.Dot(FVector::UpVector);

		const float DragMultiplier = GroundDragSpeedMultiplierCurve->GetFloatValue(State.LinearVelocity.Size());

		// read the ground drag curve for the linear drag
		LinearDrag = GroundDragCurve->GetFloatValue(SlopeDot) * DragMultiplier;

	}

	// set the linear drag
	State.PhysicsHandle->SetLinearEtherDrag(LinearDrag);
}

void ATitanRaft::AsyncKeepUpright(FTitanRaftAsyncSimulationState& State)
{
	// ensure we're simulating physics
	if (CenterOfMassOffsetCurve)
	{
		// select the desired up vector depending on whether we're on ground or not
		FVector DesiredUp = State.Inputs.bIsOnGround ? State.Inputs.GroundNormal : FVector::UpVector;

		// get the upright factor by doing a dot product with the world's up direction.
		// this will give us a map where 1 = perfectly upright, 0 = 90 degrees and <0 = capsized
		float UprightDot = FVector::DotProduct(State.Up, DesiredUp);

		// set the center of mass based on our offset curve
		float MassOffset = CenterOfMassOffsetCurve->GetFloatValue(UprightDot);
		State.PhysicsHandle->SetCenterOfMass(FVector(0.0f, 0.0f, MassOffset), false);

		// build the goal rotation from the raft's forward and desired up vector
		const FQuat Goal = FRotationMatrix::MakeFromXZ(State.Forward, DesiredUp).ToQuat();

		// calculate the target torque to orient
		const FVector TargetTorque = CalculateAlignmentTorque(State.PhysicsHandle->R(), Goal, State.PhysicsHandle->W(), KeepUprightConvergenceTime, State.DeltaTime);

		// to correct, we subtract current angular velocity from the target torque
		const FVector CorrectionTorque = TargetTorque - State.PhysicsHandle->W();

		// apply the torque
		State.PhysicsHandle->AddTorque(State.Inertia * CorrectionTorque, false);
	}

}

void ATitanRaft::AsyncApplySteering(FTitanRaftAsyncSimulationState& State)
{
	// build the movement intent vector from the inputs
	const FVector MoveIntent = State.Inputs.CameraRotation.RotateVector(FVector(State.Inputs.Throttle, State.Inputs.Steering, 0.0f)).GetClampedToMaxSize(1.0f);

	// apply the steering rotation
	AsyncApplySteeringRotation(State, MoveIntent);

	// apply steering forces depending on whether we're grounded, on water or in the air
	if (State.Inputs.bIsOnGround && !State.Inputs.bIsOnWater)
	{
		AsyncApplyGroundSteering(State, MoveIntent);

	}
	else if(State.Inputs.bIsOnWater)
	{
		AsyncApplyWaterSteering(State, MoveIntent);
	}
	else
	{
		AsyncApplyAirSteering(State, MoveIntent);
	}

}

void ATitanRaft::AsyncApplyWind(FTitanRaftAsyncSimulationState& State)
{
	// apply wind acceleration, if any
	if (!State.Inputs.Wind.IsNearlyZero())
	{
		State.PhysicsHandle->AddForce(WindAcceleration * WindForcePercentage * State.Mass, false);
	}

}

void ATitanRaft::AsyncApplyJump(FTitanRaftAsyncSimulationState& State)
{
	// is this the first frame we detect a jump pressed input indicating the player wants to jump?
	const bool bWantsToJump = State.Inputs.bJumpPressed && ! bLastJumpPressed;

	// are we holding jump while still within the press and hold window? 
	const bool bCanHoldJump = ((State.SimTime - LastJumpTime) < MaxJumpHoldTime) && State.Inputs.bJumpPressed;

	// check if we should jump
	if (bWantsToJump || bCanHoldJump)
	{
		// check if we meet the conditions for a jump
		const bool bCanJump = State.Inputs.bIsOnGround							// are we grounded?					
				|| (State.Inputs.bIsOnWater && !State.Inputs.bIsSubmerged);		// are we on water, but not submerged?

		if (bCanJump && !bCanHoldJump)
		{
			// reset the vertical impulse so we can start the jump acceleration from a neutral value
			const FVector ResetImpulse = State.PhysicsHandle->LinearImpulse() + (FVector::UpVector * JumpImpulse * State.Mass);
			State.PhysicsHandle->SetLinearImpulse(ResetImpulse, false, false);

			// save the last jump time
			LastJumpTime = State.SimTime;
		}

		if (bCanHoldJump)
		{
			// apply the jump force
			State.PhysicsHandle->AddForce(FVector::UpVector * JumpHoldAcceleration * State.Mass);
		}
	}

	// save the last state of the jump pressed button so we can compare in the next jump step
	bLastJumpPressed = State.Inputs.bJumpPressed;

}

void ATitanRaft::AsyncApplySteeringRotation(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent)
{
	// calculate the dot between the move intent and the forward vector
	const float TurnDot = MoveIntent.IsNearlyZero() ? 0.0f : State.Right.Dot(MoveIntent);
	const float TurnDotIntent = State.Forward.Dot(MoveIntent);

	// check the turn intent: side = 90 degrees of camera, no side = forward
	const float TurnAngle = TurnDotIntent < 0.0f ? 0.0f : FMath::GetMappedRangeValueClamped(FVector2D(-1.0f, 1.0f), FVector2D(-90.0f, 90.0f), TurnDot);

	// calculate the turn torque
	const FVector DesiredDir = State.Inputs.CameraRotation.RotateVector(FRotator(0.0f, TurnAngle, 0.0f).RotateVector(FVector::ForwardVector)).GetSafeNormal2D();

	// build the goal orientation quat from the desired direction and our up vector
	const FQuat Goal = FRotationMatrix::MakeFromXZ(DesiredDir, State.Up).ToQuat();

	// calculate the target torque to orient
	const FVector TargetTorque = CalculateAlignmentTorque(State.PhysicsHandle->R(), Goal, State.PhysicsHandle->W(), SteeringConvergenceTime, State.DeltaTime);

	// to correct, we subtract current angular velocity from the target torque
	const FVector CorrectionTorque = TargetTorque - State.PhysicsHandle->W();

	// apply the steering torque. Ignore mass.
	State.PhysicsHandle->AddTorque(State.Inertia * CorrectionTorque, false);
}

void ATitanRaft::AsyncApplyGroundSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent)
{
	// ignore if we don't have the scaling curves
	if (!GroundSpeedCurve
		|| !GroundSpeedMultiplierCurve
		|| !GroundAccelerationCurve
		|| !GroundAccelerationMultiplierCurve
		|| !GroundGravityCurve
		|| !GroundGravityMultiplierCurve
		)
		return;

	// get the current movement plane speed
	const float Speed = State.LinearVelocity.Size2D();

	const FVector FlatVelocityNormalized = State.LinearVelocity.GetSafeNormal2D();

	// calculate the slope dot product
	const float SlopeDot = State.Inputs.GroundNormal.Dot(FVector::UpVector);

	// check our facing relative to the slope. This will be <0 if we're facing the slope, 0 if flat
	const float ForwardDotSlope = State.Forward.Dot(State.Inputs.GroundNormal);

	// get our gravity acceleration multiplier based on our slope facing dot
	const float GravityAccelMultiplier = GroundGravityMultiplierCurve->GetFloatValue(ForwardDotSlope);

	// get the gravity acceleration magnitude from the curve. This will depend both on the slope and facing
	const float GravityAccel = GroundGravityCurve->GetFloatValue(SlopeDot) * GravityAccelMultiplier;

	// apply the gravity acceleration
	State.PhysicsHandle->AddForce(FVector::DownVector * GravityAccel * State.Mass, false);

	// get the steering acceleration multiplier based on the slope facing dot
	const float SteeringAccelMultiplier = GroundAccelerationMultiplierCurve->GetFloatValue(ForwardDotSlope);

	// get the steering acceleration magnitude based on the slope and the facing multiplier
	const float SteeringAccel = GroundAccelerationCurve->GetFloatValue(SlopeDot) * SteeringAccelMultiplier;

	// apply the steering acceleration
	const FVector Steering = FVector::VectorPlaneProject(MoveIntent, State.Inputs.GroundNormal).GetSafeNormal();

	State.PhysicsHandle->AddForce(Steering * SteeringAccel * State.Mass, false);

	// calculate the max speed dot based on the difference between the velocity and the forward vector
	const float MaxSpeedDot = MoveIntent.Dot(FlatVelocityNormalized);

	// get the max speed multiplier from the curve
	const float SlopeMaxSpeedMultiplier = MoveIntent.IsNearlyZero() ? 1.0f : GroundSpeedMultiplierCurve->GetFloatValue(MaxSpeedDot);

	const float SlopeMaxSpeed = GroundSpeedCurve->GetFloatValue(SlopeDot) * SlopeMaxSpeedMultiplier;

	// calculate and apply the max speed deceleration
	if (Speed > SlopeMaxSpeed)
	{
		// calculate the desired max velocity
		const FVector DesiredVelocity = State.LinearVelocity.GetSafeNormal() * SlopeMaxSpeed;

		// calculate the brake acceleration
		const FVector BrakeAccel = DesiredVelocity - State.LinearVelocity;

		// apply the braking force
		State.PhysicsHandle->AddForce(BrakeAccel * State.Mass, false);
	}
}

void ATitanRaft::AsyncApplyWaterSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent)
{
	// get the current movement plane speed
	const float Speed = State.LinearVelocity.Size2D();

	// apply regular gravity
	State.PhysicsHandle->AddForce(FVector::UpVector * State.Mass * State.Inputs.Gravity * WaterGravityMultiplier, false);

	// apply the steering acceleration
	State.PhysicsHandle->AddForce(WaterAcceleration * MoveIntent * State.Mass, false);

	// calculate and apply the max speed deceleration
	if (Speed > MaxWaterHorizontalSpeed)
	{
		// calculate the desired max velocity
		const FVector DesiredVelocity = State.LinearVelocity.GetSafeNormal() * MaxWaterHorizontalSpeed;

		// calculate the brake acceleration
		const FVector BrakeAccel = DesiredVelocity - State.LinearVelocity;

		// apply the braking force
		State.PhysicsHandle->AddForce(BrakeAccel * State.Mass, false);
	}
}

void ATitanRaft::AsyncApplyAirSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent)
{
	// apply regular gravity
	State.PhysicsHandle->AddForce(FVector::UpVector * State.Mass * State.Inputs.Gravity, false);
}

FVector ATitanRaft::CalculateAlignmentTorque(const FQuat& StartingRot, const FQuat& GoalRot, const FVector& AngularVelocity, float HalfLife, float DeltaTime) const
{
	// apply spring damping interpolation to the torque
	// TODO: clean up, maybe move to a separate library that is accessible to this and camera
	const float Damp = ((4.0f * 0.69314718056f) / (HalfLife + 1e-5f)) * 0.5f;

	const float eydt = FMath::InvExpApprox(Damp * DeltaTime);

	FQuat xDiff = StartingRot * GoalRot.Inverse();
	xDiff.EnforceShortestArcWith(FQuat::Identity);

	FVector j0 = xDiff.ToRotationVector();
	FVector j1 = AngularVelocity + j0 * Damp;

	return eydt * (AngularVelocity - j1 * Damp * DeltaTime);
}

void FTitanRaftAsyncCallback::InitializeCallback(UWorld* InWorld, ATitanRaft* InRaft, Chaos::FSingleParticlePhysicsProxy* InProxy)
{
	World = InWorld;
	Raft = InRaft;
	Proxy = InProxy;
}

FName FTitanRaftAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FTitanRaftAsyncCallback");
	return StaticName;
}

void FTitanRaftAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{
	
}

void FTitanRaftAsyncCallback::OnPreSimulate_Internal()
{
	// get the delta and simulation time
	float DeltaTime = GetDeltaTime_Internal();
	float SimTime = GetSimTime_Internal();

	// validate the input
	const FTitanRaftAsyncInput* Input = GetConsumerInput_Internal();
	if (Input == nullptr)
	{
		return;
	}

	// validate the world
	if (World.Get() == nullptr)
	{
		return;
	}

	// validate the raft
	if (Raft == nullptr)
	{
		return;
	}

	// validate the physics solver
	Chaos::FPhysicsSolver* PhysicsSolver = static_cast<Chaos::FPhysicsSolver*>(GetSolver());
	if (PhysicsSolver == nullptr)
	{
		return;
	}

	// ensure the physics proxy is valid
	if (Proxy == nullptr || Proxy->GetPhysicsThreadAPI() == nullptr)
	{
		return;
	}

	// ensure the simulated object is dynamic
	Chaos::FRigidBodyHandle_Internal* Handle = Proxy->GetPhysicsThreadAPI();
	if (Handle->ObjectState() != Chaos::EObjectStateType::Dynamic)
	{
		return;
	}

	// get the output data
	FTitanRaftAsyncOutput& Output = GetProducerOutputData_Internal();

	// pass control to the raft to simulate physics
	// NOTE: keep in mind this is running on physics thread, so most write operations will be very unsafe!
	Raft->AsyncPhysicsSimulate(Input, Output, Handle, DeltaTime, SimTime);
}
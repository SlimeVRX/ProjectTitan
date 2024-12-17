// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsPublic.h"
#include "Chaos/Core.h"
#include "TitanMoverTypes.h"
#include "TitanRaftActor.generated.h"

class UBuoyancyComponent;
class UTitanWaterDetectionComponent;
class UCurveFloat;
class UPhysicalMaterial;
class UBoxComponent;
class USkeletalMeshComponent;

/**
 *  ITitanPilotInterface
 *  Provides information and initialization flow for the Raft's pilot actor (usually the player pawn)
 */
UINTERFACE(MinimalAPI)
class UTitanRaftPilotInterface : public UInterface
{
	GENERATED_BODY()
};

class TITANRAFT_API ITitanRaftPilotInterface
{
	GENERATED_BODY()

public:

	/** Initializes the pilot for the raft they're piloting */
	virtual void InitializeRaft(ATitanRaft* PilotedRaft) = 0;

	/** De-initializes the pilot and restores movement functionality */
	virtual void DeInitRaft(ATitanRaft* PilotedRaft, bool bDismount) = 0;

	/** Updates the pilot when the raft's post physics tick is called */
	virtual void RaftPostPhysicsTick(float DeltaTime, ATitanRaft* PilotedRaft) = 0;

	/** Returns the pilot's current velocity */
	virtual FVector GetPilotVelocity() const = 0;

	/** Returns the pilot's control rotation */
	virtual FRotator GetPilotControlRotation() const = 0;

	/** Sets the height of the camera clipping plane so it stays above water */
	virtual void SetWaterPlaneHeight(float Height, bool bEnable) = 0;

	/** Aligns the pilot's camera to the provided facing vector */
	virtual void AlignCameraToVector(const FVector& InFacing, float DeltaTime, float AlignSpeed) = 0;
};

/**
* Tick function that calls arbitrary function. Used here for post-physics operations.
**/
struct FTitanRaftTickFunction : public FTickFunction
{
	/**  Tick function to call **/
	TFunction<void(float DeltaTime)> TickFunction;
	FString DiagnosticMessageString;
	FString DiagnosticContextString;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override {
		if (TickFunction)
		{
			TickFunction(DeltaTime);
		}
	}
	virtual FString DiagnosticMessage() override { return DiagnosticMessageString; }
	virtual FName DiagnosticContext(bool bDetailed) override { return FName(*DiagnosticContextString); }
};

/**
 *	FTitanRaftInputs
 *	Encapsulates all raft input data consumed by the async callback.
 *	Offloaded into a separate struct so that we can declare it as mutable
 */
 USTRUCT()
struct TITANRAFT_API FTitanRaftInputs
{
	GENERATED_USTRUCT_BODY()

	FTitanRaftInputs()
	: Steering(0.0f)
	, Throttle(0.0f)
	, Gravity(0.0f)
	, CameraRotation(FRotator::ZeroRotator)
	, bIsOnGround(false)
	, GroundNormal(FVector::ZeroVector)
	, bIsOnWater(false)
	, bIsAboveWater(false)
	, bIsSubmerged(false)
	, bJumpPressed(false)
	, bWantsToJump(false)
	, Wind(FVector::ZeroVector)
	{}

	/** Left-right steering input. Range -1 to 1 */
	UPROPERTY()
	float Steering;

	/** Forward-back throttle input. Range -1 to 1 */
	UPROPERTY()
	float Throttle;

	/** Gravity acceleration */
	UPROPERTY()
	float Gravity;

	/** Camera rotation, used to calculate throttle and steering direction. Normalized */
	UPROPERTY()
	FRotator CameraRotation;

	/** If true, the raft is in contact with the floor */
	UPROPERTY()
	bool bIsOnGround;

	/** Last found ground normal. Only valid if bIsOnGround is true */
	UPROPERTY()
	FVector GroundNormal;

	/** If true, the raft is overlapping a water body */
	UPROPERTY()
	bool bIsOnWater;

	/** If true, the raft is currently above a water surface */
	UPROPERTY()
	bool bIsAboveWater;

	/** If true, the raft is currently submerged */
	UPROPERTY()
	bool bIsSubmerged;

	/** If true, the jump button is pressed */
	UPROPERTY()
	bool bJumpPressed;

	/** If true, the raft wants to jump this frame */
	UPROPERTY()
	bool bWantsToJump;

	/** Accumulated wind force to apply */
	UPROPERTY()
	FVector Wind;
};

/**
 *	FTitanRaftAsyncInput
 *	Input async physics simulation data
 *	coming out of the main thread, so it can be consumed by the physics thread
 */
struct TITANRAFT_API FTitanRaftAsyncInput : public Chaos::FSimCallbackInput
{
	/** Mutable input values that will be consumed by the async physics thread */
	FTitanRaftInputs Inputs;

	/** Constructor */
	FTitanRaftAsyncInput()
	: Inputs()
	{
	}

	/** Reset */
	void Reset()
	{
		Inputs = FTitanRaftInputs();
	}

	/** Destructor */
	virtual ~FTitanRaftAsyncInput() = default;
};

/**
 *	FTitanRaftAsyncOutput
 *	Output async physics simulation data
 *	coming out of the physics thread, so it can be consumed by the game thread
 */
struct TITANRAFT_API FTitanRaftAsyncOutput : public Chaos::FSimCallbackOutput
{
	/** True if this output contains valid simulation data */
	bool bValid;

	/** Constructor */
	FTitanRaftAsyncOutput()
		: bValid(false)
	{
	}

	/** Reset */
	void Reset()
	{
	}

	/** Destructor */
	virtual ~FTitanRaftAsyncOutput() = default;
};

/**
 *	FTitanRaftAsyncSimulationState
 *	Helper struct to hold common data to pass between simulation functions
 */
struct TITANRAFT_API FTitanRaftAsyncSimulationState
{
	/** Handle to the physics thread representation of the raft body */
	Chaos::FRigidBodyHandle_Internal* PhysicsHandle;

	/** Reference to the simulation inputs */
	FTitanRaftInputs Inputs;

	/** Total simulation time, in seconds */
	float SimTime;

	/** Delta time for this simulation step, in seconds */
	float DeltaTime;

	/** Mass of the physics body */
	float Mass;

	/** Inertia matrix */
	Chaos::FMatrix33 Inertia;

	/** Linear velocity of the physics body */
	FVector LinearVelocity;

	/** Angular Velocity of the physics body */
	FVector AngularVelocity;

	/** Forward direction of the physics body */
	FVector Forward;

	/** Up direction of the physics body */
	FVector Up;

	/** Right direction of the physics body */
	FVector Right;

	/** Constructor */
	FTitanRaftAsyncSimulationState()
	: PhysicsHandle(nullptr)
	, Inputs()
	, SimTime(0.0f)
	, DeltaTime(0.0f)
	, Mass(0.0f)
	, LinearVelocity(FVector::ZeroVector)
	, AngularVelocity(FVector::ZeroVector)
	, Forward(FVector::ZeroVector)
	, Up(FVector::ZeroVector)
	, Right(FVector::ZeroVector)
	{}

	/** Destructor */
	virtual ~FTitanRaftAsyncSimulationState() = default;
};

/**
 *	FTitanRaftAsyncCallback
 *	Physics Async Callback object used by the Raft
 *	Ensures the physics simulation takes place in the Physics thread,
 *	allowing for a more stable physics simulation regardless of game thread framerate
 */
class TITANRAFT_API FTitanRaftAsyncCallback : public Chaos::TSimCallbackObject<FTitanRaftAsyncInput, FTitanRaftAsyncOutput, Chaos::ESimCallbackOptions::Presimulate>
{
private:

	/** Pointer to the world where the simulation is happening */
	TWeakObjectPtr<UWorld> World;

	/** Pointer to the Raft */
	ATitanRaft* Raft;

	/** Pointer to the physics thread proxy */
	Chaos::FSingleParticlePhysicsProxy* Proxy;

public:

	/** Initializes callback data */
	void InitializeCallback(UWorld* InWorld, ATitanRaft* InRaft, Chaos::FSingleParticlePhysicsProxy* InProxy);

public:
	/** Object name for engine stats gathering */
	virtual FName GetFNameForStatId() const override;
private:
	/** Processes simulation inputs */
	virtual void ProcessInputs_Internal(int32 PhysicsStep) override;

	/** Handles input and other forces on physics thread */
	virtual void OnPreSimulate_Internal() override;
};

/**
 *  ATitanRaft
 *  A Physics + Buoyancy-driven raft vehicle that can be indirectly controlled by the player.
 *  Unlike a standard vehicle, the raft is not a Pawn and is not possessed by the PlayerController.
 *  Instead it uses Pawn delegates to subscribe to exposed Pawn inputs.
 */
UCLASS(abstract, notplaceable)
class TITANRAFT_API ATitanRaft : public AActor
{
	GENERATED_BODY()
	
public:	
	
	/** Constructor */
	ATitanRaft();

protected:

	/** Physics driven sphere component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> RootBox;

	/** Skeletal mesh skin. It will only be used for visual representation, and will neither simulate nor collide */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> RaftMesh;

		/** Water Buoyancy component that will drive the raft */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBuoyancyComponent> Buoyancy;

public:

	/** Water/Ground detection component to help keep tabs on raft movement capabilities */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTitanWaterDetectionComponent> WaterDetection;

public:

	/** Registers the post-physics tick function */
	virtual void RegisterActorTickFunctions(bool bRegister) override;

	/** Register async callbacks on BeginPlay */
	virtual void BeginPlay() override;

	/** Deregister async callbacks and clear timers on EndPlay */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Raft post-physics update */
	virtual void PostPhysicsTick(float DeltaTime);

protected:

	/** Pointer to the pilot of the raft */
	UPROPERTY(BlueprintReadOnly, Category="Titan Raft|Pilot")
	TScriptInterface<ITitanRaftPilotInterface> Pilot;

	/** Name of the socket to attach the player to on the raft mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Pilot")
	FName PilotAttachmentSocket;

	/** World space offset of the pilot's attachment point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Pilot")
	FVector PilotOffset;

	/** Name of the Movement Mode the player should transition to when riding the raft */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Pilot")
	FName PilotMovementMode;

	/** Name of the Movement Mode the player should transition to when dismounting the raft */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Pilot")
	FName DismountMovementMode;

	/** Jump impulse to apply to the pilot when dismounting the raft */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Pilot", meta=(ClampMin=0))
	float DismountImpulse = 200;

	/** Forward acceleration curve to apply based on raft tilt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Acceleration")
	TObjectPtr<UCurveFloat> GroundAccelerationCurve;

	/** Multiplier to apply to the raft acceleration based on player input  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Acceleration")
	TObjectPtr<UCurveFloat> GroundAccelerationMultiplierCurve;

	/** Time for the raft to converge to its steering orientation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Keep Upright", meta=(ClampMin=0, ClampMax=10))
	float SteeringConvergenceTime = 0.5f;

	/** Wind acceleration to apply to the raft */
	FVector WindAcceleration = FVector::ZeroVector;

	/** Percentage of the wind volume force to apply to the raft */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Acceleration", meta=(ClampMin=0, ClampMax=1))
	float WindForcePercentage = 0.5f;

	/** Curve to control the downwards gravity force depending on slope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundGravityCurve;

	/** Curve to control the downwards gravity force depending on character facing relative to slope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundGravityMultiplierCurve;

	/** Curve to control the max movement speed based on ground slope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundSpeedCurve;

	/** Curve to control the max movement speed based on movement intent and relative facing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundSpeedMultiplierCurve;

	/** Curve to control linear drag based on slope angle. Allows us to slide down faster on slopes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundDragCurve;

	/** Curve to control linear drag based on velocity. Allows us to more easily maintain momentum on flat surfaces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Ground Movement")
	TObjectPtr<UCurveFloat> GroundDragSpeedMultiplierCurve;

	/** Downtime between successful jumps to prevent underwater jumping */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Air Movement", meta=(ClampMin=0))
	float MaxJumpHoldTime = 0.5f;

	/** Initial jump impulse to apply as soon as the jump button is pressed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Air Movement", meta=(ClampMin=0))
	float JumpImpulse = 500.0f;

	/** Extra jump acceleration to apply while the jump button is down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Air Movement", meta=(ClampMin=0))
	float JumpHoldAcceleration = 2500.0f;

	/** Linear drag to apply while airborne */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Air Movement", meta=(ClampMin=0))
	float AirborneDrag = 0.01f;

	/** Set to true when the jump button is pressed */
	bool bJumpPressed = false;

	/** Cached jump pressed from the last time we jumped */
	mutable bool bLastJumpPressed = false;

	/** Cached last jump time to determine maximum allowed press and hold input */
	mutable float LastJumpTime = 0.0f;

	/** Forward acceleration to apply to the raft in response to the forward input while on water. Independent of mass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Water Movement", meta=(ClampMin=0))
	float WaterAcceleration = 800.0f;

	/** Max speed the raft is allowed on water before we start applying a breaking force */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Water Movement", meta=(ClampMin=0))
	float MaxWaterHorizontalSpeed = 400.0f;

	/** Multiplies the base gravity acceleration while we're on water */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Water Movement", meta=(ClampMin=0, ClampMax=1))
	float WaterGravityMultiplier = 0.5f;

	/** Min height above the water plane the camera should clip to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Water Movement", meta = (ClampMin = 0))
	float WaterPlaneOffset = 30.0f;

	/** Time for the raft to converge to its upright orientation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Keep Upright", meta=(ClampMin=0, ClampMax=10))
	float KeepUprightConvergenceTime = 0.5f;

	/** Curve to offset the center of mass of the raft based on the angle, helping keep it upright */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Keep Upright")
	TObjectPtr<UCurveFloat> CenterOfMassOffsetCurve;

	/** If true, we should disable movement handling */
	bool bMovementDisabledState = false;

	/** Minimum steering input magnitude needed to trigger a camera turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Camera Turn", meta=(ClampMin=0, ClampMax=1))
	float MinSteeringInputForCameraTurn = 0.5f;

	/** Maximum allowed dot product of the angle difference between camera and raft facing to trigger a camera turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Camera Turn", meta=(ClampMin=0, ClampMax=1))
	float MaxFacingDotForCameraTurn = 0.5f;

	/** Speed at which the camera should turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Titan Raft|Camera Turn", meta=(ClampMin=0, ClampMax=100))
	float CameraTurnSpeed = 10.0f;

public:

	/** Returns the dismount movement mode */
	FName GetDismountMovementMode() const { return DismountMovementMode; };

	/** Returns the dismount vertical impulse */
	float GetDismountImpulse() const { return DismountImpulse; };

	/** Returns the dismount carried momentum */
	FVector GetDismountMomentum() const;

public:

	/** Initializes the raft for the passed pilot */
	UFUNCTION(Blueprintcallable, Category="Titan Raft")
	void InitializeRaft(TScriptInterface<ITitanRaftPilotInterface> PilotPawn);

protected:

	/** BP hook called after the raft is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category="Titan Raft")
	void OnRaftInitialized();

public:

	/** Called when the pilot presses a forward input. Applies the forward acceleration. */
	UFUNCTION()
	void OnMoveInput(FVector2D Value);

	/** Called when the pilot presses a jump input. Causes the player to jump off the raft and starts the destruction process. */
	UFUNCTION()
	void OnJumpInput(bool bPressed);

	/** Allows the pilot to set the initial movement disabled state */
	void SetInitialMovementDisabledState(bool bNewDisabledState);

	/** Handles movement disabled changes to the pilot */
	UFUNCTION()
	void OnMovementDisabledStateChanged(bool bNewDisabledState);

protected:

	/** Sets the raft's physics state based on movement disabled state */
	void HandleDisabledMovement();

public:

	/** Starts the raft destruction process. Does not actually call DestroyActor(); this is expected to be done from Blueprint in OnDestroyRaft */
	UFUNCTION(BlueprintCallable, Category="Titan Raft")
	void DespawnRaft(bool bDismount);

	/** Returns the location of the pilot's attachment socket */
	FVector GetPilotLocation();

	/** Returns the rotation of the pilot's attachment socket */
	FRotator GetPilotRotation();

	/** Returns the name of the pilot's attachment socket */
	const FName& GetPilotSocket() { return PilotAttachmentSocket; }

	/** Returns the linear velocity at the pilot's attachment socket */
	FVector GetPilotVelocity();

	/** Adds a wind acceleration vector to the raft */
	UFUNCTION(BlueprintCallable, Category="Titan Raft")
	void AddWind(const FVector Wind) { WindAcceleration += Wind; }

protected:
	/** Passes control to Blueprint to handle destroying the raft after the player has gotten off */
	UFUNCTION(BlueprintImplementableEvent, Category="Titan Raft", meta=(DisplayName="Destroy Raft"))
	void BP_DestroyRaft();

public:
	FTitanRaftTickFunction PostPhysicsTickFunction;

protected:

	/** Cached throttle input */
	float CachedThrottle = 0.0f;

	/** Cached steering input */
	float CachedSteering = 0.0f;

	FRotator CachedCameraView = FRotator::ZeroRotator;

	/** Pointer to the physics simulation async callback */
	FTitanRaftAsyncCallback* AsyncCallback = nullptr;

	/** Handle to the physics simulation pre-tick delegate */
	FDelegateHandle OnPhysScenePreTickHandle;

	/** Pre-tick physics update. Builds the input data that will be consumed by the simulation */
	void AsyncPhysicsGenerateInput(FPhysScene* PhysScene, float DeltaTime);

public:

	/** Blueprint overrideable event to return the IK target Transform for the raft's left hand */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Titan|Raft")
	FTransform GetLeftHandTransform() const;

	/** Blueprint overrideable event to return the IK target Transform for the raft's right hand */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Titan|Raft")
	FTransform GetRightHandTransform() const;

	/** Blueprint overrideable event to return the IK target Transform for the raft's left foot */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Titan|Raft")
	FTransform GetLeftFootTransform() const;

	/** Blueprint overrideable event to return the IK target Transform for the raft's right foot */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Titan|Raft")
	FTransform GetRightFootTransform() const;

	/** Blueprint overrideable event to return the IK target Transform for the raft's pelvis socket */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Titan|Raft")
	FTransform GetPelvisTransform() const;

	/** Calculate the approximate movement intent vector for the raft */
	UFUNCTION(BlueprintPure, Category="Titan|Raft")
	FVector GetMoveIntent() const;

public:

	/**
	 *	Performs physics thread simulation.
	 *	NOTE: these functions run on physics thread, so writing to the Actor or world will be unsafe!
	 *	Use the Output struct if you need to pass data back to the game thread.
	 */
	void AsyncPhysicsSimulate(const FTitanRaftAsyncInput* Input, FTitanRaftAsyncOutput& Output, Chaos::FRigidBodyHandle_Internal* PhysicsHandle, const float DeltaSeconds, const float TotalSeconds);

protected:

	/** Sets the linear drag for the raft */
	void AsyncApplyDrag(FTitanRaftAsyncSimulationState& State);
	
	/** Apply a torque to keep the raft upright */
	void AsyncKeepUpright(FTitanRaftAsyncSimulationState& State);

	/** Apply forces and torque to move and orient the raft */
	void AsyncApplySteering(FTitanRaftAsyncSimulationState& State);

	/** Apply wind forces to the raft */
	void AsyncApplyWind(FTitanRaftAsyncSimulationState& State);

	/** Apply jump impulse to the raft */
	void AsyncApplyJump(FTitanRaftAsyncSimulationState& State);

	/** Apply torque to rotate the raft towards the movement intent vector */
	void AsyncApplySteeringRotation(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent);

	/** Apply forces to steer the raft while it's on the ground */
	void AsyncApplyGroundSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent);

	/** Apply forces to steer the raft while it's on water */
	void AsyncApplyWaterSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent);

	/** Apply forces to steer the raft while it's in the air */
	void AsyncApplyAirSteering(FTitanRaftAsyncSimulationState& State, const FVector& MoveIntent);

	/** Helper function to calculate the target torque needed to orient a rotation towards a goal. Used by async functions to keep the raft upright and facing its desired orientation */
	FVector CalculateAlignmentTorque(const FQuat& StartingRot, const FQuat& GoalRot, const FVector& AngularVelocity, float HalfLife, float DeltaTime) const;
};

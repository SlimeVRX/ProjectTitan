// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "MovementMode.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Engine/EngineTypes.h"
#include "TitanMoverTypes.generated.h"

class UCurveFloat;

/** Titan-specific movement mode names */
namespace TitanMovementModeNames
{
	const FName Grappling = TEXT("Grappling");
	const FName Raft = TEXT("Sailing");
	const FName Teleport = TEXT("Teleporting");
}

/** Titan-specific blackboard object keys */
namespace TitanBlackboard
{
	const FName LastRaft = TEXT("LastRaft");
	const FName LastFallTime = TEXT("LastFallTime");
	const FName LastJumpTime = TEXT("LastJumpTime");
	const FName GrappleGoal = TEXT("GrappleGoal");
	const FName GrappleNormal = TEXT("GrappleNormal");
	const FName GrappleStartTime = TEXT("GrappleStartTime");
	const FName LastGrappleTime = TEXT("LastGrappleTime");
	const FName SoftLandDistance = TEXT("SoftLandDistance");
}

// Movement tags
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Walking);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Exhausted);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Falling);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Grappling);
TITANMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Movement_Sailing);

/**
 *  FTitanTagsSyncState
 *  Extends the Mover sync state to provide gameplay tag tracking.
 */
USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanTagsSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()

public:

	FTitanTagsSyncState()
	{
	};

	virtual ~FTitanTagsSyncState() {};

public:

	/** Returns the movement tags container */
	const FGameplayTagContainer& GetMovementTags() const { return MovementTags; };

	/** Returns true if the sync state contains the exact leaf tag */
	bool HasTagExact(const FGameplayTag& Tag) const;

	/** Returns true if the sync state contains the tag as part of its hierarchy */
	bool HasTagAny(const FGameplayTag& Tag) const;

	/** Adds a tag to the sync state */
	void AddTag(const FGameplayTag& Tag);

	/** Removes a tag from the sync state */
	void RemoveTag(const FGameplayTag& Tag);

	/** Clears all tags from the sync state */
	void ClearTags();


protected:

	/** Tags container */
	FGameplayTagContainer MovementTags;

	// FStruct utility
public:

	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

/**
 *  FTitanStaminaSyncState
 *  Extends the Mover sync state to provide stamina management.
 */
USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanStaminaSyncState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

public:

	FTitanStaminaSyncState()
		: MaxStamina(100.0f)
		, Stamina(MaxStamina)
		, bIsExhausted(false)
	{
	};

	virtual ~FTitanStaminaSyncState() {};

public:

	/**
	 * Updates Stamina value and clamps to 0-Max range.
	 * Returns true if stamina was depleted. 
	 */
	void UpdateStamina(float Delta, bool& bDepleted, bool& bMaxedOut, bool bUseExhaustion = false);

	/** Const getters for reconcile */
	float GetStamina() const { return Stamina; };
	float GetMaxStamina() const { return MaxStamina; };
	bool IsExhausted() const { return bIsExhausted; };

protected:

	/** Max stamina value allowed. Stamina will be clamped between 0 and this */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Titan)
	float MaxStamina;

	/** Current stamina value. Can be consumed by movement modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Titan)
	float Stamina;

	/** If true, depleting stamina will exhaust the character until it is fully restored */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Titan)
	bool bIsExhausted;

	// FStruct utility
public:

	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits< FTitanStaminaSyncState > : public TStructOpsTypeTraitsBase2< FTitanStaminaSyncState >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

/**
 *  FTitanMovementInputs
 *  Input data block for Titan movement modes
 */
USTRUCT(BlueprintType)
struct TITANMOVEMENT_API FTitanMovementInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	FTitanMovementInputs()
	: bIsSprintJustPressed(false)
	, bIsSprintPressed(false)
	, bIsGlideJustPressed(false)
	, bIsGlidePressed(false)
	{
		Wind = FVector::ZeroVector;
	};

	virtual ~FTitanMovementInputs() {}

public:

	/** Was the Sprint input just pressed? */
	UPROPERTY(BlueprintReadWrite, Category = Titan)
	bool bIsSprintJustPressed;

	/** Is the Sprint input held down? */
	UPROPERTY(BlueprintReadWrite, Category = Titan)
	bool bIsSprintPressed;

	/** Was the Glide input just pressed? */
	UPROPERTY(BlueprintReadWrite, Category = Titan)
	bool bIsGlideJustPressed;

	/** Is the Glide input held down? */
	UPROPERTY(BlueprintReadWrite, Category = Titan)
	bool bIsGlidePressed;

	/** Wind speed vector applied while gliding */
	UPROPERTY(BlueprintReadWrite, Category = Titan)
	FVector Wind;

	/** FStruct Utility */
	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

template<>
struct TStructOpsTypeTraits< FTitanMovementInputs > : public TStructOpsTypeTraitsBase2< FTitanMovementInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

/**
 *  UTitanMovementSettings
 *  Common movement settings used by Titan movement modes
 */
UCLASS(BlueprintType)
class TITANMOVEMENT_API UTitanMovementSettings : public UObject, public IMovementSettingsInterface
{
	GENERATED_BODY()

	virtual FString GetDisplayName() const override { return GetName(); }

public:

	// Movement Modes
	///////////////////////////////

	/** Movement mode to use when grappling */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FName GrapplingMovementModeName = TitanMovementModeNames::Grappling;

	/** Movement mode to use when using the raft */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FName RaftMovementModeName = TitanMovementModeNames::Raft;

	/** Movement mode to use when waiting for a teleport */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FName TeleportMovementModeName = TitanMovementModeNames::Teleport;

	/** Gameplay Tag to use when falling and jumping */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FGameplayTag FallingTag = TAG_Titan_Movement_Falling;

	/** Gameplay Tag to use when on the ground */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FGameplayTag WalkingTag = TAG_Titan_Movement_Walking;

	/** Gameplay Tag to use when exhausted */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ExhaustedTag = TAG_Titan_Movement_Exhausted;

	/** Gameplay Tag to use when grappling */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FGameplayTag GrapplingTag = TAG_Titan_Movement_Grappling;

	/** Gameplay Tag to use when sailing on the raft */
	UPROPERTY(Category="Movement Modes", EditAnywhere, BlueprintReadWrite)
	FGameplayTag SailingTag = TAG_Titan_Movement_Sailing;

	// Stamina Settings
	///////////////////////////////
	
	/** If true, depleting stamina will induce exhaustion and prevent some actions until it is fully recovered. */
	UPROPERTY(Category="Stamina", EditAnywhere, BlueprintReadWrite)
	bool bUseExhaustion = true;

	/** General-purpose stamina regeneration rate */
	UPROPERTY(Category="Stamina", EditAnywhere, BlueprintReadWrite)
	float StaminaRegeneration = 20.0f;

	/** Gameplay event to send to the character when it becomes exhausted */
	UPROPERTY(Category="Stamina", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ExhaustionEvent;

	/** Gameplay event to send to the character when it recovers from exhaustion */
	UPROPERTY(Category="Stamina", EditAnywhere, BlueprintReadWrite)
	FGameplayTag ExhaustionRecoveryEvent;

	// Grapple Pull
	///////////////////////////////
	
	/** Time after a grapple jump that the character is prevented from landing */
	UPROPERTY(Category = "Grappling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	float GrappleBoostForcedAirModeDuration = 1.2f;

};
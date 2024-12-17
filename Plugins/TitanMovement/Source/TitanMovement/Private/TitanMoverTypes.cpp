// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanMoverTypes.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Walking, "Titan.Movement.Walking");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Exhausted, "Titan.Movement.Walking.Exhausted");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Falling, "Titan.Movement.Falling");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Grappling, "Titan.Movement.Grappling");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Movement_Sailing, "Titan.Movement.Sailing");

/////////////////////////////////
// FTitanTagsSyncState
/////////////////////////////////

bool FTitanTagsSyncState::HasTagExact(const FGameplayTag& Tag) const
{
	return MovementTags.HasTagExact(Tag);
}


bool FTitanTagsSyncState::HasTagAny(const FGameplayTag& Tag) const
{
	return MovementTags.HasTag(Tag);
}

void FTitanTagsSyncState::AddTag(const FGameplayTag& Tag)
{
	MovementTags.AddTag(Tag);
}

void FTitanTagsSyncState::RemoveTag(const FGameplayTag& Tag)
{
	MovementTags.RemoveTag(Tag);
}

void FTitanTagsSyncState::ClearTags()
{
	MovementTags.Reset();
}

FMoverDataStructBase* FTitanTagsSyncState::Clone() const
{
	FTitanTagsSyncState* CopyPtr = new FTitanTagsSyncState(*this);
	return CopyPtr;
}

bool FTitanTagsSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);

	MovementTags.NetSerialize(Ar, Map, bSuccess);

	return bSuccess;
}

UScriptStruct* FTitanTagsSyncState::GetScriptStruct() const
{
	return FTitanTagsSyncState::StaticStruct();
}

void FTitanTagsSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("Tags[%s] \n", *MovementTags.ToString());
}

bool FTitanTagsSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FTitanTagsSyncState* AuthoritySyncState = static_cast<const FTitanTagsSyncState*>(&AuthorityState);

	// reconcile if the tags don't match
	return MovementTags != AuthoritySyncState->GetMovementTags();
}

void FTitanTagsSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FTitanTagsSyncState* ToState = static_cast<const FTitanTagsSyncState*>(&From);

	// just copy the target state tags
	MovementTags = ToState->GetMovementTags();
}

/////////////////////////////////
// FTitanStaminaSyncState
/////////////////////////////////

void FTitanStaminaSyncState::UpdateStamina(float Delta, bool& bDepleted, bool& bMaxedOut, bool bUseExhaustion)
{
	// cache the old value
	float OldStamina = Stamina;

	// update to the new value and clamp
	Stamina = FMath::Clamp(Stamina + Delta, 0.0f, MaxStamina);

	// set the depleted and maxed out flags
	bDepleted = Stamina == 0.0f && OldStamina > 0.0f;
	bMaxedOut = Stamina == MaxStamina && OldStamina < MaxStamina;

	// check for exhaustion
	if (bDepleted && bUseExhaustion)
	{
		bIsExhausted = true;
	}

	// reset exhaustion if we're maxed out
	if (bMaxedOut && bIsExhausted)
	{
		bIsExhausted = false;
	}
}

FMoverDataStructBase* FTitanStaminaSyncState::Clone() const
{
	FTitanStaminaSyncState* CopyPtr = new FTitanStaminaSyncState(*this);
	return CopyPtr;
}

bool FTitanStaminaSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << Stamina;
	Ar << MaxStamina;
	Ar.SerializeBits(&bIsExhausted, 1);

	return bSuccess;
}

UScriptStruct* FTitanStaminaSyncState::GetScriptStruct() const
{
	return FTitanStaminaSyncState::StaticStruct();
}

void FTitanStaminaSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("Stamina=%.2f Max=%.2f \n", Stamina, MaxStamina);
}

bool FTitanStaminaSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	// cast the struct
	const FTitanStaminaSyncState* AuthoritySyncState = static_cast<const FTitanStaminaSyncState*>(&AuthorityState);
	const float ErrorTolerance = 0.01f;

	// check the stamina error tolerance
	bool bIsNearEnough = FMath::Abs(Stamina - AuthoritySyncState->GetStamina()) < ErrorTolerance;
	bIsNearEnough &= FMath::Abs(MaxStamina - AuthoritySyncState->GetMaxStamina()) < ErrorTolerance;
	bIsNearEnough &= bIsExhausted == AuthoritySyncState->IsExhausted();

	return !bIsNearEnough;
}

void FTitanStaminaSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// cast the structs
	const FTitanStaminaSyncState* FromState = static_cast<const FTitanStaminaSyncState*>(&From);
	const FTitanStaminaSyncState* ToState = static_cast<const FTitanStaminaSyncState*>(&To);

	// lerp the values
	MaxStamina = FMath::Lerp(FromState->GetMaxStamina(), ToState->GetMaxStamina(), Pct);
	Stamina = FMath::Lerp(FromState->GetStamina(), ToState->GetStamina(), Pct);

	// final sanity check to ensure Stamina stays within bounds
	Stamina = FMath::Clamp(Stamina, 0.0f, MaxStamina);

	// copy exhaustion from the target state
	bIsExhausted = ToState->IsExhausted();
}

/////////////////////////////////
// FTitanMovementInputs
/////////////////////////////////

FMoverDataStructBase* FTitanMovementInputs::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FTitanMovementInputs* CopyPtr = new FTitanMovementInputs(*this);
	return CopyPtr;
}

bool FTitanMovementInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	// serialize the digital input flags
	Ar.SerializeBits(&bIsSprintJustPressed, 1);
	Ar.SerializeBits(&bIsSprintPressed, 1);
	Ar.SerializeBits(&bIsGlideJustPressed, 1);
	Ar.SerializeBits(&bIsGlidePressed, 1);

	// serialize the wind vector
	Ar << Wind;

	bOutSuccess = true;
	return true;
}

void FTitanMovementInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("bIsSprintPressed: %i\tbIsSprintJustPressed: %i\n", bIsSprintPressed, bIsSprintJustPressed);
	Out.Appendf("bIsGlidePressed: %i\tbIsGlideJustPressed: %i\n", bIsGlidePressed, bIsGlideJustPressed);
}

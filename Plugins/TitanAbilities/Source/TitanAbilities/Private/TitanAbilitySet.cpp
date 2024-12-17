// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanAbilitySet.h"

#include "TitanAbilitiesLogging.h"
#include "TitanAbilitySystemComponent.h"
#include "TitanGameplayAbility.h"
#include "TitanGameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitanAbilitySet)

void FTitanAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FTitanAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FTitanAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (IsValid(Set))
	{
		GrantedAttributeSets.Add(Set);
	}
}

void FTitanAbilitySet_GrantedHandles::TakeFromAbilitySystem(UTitanAbilitySystemComponent* ASC)
{
	check(ASC);

	// don't grant or remove ability sets unless the actor has authority
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// remove all abilities from the ASC
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	// remove all gameplay effects from the ASC
	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	// remove all attribute sets from the ASC
	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		ASC->RemoveSpawnedAttribute(Set);
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

UTitanAbilitySet::UTitanAbilitySet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UTitanAbilitySet::GiveToAbilitySystem(UTitanAbilitySystemComponent* ASC, FTitanAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	check(ASC);

	// don't grant or remove ability sets unless the actor has authority
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// grant the attribute sets
	for (int32 SetIndex = 0; SetIndex < GrantedAttributeSets.Num(); ++SetIndex)
	{
		const FTitanAbilitySet_AttributeSet& SetToGrant = GrantedAttributeSets[SetIndex];

		// check the ability set is valid
		if (!IsValid(SetToGrant.AttributeSet))
		{
			UE_LOG(LogTitanAbilitySystem, Error, TEXT("GrantedAttributes[%d] on ability set [%s] is not valid"), SetIndex, *GetNameSafe(this));
			continue;
		}

		// create the attribute set and give it to the ASC
		UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC->GetOwner(), SetToGrant.AttributeSet);
		ASC->AddAttributeSetSubobject(NewSet);

		// save the handle
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}

	// grant the gameplay effects
	for (int32 EffectIndex = 0; EffectIndex < GrantedGameplayEffects.Num(); ++EffectIndex)
	{
		const FTitanAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[EffectIndex];

		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			UE_LOG(LogTitanAbilitySystem, Error, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid"), EffectIndex, *GetNameSafe(this));
			continue;
		}

		// get the CDO and grant the GE
		const UTitanGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UTitanGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = ASC->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, ASC->MakeEffectContext());

		// save the handle
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GameplayEffectHandle);
		}
	}

	// grant the gameplay abilities
	for (int32 AbilityIndex = 0; AbilityIndex < GrantedGameplayAbilities.Num(); ++AbilityIndex)
	{
		const FTitanAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[AbilityIndex];

		// ensure the ability is valid
		if (!IsValid(AbilityToGrant.Ability))
		{
			UE_LOG(LogTitanAbilitySystem, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		// get the CDO and build the ability spec
		UTitanGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<UTitanGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;

		const FGameplayAbilitySpecHandle AbilitySpecHandle = ASC->GiveAbility(AbilitySpec);

		// save the handle
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}

		// check if we should activate the ability right away
		if (AbilityToGrant.bActivateImmediately)
		{
			ASC->TryActivateAbility(AbilitySpecHandle);
		}
	}

}
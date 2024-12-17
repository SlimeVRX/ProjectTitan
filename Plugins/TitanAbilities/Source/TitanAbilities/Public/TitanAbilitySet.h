// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "TitanAbilitySet.generated.h"

class UTitanGameplayAbility;
class UTitanGameplayEffect;
class UAttributeSet;
class UTitanAbilitySystemComponent;

/**
 * FTitanAbilitySet_GameplayAbility
 * Data used by the ability set to grant a gameplay ability.
 */
USTRUCT(BlueprintType)
struct TITANABILITIES_API FTitanAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:

	// Class of the gameplay ability to grant
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UTitanGameplayAbility> Ability = nullptr;

	// Level of the ability to grant
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	// If true, the ability will be activated as soon as it's granted
	UPROPERTY(EditDefaultsOnly)
	bool bActivateImmediately = false;
};

/**
 * FTitanAbilitySet_GameplayEffect
 * Data used by the ability set to grant a gameplay effect.
 */
USTRUCT(BlueprintType)
struct TITANABILITIES_API FTitanAbilitySet_GameplayEffect
{
	GENERATED_BODY()

public:

	// Class of the gameplay effect to grant
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UTitanGameplayEffect> GameplayEffect = nullptr;

	// Level of the gameplay effect to grant
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/**
 * FTitanAbilitySet_AttributeSet
 * Data used by the ability set to grant an attribute set.
 */
USTRUCT(BlueprintType)
struct TITANABILITIES_API FTitanAbilitySet_AttributeSet
{
	GENERATED_BODY()

public:

	// Class of the gameplay effect to grant
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet = nullptr;

	// Level of the gameplay effect to grant
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/**
 * FTitanAbilitySet_GrantedHandles
 * Data used to store ASC handles granted by the ability set.
 */
USTRUCT(BlueprintType)
struct TITANABILITIES_API FTitanAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(UTitanAbilitySystemComponent* ASC);

protected:

	// Handles to the granted abilities.
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// Handles to the granted gameplay effects.
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	// Pointers to the granted attribute sets
	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 *  UTitanAbilitySet
 *  
 *  Non-mutable data asset used to grant gameplay abilities, effects and attributes to an ASC
 */
UCLASS(BlueprintType, Const)
class TITANABILITIES_API UTitanAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UTitanAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/*
	 *  Grants the ability set to the specified ability system component.
	 *  The returned handles can be used later to take away anything that was granted.
	 */
	void GiveToAbilitySystem(UTitanAbilitySystemComponent* ASC, FTitanAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

protected:

	// Gameplay Abilities to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category="Gameplay Abilities", meta=(TitleProperty=Ability))
	TArray<FTitanAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	// Gameplay Effects to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category="Gameplay Abilities", meta=(TitleProperty=GameplayEffect))
	TArray<FTitanAbilitySet_GameplayEffect> GrantedGameplayEffects;

	// Attribute Sets to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category="Gameplay Abilities", meta=(TitleProperty=AttributeSet))
	TArray<FTitanAbilitySet_AttributeSet> GrantedAttributeSets;
};

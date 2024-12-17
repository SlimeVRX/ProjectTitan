// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "TitanGameplayAbility.generated.h"

// input event tags
TITANABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Input_Pressed);
TITANABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Input_Ongoing);
TITANABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Titan_Input_Released);

/**
 *  UTitanGameplayAbility
 *  Extends GameplayAbility with extra functionality:
 *  - Input event handling
 */
UCLASS(abstract)
class TITANABILITIES_API UTitanGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

	UTitanGameplayAbility(const FObjectInitializer& ObjectInitializer);

protected:

	/** Input event tag to associate with this ability */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	FGameplayTag InputEventTag;

	/** Flags to keep track of whether we have BP implementations of the input events */
	bool bHasBlueprintEventPressed : 1;
	bool bHasBlueprintEventOngoing : 1;
	bool bHasBlueprintEventReleased : 1;

	/** ActivateAbility override to handle input events */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Delegate that handles input event */
	void OnInputEventReceived(const FGameplayEventData* Payload);

	/** Called when the matching input event was pressed */
	UFUNCTION(BlueprintImplementableEvent, DisplayName="Input Event Pressed", Category = "Titan Ability")
	void K2_InputEventPressed(const FGameplayEventData& TriggerEventData);

	/** Called while the matching input event is ongoing */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Input Event Ongoing", Category="Titan Ability")
	void K2_InputEventOngoing(const FGameplayEventData& TriggerEventData);

	/** Called when the matching input event was released */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Input Event Released", Category="Titan Ability")
	void K2_InputEventReleased(const FGameplayEventData& TriggerEventData);
};

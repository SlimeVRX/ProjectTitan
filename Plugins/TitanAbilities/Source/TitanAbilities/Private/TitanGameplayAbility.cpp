// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanGameplayAbility.h"
#include "TitanAbilitiesLogging.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"

// event tag definitions
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Input_Pressed, "Titan.Input.Pressed");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Input_Ongoing, "Titan.Input.Ongoing");
UE_DEFINE_GAMEPLAY_TAG(TAG_Titan_Input_Released, "Titan.Input.Released");

UTitanGameplayAbility::UTitanGameplayAbility(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// check for blueprint implementations of the input events
	auto ImplementedInBlueprint = [](const UFunction* Func) -> bool
		{
			return Func && ensure(Func->GetOuter())
				&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
		};

	{
		static FName FuncName = FName(TEXT("K2_InputEventPressed"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintEventPressed = ImplementedInBlueprint(CanActivateFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_InputEventOngoing"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintEventOngoing = ImplementedInBlueprint(CanActivateFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_InputEventReleased"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintEventReleased = ImplementedInBlueprint(CanActivateFunction);
	}
}

void UTitanGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (InputEventTag != FGameplayTag::EmptyTag)
	{
		ActorInfo->AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(InputEventTag).AddUObject(this, &UTitanGameplayAbility::OnInputEventReceived);
	}

	// handle activation normally
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTitanGameplayAbility::OnInputEventReceived(const FGameplayEventData* Payload)
{
	// make sure we have an event payload
	if (Payload)
	{
		// do we have an input pressed tag and an event?
		if (Payload->InstigatorTags.HasTag(TAG_Titan_Input_Pressed) && bHasBlueprintEventPressed)
		{
			K2_InputEventPressed(*Payload);
			return;
		}
		// do we have an input ongoing tag and an event?
		else if (Payload->InstigatorTags.HasTag(TAG_Titan_Input_Ongoing) && bHasBlueprintEventOngoing)
		{
			K2_InputEventOngoing(*Payload);
			return;
		}
		// do we have an input released tag and an event?
		else if (Payload->InstigatorTags.HasTag(TAG_Titan_Input_Released) && bHasBlueprintEventReleased)
		{
			K2_InputEventReleased(*Payload);
			return;
		}
	}
}
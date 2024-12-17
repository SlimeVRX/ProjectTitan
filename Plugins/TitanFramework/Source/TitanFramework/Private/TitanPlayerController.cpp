// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanPlayerController.h"

#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

ATitanPlayerController::ATitanPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ATitanPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only deal with input if we're the local player
	if (HasAuthority())
	{
		// get the input subsystem
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// add the default input mapping contexts
			for (TObjectPtr<UInputMappingContext>& CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}
}

void ATitanPlayerController::SetMenuInputsEnabled(bool bEnabled)
{
	// only deal with input if we're the local player
	if (HasAuthority())
	{
		// get the input subsystem
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// should we add the mappings?
			if (bEnabled && !bMenuMappingsActive)
			{
				// add the default input mapping contexts
				for (TObjectPtr<UInputMappingContext>& CurrentContext : MenuMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			
				// set the flag
				bMenuMappingsActive = true;
			}
			// should we remove the mappings?
			else if (!bEnabled && bMenuMappingsActive)
			{
				// add the default input mapping contexts
				for (TObjectPtr<UInputMappingContext>& CurrentContext : MenuMappingContexts)
				{
					Subsystem->RemoveMappingContext(CurrentContext);
				}

				// set the flag
				bMenuMappingsActive = false;
			}
		}
	}
}

float ATitanPlayerController::GetTimeOfDayInHours_Implementation()
{
	return 0.0f;
}

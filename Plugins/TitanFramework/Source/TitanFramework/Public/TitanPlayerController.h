// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TitanPlayerController.generated.h"

class UInputMappingContext;

/**
 *  ATitanPlayerController
 *  Provides simplified Controller-level input mappings
 */
UCLASS()
class TITANFRAMEWORK_API ATitanPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ATitanPlayerController(const FObjectInitializer& ObjectInitializer);

protected:

	/** BeginPlay initialization */
	virtual void BeginPlay() override;

protected:

	/** Default Input Mapping Contexts to add to the player on initialization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TArray< TObjectPtr<UInputMappingContext> > DefaultMappingContexts;

	/** Input Mapping Contexts to add to the player when menus are open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TArray< TObjectPtr<UInputMappingContext> > MenuMappingContexts;

	/** If true, menu input mappings are active */
	bool bMenuMappingsActive = false;

public:

	/** Enables or disables menu input mappings */
	UFUNCTION(BlueprintCallable, Category="Titan")
	void SetMenuInputsEnabled(bool bEnabled);

	/** Stub to pass control to the batch landmark photo feature */
	UFUNCTION(BlueprintImplementableEvent, Category="Titan")
	void BatchLandmarkPhotos();

	/** Stub to pass control to the time of day setter */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Titan")
	void SetTimeOfDayInHours(float Hours);

	/** Stub to get the time of day from BP */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Titan")
	float GetTimeOfDayInHours();

	/** Stub to allow BP to update user preferences when they've been changed */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Titan")
	void UpdateUserPreferences();
};

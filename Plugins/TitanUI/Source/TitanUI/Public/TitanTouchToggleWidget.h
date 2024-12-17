// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TitanSimulatedInputWidget.h"
#include "TitanTouchToggleWidget.generated.h"

class UObject;
struct FFrame;
struct FGeometry;
struct FPointerEvent;

/**
 *  UTitanTouchToggleWidget
 *  Defines an area of the screen that should trigger an input when the user touches it.
 *  Touching the region toggles input injection on and off.
 *  The input is injected every Tick as long as the widget is on.
 */
UCLASS(meta=(DisplayName="Titan Touch Toggle Widget"))
class TITANUI_API UTitanTouchToggleWidget : public UTitanSimulatedInputWidget
{
	GENERATED_BODY()

public:
	
	//~ Begin UUserWidget
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface

	/** Returns whether the widget should simulate the input */
	UFUNCTION(BlueprintCallable)
	bool ShouldSimulateInput() const { return bShouldSimulateInput; }

	/** Implements the reset functionality */
	virtual void ResetControl() override;

protected:

	/** True while this widget is being touched */
	bool bShouldSimulateInput = false;

};

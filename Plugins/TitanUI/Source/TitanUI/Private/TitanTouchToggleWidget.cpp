// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanTouchToggleWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitanTouchToggleWidget)

struct FGeometry;
struct FPointerEvent;

FReply UTitanTouchToggleWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// toggle the simulate input flag
	bShouldSimulateInput = !bShouldSimulateInput;

	// should we activate/deactivate?
	if (bShouldSimulateInput)
	{
		// broadcast the activated delegate
		OnInputActivated.Broadcast();
	}
	else {
		// broadcast the deactivated delegate
		OnInputDeactivated.Broadcast();
	}

	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

void UTitanTouchToggleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// if the simulate flag is raised, inject the input
	if (bShouldSimulateInput)
	{
		InputKeyValue(FVector::OneVector);
	}
}

void UTitanTouchToggleWidget::ResetControl()
{
	// reset the flag
	bShouldSimulateInput = false;

	// broadcast the deactivated delegate
	OnInputDeactivated.Broadcast();
}

// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanTouchRegionWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitanTouchRegionWidget)

struct FGeometry;
struct FPointerEvent;

FReply UTitanTouchRegionWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// raise the simulate input flag
	bShouldSimulateInput = true;

	// broadcast the activated delegate
	OnInputActivated.Broadcast();

	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UTitanTouchRegionWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// raise the simulate input flag
	bShouldSimulateInput = true;

	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UTitanTouchRegionWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// lower the simulate input flag
	bShouldSimulateInput = false;

	// broadcast the deactivated delegate
	OnInputDeactivated.Broadcast();

	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void UTitanTouchRegionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// if the simulate flag is raised, inject the input
	if (bShouldSimulateInput)
	{
		InputKeyValue(FVector::OneVector);
	}
}

void UTitanTouchRegionWidget::ResetControl()
{
	// reset the flag
	bShouldSimulateInput = false;

	// broadcast the deactivated delegate
	OnInputDeactivated.Broadcast();
}

// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanJoystickWidget.h"

#include "CommonHardwareVisibilityBorder.h"
#include "Components/Image.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitanJoystickWidget)

#define LOCTEXT_NAMESPACE "TitanJoystick"

UTitanJoystickWidget::UTitanJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// force consume pointer inputs
	SetConsumePointerInput(true);
}

FReply UTitanJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);

	// broadcast the activated delegate
	OnInputActivated.Broadcast();

	// update the touch origin vector
	TouchOrigin = InGestureEvent.GetScreenSpacePosition();

	// are we already capturing the mouse?
	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		// capture the mouse
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}

	return Reply;
}

FReply UTitanJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchMoved(InGeometry, InGestureEvent);

	// update the touch delta
	HandleTouchDelta(InGeometry, InGestureEvent);

	// are we already capturing the mouse?
	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		// capture the mouse
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}

	return Reply;
}

FReply UTitanJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// stop injecting inputs
	StopInputSimulation();

	// broadcast the deactivated delegate
	OnInputDeactivated.Broadcast();

	// release the mouse capture
	return FReply::Handled().ReleaseMouseCapture();
}

void UTitanJoystickWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	// stop injecting inputs if the mouse leaves the widget area
	StopInputSimulation();
}

void UTitanJoystickWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// check if we have a visibility border
	if (!CommonVisibilityBorder || CommonVisibilityBorder->IsVisible())
	{
		// move the inner stick icon around with the vector
		if (JoystickForeground && JoystickBackground)
		{
			JoystickForeground->SetRenderTranslation(
				(bNegateYAxis ? FVector2D(1.0f, -1.0f) : FVector2D(1.0f)) *
				StickVector *
				(JoystickBackground->GetDesiredSize() * 0.5f)
			);
		}

		// inject the input
		InputKeyValue2D(StickVector);
	}
}

void UTitanJoystickWidget::ResetControl()
{
	// stop injecting inputs
	StopInputSimulation();

	// broadcast the deactivated delegate
	OnInputDeactivated.Broadcast();
}

void UTitanJoystickWidget::HandleTouchDelta(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// get the screen space position from the event
	const FVector2D& ScreenSpacePos = InGestureEvent.GetScreenSpacePosition();

	// get the local center of the geometry; it's just its absolute size
	FVector2D LocalStickCenter = InGeometry.GetAbsoluteSize() * 0.5f;

	// convert the local stick center to screen space
	FVector2D ScreenSpaceStickCenter = InGeometry.LocalToAbsolute(LocalStickCenter);

	// Get the offset from the origin
	FVector2D MoveStickOffset = (ScreenSpacePos - ScreenSpaceStickCenter);

	// optionally negate the Y axis for the input
	if (bNegateYAxis)
	{
		MoveStickOffset *= FVector2D(1.0f, -1.0f);
	}

	// extract the direction and length of the stick vector
	FVector2D MoveStickDir = FVector2D::ZeroVector;
	float MoveStickLength = 0.0f;

	MoveStickOffset.ToDirectionAndLength(MoveStickDir, MoveStickLength);

	// clamp the length to the stick range
	MoveStickLength = FMath::Min(MoveStickLength, StickRange);

	// update the stick offset
	MoveStickOffset = MoveStickDir * MoveStickLength;

	// normalize to get the -1 to 1 range stick vector
	StickVector = MoveStickOffset / StickRange;
}

void UTitanJoystickWidget::StopInputSimulation()
{
	// reset the touch origin and stick vector
	TouchOrigin = FVector2D::ZeroVector;
	StickVector = FVector2D::ZeroVector;
}

#undef LOCTEXT_NAMESPACE
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TitanCameraMath.generated.h"

/**
 *  UTitanCameraMath
 *  Implementation of Quat Spring Smoothing for camera rotation lag
 */
UCLASS()
class TITANCAMERA_API UTitanCameraMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/** Spring smoothing over a quaternion */
	static void QuatSpringSmoothing(FQuat& OutValue, FVector& OutRate, const FQuat& InGoal, float HalfLife, float DeltaTime);

private:

	/** Utility to convert spring half life to a damping factor */
	static float HalfLife_To_Damping(float HalfLife, float Epsilon = 1e-5f);
};
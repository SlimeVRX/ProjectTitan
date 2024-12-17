// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanCameraMath.h"
#include "Math/UnrealMathUtility.h"

void UTitanCameraMath::QuatSpringSmoothing(FQuat& OutValue, FVector& OutRate, const FQuat& InGoal, float HalfLife, float DeltaTime)
{
	float y = HalfLife_To_Damping(HalfLife) * 0.5f;

	FQuat xDiff = OutValue * InGoal.Inverse();
	xDiff.EnforceShortestArcWith(FQuat::Identity);

	FVector j0 = xDiff.ToRotationVector();
	
	FVector j1 = OutRate + j0 * y;

	float eydt = FMath::InvExpApprox(y * DeltaTime);

	OutValue = FQuat::MakeFromRotationVector(eydt * (j0 + j1 * DeltaTime)) * InGoal;
	
	OutRate = eydt * (OutRate - j1 * y * DeltaTime);
}

float UTitanCameraMath::HalfLife_To_Damping(float HalfLife, float Epsilon /*= 1e-5f*/)
{
	return (4.0f * 0.69314718056f) / (HalfLife + Epsilon);
}
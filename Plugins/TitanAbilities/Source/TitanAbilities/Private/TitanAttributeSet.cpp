// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanAttributeSet.h"

UTitanAttributeSet::UTitanAttributeSet()
{
	
}

void UTitanAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

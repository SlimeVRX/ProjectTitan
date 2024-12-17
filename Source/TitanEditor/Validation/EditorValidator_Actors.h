// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Validation/TitanEditorValidator.h"
#include "EditorValidator_Actors.generated.h"

/**
 *  UEditorValidator_Actors
 *  Validates Level Construction Actor properties
 */
UCLASS(config = EditorPerProjectUserSettings)
class TITANEDITOR_API UEditorValidator_Actors : public UTitanEditorValidator
{
	GENERATED_BODY()
	
public:

	UEditorValidator_Actors();

protected:

	using Super::CanValidateAsset_Implementation;

	/** returns true if the validator applies to the given asset */
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;

	/** performs asset validation */
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;

};

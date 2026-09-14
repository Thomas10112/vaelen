// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"

class UObject;

class UMaterialInstanceDynamic : public UMaterialInterface
{
public:
	static UMaterialInstanceDynamic* Create(UMaterialInterface* Parent, UObject* Outer);
	/// FName, not const TCHAR*: the engine takes a name here and the project's
	/// code passes one straight out of FMaterialParameterInfo.
	void SetVectorParameterValue(FName ParameterName, const FLinearColor& Value);
	void SetScalarParameterValue(FName ParameterName, float Value);
};

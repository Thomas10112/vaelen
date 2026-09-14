// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

/// Names one parameter of a material. The engine's has more in it; what is
/// here is what the project's code reads.
struct FMaterialParameterInfo
{
	FName Name;
	int32 Association = 0;
	int32 Index = -1;
};

using FGuid = uint64;

class UMaterialInterface : public UObject
{
public:
	void GetAllVectorParameterInfo(TArray<FMaterialParameterInfo>& OutInfo, TArray<FGuid>& OutIds) const;
	void GetAllScalarParameterInfo(TArray<FMaterialParameterInfo>& OutInfo, TArray<FGuid>& OutIds) const;
};

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "StargameM1FixtureAuthoring.generated.h"

UCLASS()
class STARGAME_API UStargameCreateM1FixtureAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};

UCLASS()
class STARGAME_API UStargameValidateContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};

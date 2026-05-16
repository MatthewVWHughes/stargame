#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/SaveGame.h"
#include "StargameM0SaveGame.generated.h"

UCLASS()
class STARGAME_API UStargameM0SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FStargameM0SaveState State;
};

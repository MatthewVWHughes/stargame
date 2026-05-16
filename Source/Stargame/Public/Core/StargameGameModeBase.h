#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StargameGameModeBase.generated.h"

UCLASS()
class STARGAME_API AStargameGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStargameGameModeBase();

protected:
	virtual void BeginPlay() override;
};

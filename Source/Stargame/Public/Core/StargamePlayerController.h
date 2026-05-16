#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StargamePlayerController.generated.h"

UCLASS()
class STARGAME_API AStargamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStargamePlayerController();

protected:
	virtual void BeginPlay() override;
};


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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|UI")
	TSubclassOf<class UStargameBootMenuWidget> BootMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Startup")
	bool bAutoStartWhenBootMenuMissing = true;
};

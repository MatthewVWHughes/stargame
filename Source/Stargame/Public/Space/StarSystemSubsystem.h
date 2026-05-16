#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StarSystemSubsystem.generated.h"

UCLASS()
class STARGAME_API UStarSystemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	double GetGameTimeSeconds() const { return GameTimeSeconds; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	void AdvanceGameTime(double DeltaSeconds);

private:
	double GameTimeSeconds = 0.0;
};


#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StargameSessionSubsystem.generated.h"

UCLASS()
class STARGAME_API UStargameSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void StartNewSession();

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetCurrentSystemId() const { return CurrentSystemId; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetCurrentSystemId(FName SystemId) { CurrentSystemId = SystemId; }

private:
	UPROPERTY()
	FName CurrentSystemId = TEXT("sol");
};


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/StargameDataTypes.h"
#include "ShipFlightModeComponent.generated.h"

UCLASS(ClassGroup = (Stargame), meta = (BlueprintSpawnableComponent))
class STARGAME_API UShipFlightModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipFlightModeComponent();

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	EShipFlightMode GetFlightMode() const { return FlightMode; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	void SetFlightMode(EShipFlightMode NewMode) { FlightMode = NewMode; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	double GetNormalFlightMaxSpeedCmPerSec() const { return NormalFlightMaxSpeedCmPerSec; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	void SetNormalFlightMaxSpeedCmPerSec(double NewMaxSpeedCmPerSec);

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	FString GetFlightModeDebugSummary() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Flight")
	EShipFlightMode FlightMode = EShipFlightMode::Normal;

	UPROPERTY(VisibleAnywhere, Category = "Flight", meta = (Units = "cm/s"))
	double NormalFlightMaxSpeedCmPerSec = 24000.0;
};

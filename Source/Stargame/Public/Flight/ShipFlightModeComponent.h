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

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	void ConfigureSupercruiseFromScale(const FStargameScaleContract& Scale);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	ESupercruiseRequestResult RequestEnterSupercruise(const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	void RequestManualDropout();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight")
	void TickSupercruise(double DeltaSeconds, const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target);

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	FSupercruiseGuidanceResult ComputeSupercruiseGuidance(const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	ESupercruiseState GetSupercruiseState() const { return SupercruiseState; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	FSupercruiseTelemetry GetSupercruiseTelemetry() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	double GetCurrentSpeedLimitCmPerSec() const { return CurrentSpeedLimitCmPerSec; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	double GetCurrentSupercruiseSpeedCmPerSec() const { return CurrentSupercruiseSpeedCmPerSec; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Flight")
	FString GetFlightModeDebugSummary() const;

private:
	void EnterDropoutCooldown(ESupercruiseState DropoutReason);

	UPROPERTY(VisibleAnywhere, Category = "Flight")
	EShipFlightMode FlightMode = EShipFlightMode::Normal;

	UPROPERTY(VisibleAnywhere, Category = "Flight", meta = (Units = "cm/s"))
	double NormalFlightMaxSpeedCmPerSec = 24000.0;

	UPROPERTY(VisibleAnywhere, Category = "Flight")
	ESupercruiseState SupercruiseState = ESupercruiseState::Inactive;

	UPROPERTY(VisibleAnywhere, Category = "Flight", meta = (Units = "cm/s"))
	double CurrentSupercruiseSpeedCmPerSec = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Flight", meta = (Units = "cm/s"))
	double CurrentSpeedLimitCmPerSec = 24000.0;

	UPROPERTY(VisibleAnywhere, Category = "Flight", meta = (Units = "s"))
	double StateRemainingSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Flight")
	FStargameScaleContract SupercruiseScale;

	UPROPERTY(VisibleAnywhere, Category = "Flight")
	FGravityWellQueryResult LastGravityTelemetry;

	UPROPERTY(VisibleAnywhere, Category = "Flight")
	FSupercruiseTargetTelemetry LastTargetTelemetry;
};

#include "Flight/ShipFlightModeComponent.h"

UShipFlightModeComponent::UShipFlightModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UShipFlightModeComponent::SetNormalFlightMaxSpeedCmPerSec(double NewMaxSpeedCmPerSec)
{
	NormalFlightMaxSpeedCmPerSec = FMath::Max(1.0, NewMaxSpeedCmPerSec);
}

FString UShipFlightModeComponent::GetFlightModeDebugSummary() const
{
	return FString::Printf(
		TEXT("FlightMode=%s\nNormalFlightMaxSpeedCmPerSec=%.0f"),
		*UEnum::GetValueAsString(FlightMode),
		NormalFlightMaxSpeedCmPerSec);
}

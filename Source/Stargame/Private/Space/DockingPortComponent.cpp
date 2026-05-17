#include "Space/DockingPortComponent.h"

UDockingPortComponent::UDockingPortComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDockingPortComponent::ConfigureDockingPort(FName InStationId, const FDockingPortDefinition& InDefinition)
{
	StationId = InStationId;
	Definition = InDefinition;
	RuntimeState = FDockingPortRuntimeState();
	RuntimeState.StationId = StationId;
	RuntimeState.PortId = Definition.PortId;
}

void UDockingPortComponent::SetRuntimeState(const FDockingPortRuntimeState& NewRuntimeState)
{
	RuntimeState = NewRuntimeState;
}

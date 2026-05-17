#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Data/StargameDataTypes.h"
#include "DockingPortComponent.generated.h"

UCLASS(ClassGroup = (Stargame), meta = (BlueprintSpawnableComponent))
class STARGAME_API UDockingPortComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDockingPortComponent();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Docking")
	void ConfigureDockingPort(FName InStationId, const FDockingPortDefinition& InDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Docking")
	FName GetStationId() const { return StationId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Docking")
	FName GetPortId() const { return Definition.PortId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Docking")
	FDockingPortDefinition GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Docking")
	FDockingPortRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Docking")
	void SetRuntimeState(const FDockingPortRuntimeState& NewRuntimeState);

private:
	UPROPERTY(EditAnywhere, Category = "Docking")
	FName StationId;

	UPROPERTY(EditAnywhere, Category = "Docking")
	FDockingPortDefinition Definition;

	UPROPERTY(VisibleAnywhere, Category = "Docking")
	FDockingPortRuntimeState RuntimeState;
};

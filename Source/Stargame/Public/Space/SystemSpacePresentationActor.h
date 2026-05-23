#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SystemSpacePresentationActor.generated.h"

class USceneComponent;
class UDirectionalLightComponent;
class UPostProcessComponent;
class USkyLightComponent;
class UStaticMeshComponent;

UCLASS()
class STARGAME_API ASystemSpacePresentationActor : public AActor
{
	GENERATED_BODY()

public:
	ASystemSpacePresentationActor();

	virtual void Tick(float DeltaSeconds) override;

	void ConfigureForSystem(FName InSystemId);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Space")
	void OnConfiguredForSystem(FName InSystemId);

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SystemOriginMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyLightComponent> AmbientDebugLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> StarDirectionalLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SkySphereMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> SpacePostProcess;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Space")
	FName SystemId;

	FVector LastLightingPlayerLocation = FVector(ForceInit);
	FVector LastLightingStarLocation = FVector(ForceInit);
	float LightingUpdateAccumulatorSeconds = 0.0f;
	float LightingUpdateIntervalSeconds = 0.25f;
	float LightingMoveThresholdCm = 50000.0f;
	float LightingAngleThresholdDegrees = 2.0f;

	void RefreshSpaceVisuals(float DeltaSeconds);
	bool ResolveStarAndPlayerLocations(FVector& OutLogicalStarLocation, FVector& OutLogicalPlayerLocation, FVector& OutActorPlayerLocation) const;
	void UpdateStarLight(const FVector& LogicalStarLocation, const FVector& LogicalPlayerLocation, const FVector& ActorPlayerLocation);
};

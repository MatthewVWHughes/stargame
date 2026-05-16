#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasGiantVolumetricCloudActor.generated.h"

class UVolumetricCloudComponent;

UCLASS()
class STARGAME_API AGasGiantVolumetricCloudActor : public AActor
{
	GENERATED_BODY()

public:
	AGasGiantVolumetricCloudActor();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyCloudSettings();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud", meta = (ClampMin = "0.1"))
	float PrototypePlanetRadiusKilometers = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud", meta = (ClampMin = "0.0"))
	float CloudBottomAltitudeKilometers = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud", meta = (ClampMin = "0.1"))
	float CloudLayerHeightKilometers = 3.2f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud", meta = (ClampMin = "1.0"))
	float TracingMaxDistanceKilometers = 42.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud", meta = (ClampMin = "0.05"))
	float ViewSampleCountScale = 2.25f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Volumetric Cloud")
	FColor GroundAlbedo = FColor(18, 92, 24);
};

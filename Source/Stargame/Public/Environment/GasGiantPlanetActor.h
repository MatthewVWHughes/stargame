#pragma once

#include "CoreMinimal.h"
#include "GasGiantAtmosphereTypes.h"
#include "GameFramework/Actor.h"
#include "GasGiantPlanetActor.generated.h"

class AGasGiantCloudField;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class STARGAME_API AGasGiantPlanetActor : public AActor
{
	GENERATED_BODY()

public:
	AGasGiantPlanetActor();

	UFUNCTION(BlueprintPure, Category = "Gas Giant|Shape")
	float GetPlanetRadius() const { return VisualRadius; }

	UFUNCTION(BlueprintPure, Category = "Gas Giant|Shape")
	float GetVisualRadius() const { return VisualRadius; }

	UFUNCTION(BlueprintPure, Category = "Gas Giant|Atmosphere")
	float GetAtmosphereEntryRadius() const { return VisualRadius + AtmosphereEntryAltitude; }

	UFUNCTION(BlueprintPure, Category = "Gas Giant|Atmosphere")
	FGasGiantAtmosphereState ComputeAtmosphereState(FVector WorldLocation) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ValidateAtmosphereBands();
	void ApplyPlanetScale();
	void SpawnCloudField();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlanetMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Shape", meta = (AllowPrivateAccess = "true", ClampMin = "1000.0", Units = "cm"))
	float VisualRadius = 5000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float AtmosphereEntryAltitude = 4500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float DenseLayerTopAltitude = 1500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float DeepLayerTopAltitude = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float CrushDepthStartAltitude = -400000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float CrushDepthFatalAltitude = -1500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Appearance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float PlanetGrey = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Appearance", meta = (AllowPrivateAccess = "true"))
	FLinearColor PlanetCoreColor = FLinearColor(0.035f, 0.16f, 0.045f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Prototype", meta = (AllowPrivateAccess = "true"))
	bool bEnableDebugCoreCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Deprecated", meta = (AllowPrivateAccess = "true"))
	bool bSpawnCloudField = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Deprecated", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGasGiantCloudField> CloudFieldClass;

	UPROPERTY()
	TObjectPtr<AGasGiantCloudField> RuntimeCloudField;
};

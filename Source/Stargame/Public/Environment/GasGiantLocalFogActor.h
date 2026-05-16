#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasGiantAtmosphereTypes.h"
#include "GasGiantLocalFogActor.generated.h"

class AGasGiantPlanetActor;
class ASpaceFlightPawn;
class ULocalFogVolumeComponent;
class USceneComponent;

UCLASS()
class STARGAME_API AGasGiantLocalFogActor : public AActor
{
	GENERATED_BODY()

public:
	AGasGiantLocalFogActor();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void ResolveSceneActors();
	void ApplyFogState(const FGasGiantAtmosphereState& AtmosphereState, float DeltaSeconds);
	void ApplyStructuredVolumes(const FGasGiantAtmosphereState& AtmosphereState, float DeltaSeconds);
	void SetVolumeTransform(ULocalFogVolumeComponent* Volume, const FVector& Location, const FQuat& Rotation, const FVector& Diameter) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> FieldRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<ULocalFogVolumeComponent> BaseHazeVolume;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<ULocalFogVolumeComponent>> DeckVolumes;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<ULocalFogVolumeComponent>> StormVolumes;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog", meta = (ClampMin = "1000.0", Units = "cm"))
	float BaseHazeRadius = 180000.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog", meta = (ClampMin = "0.0"))
	float MaxBaseRadialExtinction = 0.14f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog", meta = (ClampMin = "0.0"))
	float MaxBaseHeightExtinction = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Cloud Deck", meta = (ClampMin = "1000.0", Units = "cm"))
	float DeckFieldRadius = 900000.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Cloud Deck", meta = (ClampMin = "1000.0", Units = "cm"))
	float DeckMinBelowShip = 120000.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Cloud Deck", meta = (ClampMin = "1000.0", Units = "cm"))
	float DeckMaxBelowShip = 1200000.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Cloud Deck", meta = (ClampMin = "0.0"))
	float MaxDeckRadialExtinction = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Cloud Deck", meta = (ClampMin = "0.0"))
	float MaxDeckHeightExtinction = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Storms", meta = (ClampMin = "1000.0", Units = "cm"))
	float StormFieldRadius = 760000.0f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Storms", meta = (ClampMin = "0.0"))
	float MaxStormRadialExtinction = 0.76f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Storms", meta = (ClampMin = "0.0"))
	float MaxStormHeightExtinction = 0.52f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Storms", meta = (ClampMin = "0.0"))
	float StormRotationDegreesPerSecond = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog", meta = (ClampMin = "1.0"))
	float HeightFalloff = 2.1f;

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog")
	FLinearColor FogAlbedo = FLinearColor(0.36f, 0.72f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog")
	FLinearColor DeepFogEmissive = FLinearColor(0.005f, 0.030f, 0.004f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Gas Giant|Fog", meta = (ClampMin = "0.0"))
	float FogInterpSpeed = 4.0f;

	UPROPERTY()
	TObjectPtr<AGasGiantPlanetActor> CachedPlanet;

	UPROPERTY()
	TObjectPtr<ASpaceFlightPawn> CachedPawn;

	float CurrentRadialExtinction = 0.0f;
	float CurrentHeightExtinction = 0.0f;
	float StormRotationDegrees = 0.0f;
};

#pragma once

#include "CoreMinimal.h"
#include "GasGiantAtmosphereTypes.generated.h"

UENUM(BlueprintType)
enum class EGasGiantAtmosphereZone : uint8
{
	Far,
	Entry,
	Dense,
	Deep,
	Crush
};

USTRUCT(BlueprintType)
struct STARGAME_API FGasGiantAtmosphereState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	EGasGiantAtmosphereZone Zone = EGasGiantAtmosphereZone::Far;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	FVector PlanetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (Units = "cm"))
	float DistanceFromCenter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere", meta = (Units = "cm"))
	float VisualAltitude = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	float EntryAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	float DenseAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	float DeepAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	float CrushAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gas Giant|Atmosphere")
	float RenderFogDensity = 0.0f;
};

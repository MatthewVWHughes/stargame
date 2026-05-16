#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StargameDataTypes.generated.h"

UENUM(BlueprintType)
enum class EShipFlightMode : uint8
{
	Normal,
	Cruise,
	Supercruise,
	DockingAutopilot
};

USTRUCT(BlueprintType)
struct STARGAME_API FGravityWellDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	FName BodyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	double RadiusMeters = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	bool bIsMajorBody = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	double StationSoiOverrideMeters = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCommodityStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	FName CommodityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	double Quantity = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FGuid ShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	EShipFlightMode FlightMode = EShipFlightMode::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	double Hull = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	double Shields = 100.0;
};

UCLASS(BlueprintType)
class STARGAME_API UShipArchetypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double NormalMaxSpeed = 600.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double CruiseSpeed = 6000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double CargoCapacity = 20.0;
};

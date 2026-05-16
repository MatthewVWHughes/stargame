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

UENUM(BlueprintType)
enum class EStartSessionResult : uint8
{
	Success,
	MissingStartProfile,
	MissingSystemDefinition,
	MissingSpawnZone,
	ValidationFailed
};

USTRUCT(BlueprintType)
struct STARGAME_API FNavigationTargetDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName FrameType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bShowInHud = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bShowInSystemMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bCanTarget = true;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStartProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName StartProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName SpawnZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName DockedStationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName DockingPortId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FBodyDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName BodyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName BodyType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName FrameType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double VisualRadiusCm = 100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FNavigationTargetDefinition NavigationTarget;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStationDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName FrameType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double VisualRadiusCm = 12000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FNavigationTargetDefinition NavigationTarget;
};

USTRUCT(BlueprintType)
struct STARGAME_API FGateDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName GateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName FrameType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double VisualRadiusCm = 16000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double ActivationRangeCm = 50000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName DestinationSystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName DestinationGateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName DestinationArrivalId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FNavigationTargetDefinition NavigationTarget;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSpawnZoneDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName SpawnZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName FrameType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double RadiusCm = 10000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FName> AllowedContexts;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStarSystemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FBodyDefinition> Bodies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FStationDefinition> Stations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FGateDefinition> Gates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FSpawnZoneDefinition> SpawnZones;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStargameM0SaveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 SaveFormatVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 GameContentVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FString BuildCompatibilityId = TEXT("m0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SpawnZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SelectedTargetId;
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

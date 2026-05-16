#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StargameDataTypes.generated.h"

class APawn;

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

UENUM(BlueprintType)
enum class EStargameValidationSeverity : uint8
{
	Info,
	Warning,
	Error,
	Fatal
};

UENUM(BlueprintType)
enum class EStargameValidationProfile : uint8
{
	Editor,
	Build,
	Cook,
	M0,
	M1
};

UENUM(BlueprintType)
enum class ESystemSourceType : uint8
{
	Authored,
	Generated,
	Imported
};

USTRUCT(BlueprintType)
struct STARGAME_API FStargameValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	EStargameValidationSeverity Severity = EStargameValidationSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	FName RuleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	FName ObjectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	FString Message;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStargameValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	EStargameValidationProfile Profile = EStargameValidationProfile::Editor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	TArray<FStargameValidationIssue> Issues;

	bool HasBlockingIssues() const
	{
		for (const FStargameValidationIssue& Issue : Issues)
		{
			if (Issue.Severity == EStargameValidationSeverity::Error || Issue.Severity == EStargameValidationSeverity::Fatal)
			{
				return true;
			}
		}
		return false;
	}
};

USTRUCT(BlueprintType)
struct STARGAME_API FOrbitDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
	FName ParentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
	FName OrbitFrameBasis = TEXT("parent_ecliptic");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit", meta = (Units = "cm"))
	double SemiMajorAxisCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit", meta = (Units = "s"))
	double PeriodSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
	double PhaseOffsetRadians = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
	double Eccentricity = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit", meta = (Units = "deg"))
	double InclinationDegrees = 0.0;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName ShipArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FName InitialInventoryProfileId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockingPortDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FName PortId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FTransform ApproachTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FTransform DockedTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FTransform UndockTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking")
	FGameplayTagContainer AllowedShipClasses;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FOrbitDefinition Orbit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double PhysicalReferenceRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double VisualRadiusCm = 100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System", meta = (Units = "cm"))
	double CollisionRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FPrimaryAssetId VisualProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FPrimaryAssetId AtmosphereProfileId;

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
	FPrimaryAssetId StationProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FDockingPortDefinition> DockingPorts;

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
	FPrimaryAssetId GateProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FNavigationTargetDefinition NavigationTarget;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMapEntryDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName MapEntryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName EntryType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	bool bVisibleInSystemMap = true;
};

USTRUCT(BlueprintType)
struct STARGAME_API FGravityWellDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	FName WellId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	FName AnchorBodyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (Units = "cm"))
	double SlowdownRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (Units = "cm"))
	double LockoutRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (Units = "cm"))
	double DropoutRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	double Strength = 1.0;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	ESystemSourceType SourceType = ESystemSourceType::Authored;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FGravityWellDefinition> GravityWells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FMapEntryDefinition> MapEntries;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipMovementProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName MovementProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FGameplayTagContainer SupportedFlightModeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName NormalFlightTuningId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName SupercruiseTuningId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship", meta = (Units = "cm/s"))
	double MaxLocalSpeedCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship", meta = (Units = "deg/s"))
	double MaxAngularSpeedDegPerSec = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipDurabilityProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DurabilityProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double MaxHull = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double MaxShield = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double DisabledHullFraction = 0.15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double DestroyedHullFraction = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipLoadoutProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName LoadoutProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TArray<FName> DefaultInstalledSlotIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FGameplayTagContainer HardpointTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName RequiredResourceProfileId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipResourceCapacityDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName ResourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double Capacity = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipResourceProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName ResourceProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TArray<FShipResourceCapacityDefinition> ResourceCapacities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DefaultFillPolicy = TEXT("full");
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipArchetypeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName ShipArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FGameplayTagContainer ShipClassTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TSoftClassPtr<APawn> PawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName MovementProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double DefaultCargoCapacityMassKg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	double DefaultCargoCapacityVolumeM3 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DefaultDurabilityProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DefaultLoadoutProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DefaultResourceProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName DockingSizeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FGameplayTagContainer AllowedDockingPortTags;
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

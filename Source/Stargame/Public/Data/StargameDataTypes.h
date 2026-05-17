#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StargameDataTypes.generated.h"

class APawn;
class UCurveFloat;

UENUM(BlueprintType)
enum class EShipFlightMode : uint8
{
	Normal,
	Cruise,
	Supercruise,
	DockingAssist,
	Docked,
	GateArrival,
	Disabled,
	DockingAutopilot
};

UENUM(BlueprintType)
enum class ESupercruiseState : uint8
{
	Inactive,
	Spooling,
	Cruising,
	ForcedDropout,
	ManualDropout,
	DropoutCooldown
};

UENUM(BlueprintType)
enum class ESupercruiseRequestResult : uint8
{
	Success,
	AlreadyInSupercruise,
	InsideLockout,
	DriveDisabled,
	NoValidLogicalLocation,
	TargetTooClose,
	SpoolInterrupted
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
	M1,
	M2,
	M3,
	M4,
	M5,
	M6
};

UENUM(BlueprintType)
enum class EShipLocationMode : uint8
{
	FreeFlight,
	StationDocked,
	GateArrival,
	Respawn
};

UENUM(BlueprintType)
enum class EDockingState : uint8
{
	None,
	Requested,
	Reserved,
	Approach,
	FinalAssist,
	Docked,
	Undocking,
	Aborted
};

UENUM(BlueprintType)
enum class EDockingPortTransformKind : uint8
{
	Approach,
	Docked,
	Undock
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
struct STARGAME_API FStargameCoordinateFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FName FrameType = TEXT("system_barycentric");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FName AnchorId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSectorPosition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (Units = "ly"))
	FVector PositionLy = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemSpaceLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct STARGAME_API FBodyRelativeLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FName BodyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLocalSpaceLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (Units = "cm"))
	FVector BubbleOriginCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSimulationClockSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	double AuthoritativeSimulationTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	double TimeScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	FName ClockOwner = TEXT("session");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	FName RngStreamId = TEXT("frontier_test_01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	int64 RngCounter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	int64 ProcessedEventWatermark = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	TMap<FName, int64> ProcessedEventWatermarks;
};

USTRUCT(BlueprintType)
struct STARGAME_API FFrameResolvedTransform
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FStargameCoordinateFrame CoordinateFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	double SimulationTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (Units = "cm/s"))
	FVector LinearVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (Units = "rad/s"))
	FVector AngularVelocityRadPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	bool bActorSpaceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FTransform ActorSpaceTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLocalBubbleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local Bubble")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local Bubble", meta = (Units = "cm"))
	FVector BubbleOriginCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local Bubble", meta = (Units = "cm"))
	double LocalBubbleRadiusCm = 5000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local Bubble", meta = (Units = "cm"))
	double OriginShiftThresholdCm = 2000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local Bubble")
	int32 RebaseGeneration = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipSaveLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FStargameCoordinateFrame CoordinateFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EShipLocationMode LocationMode = EShipLocationMode::Respawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (Units = "cm/s"))
	FVector LinearVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FStargameCoordinateFrame VelocityFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bInheritAnchorVelocity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	double AuthoritativeSimulationTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName DockedStationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName DockingPortId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EDockingState DockingState = EDockingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FTransform PortRelativeTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStargameScaleContract
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm/s"))
	double NormalFlightMaxSpeedCmPerSec = 24000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double LocalBubbleRadiusCm = 5000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double OriginShiftThresholdCm = 2000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double StationApproachBubbleRadiusCm = 500000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double DockingCorridorLengthCm = 25000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm/s"))
	double SupercruiseMinSpeedCmPerSec = 100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm/s"))
	double SupercruiseMaxSpeedCmPerSec = 20000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "s"))
	double SupercruiseSpoolSeconds = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "s"))
	double DropoutCooldownSeconds = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double SupercruiseTargetDropoutMinRadiusCm = 250000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double SupercruiseTargetDropoutMaxRadiusCm = 750000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm/s^2"))
	double SupercruiseAccelerationCmPerSec2 = 250000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm/s^2"))
	double SupercruiseBrakingCmPerSec2 = 450000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double GravitySlowdownRadiusCm = 2000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double GravityLockoutRadiusCm = 500000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (Units = "cm"))
	double AtmosphereTransitionRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	double MapDistanceScaleCmPerUnit = 1000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	double MapMinIconScale = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	double MapMaxIconScale = 2.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FGravityWellQueryResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	FName NearestWellId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	FName AnchorBodyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	double SlowdownFactor = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (Units = "cm/s"))
	double SpeedCeilingCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	bool bInsideSlowdown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	bool bInsideLockout = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	bool bInsideDropout = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
	bool bForcedDropout = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSupercruiseTargetTelemetry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double ClosingSpeedCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm"))
	double DistanceToDropoutBandCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	bool bInsideDropoutBand = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	bool bApproachReady = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSupercruiseGuidanceResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double GravitySpeedLimitCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double BrakingSpeedLimitCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double DesiredSpeedCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	bool bTargetDropoutReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	bool bGravityDropoutRequired = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSupercruiseTelemetry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	EShipFlightMode FlightMode = EShipFlightMode::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	ESupercruiseState SupercruiseState = ESupercruiseState::Inactive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double CurrentSpeedCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "cm/s"))
	double SpeedLimitCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise", meta = (Units = "s"))
	double StateRemainingSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	FGravityWellQueryResult Gravity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supercruise")
	FSupercruiseTargetTelemetry Target;
};

USTRUCT(BlueprintType)
struct STARGAME_API FNavigationTargetViewModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FStargameCoordinateFrame TargetFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bIsSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bResolved = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (Units = "cm/s"))
	double ClosingSpeedCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FFrameResolvedTransform ResolvedTargetTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FTransform ActorSpaceProjection = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bActorSpaceProjectionValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bInsideLocalBubble = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bInsideStationApproachBubble = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	bool bInsideGateActivationRange = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemMapEntryViewModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName ParentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName EntryType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D MapPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	double IconScale = 1.0;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking", meta = (Units = "cm"))
	double ActivationRangeCm = 15000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking", meta = (Units = "cm/s"))
	double MaxApproachSpeedCmPerSec = 2500.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Docking", meta = (Units = "deg"))
	double RequiredAlignmentDegrees = 10.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockingPortRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName PortId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName ReservedShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName OccupyingShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName ClearanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	double ReservationExpiryTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	EDockingState DockingState = EDockingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FString LastFailureReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockingOperationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName ShipInstanceId = TEXT("player_ship");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName PortId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	EDockingState DockingState = EDockingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FName ClearanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FTransform CapturedPortRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking", meta = (Units = "cm/s"))
	FVector CapturedPortRelativeVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	double StateStartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	double LastStateChangeTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking")
	FString LastFailureReason;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FOrbitDefinition Orbit;

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
struct STARGAME_API FResourceZoneDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FName ZoneType = TEXT("asteroid_field");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FName FrameType = TEXT("system_barycentric");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone", meta = (Units = "cm"))
	double RadiusCm = 250000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FGameplayTagContainer ResourceTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
	FGameplayTagContainer HazardTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Zone")
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
struct STARGAME_API FGeneratedSystemSourceMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 GeneratedSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FName GeneratorVersion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FName SourceSystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FName GenerationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FString GeneratedAtUtc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TArray<FName> AuthoredOverrideIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TArray<FName> GenerationInputIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FString SourceFingerprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bDependsOnArrayOrder = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bDependsOnActorNames = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bDependsOnEditorOnlyState = false;
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
	FStargameScaleContract Scale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FGravityWellDefinition> GravityWells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FResourceZoneDefinition> ResourceZones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FMapEntryDefinition> MapEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FGeneratedSystemSourceMetadata GeneratedSourceMetadata;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TSoftObjectPtr<UCurveFloat> SupercruiseAccelerationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TSoftObjectPtr<UCurveFloat> SupercruiseDecelerationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TSoftObjectPtr<UCurveFloat> SupercruiseTurnResponseCurve;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FSimulationClockSnapshot ClockSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FShipSaveLocation ShipLocation;
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

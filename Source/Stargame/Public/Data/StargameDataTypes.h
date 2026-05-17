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
	M6,
	M7,
	M8,
	M9,
	M10,
	M11
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

UENUM(BlueprintType)
enum class EShipGoalKind : uint8
{
	None,
	TradeRoute,
	Patrol,
	Escort,
	Pirate,
	Flee,
	Fight
};

UENUM(BlueprintType)
enum class EShipGoalExecutionResult : uint8
{
	Success,
	NoWork,
	InvalidGoal,
	ReservedUntilM9,
	ReservedUntilM10,
	Interrupted,
	BudgetExceeded
};

UENUM(BlueprintType)
enum class ELogicalTrafficTier : uint8
{
	Tier2Logical,
	Tier1Realized
};

UENUM(BlueprintType)
enum class ESystemicActionResult : uint8
{
	Accepted,
	Rejected,
	Partial,
	Pending,
	Duplicate
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
struct STARGAME_API FMovingFrameTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FStargameCoordinateFrame CoordinateFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName AnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target", meta = (Units = "cm"))
	FVector LocalOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	double RouteProgress01 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Target")
	FName GroupId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FAIPredictedTransform
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Prediction")
	FMovingFrameTarget Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Prediction")
	double SimulationTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Prediction")
	FFrameResolvedTransform ResolvedTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Prediction")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Prediction")
	FString InvalidReason;
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
	FName DockedShipInstanceId = TEXT("player_ship");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName DockingClearanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EDockingState DockingState = EDockingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FTransform PortRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName DockingReservedShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName DockingOccupyingShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	double DockingReservationExpiryTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EDockingState DockingPortRuntimeState = EDockingState::None;
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
struct STARGAME_API FRouteControlData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route", meta = (Units = "cm"))
	double ArcHeightCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FVector ArcNormalHint = FVector::UpVector;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRouteRiskStub
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	double SecurityRating = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RiskProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	bool bOpaqueUntilM9 = true;
};

USTRUCT(BlueprintType)
struct STARGAME_API FTrafficRouteSegmentDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName SourceAnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName DestinationAnchorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route", meta = (Units = "cm"))
	FVector SourceLocalOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route", meta = (Units = "cm"))
	FVector DestinationLocalOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RoutePolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RouteGeometryPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName TravelModelId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RouteProgressSemantic = TEXT("time_fraction");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	TArray<FName> AvoidanceAnchorIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	TArray<FName> ExclusionZoneIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RouteFrameBasisPolicy = TEXT("source_to_destination");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FRouteControlData ControlData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	double SecurityRating = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	double RouteValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FGameplayTagContainer AllowedShipClassTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	FName RiskProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	bool bSupportsPatrolCoverage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic Route")
	bool bSupportsPirateAmbush = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRouteSample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double SimulationTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double RouteProgress01 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route", meta = (Units = "cm"))
	double RouteDistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FName RouteProgressSemantic = TEXT("time_fraction");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FFrameResolvedTransform ResolvedTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FVector Tangent = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route", meta = (Units = "cm/s"))
	FVector EstimatedVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FFrameResolvedTransform SourceAnchorTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FFrameResolvedTransform DestinationAnchorTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FName RouteGeometryPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FName TravelModelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double SecurityRating = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double RiskScore = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRouteClosestProgressResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double RouteProgress01 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route", meta = (Units = "cm"))
	double DistanceErrorCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	double BasisErrorDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route", meta = (Units = "cm/s"))
	double VelocityErrorCmPerSec = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Route")
	bool bValid = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipGoalState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName GoalId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	EShipGoalKind GoalKind = EShipGoalKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FMovingFrameTarget TargetFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	double RouteProgress01 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	double DesiredRouteSpeedFractionPerSecond = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	int32 InterruptPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	bool bAutonomousExecutionAllowed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName DecisionReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipFormationSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName SlotFrame = TEXT("leader_route_sample");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "cm"))
	FVector LocalOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	double RouteProgressOffset = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipGroupState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName LeaderShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName GroupRole = TEXT("trade");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	TArray<FName> MemberShipInstanceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	TArray<FShipFormationSlot> FormationSlots;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipTrafficInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName ShipArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FShipGoalState CurrentGoal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FShipGoalState SavedGoal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	bool bHasSavedGoal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName FormationSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FMovingFrameTarget LogicalLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FRotator LogicalRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FStargameCoordinateFrame VelocityFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "cm/s"))
	FVector LogicalVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	bool bInheritsFrameVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "cm/s"))
	FVector FrameVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	ELogicalTrafficTier TrafficTier = ELogicalTrafficTier::Tier2Logical;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName RealizationToken;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FRouteSample LastRouteSample;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	double LastUpdateTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName LastDecisionReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	bool bRouteRecoverable = true;
};

USTRUCT(BlueprintType)
struct STARGAME_API FActiveTrafficUpdateBudget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	int32 MaxShipsPerUpdate = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "s"))
	double MaxSimulationStepSeconds = 5.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FActiveTrafficUpdateStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	int32 ConsideredShips = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	int32 UpdatedShips = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	int32 SkippedShips = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "s"))
	double RequestedDeltaSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI", meta = (Units = "s"))
	double AppliedDeltaSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName CapReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FActiveTrafficSimulationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	TArray<FShipTrafficInstance> Ships;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	TArray<FShipGroupState> Groups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FActiveTrafficUpdateStats LastUpdateStats;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLogicalTrafficDebugView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName Goal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FMovingFrameTarget TargetFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	double RouteProgress01 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FName DecisionReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic AI")
	FString DebugSummary;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemicTargetRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	FName TargetId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSimulationEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName SourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FMovingFrameTarget LocationTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	TArray<FName> ParticipantShipIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	TArray<FName> ParticipantGroupIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName PayloadRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	double CreatedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	double ScheduledTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	double ExpiryTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName RngStreamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	int64 RngCounter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName IdempotencyKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	int64 ProcessedEventWatermark = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSimulationEventResultRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName ResultId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	double AppliedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName OutcomeType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	TArray<FName> AffectedStateRefs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FString DebugReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Event")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FGameplayTransactionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName TransactionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName InitiatorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName InitiatorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName State = TEXT("preparing");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName IdempotencyKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	double CreatedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	double LastChangedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FGameplayTransactionJournalEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName JournalEntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	int32 Sequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName SideEffectType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName SideEffectRefId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName State = TEXT("pending");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FName IdempotencyKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	double AppliedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Transaction")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCreditAccountRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName AccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName OwnerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName OwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName CurrencyId = TEXT("credits");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	int64 AvailableBalance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	int64 HeldBalance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName AccountState = TEXT("active");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName LastLedgerEntryId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCreditLedgerEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName LedgerEntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName DebitAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName CreditAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	int64 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName CurrencyId = TEXT("credits");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName Reason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName SourceTransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName IdempotencyKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	double AppliedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName State = TEXT("pending");
};

USTRUCT(BlueprintType)
struct STARGAME_API FEscrowHoldRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName EscrowId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName HoldingAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName BeneficiaryAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	int64 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName CurrencyId = TEXT("credits");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName Reason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName State = TEXT("held");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Credits")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FFactionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName FactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName FactionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName DefaultLawProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName RelationshipGroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	bool bCanOwnStations = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	bool bCanOwnShips = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	bool bCanIssueMissions = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FFactionRelationshipRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName SourceFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName TargetFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	double Standing = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName HostilityState = TEXT("neutral");
};

USTRUCT(BlueprintType)
struct STARGAME_API FFactionOperationalState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName FactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	double Influence = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	double SecurityPressure = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	double CriminalPressure = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	TArray<FName> PatrolAssetIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	int32 AvailablePatrolBudget = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	int32 ReservedPatrolBudget = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Faction")
	FName AlertLevel = TEXT("normal");
};

USTRUCT(BlueprintType)
struct STARGAME_API FJurisdictionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName AuthorityFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName LawProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FOffenseEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName OffenseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName OffenderType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName OffenderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName VictimType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName VictimId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName OffenseType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	double OccurredTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FEvidenceRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName OffenseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName WitnessType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName WitnessId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName EvidenceState = TEXT("reported");
};

USTRUCT(BlueprintType)
struct STARGAME_API FCriminalRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName CriminalRecordId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName SubjectType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName SubjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	int32 WantedLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Legal")
	TArray<FName> OffenseIds;
};

USTRUCT(BlueprintType)
struct STARGAME_API FItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ItemType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	int32 StackLimit = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	double MassPerUnitKg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	double VolumePerUnitM3 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	int64 BaseValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FGameplayTagContainer LegalityTags;
};

USTRUCT(BlueprintType)
struct STARGAME_API FItemStackState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName StackId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FGameplayTagContainer Flags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName OwnerFactionId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FContainerState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName OwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName LocationType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName LocationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	double CapacityMassKg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	double CapacityVolumeM3 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	TArray<FItemStackState> Stacks;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCargoTransferRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName SourceContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName DestinationContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ExpectedSourceOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ExpectedDestinationOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	TArray<FName> StackIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName Reason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName LegalContextId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCargoTransferResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName TransferId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	ESystemicActionResult Result = ESystemicActionResult::Rejected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	TArray<FName> MovedStackIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	TArray<FName> CreatedStackIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FString RejectedReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Inventory")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCommodityDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CommodityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int64 BasePrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	double MassPerUnitKg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	double VolumePerUnitM3 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FGameplayTagContainer LegalityTags;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCommodityItemBridge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CommodityItemBridgeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CommodityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName BridgePolicy = TEXT("carried_stack");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	bool bMarketTradable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	bool bCargoHoldStorable = true;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStationMarketState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName MarketId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName MarketProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	TMap<FName, int32> StockByCommodity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	TMap<FName, int32> ReservedStockByCommodity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	TArray<FName> LastTransactionIds;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMarketTransactionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName MarketId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName BuyerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName SellerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CommodityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CommodityItemBridgeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int64 QuotedUnitPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName SourceContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName DestinationContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName DebitAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CreditAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName LegalContextId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMarketTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	ESystemicActionResult Result = ESystemicActionResult::Rejected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int32 AppliedQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int64 UnitPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int32 StockDelta = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	int64 DisplayCreditDelta = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	TArray<FName> LedgerEntryIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName CargoTransferResultId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FString RejectionReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Market")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FStationServiceEndpointDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName ServiceEndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName ServiceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName ProviderFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName CommsEndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName MarketId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName InventoryContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName CreditAccountId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName AccessPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	FName LegalPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Service")
	TArray<FName> PresentationModes;
};

USTRUCT(BlueprintType)
struct STARGAME_API FCommsEndpointDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName EndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName OwnerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName OwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	TArray<FName> AvailableChannels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName AccessPolicyId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMessageDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName MessageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName MessageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName TextKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	TArray<FName> GameplayEffectRefs;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMessageLogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName MessageInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName MessageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName SourceEndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName DeliveryChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	double CreatedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	double ExpiryTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Comms")
	FName SourceEventId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMissionOfferRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName OfferId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName MissionDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName SourceStationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName SourceServiceEndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName IssuerFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName State = TEXT("available");
};

USTRUCT(BlueprintType)
struct STARGAME_API FMissionInstanceState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName MissionInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName MissionDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName OfferId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName OwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName IssuerFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName CurrentState = TEXT("offered");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	TArray<FName> ObjectiveStateIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	TArray<FName> RewardEscrowIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName ObjectiveStateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName MissionInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName ObjectiveType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName TargetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	TArray<FName> RouteSegmentIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName RequiredCargoManifestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Mission")
	FName State = TEXT("inactive");
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemicDecisionInputSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName AuthorityFactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName MarketId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName CargoContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FName LegalContextId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Decision")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLogicalContactTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName TargetShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FMovingFrameTarget LastKnownTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double Confidence = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double LastSeenTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRouteSecuritySnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SnapshotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double WindowStartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double WindowEndTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double SecurityRating = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double PatrolCoverageScore = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double PirateRiskScore = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double RouteValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	int64 ProcessedWatermark = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FPatrolReservationRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName ReservationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName PatrolAssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName FactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName JurisdictionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName State = TEXT("reserved");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double ExpiresAtTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FInterdictionHazardRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName HazardId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName OwnerGroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName TargetShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FRouteSample RouteSample;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName State = TEXT("pending");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	double ExpiresAtTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SaveLoadEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDistressEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName DistressEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName ThreatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FMovingFrameTarget LocationTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> RespondingPatrolReservationIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> MessageInstanceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName State = TEXT("open");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLogicalEncounterRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EncounterType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> ParticipantShipIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> ParticipantGroupIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName InterdictionHazardId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName DistressEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EngagementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName State = TEXT("pending");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	int64 ProcessedWatermark = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	bool bRequiresActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FAbstractEngagementRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EngagementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName AttackerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName DefenderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName OutcomeType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> CargoTransferResultIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> MarketTransactionResultIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> GameplayTransactionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> CreditLedgerEntryIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> OffenseIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> EvidenceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> CriminalRecordIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	TArray<FName> MessageInstanceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	bool bRequiresActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLogicalActorPromotionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName PromotionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName RealizationToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	bool bCanResolveEncounter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedActorBudgetProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName BudgetProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	int32 MaxRealizedActors = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	int32 MaxPromotionsPerTick = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName PriorityPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization", meta = (Units = "cm"))
	double PromotionRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedActorMappingRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName MappingId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName RealizationToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ActorBudgetProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	int32 PromotionPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization", meta = (Units = "cm"))
	double LastObserverDistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName State = TEXT("eligible");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedAIDemotionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName SnapshotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName RealizationToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FShipGoalState GoalState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FMovingFrameTarget TargetFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ThreatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FStargameCoordinateFrame VelocityFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization", meta = (Units = "cm/s"))
	FVector LogicalVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName RecoveryPolicyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedAISteeringIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IntentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ShipInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IntentType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName TargetShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName TargetGroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FMovingFrameTarget TargetFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName RouteSegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName FormationSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName ThreatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName State = TEXT("planned");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedAICommsHook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName HookId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName HookType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName MessageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName SourceShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName TargetShipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Realization")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipDurabilityState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName DurabilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName CombatantId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double Shield = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double Hull = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName State = TEXT("active");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName LastDamageEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	bool bCanSurrender = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	bool bCanEscape = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FThreatRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName ThreatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName AttackerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName DefenderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FMovingFrameTarget LastKnownTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double Severity = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double Confidence = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double ExpiresAtTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName SourceEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDamageEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName DamageEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName SourceCombatantId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName TargetCombatantId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double Amount = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	double AuthorityTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName ResultState = TEXT("pending");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName ThreatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Combat")
	FName IdempotencyKey;
};

USTRUCT(BlueprintType)
struct STARGAME_API FLogicalEncounterResolutionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName ResultEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FName OutcomeType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	bool bDuplicate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic|Encounter")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemicGameplayState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FSimulationEventRecord> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FSimulationEventResultRecord> EventResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FGameplayTransactionRecord> Transactions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FGameplayTransactionJournalEntry> TransactionJournal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCreditAccountRecord> CreditAccounts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCreditLedgerEntry> CreditLedger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FEscrowHoldRecord> EscrowHolds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FFactionDefinition> Factions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FFactionRelationshipRecord> FactionRelationships;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FFactionOperationalState> FactionOperations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FJurisdictionDefinition> Jurisdictions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FOffenseEvent> Offenses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FEvidenceRecord> EvidenceRecords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCriminalRecord> CriminalRecords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FItemDefinition> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FContainerState> Containers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCargoTransferResult> CargoTransferResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCommodityDefinition> Commodities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCommodityItemBridge> CommodityItemBridges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FStationMarketState> Markets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FMarketTransactionResult> MarketTransactionResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FStationServiceEndpointDefinition> StationServiceEndpoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FCommsEndpointDefinition> CommsEndpoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FMessageDefinition> MessageDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FMessageLogEntry> MessageLog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FMissionOfferRecord> MissionOffers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FMissionInstanceState> MissionInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FObjectiveState> ObjectiveStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FLogicalContactTrack> LogicalContactTracks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRouteSecuritySnapshot> RouteSecuritySnapshots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FPatrolReservationRecord> PatrolReservations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FInterdictionHazardRecord> InterdictionHazards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FDistressEventRecord> DistressEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FLogicalEncounterRecord> LogicalEncounters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FAbstractEngagementRecord> AbstractEngagements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FLogicalActorPromotionRecord> ActorPromotionAttachments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRealizedActorBudgetProfile> RealizedActorBudgetProfiles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRealizedActorMappingRecord> RealizedActorMappings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRealizedAIDemotionSnapshot> RealizedDemotionSnapshots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRealizedAISteeringIntent> RealizedSteeringIntents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FRealizedAICommsHook> RealizedCommsHooks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FShipDurabilityState> ShipDurabilityStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FThreatRecord> ThreatRecords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Systemic")
	TArray<FDamageEventRecord> DamageEvents;
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
	TArray<FTrafficRouteSegmentDefinition> TrafficRoutes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FShipTrafficInstance> LogicalTraffic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	TArray<FShipGroupState> ShipGroups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FSystemicGameplayState SystemicGameplay;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FActiveTrafficSimulationState ActiveTrafficState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FSystemicGameplayState SystemicGameplayState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSavedSystemIsGenerated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGeneratedSystemSourceMetadata GeneratedSystemSourceMetadata;
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

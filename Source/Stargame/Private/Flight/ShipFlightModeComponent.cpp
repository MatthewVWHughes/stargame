#include "Flight/ShipFlightModeComponent.h"

UShipFlightModeComponent::UShipFlightModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UShipFlightModeComponent::SetFlightMode(EShipFlightMode NewMode)
{
	FlightMode = NewMode;
	if (NewMode != EShipFlightMode::Supercruise)
	{
		SupercruiseState = ESupercruiseState::Inactive;
		CurrentSupercruiseSpeedCmPerSec = 0.0;
		CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
		StateRemainingSeconds = 0.0;
	}
}

void UShipFlightModeComponent::SetNormalFlightMaxSpeedCmPerSec(double NewMaxSpeedCmPerSec)
{
	NormalFlightMaxSpeedCmPerSec = FMath::Max(1.0, NewMaxSpeedCmPerSec);
	if (FlightMode == EShipFlightMode::Normal)
	{
		CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
	}
}

void UShipFlightModeComponent::ConfigureSupercruiseFromScale(const FStargameScaleContract& Scale)
{
	SupercruiseScale = Scale;
	SetNormalFlightMaxSpeedCmPerSec(Scale.NormalFlightMaxSpeedCmPerSec);
	CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
}

ESupercruiseRequestResult UShipFlightModeComponent::RequestEnterSupercruise(const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target)
{
	LastGravityTelemetry = Gravity;
	LastTargetTelemetry = Target;

	if (FlightMode == EShipFlightMode::Disabled)
	{
		return ESupercruiseRequestResult::DriveDisabled;
	}
	if (SupercruiseState == ESupercruiseState::Spooling || SupercruiseState == ESupercruiseState::Cruising)
	{
		return ESupercruiseRequestResult::AlreadyInSupercruise;
	}
	if (SupercruiseState == ESupercruiseState::DropoutCooldown)
	{
		return ESupercruiseRequestResult::SpoolInterrupted;
	}
	if (Gravity.bInsideLockout)
	{
		return ESupercruiseRequestResult::InsideLockout;
	}
	if (Target.TargetId.IsNone())
	{
		return ESupercruiseRequestResult::NoValidLogicalLocation;
	}
	if (Target.DistanceCm < SupercruiseScale.SupercruiseTargetDropoutMaxRadiusCm)
	{
		return ESupercruiseRequestResult::TargetTooClose;
	}

	FlightMode = EShipFlightMode::Supercruise;
	SupercruiseState = ESupercruiseState::Spooling;
	CurrentSupercruiseSpeedCmPerSec = FMath::Max(CurrentSupercruiseSpeedCmPerSec, SupercruiseScale.SupercruiseMinSpeedCmPerSec);
	CurrentSpeedLimitCmPerSec = FMath::Min(Gravity.SpeedCeilingCmPerSec > 0.0 ? Gravity.SpeedCeilingCmPerSec : SupercruiseScale.SupercruiseMaxSpeedCmPerSec, SupercruiseScale.SupercruiseMaxSpeedCmPerSec);
	StateRemainingSeconds = SupercruiseScale.SupercruiseSpoolSeconds;
	return ESupercruiseRequestResult::Success;
}

void UShipFlightModeComponent::RequestManualDropout()
{
	if (SupercruiseState == ESupercruiseState::Spooling || SupercruiseState == ESupercruiseState::Cruising)
	{
		EnterDropoutCooldown(ESupercruiseState::ManualDropout);
	}
}

void UShipFlightModeComponent::TickSupercruise(double DeltaSeconds, const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target)
{
	const double ClampedDeltaSeconds = FMath::Max(0.0, DeltaSeconds);
	const FSupercruiseTargetTelemetry PreviousTargetTelemetry = LastTargetTelemetry;
	LastGravityTelemetry = Gravity;
	LastTargetTelemetry = Target;

	if (SupercruiseState == ESupercruiseState::Inactive)
	{
		FlightMode = FlightMode == EShipFlightMode::Supercruise ? EShipFlightMode::Normal : FlightMode;
		CurrentSupercruiseSpeedCmPerSec = 0.0;
		CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
		return;
	}

	if (SupercruiseState == ESupercruiseState::DropoutCooldown)
	{
		StateRemainingSeconds = FMath::Max(0.0, StateRemainingSeconds - ClampedDeltaSeconds);
		if (StateRemainingSeconds <= 0.0)
		{
			SupercruiseState = ESupercruiseState::Inactive;
			FlightMode = EShipFlightMode::Normal;
			CurrentSupercruiseSpeedCmPerSec = 0.0;
			CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
		}
		return;
	}

	const bool bCrossedTargetDropoutBand =
		!PreviousTargetTelemetry.TargetId.IsNone() &&
		PreviousTargetTelemetry.TargetId == Target.TargetId &&
		(PreviousTargetTelemetry.ClosingSpeedCmPerSec > 0.0 || Target.ClosingSpeedCmPerSec > 0.0) &&
		((PreviousTargetTelemetry.DistanceCm > SupercruiseScale.SupercruiseTargetDropoutMaxRadiusCm &&
			Target.DistanceCm < SupercruiseScale.SupercruiseTargetDropoutMinRadiusCm) ||
			(PreviousTargetTelemetry.bInsideDropoutBand &&
				Target.DistanceCm < SupercruiseScale.SupercruiseTargetDropoutMinRadiusCm));
	const bool bTargetDropoutBandReached = Target.bInsideDropoutBand || bCrossedTargetDropoutBand;

	const FSupercruiseGuidanceResult Guidance = ComputeSupercruiseGuidance(Gravity, Target);
	if (Guidance.bGravityDropoutRequired || Guidance.bTargetDropoutReady || bTargetDropoutBandReached)
	{
		EnterDropoutCooldown(Guidance.bGravityDropoutRequired ? ESupercruiseState::ForcedDropout : ESupercruiseState::ManualDropout);
		return;
	}

	if (SupercruiseState == ESupercruiseState::Spooling)
	{
		StateRemainingSeconds = FMath::Max(0.0, StateRemainingSeconds - ClampedDeltaSeconds);
		if (StateRemainingSeconds <= 0.0)
		{
			SupercruiseState = ESupercruiseState::Cruising;
		}
	}

	CurrentSpeedLimitCmPerSec = FMath::Clamp(
		Guidance.DesiredSpeedCmPerSec,
		SupercruiseScale.SupercruiseMinSpeedCmPerSec,
		SupercruiseScale.SupercruiseMaxSpeedCmPerSec);

	const double SpeedStep = (CurrentSupercruiseSpeedCmPerSec <= CurrentSpeedLimitCmPerSec
		? SupercruiseScale.SupercruiseAccelerationCmPerSec2
		: SupercruiseScale.SupercruiseBrakingCmPerSec2) * ClampedDeltaSeconds;
	CurrentSupercruiseSpeedCmPerSec = FMath::FInterpConstantTo(CurrentSupercruiseSpeedCmPerSec, CurrentSpeedLimitCmPerSec, ClampedDeltaSeconds, SpeedStep / FMath::Max(ClampedDeltaSeconds, SMALL_NUMBER));
}

FSupercruiseGuidanceResult UShipFlightModeComponent::ComputeSupercruiseGuidance(const FGravityWellQueryResult& Gravity, const FSupercruiseTargetTelemetry& Target) const
{
	FSupercruiseGuidanceResult Guidance;
	Guidance.GravitySpeedLimitCmPerSec = Gravity.SpeedCeilingCmPerSec > 0.0
		? Gravity.SpeedCeilingCmPerSec
		: SupercruiseScale.SupercruiseMaxSpeedCmPerSec;
	Guidance.BrakingSpeedLimitCmPerSec = Target.DistanceToDropoutBandCm > 0.0
		? FMath::Sqrt(2.0 * SupercruiseScale.SupercruiseBrakingCmPerSec2 * Target.DistanceToDropoutBandCm)
		: SupercruiseScale.SupercruiseMinSpeedCmPerSec;
	Guidance.DesiredSpeedCmPerSec = FMath::Clamp(
		FMath::Min(Guidance.GravitySpeedLimitCmPerSec, Guidance.BrakingSpeedLimitCmPerSec),
		SupercruiseScale.SupercruiseMinSpeedCmPerSec,
		SupercruiseScale.SupercruiseMaxSpeedCmPerSec);
	Guidance.bTargetDropoutReady = Target.bApproachReady;
	Guidance.bGravityDropoutRequired = Gravity.bForcedDropout;
	return Guidance;
}

FSupercruiseTelemetry UShipFlightModeComponent::GetSupercruiseTelemetry() const
{
	FSupercruiseTelemetry Telemetry;
	Telemetry.FlightMode = FlightMode;
	Telemetry.SupercruiseState = SupercruiseState;
	Telemetry.CurrentSpeedCmPerSec = CurrentSupercruiseSpeedCmPerSec;
	Telemetry.SpeedLimitCmPerSec = CurrentSpeedLimitCmPerSec;
	Telemetry.StateRemainingSeconds = StateRemainingSeconds;
	Telemetry.Gravity = LastGravityTelemetry;
	Telemetry.Target = LastTargetTelemetry;
	return Telemetry;
}

void UShipFlightModeComponent::EnterDropoutCooldown(ESupercruiseState DropoutReason)
{
	SupercruiseState = DropoutReason;
	FlightMode = EShipFlightMode::Normal;
	CurrentSupercruiseSpeedCmPerSec = FMath::Min(CurrentSupercruiseSpeedCmPerSec, NormalFlightMaxSpeedCmPerSec);
	CurrentSpeedLimitCmPerSec = NormalFlightMaxSpeedCmPerSec;
	StateRemainingSeconds = SupercruiseScale.DropoutCooldownSeconds;
	SupercruiseState = ESupercruiseState::DropoutCooldown;
}

FString UShipFlightModeComponent::GetFlightModeDebugSummary() const
{
	return FString::Printf(
		TEXT("FlightMode=%s\nSupercruiseState=%s\nNormalFlightMaxSpeedCmPerSec=%.0f\nCurrentSupercruiseSpeedCmPerSec=%.0f\nCurrentSpeedLimitCmPerSec=%.0f\nNearestGravityWell=%s\nInsideLockout=%s\nTargetDistanceCm=%.0f"),
		*UEnum::GetValueAsString(FlightMode),
		*UEnum::GetValueAsString(SupercruiseState),
		NormalFlightMaxSpeedCmPerSec,
		CurrentSupercruiseSpeedCmPerSec,
		CurrentSpeedLimitCmPerSec,
		*LastGravityTelemetry.NearestWellId.ToString(),
		LastGravityTelemetry.bInsideLockout ? TEXT("true") : TEXT("false"),
		LastTargetTelemetry.DistanceCm);
}

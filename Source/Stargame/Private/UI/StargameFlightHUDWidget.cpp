#include "UI/StargameFlightHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Flight/SpaceFlightPawn.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"

void UStargameFlightHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	RefreshAccumulatorSeconds = 0.0f;
	RefreshFlightHUD();
}

void UStargameFlightHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulatorSeconds += InDeltaTime;
	if (RefreshAccumulatorSeconds < RefreshIntervalSeconds)
	{
		return;
	}
	RefreshAccumulatorSeconds = 0.0f;
	RefreshFlightHUD();
}

void UStargameFlightHUDWidget::RefreshFlightHUD()
{
	FStargameFlightHUDView View;

	const APlayerController* PlayerController = GetOwningPlayer();
	const ASpaceFlightPawn* FlightPawn = PlayerController ? Cast<ASpaceFlightPawn>(PlayerController->GetPawn()) : nullptr;
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;

	View.bHasFlightPawn = FlightPawn != nullptr;
	View.FlightModeText = TEXT("NO LINK");
	View.SelectedTargetText = TEXT("TARGET NONE");
	View.DockingText = TEXT("DOCKING IDLE");
	View.MissionText = TEXT("MISSION: DOCK AT BRINK WATCH AND TALK TO DISPATCH");
	View.ActionHintText = TEXT("SPACE MOUSE FLIGHT   T TARGET   F INTERACT / DOCK   C SUPERCRUISE");
	View.WeaponText = TEXT("WEAPONS STANDBY");
	View.RadarText = TEXT("RADAR 0 / TRAFFIC 0");

	if (FlightPawn)
	{
		View.SpeedMetersPerSecond = FlightPawn->GetSpeedMetersPerSecond();
		View.AccelerationMetersPerSecondSquared = FlightPawn->GetAccelerationMetersPerSecondSquared();
		View.Throttle01 = FMath::Clamp(static_cast<double>(FlightPawn->GetThrottlePercent()), 0.0, 1.0);
		const FSupercruiseTelemetry Supercruise = FlightPawn->GetSupercruiseTelemetry();
		View.FlightModeText = MakeReadableName(UEnum::GetValueAsName(FlightPawn->GetFlightMode()));
		if (Supercruise.SupercruiseState == ESupercruiseState::Spooling)
		{
			View.FlightModeText = FString::Printf(TEXT("SUPERCRUISE SPOOL %.0fs"), Supercruise.StateRemainingSeconds);
		}
		else if (Supercruise.SupercruiseState == ESupercruiseState::Cruising)
		{
			View.FlightModeText = FString::Printf(TEXT("SUPERCRUISE %.0f km/s"), Supercruise.CurrentSpeedCmPerSec / 100000.0);
		}
		else if (Supercruise.SupercruiseState == ESupercruiseState::DropoutCooldown)
		{
			View.FlightModeText = FString::Printf(TEXT("DROPOUT COOLDOWN %.0fs"), Supercruise.StateRemainingSeconds);
		}
		View.DockingText = MakeDockingText(FlightPawn->GetDockingOperationState());
	}

	SetVisibility(View.bHasFlightPawn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (Session)
	{
		FMissionWaypointViewModel Mission;
		if (Session->GetActiveMissionWaypoint(Mission) && Mission.bHasActiveMission)
		{
			View.MissionText = MakeMissionObjectiveText(Mission);
		}

		const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
		View.WeaponEnergy01 = Encounter.PlayerWeaponMaxEnergy > 0.0
			? FMath::Clamp(Encounter.PlayerWeaponEnergy / Encounter.PlayerWeaponMaxEnergy, 0.0, 1.0)
			: 1.0;
		View.WeaponCooldown01 = Encounter.PlayerWeaponCooldownSeconds > 0.0
			? 1.0 - FMath::Clamp(Encounter.PlayerWeaponCooldownRemainingSeconds / Encounter.PlayerWeaponCooldownSeconds, 0.0, 1.0)
			: 1.0;
		View.bWeaponReady = Encounter.bPlayerWeaponReady && Encounter.bPlayerWeaponEnergyReady && Encounter.bPlayerWeaponAligned;
		const FString WeaponName = Encounter.PlayerWeaponDisplayNameId.IsNone()
			? TEXT("PRIMARY")
			: MakeReadableName(Encounter.PlayerWeaponDisplayNameId);
		View.WeaponText = FString::Printf(TEXT("%s  %s  %s"),
			*WeaponName,
			View.bWeaponReady ? TEXT("READY") : TEXT("CHARGING"),
			Encounter.bPlayerFireSolutionReady ? TEXT("SOLUTION") : TEXT("NO LOCK"));

		const FActiveTrafficSimulationState TrafficState = Session->GetActiveTrafficState();
		View.TrafficShipCount = TrafficState.Ships.Num();
	}

	if (Session && StarSystem && FlightPawn)
	{
		TArray<FNavigationTargetViewModel> Targets;
		StarSystem->BuildNavigationTargetViewModels(
			Session->GetSelectedTargetId(),
			FlightPawn->GetLogicalSystemPositionCm(),
			FlightPawn->GetLinearVelocityCmPerSec(),
			Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds,
			Targets);
		View.NavigationTargetCount = Targets.Num();
		for (const FNavigationTargetViewModel& Target : Targets)
		{
			if (Target.bIsSelected)
			{
				View.SelectedTargetDistanceKm = Target.DistanceCm / 100000.0;
				const FString TargetName = Target.DisplayName.IsEmpty() ? MakeReadableName(Target.TargetId) : Target.DisplayName.ToString().ToUpper();
				View.SelectedTargetText = FString::Printf(TEXT("TARGET %s  %.1f km"), *TargetName, View.SelectedTargetDistanceKm);
				if (Target.TargetType == FName(TEXT("station")))
				{
					View.ActionHintText = Target.bInsideStationApproachBubble
						? TEXT("F REQUEST DOCKING   RMB FIRE")
						: TEXT("C SUPERCRUISE   FLY TO STATION APPROACH RANGE");
				}
				else if (Target.TargetType == FName(TEXT("gate")))
				{
					View.ActionHintText = Target.bInsideGateActivationRange
						? TEXT("F TRANSIT GATE")
						: TEXT("C SUPERCRUISE   FLY TO GATE ACTIVATION RANGE");
				}
				break;
			}
		}
		View.RadarText = FString::Printf(TEXT("RADAR %d TARGETS / %d TRAFFIC"), View.NavigationTargetCount, View.TrafficShipCount);
	}

	CachedView = View;
	ApplyViewToBoundWidgets();
	OnFlightHUDViewChanged(CachedView);
}

FString UStargameFlightHUDWidget::MakeReadableName(FName Name)
{
	FString Text = Name.IsNone() ? FString(TEXT("NONE")) : Name.ToString();
	Text.RemoveFromStart(TEXT("EShipFlightMode::"));
	Text.ReplaceInline(TEXT("_"), TEXT(" "));
	return Text.ToUpper();
}

FString UStargameFlightHUDWidget::MakeDockingText(const FDockingOperationState& Docking)
{
	if (Docking.DockingState == EDockingState::None)
	{
		return TEXT("DOCKING IDLE");
	}
	FString State = UEnum::GetValueAsString(Docking.DockingState);
	State.RemoveFromStart(TEXT("EDockingState::"));
	State = State.ToUpper();
	if (!Docking.StationId.IsNone())
	{
		return FString::Printf(TEXT("DOCKING %s  %s"), *State, *Docking.StationId.ToString().ToUpper());
	}
	return FString::Printf(TEXT("DOCKING %s"), *State);
}

FString UStargameFlightHUDWidget::MakeMissionObjectiveText(const FMissionWaypointViewModel& Mission)
{
	if (Mission.bReadyToTurnIn)
	{
		return TEXT("MISSION: RETURN TO THE CONTACT FOR TURN-IN");
	}

	const FString Target = MakeReadableName(Mission.TargetId);
	if (Mission.ObjectiveType == FName(TEXT("reach_station")))
	{
		return Mission.bTargetResolved
			? FString::Printf(TEXT("MISSION: FLY TO %s  %.1f km"), *Target, Mission.DistanceCm / 100000.0)
			: FString::Printf(TEXT("MISSION: FLY TO %s"), *Target);
	}
	if (Mission.ObjectiveType == FName(TEXT("clear_station_hostiles")) ||
		Mission.ObjectiveType == FName(TEXT("clear_hostiles")) ||
		Mission.ObjectiveType == FName(TEXT("hostile_boarding")) ||
		Mission.ObjectiveType == FName(TEXT("boarding_clearance")))
	{
		return FString::Printf(TEXT("MISSION: BOARD %s AND CLEAR HOSTILES"), *Target);
	}
	if (!Mission.TargetId.IsNone())
	{
		return FString::Printf(TEXT("MISSION: %s AT %s"), *MakeReadableName(Mission.ObjectiveType), *Target);
	}
	return FString::Printf(TEXT("MISSION: %s"), *MakeReadableName(Mission.MissionInstanceId));
}

void UStargameFlightHUDWidget::ApplyViewToBoundWidgets()
{
	if (SpeedText)
	{
		SpeedText->SetText(FText::FromString(FString::Printf(TEXT("SPD %05.0f m/s  ACC %.0f"), CachedView.SpeedMetersPerSecond, CachedView.AccelerationMetersPerSecondSquared)));
	}
	if (ThrottleText)
	{
		ThrottleText->SetText(FText::FromString(FString::Printf(TEXT("THR %03.0f%%"), CachedView.Throttle01 * 100.0)));
	}
	if (FlightModeText)
	{
		FlightModeText->SetText(FText::FromString(CachedView.FlightModeText));
	}
	if (TargetText)
	{
		TargetText->SetText(FText::FromString(CachedView.SelectedTargetText));
	}
	if (DockingText)
	{
		DockingText->SetText(FText::FromString(CachedView.DockingText));
	}
	if (MissionText)
	{
		MissionText->SetText(FText::FromString(CachedView.MissionText));
	}
	if (CommsLineText)
	{
		CommsLineText->SetText(FText::FromString(CachedView.ActionHintText));
	}
	if (WeaponText)
	{
		WeaponText->SetText(FText::FromString(CachedView.WeaponText));
	}
	if (RadarText)
	{
		RadarText->SetText(FText::FromString(CachedView.RadarText));
	}
	if (SystemsText)
	{
		SystemsText->SetText(FText::FromString(TEXT("SHD / HULL / FUEL")));
	}
	if (ThrottleBar)
	{
		ThrottleBar->SetPercent(static_cast<float>(CachedView.Throttle01));
	}
	if (WeaponEnergyBar)
	{
		WeaponEnergyBar->SetPercent(static_cast<float>(CachedView.WeaponEnergy01));
	}
	if (WeaponCooldownBar)
	{
		WeaponCooldownBar->SetPercent(static_cast<float>(CachedView.WeaponCooldown01));
	}
	if (ShieldBar)
	{
		ShieldBar->SetPercent(static_cast<float>(CachedView.Shields01));
	}
	if (HullBar)
	{
		HullBar->SetPercent(static_cast<float>(CachedView.Hull01));
	}
	if (FuelBar)
	{
		FuelBar->SetPercent(static_cast<float>(CachedView.Fuel01));
	}
}

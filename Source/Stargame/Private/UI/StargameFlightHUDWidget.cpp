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
	RefreshFlightHUD();
}

void UStargameFlightHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
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
	View.WeaponText = TEXT("WEAPONS STANDBY");
	View.RadarText = TEXT("RADAR 0 / TRAFFIC 0");

	if (FlightPawn)
	{
		View.SpeedMetersPerSecond = FlightPawn->GetSpeedMetersPerSecond();
		View.AccelerationMetersPerSecondSquared = FlightPawn->GetAccelerationMetersPerSecondSquared();
		View.Throttle01 = FMath::Clamp(static_cast<double>(FlightPawn->GetThrottlePercent()), 0.0, 1.0);
		View.FlightModeText = MakeReadableName(UEnum::GetValueAsName(FlightPawn->GetFlightMode()));
		View.DockingText = MakeDockingText(FlightPawn->GetDockingOperationState());
	}

	SetVisibility(View.bHasFlightPawn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (Session)
	{
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

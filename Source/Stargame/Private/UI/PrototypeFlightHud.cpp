#include "UI/PrototypeFlightHud.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Core/StargamePlayerController.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarStargameDebugCanvasHud(
		TEXT("stargame.DebugCanvasHud"),
		0,
		TEXT("Draw the legacy C++ Canvas flight HUD. Normal HUD presentation is Blueprint-owned."),
		ECVF_Default);

	FString MakeReadableId(FName Id)
	{
		FString Text = Id.IsNone() ? FString(TEXT("NONE")) : Id.ToString();
		Text.ReplaceInline(TEXT("_"), TEXT(" "));
		return Text.ToUpper();
	}

	FString MakeMissionObjectiveLine(FName ObjectiveType, FName TargetId)
	{
		const FString Target = MakeReadableId(TargetId);
		if (ObjectiveType == FName(TEXT("reach_station")))
		{
			return FString::Printf(TEXT("FLY TO %s"), *Target);
		}
		if (ObjectiveType == FName(TEXT("clear_station_hostiles")) ||
			ObjectiveType == FName(TEXT("clear_hostiles")) ||
			ObjectiveType == FName(TEXT("hostile_boarding")) ||
			ObjectiveType == FName(TEXT("boarding_clearance")))
		{
			return FString::Printf(TEXT("BOARD AND CLEAR HOSTILES AT %s"), *Target);
		}
		return FString::Printf(TEXT("%s  %s"), *MakeReadableId(ObjectiveType), *Target);
	}

	FString MakeMissionContactLine(const FDockedMissionContactOption& Contact)
	{
		if (Contact.bCanTurnIn)
		{
			return TEXT("TURN IN READY");
		}
		if (Contact.bCanAccept)
		{
			return TEXT("MISSION AVAILABLE");
		}
		if (Contact.bCanContinue)
		{
			return Contact.ActiveObjectiveTargetId.IsNone()
				? TEXT("MISSION IN PROGRESS")
				: MakeMissionObjectiveLine(Contact.ActiveObjectiveType, Contact.ActiveObjectiveTargetId);
		}
		return MakeReadableId(Contact.InteractionState);
	}
}

void APrototypeFlightHud::DrawHUD()
{
	Super::DrawHUD();

	if (CVarStargameDebugCanvasHud.GetValueOnGameThread() == 0 || !Canvas || !GEngine)
	{
		return;
	}

	const APlayerController* PlayerController = GetOwningPlayerController();
	const ASpaceFlightPawn* FlightPawn = PlayerController
		? Cast<ASpaceFlightPawn>(PlayerController->GetPawn())
		: nullptr;

	if (!FlightPawn)
	{
		return;
	}

	const float SpeedMeters = FlightPawn->GetSpeedMetersPerSecond();
	const float AccelerationMeters = FlightPawn->GetAccelerationMetersPerSecondSquared();
	const float Throttle = FlightPawn->GetThrottlePercent() * 100.0f;

	const float ViewWidth = Canvas->ClipX;
	const float ViewHeight = Canvas->ClipY;
	const float Scale = FMath::Clamp(ViewHeight / 1440.0f, 0.82f, 1.20f);

	TArray<FNavigationTargetViewModel> TargetViewModels;
	BuildHudTargetViewModels(FlightPawn, TargetViewModels);

	DrawTopStrip(ViewWidth, Scale, TargetViewModels);
	DrawCenterSymbology(ViewWidth, ViewHeight, Scale);
	DrawCombatLeadPip(ViewWidth, ViewHeight, Scale);
	DrawFlightCluster(ViewWidth, ViewHeight, Scale, SpeedMeters, AccelerationMeters, Throttle);
	DrawSystemsCluster(ViewWidth, ViewHeight, Scale);
	DrawNavigationTargets(ViewWidth, Scale, TargetViewModels);
	DrawSupercruiseTelemetry(ViewWidth, Scale, FlightPawn->GetSupercruiseTelemetry());
	DrawDockingTelemetry(ViewWidth, Scale, FlightPawn->GetDockingOperationState());
	DrawActiveMissionPanel(ViewWidth, Scale);
	DrawRuntimeEncounterPanel(ViewWidth, ViewHeight, Scale);
	DrawDockedStationPanel(ViewWidth, ViewHeight, Scale);
	DrawRadarReserve(ViewWidth, ViewHeight, Scale);
}

void APrototypeFlightHud::BuildHudTargetViewModels(const ASpaceFlightPawn* FlightPawn, TArray<FNavigationTargetViewModel>& OutTargets) const
{
	OutTargets.Reset();

	UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem || !Session || !FlightPawn)
	{
		return;
	}

	StarSystem->BuildNavigationTargetViewModels(
		Session->GetSelectedTargetId(),
		FlightPawn->GetLogicalSystemPositionCm(),
		FlightPawn->GetLinearVelocityCmPerSec(),
		Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds,
		OutTargets);
}

void APrototypeFlightHud::DrawTopStrip(float ViewWidth, float Scale, const TArray<FNavigationTargetViewModel>& Targets)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.95f);
	const FLinearColor SoftCyan(0.72f, 0.94f, 1.0f, 0.34f);
	const FLinearColor Yellow(1.0f, 0.86f, 0.30f, 0.92f);

	const float CenterX = ViewWidth * 0.5f;
	const float Top = 22.0f * Scale;
	const float Wing = 156.0f * Scale;
	const float Gap = 76.0f * Scale;

	DrawText(TEXT("FRONTIER TEST FLIGHT"), Cyan, CenterX - 120.0f * Scale, Top, GEngine->GetMediumFont(), Scale, false);
	DrawLine(CenterX - Gap - Wing, Top + 26.0f * Scale, CenterX - Gap, Top + 26.0f * Scale, SoftCyan, 1.6f * Scale);
	DrawLine(CenterX + Gap, Top + 26.0f * Scale, CenterX + Gap + Wing, Top + 26.0f * Scale, SoftCyan, 1.6f * Scale);

	FString TargetLabel = TEXT("TARGET  NONE");
	for (const FNavigationTargetViewModel& Target : Targets)
	{
		if (Target.bIsSelected)
		{
			TargetLabel = FString::Printf(TEXT("TARGET  %s  %.1f km"), *Target.TargetId.ToString().ToUpper(), Target.DistanceCm / 100000.0);
			break;
		}
	}
	DrawText(TargetLabel, Yellow, ViewWidth - 276.0f * Scale, Top + 3.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawText(TEXT("NAV  FREE FLIGHT"), Cyan, 28.0f * Scale, Top + 3.0f * Scale, GEngine->GetSmallFont(), Scale, false);
}

void APrototypeFlightHud::DrawCenterSymbology(float ViewWidth, float ViewHeight, float Scale)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.78f);
	const FLinearColor SoftCyan(0.72f, 0.94f, 1.0f, 0.28f);

	const float CenterX = ViewWidth * 0.5f;
	const float CenterY = ViewHeight * 0.5f;
	const float Gap = 9.0f * Scale;
	const float Arm = 26.0f * Scale;
	const float Bracket = 42.0f * Scale;

	DrawLine(CenterX - Arm, CenterY, CenterX - Gap, CenterY, Cyan, 1.8f * Scale);
	DrawLine(CenterX + Gap, CenterY, CenterX + Arm, CenterY, Cyan, 1.8f * Scale);
	DrawLine(CenterX, CenterY - Arm, CenterX, CenterY - Gap, Cyan, 1.8f * Scale);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Arm, Cyan, 1.8f * Scale);

	DrawLine(CenterX - Bracket, CenterY - Bracket * 0.55f, CenterX - Bracket, CenterY - Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX - Bracket, CenterY - Bracket, CenterX - Bracket * 0.55f, CenterY - Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX + Bracket, CenterY - Bracket * 0.55f, CenterX + Bracket, CenterY - Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX + Bracket, CenterY - Bracket, CenterX + Bracket * 0.55f, CenterY - Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX - Bracket, CenterY + Bracket * 0.55f, CenterX - Bracket, CenterY + Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX - Bracket, CenterY + Bracket, CenterX - Bracket * 0.55f, CenterY + Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX + Bracket, CenterY + Bracket * 0.55f, CenterX + Bracket, CenterY + Bracket, SoftCyan, 1.2f * Scale);
	DrawLine(CenterX + Bracket, CenterY + Bracket, CenterX + Bracket * 0.55f, CenterY + Bracket, SoftCyan, 1.2f * Scale);
}

void APrototypeFlightHud::DrawCombatLeadPip(float ViewWidth, float ViewHeight, float Scale)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		return;
	}

	const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
	if (!Encounter.bPlayerWeaponHasLeadPoint || Encounter.PlayerWeaponLeadPointCm.IsNearlyZero())
	{
		return;
	}

	const FVector ScreenPosition = Project(Encounter.PlayerWeaponLeadPointCm);
	if (ScreenPosition.Z <= 0.0f)
	{
		return;
	}

	const float Margin = 18.0f * Scale;
	const float X = FMath::Clamp(ScreenPosition.X, Margin, ViewWidth - Margin);
	const float Y = FMath::Clamp(ScreenPosition.Y, Margin, ViewHeight - Margin);
	const float Radius = 13.0f * Scale;
	const float Tick = 6.0f * Scale;
	const FLinearColor PipColor = Encounter.bPlayerFireSolutionReady
		? FLinearColor(0.42f, 1.0f, 0.62f, 0.94f)
		: FLinearColor(1.0f, 0.86f, 0.30f, 0.92f);

	DrawLine(X - Radius, Y, X - Tick, Y, PipColor, 1.6f * Scale);
	DrawLine(X + Tick, Y, X + Radius, Y, PipColor, 1.6f * Scale);
	DrawLine(X, Y - Radius, X, Y - Tick, PipColor, 1.6f * Scale);
	DrawLine(X, Y + Tick, X, Y + Radius, PipColor, 1.6f * Scale);
	DrawOutlinedRect(X - Tick, Y - Tick, Tick * 2.0f, Tick * 2.0f, PipColor, 1.0f * Scale);
}

void APrototypeFlightHud::DrawFlightCluster(float ViewWidth, float ViewHeight, float Scale, float SpeedMeters, float AccelerationMeters, float Throttle)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.95f);
	const FLinearColor SoftCyan(0.72f, 0.94f, 1.0f, 0.28f);
	const FLinearColor Yellow(1.0f, 0.86f, 0.30f, 0.95f);

	const float Left = 34.0f * Scale;
	const float Bottom = ViewHeight - 48.0f * Scale;
	const float LadderTop = Bottom - 190.0f * Scale;
	const float LadderX = Left + 20.0f * Scale;

	DrawText(TEXT("SPEED"), Cyan, Left + 36.0f * Scale, LadderTop - 4.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawText(FString::Printf(TEXT("%.0f"), SpeedMeters), Cyan, Left + 34.0f * Scale, LadderTop + 20.0f * Scale, GEngine->GetLargeFont(), Scale * 1.18f, false);
	DrawText(TEXT("m/s"), Cyan, Left + 150.0f * Scale, LadderTop + 52.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawText(FString::Printf(TEXT("ACC %.1f m/s^2"), AccelerationMeters), Cyan, Left + 36.0f * Scale, LadderTop + 94.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawText(FString::Printf(TEXT("THROTTLE %.0f%%"), Throttle), Yellow, Left + 36.0f * Scale, LadderTop + 122.0f * Scale, GEngine->GetSmallFont(), Scale, false);

	DrawLine(LadderX, LadderTop, LadderX, Bottom, SoftCyan, 2.0f * Scale);
	for (int32 Tick = 0; Tick <= 10; ++Tick)
	{
		const float T = static_cast<float>(Tick) / 10.0f;
		const float Y = FMath::Lerp(Bottom, LadderTop, T);
		const float Length = Tick == 5 ? 24.0f * Scale : 14.0f * Scale;
		DrawLine(LadderX, Y, LadderX + Length, Y, SoftCyan, 1.2f * Scale);
	}

	const float ThrottleY = FMath::Lerp(Bottom, LadderTop, FMath::Clamp(Throttle / 100.0f, 0.0f, 1.0f));
	DrawLine(LadderX - 4.0f * Scale, ThrottleY, LadderX + 92.0f * Scale, ThrottleY, Yellow, 3.0f * Scale);
}

void APrototypeFlightHud::DrawSystemsCluster(float ViewWidth, float ViewHeight, float Scale)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.32f);
	const FLinearColor Blue(0.30f, 0.72f, 1.0f, 0.88f);
	const FLinearColor Green(0.42f, 1.0f, 0.62f, 0.88f);
	const FLinearColor Yellow(1.0f, 0.86f, 0.30f, 0.88f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.95f);

	const float Width = 226.0f * Scale;
	const float Height = 13.0f * Scale;
	const float Right = ViewWidth - 38.0f * Scale;
	const float X = Right - Width;
	const float Bottom = ViewHeight - 62.0f * Scale;

	DrawText(TEXT("PRIMARY"), Yellow, X + 72.0f * Scale, Bottom - 132.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawText(TEXT("PULSE LASER"), Text, X + 52.0f * Scale, Bottom - 104.0f * Scale, GEngine->GetSmallFont(), Scale * 1.08f, false);

	DrawText(TEXT("WPN"), Yellow, X - 46.0f * Scale, Bottom - 60.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawBar(X, Bottom - 56.0f * Scale, Width, Height, 1.0f, Yellow, Cyan);
	DrawText(TEXT("SHD"), Blue, X - 46.0f * Scale, Bottom - 30.0f * Scale, GEngine->GetSmallFont(), Scale, false);
	DrawBar(X, Bottom - 26.0f * Scale, Width, Height, 1.0f, Blue, Cyan);
	DrawText(TEXT("HULL"), Green, X - 54.0f * Scale, Bottom, GEngine->GetSmallFont(), Scale, false);
	DrawBar(X, Bottom + 4.0f * Scale, Width, Height, 1.0f, Green, Cyan);
}

void APrototypeFlightHud::DrawNavigationTargets(float ViewWidth, float Scale, const TArray<FNavigationTargetViewModel>& Targets)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.82f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.92f);
	const FLinearColor Selected(1.0f, 0.86f, 0.30f, 0.95f);

	const float X = ViewWidth - 310.0f * Scale;
	float Y = 118.0f * Scale;
	DrawText(TEXT("TARGETS"), Cyan, X, Y, GEngine->GetSmallFont(), Scale, false);
	Y += 26.0f * Scale;

	for (const FNavigationTargetViewModel& Target : Targets)
	{
		const FString Line = FString::Printf(
			TEXT("%s  %s  %.1f km"),
			*Target.TargetType.ToString().ToUpper(),
			*Target.DisplayName.ToString(),
			Target.DistanceCm / 100000.0);
		DrawText(Line, Target.bIsSelected ? Selected : Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
		Y += 22.0f * Scale;
	}
}

void APrototypeFlightHud::DrawSupercruiseTelemetry(float ViewWidth, float Scale, const FSupercruiseTelemetry& Telemetry)
{
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.82f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.92f);
	const FLinearColor Warning(1.0f, 0.38f, 0.28f, 0.95f);

	const float X = 34.0f * Scale;
	float Y = 118.0f * Scale;
	DrawText(TEXT("SUPERCRUISE"), Cyan, X, Y, GEngine->GetSmallFont(), Scale, false);
	Y += 26.0f * Scale;

	DrawText(FString::Printf(TEXT("MODE  %s"), *UEnum::GetValueAsString(Telemetry.SupercruiseState)), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
	Y += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("LIMIT %.0f km/s"), Telemetry.SpeedLimitCmPerSec / 100000.0), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
	Y += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("WELL  %s"), *Telemetry.Gravity.NearestWellId.ToString()), Telemetry.Gravity.bInsideLockout ? Warning : Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
	Y += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("LOCKOUT %s"), Telemetry.Gravity.bInsideLockout ? TEXT("YES") : TEXT("NO")), Telemetry.Gravity.bInsideLockout ? Warning : Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
	Y += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("DROP %.1f km"), Telemetry.Target.DistanceToDropoutBandCm / 100000.0), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
}

void APrototypeFlightHud::DrawDockingTelemetry(float ViewWidth, float Scale, const FDockingOperationState& Docking)
{
	if (Docking.DockingState == EDockingState::None)
	{
		return;
	}

	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.82f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.92f);
	const FLinearColor Warning(1.0f, 0.38f, 0.28f, 0.95f);

	const float X = ViewWidth - 310.0f * Scale;
	float Y = 292.0f * Scale;
	DrawText(TEXT("DOCKING"), Cyan, X, Y, GEngine->GetSmallFont(), Scale, false);
	Y += 26.0f * Scale;
	DrawText(FString::Printf(TEXT("STATE %s"), *UEnum::GetValueAsString(Docking.DockingState)), Docking.DockingState == EDockingState::Aborted ? Warning : Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
	Y += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("PORT  %s/%s"), *Docking.StationId.ToString(), *Docking.PortId.ToString()), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
}

void APrototypeFlightHud::DrawActiveMissionPanel(float ViewWidth, float Scale)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		return;
	}

	FMissionWaypointViewModel Mission;
	if (!Session->GetActiveMissionWaypoint(Mission) || !Mission.bHasActiveMission)
	{
		return;
	}

	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.82f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.92f);
	const FLinearColor Selected(1.0f, 0.86f, 0.30f, 0.95f);
	const FLinearColor Green(0.42f, 1.0f, 0.62f, 0.88f);

	const float X = 34.0f * Scale;
	float Y = 260.0f * Scale;
	DrawText(TEXT("MISSION"), Cyan, X, Y, GEngine->GetSmallFont(), Scale, false);
	Y += 26.0f * Scale;

	DrawText(*Mission.MissionInstanceId.ToString(), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.78f, false);
	Y += 22.0f * Scale;

	if (Mission.bReadyToTurnIn)
	{
		DrawText(TEXT("STATUS  RETURN TO MISSION CONTACT"), Green, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
		return;
	}

	if (!Mission.TargetId.IsNone())
	{
		const FString TargetLine = Mission.bTargetResolved
			? FString::Printf(TEXT("TARGET  %s  %.1f km"), *Mission.TargetId.ToString().ToUpper(), Mission.DistanceCm / 100000.0)
			: FString::Printf(TEXT("TARGET  %s"), *Mission.TargetId.ToString().ToUpper());
		DrawText(TargetLine, Mission.bTargetSelected ? Selected : Text, X, Y, GEngine->GetSmallFont(), Scale * 0.9f, false);
		Y += 22.0f * Scale;
		DrawText(MakeMissionObjectiveLine(Mission.ObjectiveType, Mission.TargetId), Text, X, Y, GEngine->GetSmallFont(), Scale * 0.82f, false);
	}
}

void APrototypeFlightHud::DrawDockedStationPanel(float ViewWidth, float ViewHeight, float Scale)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		return;
	}

	FDockedStationCommandPanelView Panel;
	if (!Session->GetDockedStationCommandPanel(Panel) || !Panel.bDocked)
	{
		return;
	}

	const FLinearColor Frame(0.72f, 0.94f, 1.0f, 0.32f);
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.88f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.94f);
	const FLinearColor Yellow(1.0f, 0.86f, 0.30f, 0.95f);
	const FLinearColor Green(0.42f, 1.0f, 0.62f, 0.88f);
	const FLinearColor Warning(1.0f, 0.38f, 0.28f, 0.95f);

	const float Width = 520.0f * Scale;
	const float Height = 548.0f * Scale;
	const float X = (ViewWidth - Width) * 0.5f;
	const float Y = ViewHeight - Height - 34.0f * Scale;
	DrawOutlinedRect(X, Y, Width, Height, Frame, 1.2f * Scale);

	float TextY = Y + 18.0f * Scale;
	DrawText(TEXT("DOCKED STATION"), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale, false);
	TextY += 26.0f * Scale;
	const FString StationName = Panel.StationDisplayName.IsEmpty()
		? Panel.StationId.ToString()
		: Panel.StationDisplayName.ToString();
	DrawText(FString::Printf(TEXT("%s / %s"), *StationName.ToUpper(), *Panel.PortId.ToString()), Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.95f, false);
	TextY += 22.0f * Scale;
	const FString OwnerName = Panel.OwnerFactionDisplayName.IsEmpty() ? TEXT("UNKNOWN") : Panel.OwnerFactionDisplayName.ToString();
	DrawText(FString::Printf(TEXT("OWNER  %s"), *OwnerName.ToUpper()), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.78f, false);
	TextY += 20.0f * Scale;
	DrawText(FString::Printf(TEXT("CREDITS  %lld"), static_cast<long long>(Panel.PlayerCredits)), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.78f, false);
	DrawText(FString::Printf(TEXT("HULL  %.0f/%.0f"), Panel.Hull, Panel.MaxHull), Text, X + 172.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.78f, false);
	TextY += 24.0f * Scale;
	DrawText(FString::Printf(TEXT("SERVICES  %d"), Panel.AvailableServiceCount), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.9f, false);
	DrawText(FString::Printf(TEXT("MARKETS  %d"), Panel.AvailableMarketCount), Text, X + 210.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.9f, false);
	TextY += 24.0f * Scale;
	DrawText(FString::Printf(TEXT("MISSIONS  %d"), Panel.AvailableMissionCount), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.9f, false);
	DrawText(FString::Printf(TEXT("INV  %d/%d"), Panel.PersonalInventorySlotsUsed, Panel.PersonalInventorySlotsMax), Text, X + 210.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.9f, false);

	TextY += 24.0f * Scale;
	DrawText(TEXT("COMMANDS"), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.72f, false);
	TextY += 18.0f * Scale;
	DrawText(TEXT("1 REPAIR   2 REFUEL   3 BUY MARKET ITEM"), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.68f, false);
	TextY += 18.0f * Scale;
	DrawText(TEXT("4 ACCEPT MISSION   5 TURN IN   F ENTER   U LAUNCH"), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.68f, false);
	const int32 CommandLimit = FMath::Min(Panel.Commands.Num(), 5);
	for (int32 CommandIndex = 0; CommandIndex < CommandLimit; ++CommandIndex)
	{
		const FDockedStationCommandOption& Command = Panel.Commands[CommandIndex];
		TextY += 18.0f * Scale;
		const FString Availability = Command.bAvailable ? TEXT("OK") : TEXT("--");
		const FString CommandName = Command.DisplayName.IsEmpty() ? Command.CommandId.ToString() : Command.DisplayName.ToString();
		DrawText(
			FString::Printf(TEXT("[%s] %s"), *Availability, *CommandName.ToUpper()),
			Command.bAvailable ? Text : Warning,
			X + 18.0f * Scale,
			TextY,
			GEngine->GetSmallFont(),
			Scale * 0.68f,
			false);
	}

	FDockedMarketView MarketPanel;
	if (Session->GetDockedMarketView(MarketPanel) && MarketPanel.bAvailable)
	{
		TextY += 24.0f * Scale;
		DrawText(TEXT("MARKET"), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.72f, false);
		DrawText(FString::Printf(TEXT("CREDITS %lld"), static_cast<long long>(MarketPanel.PlayerCredits)), Text, X + 210.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.68f, false);
		const int32 CommodityLimit = FMath::Min(MarketPanel.Commodities.Num(), 4);
		for (int32 CommodityIndex = 0; CommodityIndex < CommodityLimit; ++CommodityIndex)
		{
			const FDockedMarketCommodityView& Commodity = MarketPanel.Commodities[CommodityIndex];
			TextY += 18.0f * Scale;
			const FString CommodityName = Commodity.DisplayName.IsEmpty() ? Commodity.CommodityId.ToString() : Commodity.DisplayName.ToString();
			DrawText(
				FString::Printf(TEXT("%s  %lld CR  STOCK %d/%d  HOLD %d  %s"),
					*CommodityName.ToUpper(),
					static_cast<long long>(Commodity.UnitPrice),
					Commodity.Stock,
					Commodity.MaxStock,
					Commodity.PlayerCargoQuantity + Commodity.PlayerPersonalInventoryQuantity,
					Commodity.bCanBuy ? TEXT("BUY") : TEXT("--")),
				Commodity.bCanBuy ? Text : Warning,
				X + 18.0f * Scale,
				TextY,
				GEngine->GetSmallFont(),
				Scale * 0.62f,
				false);
		}
	}

	FDockedMissionContactPanelView ContactPanel;
	if (Session->GetDockedMissionContactPanel(ContactPanel) && !ContactPanel.Contacts.IsEmpty())
	{
		const FDockedMissionContactOption& Contact = ContactPanel.Contacts[0];
		TextY += 24.0f * Scale;
		const FString ContactName = Contact.DisplayName.IsEmpty() ? Contact.GiverNpcId.ToString() : Contact.DisplayName.ToString();
		DrawText(
			FString::Printf(TEXT("CONTACT  %s"), *ContactName.ToUpper()),
			Cyan,
			X + 18.0f * Scale,
			TextY,
			GEngine->GetSmallFont(),
			Scale * 0.68f,
			false);
		TextY += 18.0f * Scale;
		DrawText(
			Contact.RewardText.IsEmpty()
				? MakeMissionContactLine(Contact)
				: FString::Printf(TEXT("%s  %s"), *MakeMissionContactLine(Contact), *Contact.RewardText.ToUpper()),
			Contact.bCanTurnIn ? Green : Contact.bHasMission ? Text : Warning,
			X + 18.0f * Scale,
			TextY,
			GEngine->GetSmallFont(),
			Scale * 0.68f,
			false);
		if (!Contact.ActiveObjectiveTargetId.IsNone())
		{
			TextY += 18.0f * Scale;
			DrawText(
				MakeMissionObjectiveLine(Contact.ActiveObjectiveType, Contact.ActiveObjectiveTargetId),
				Text,
				X + 18.0f * Scale,
				TextY,
				GEngine->GetSmallFont(),
				Scale * 0.62f,
				false);
		}
	}

	FDockedInventoryEquipmentPanelView InventoryPanel;
	if (Session->GetDockedInventoryEquipmentPanel(InventoryPanel))
	{
		TextY += 22.0f * Scale;
		DrawText(
			FString::Printf(TEXT("LOADOUT  P%d  S%d  INV %d/%d  CARGO %d"),
				InventoryPanel.PlayerLoadout.Num(),
				InventoryPanel.ShipLoadout.Num(),
				InventoryPanel.PersonalInventorySlotsUsed,
				InventoryPanel.PersonalInventorySlotsMax,
				InventoryPanel.ShipCargoStacks),
			Text,
			X + 18.0f * Scale,
			TextY,
			GEngine->GetSmallFont(),
			Scale * 0.62f,
			false);
		for (const FDockedEquipmentSlotView& Slot : InventoryPanel.ShipLoadout)
		{
			if (Slot.SlotId == FName(TEXT("ship_weapon_hardpoint_1")) || Slot.SlotId == FName(TEXT("ship_shield")))
			{
				TextY += 18.0f * Scale;
				const FString ItemName = Slot.EquippedDisplayName.IsEmpty() ? Slot.EquippedItemId.ToString() : Slot.EquippedDisplayName.ToString();
				DrawText(
					FString::Printf(TEXT("%s  %s"), *Slot.SlotId.ToString().ToUpper(), Slot.bOccupied ? *ItemName.ToUpper() : TEXT("EMPTY")),
					Slot.bOccupied ? Text : Warning,
					X + 18.0f * Scale,
					TextY,
					GEngine->GetSmallFont(),
					Scale * 0.58f,
					false);
			}
		}
		const int32 CargoLimit = FMath::Min(InventoryPanel.ShipCargo.Num(), 2);
		for (int32 CargoIndex = 0; CargoIndex < CargoLimit; ++CargoIndex)
		{
			const FDockedInventoryStackView& Stack = InventoryPanel.ShipCargo[CargoIndex];
			TextY += 18.0f * Scale;
			const FString StackName = Stack.DisplayName.IsEmpty() ? Stack.ItemId.ToString() : Stack.DisplayName.ToString();
			DrawText(
				FString::Printf(TEXT("CARGO  %s x%d"), *StackName.ToUpper(), Stack.Quantity),
				Text,
				X + 18.0f * Scale,
				TextY,
				GEngine->GetSmallFont(),
				Scale * 0.58f,
				false);
		}
	}

	const AStargamePlayerController* StargameController = Cast<AStargamePlayerController>(GetOwningPlayerController());
	if (StargameController && StargameController->HasLastDockedStationCommandResult())
	{
		const FDockedStationCommandResult Result = StargameController->GetLastDockedStationCommandResult();
		TextY += 24.0f * Scale;
		DrawText(
			FString::Printf(TEXT("LAST  %s  %s"), *Result.CommandId.ToString(), Result.bAccepted ? TEXT("ACCEPTED") : TEXT("REJECTED")),
			Result.bAccepted ? Green : Warning,
			X + 18.0f * Scale,
			TextY,
			GEngine->GetSmallFont(),
			Scale * 0.78f,
			false);
	}
}

void APrototypeFlightHud::DrawRuntimeEncounterPanel(float ViewWidth, float ViewHeight, float Scale)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		return;
	}

	const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
	if (!Encounter.bHasEncounterActor && !Encounter.bPirateAmbushActive && !Encounter.bPatrolResponseActive && !Encounter.bResolvedEncounter)
	{
		return;
	}

	const FLinearColor Frame(0.72f, 0.94f, 1.0f, 0.32f);
	const FLinearColor Cyan(0.72f, 0.94f, 1.0f, 0.88f);
	const FLinearColor Text(1.0f, 0.93f, 0.72f, 0.94f);
	const FLinearColor Yellow(1.0f, 0.86f, 0.30f, 0.95f);
	const FLinearColor Green(0.42f, 1.0f, 0.62f, 0.88f);
	const FLinearColor Warning(1.0f, 0.38f, 0.28f, 0.95f);

	const float Width = 376.0f * Scale;
	const float Height = Encounter.bResolvedEncounter ? 144.0f * Scale : 372.0f * Scale;
	const float X = (ViewWidth - Width) * 0.5f;
	const float Y = ViewHeight * 0.5f + 92.0f * Scale;
	DrawOutlinedRect(X, Y, Width, Height, Frame, 1.2f * Scale);

	float TextY = Y + 18.0f * Scale;
	DrawText(Encounter.bPirateAmbushActive ? TEXT("PIRATE INTERDICTION") : TEXT("ENCOUNTER"), Encounter.bResolvedEncounter ? Green : Warning, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale, false);
	TextY += 26.0f * Scale;
	DrawText(FString::Printf(TEXT("%s  %.1f km"), *Encounter.EncounterId.ToString().ToUpper(), Encounter.DistanceCm / 100000.0), Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.82f, false);
	TextY += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("ROLE %s  INTENT %s"), *Encounter.Role.ToString().ToUpper(), *Encounter.IntentType.ToString().ToUpper()), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.78f, false);
	TextY += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("BEHAVIOR %s  COMMS %s"), *Encounter.BehaviorVariantId.ToString().ToUpper(), *Encounter.CommsVariantId.ToString().ToUpper()), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.66f, false);
	TextY += 22.0f * Scale;
	if (!Encounter.CommsLine.IsEmpty())
	{
		DrawText(Encounter.CommsLine, Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.68f, false);
		TextY += 22.0f * Scale;
	}
	DrawText(FString::Printf(TEXT("STATE %s  CLOSING %.1f km"), *Encounter.LocalBehaviorStateId.ToString().ToUpper(), Encounter.DistanceTrendCm / 100000.0), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.66f, false);
	TextY += 22.0f * Scale;

	if (Encounter.bResolvedEncounter)
	{
		DrawText(FString::Printf(TEXT("RESOLVED  %s"), *Encounter.OutcomeType.ToString().ToUpper()), Green, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.86f, false);
		return;
	}

	DrawText(FString::Printf(TEXT("TARGET %s  STATE %s"), *Encounter.TargetShipId.ToString().ToUpper(), *Encounter.TargetDurabilityState.ToString().ToUpper()), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.74f, false);
	TextY += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("WEAPON %s  READY %.1fs"), *Encounter.PlayerWeaponItemId.ToString().ToUpper(), Encounter.PlayerWeaponCooldownRemainingSeconds), Encounter.bPlayerWeaponReady ? Green : Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.74f, false);
	TextY += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("CAP %.0f/%.0f  COST %.0f"), Encounter.PlayerWeaponEnergy, Encounter.PlayerWeaponMaxEnergy, Encounter.PlayerWeaponEnergyCost), Encounter.bPlayerWeaponEnergyReady ? Green : Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.66f, false);
	TextY += 22.0f * Scale;
	DrawText(FString::Printf(TEXT("ALIGN %.3f/%.3f"), Encounter.PlayerWeaponAlignmentDot, Encounter.PlayerWeaponMinAlignment), Encounter.bPlayerWeaponAligned ? Green : Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.66f, false);
	TextY += 22.0f * Scale;
	if (!Encounter.PlayerFireSolutionText.IsEmpty())
	{
		DrawText(Encounter.PlayerFireSolutionText.ToUpper(), Encounter.bPlayerFireSolutionReady ? Green : Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.62f, false);
		TextY += 22.0f * Scale;
	}
	if (!Encounter.PlayerWeaponStatId.IsNone())
	{
		DrawText(FString::Printf(TEXT("STAT %s  %.0f DMG  %.1fs"), *Encounter.PlayerWeaponStatId.ToString().ToUpper(), Encounter.PlayerWeaponDamageAmount, Encounter.PlayerWeaponCooldownSeconds), Text, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.66f, false);
		TextY += 22.0f * Scale;
	}
	if (!Encounter.HostileResponseStateId.IsNone())
	{
		DrawText(FString::Printf(TEXT("HOSTILE %s"), *Encounter.HostileResponseStateId.ToString().ToUpper()), Warning, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.74f, false);
		TextY += 22.0f * Scale;
	}
	if (!Encounter.HostileManeuverStateId.IsNone())
	{
		DrawText(FString::Printf(TEXT("RESPONSE %s"), *Encounter.HostileManeuverStateId.ToString().ToUpper()), Warning, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.70f, false);
		TextY += 22.0f * Scale;
	}
	if (!Encounter.ShotPresentationId.IsNone())
	{
		DrawText(FString::Printf(TEXT("SHOT %s  %s"), *Encounter.ShotPresentationType.ToString().ToUpper(), Encounter.bRenderedShotActive ? TEXT("RENDERED") : TEXT("FADED")), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.70f, false);
		TextY += 22.0f * Scale;
	}
	if (!Encounter.ProjectileState.IsNone())
	{
		DrawText(FString::Printf(TEXT("PROJECTILE %s  %.0f/%.0f"), *Encounter.ProjectileState.ToString().ToUpper(), Encounter.ProjectileTravelledCm, Encounter.ProjectileTargetDistanceCm), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.70f, false);
		TextY += 22.0f * Scale;
	}
	if (Encounter.bPlayerWeaponFireAccepted)
	{
		DrawText(FString::Printf(TEXT("FIRED %s  %.0f DMG"), *Encounter.PlayerWeaponItemId.ToString().ToUpper(), Encounter.PlayerWeaponDamageAmount), Yellow, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.74f, false);
		TextY += 26.0f * Scale;
	}
	DrawText(TEXT("6 ESCAPE    7 SURRENDER    8 PATROL    9 FIRE"), Cyan, X + 18.0f * Scale, TextY, GEngine->GetSmallFont(), Scale * 0.70f, false);
}

void APrototypeFlightHud::DrawRadarReserve(float ViewWidth, float ViewHeight, float Scale)
{
	const FLinearColor SoftCyan(0.72f, 0.94f, 1.0f, 0.20f);
	const float CenterX = ViewWidth * 0.5f;
	const float CenterY = ViewHeight - 76.0f * Scale;
	const float Radius = 48.0f * Scale;

	DrawOutlinedRect(CenterX - Radius, CenterY - Radius, Radius * 2.0f, Radius * 2.0f, SoftCyan, 1.0f * Scale);
	DrawLine(CenterX - Radius, CenterY, CenterX + Radius, CenterY, SoftCyan, 1.0f * Scale);
	DrawLine(CenterX, CenterY - Radius, CenterX, CenterY + Radius, SoftCyan, 1.0f * Scale);
	DrawText(TEXT("LOCAL"), SoftCyan, CenterX - 23.0f * Scale, CenterY + Radius + 8.0f * Scale, GEngine->GetSmallFont(), Scale * 0.86f, false);
}

void APrototypeFlightHud::DrawOutlinedRect(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + Width, Y, Color, Thickness);
	DrawLine(X + Width, Y, X + Width, Y + Height, Color, Thickness);
	DrawLine(X + Width, Y + Height, X, Y + Height, Color, Thickness);
	DrawLine(X, Y + Height, X, Y, Color, Thickness);
}

void APrototypeFlightHud::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor, const FLinearColor& FrameColor)
{
	DrawOutlinedRect(X, Y, Width, Height, FrameColor, 1.0f);
	DrawRect(FillColor, X + 2.0f, Y + 2.0f, (Width - 4.0f) * FMath::Clamp(Fraction, 0.0f, 1.0f), Height - 4.0f);
}

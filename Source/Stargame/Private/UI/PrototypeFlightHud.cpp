#include "UI/PrototypeFlightHud.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"

void APrototypeFlightHud::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
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
	DrawFlightCluster(ViewWidth, ViewHeight, Scale, SpeedMeters, AccelerationMeters, Throttle);
	DrawSystemsCluster(ViewWidth, ViewHeight, Scale);
	DrawNavigationTargets(ViewWidth, Scale, TargetViewModels);
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
		FlightPawn->GetActorLocation(),
		FVector::ZeroVector,
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

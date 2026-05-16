#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/HUD.h"
#include "PrototypeFlightHud.generated.h"

UCLASS()
class STARGAME_API APrototypeFlightHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void BuildHudTargetViewModels(const class ASpaceFlightPawn* FlightPawn, TArray<FNavigationTargetViewModel>& OutTargets) const;
	void DrawFlightCluster(float ViewWidth, float ViewHeight, float Scale, float SpeedMeters, float AccelerationMeters, float Throttle);
	void DrawSystemsCluster(float ViewWidth, float ViewHeight, float Scale);
	void DrawNavigationTargets(float ViewWidth, float Scale, const TArray<FNavigationTargetViewModel>& Targets);
	void DrawSupercruiseTelemetry(float ViewWidth, float Scale, const FSupercruiseTelemetry& Telemetry);
	void DrawCenterSymbology(float ViewWidth, float ViewHeight, float Scale);
	void DrawTopStrip(float ViewWidth, float Scale, const TArray<FNavigationTargetViewModel>& Targets);
	void DrawRadarReserve(float ViewWidth, float ViewHeight, float Scale);
	void DrawOutlinedRect(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness = 1.0f);
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor, const FLinearColor& FrameColor);
};

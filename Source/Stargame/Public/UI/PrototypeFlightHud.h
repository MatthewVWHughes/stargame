#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PrototypeFlightHud.generated.h"

UCLASS()
class STARGAME_API APrototypeFlightHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawFlightCluster(float ViewWidth, float ViewHeight, float Scale, float SpeedMeters, float AccelerationMeters, float Throttle);
	void DrawSystemsCluster(float ViewWidth, float ViewHeight, float Scale);
	void DrawNavigationTargets(float ViewWidth, float Scale);
	void DrawCenterSymbology(float ViewWidth, float ViewHeight, float Scale);
	void DrawTopStrip(float ViewWidth, float Scale);
	void DrawRadarReserve(float ViewWidth, float ViewHeight, float Scale);
	void DrawOutlinedRect(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness = 1.0f);
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor, const FLinearColor& FrameColor);
};

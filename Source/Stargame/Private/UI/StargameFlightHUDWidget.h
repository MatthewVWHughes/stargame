#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/StargameDataTypes.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "StargameFlightHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

USTRUCT(BlueprintType)
struct FStargameFlightHUDView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	bool bHasFlightPawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double SpeedMetersPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double AccelerationMetersPerSecondSquared = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double Throttle01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString FlightModeText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString SelectedTargetText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double SelectedTargetDistanceKm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString DockingText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString MissionText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString ActionHintText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString WeaponText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double WeaponEnergy01 = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double WeaponCooldown01 = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	bool bWeaponReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	int32 NavigationTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	int32 TrafficShipCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString RadarText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double Hull01 = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double Shields01 = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double Fuel01 = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	FString EnvironmentText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double StellarHazard01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double HeatLoad01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Flight HUD")
	double RadiationLoad01 = 0.0;
};

UCLASS()
class UStargameFlightHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Stargame|Flight HUD")
	FStargameFlightHUDView GetCachedFlightHUDView() const { return CachedView; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Flight HUD")
	void RefreshFlightHUD();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Flight HUD")
	void OnFlightHUDViewChanged(const FStargameFlightHUDView& View);

private:
	static FString MakeReadableName(FName Name);
	static FString MakeDockingText(const FDockingOperationState& Docking);
	static FString MakeMissionObjectiveText(const FMissionWaypointViewModel& Mission);
	void ApplyViewToBoundWidgets();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ThrottleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> FlightModeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TargetText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DockingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MissionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CommsLineText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> WeaponText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> RadarText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SystemsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ThrottleBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> WeaponEnergyBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> WeaponCooldownBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> HullBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> FuelBar;

	UPROPERTY()
	FStargameFlightHUDView CachedView;

	float RefreshAccumulatorSeconds = 0.0f;
	static constexpr float RefreshIntervalSeconds = 0.10f;
};

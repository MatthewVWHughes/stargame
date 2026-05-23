#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Pawn.h"
#include "SpaceFlightPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UShipFlightModeComponent;
class UStarSystemSubsystem;
class UStargameSessionSubsystem;

UENUM(BlueprintType)
enum class EStellarHazardBand : uint8
{
	Safe UMETA(DisplayName = "Safe"),
	Caution UMETA(DisplayName = "Caution"),
	Damage UMETA(DisplayName = "Damage"),
	Extreme UMETA(DisplayName = "Extreme"),
	Kill UMETA(DisplayName = "Kill"),
};

USTRUCT(BlueprintType)
struct STARGAME_API FStellarEnvironmentTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	EStellarHazardBand HazardBand = EStellarHazardBand::Safe;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	FName StarId;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	double DistanceInStarRadii = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	double Hazard01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	double HeatLoad01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	double RadiationLoad01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Flight|Environment")
	FString WarningText;
};

UCLASS()
class STARGAME_API ASpaceFlightPawn : public APawn
{
	GENERATED_BODY()

public:
	ASpaceFlightPawn();

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetSpeedCentimetersPerSecond() const { return LinearVelocity.Size(); }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetSpeedMetersPerSecond() const { return LinearVelocity.Size() / 100.0f; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetAccelerationCentimetersPerSecondSquared() const { return LastAcceleration.Size(); }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetAccelerationMetersPerSecondSquared() const { return LastAcceleration.Size() / 100.0f; }

	UFUNCTION(BlueprintPure, Category = "Flight|Tuning")
	float GetNormalMaxSpeedCentimetersPerSecond() const { return NormalMaxSpeed; }

	UFUNCTION(BlueprintPure, Category = "Flight|Tuning")
	float GetThrustAccelerationCentimetersPerSecondSquared() const { return ThrustAcceleration; }

	UFUNCTION(BlueprintPure, Category = "Flight|Tuning")
	float GetStrafeAccelerationCentimetersPerSecondSquared() const { return StrafeAcceleration; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetThrottlePercent() const { return ThrottlePercent; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	EShipFlightMode GetFlightMode() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FVector GetLinearVelocityCmPerSec() const { return LinearVelocity; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FVector2D GetMouseSteeringInput() const { return MouseSteeringInput; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetForwardSpeedCentimetersPerSecond() const { return ForwardSpeedCmPerSec; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetShipVisualRollDegrees() const { return ShipVisualRotation.Roll; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetCameraRollDegrees() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FVector GetLogicalSystemPositionCm() const { return LogicalSystemPositionCm; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FSupercruiseTelemetry GetSupercruiseTelemetry() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Docking")
	EDockingState GetDockingState() const { return DockingOperation.DockingState; }

	UFUNCTION(BlueprintPure, Category = "Flight|Docking")
	FDockingOperationState GetDockingOperationState() const { return DockingOperation; }

	UFUNCTION(BlueprintPure, Category = "Flight|Docking")
	FString GetDockingDebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FString GetFlightDebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Environment")
	FStellarEnvironmentTelemetry GetStellarEnvironmentTelemetry() const { return StellarEnvironment; }

	UFUNCTION(BlueprintPure, Category = "Flight|Combat")
	float GetShieldFraction() const { return Shield01; }

	UFUNCTION(BlueprintPure, Category = "Flight|Combat")
	float GetHullFraction() const { return Hull01; }

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool RequestDocking(FName StationId, FName PortId);

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool RestoreDockedAt(FName StationId, FName PortId, double SimulationTimeSeconds);

	bool RestoreDockedAt(FName StationId, FName PortId, FName ShipInstanceId, FName ClearanceId, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool Undock();

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void SetFlightTestVelocity(FVector NewVelocityCmPerSec);

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void SetFlightTestTransformAndVelocity(const FTransform& NewTransform, FVector NewVelocityCmPerSec);

	void SetFlightTestLogicalTransformAndActorLocation(const FTransform& NewLogicalTransform, const FVector& NewActorLocationCm, FVector NewVelocityCmPerSec);

	void SetFlightTestInputs(float NewThrottleInput, float NewStrafeRightInput, float NewStrafeUpInput, float NewRollInput, FVector2D NewMouseSteeringInput);

	void TickNormalFlightForTest(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void TickDockingForTest(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Flight|Equipment")
	void ApplyShipEquipmentFlightStats(float NormalMaxSpeedMultiplier, float ThrustAccelerationMultiplier, float StrafeAccelerationMultiplier);

	UFUNCTION(Exec)
	void CycleNavigationTarget();

	UFUNCTION(Exec)
	void RequestSupercruise();

	UFUNCTION(Exec)
	void InteractWithSelectedTarget();

	UFUNCTION(BlueprintCallable, Category = "Flight|Combat")
	bool FirePrimaryWeapon();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void Throttle(float Value);
	void StrafeRight(float Value);
	void StrafeUp(float Value);
	void Roll(float Value);
	void ActivateMouseFlight();
	void ToggleEngineKill();
	void StartPrimaryMouse();
	void StopPrimaryMouse();
	void StartSecondaryMouse();
	void StopSecondaryMouse();
	bool IsSupercruiseUpdateRequired() const;
	void RequestDockingWithSelectedStation(UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem);
	void RequestGateTransitionWithSelectedGate(UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem);
	void UpdateThrottle(float DeltaSeconds);
	void UpdateSteering(float DeltaSeconds);
	void UpdateNormalFlight(float DeltaSeconds);
	void UpdateSupercruise(float DeltaSeconds);
	void UpdateDocking(float DeltaSeconds);
	void UpdateStellarEnvironment(float DeltaSeconds);
	void UpdateShipVisuals(float DeltaSeconds);
	void UpdateCameraResponse(float DeltaSeconds, const FVector& PreviousVelocity);
	FVector2D ReadMouseSteeringInput() const;
	FVector2D ApplySteeringDeadZone(FVector2D RawSteering) const;
	bool IsDockingControlLocked() const;
	void RebuildArcadeVelocityStateFromLinearVelocity();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> ShipVisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> NativeDebugShipMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UShipFlightModeComponent> FlightModeComponent;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.01"))
	float ThrottleRiseRate = 1.45f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.01"))
	float ThrottleFallRate = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float ThrustAcceleration = 30000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float StrafeAcceleration = 16000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float ZeroThrottleBrakeAcceleration = 22000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0", Units = "cm/s"))
	float NormalMaxSpeed = 24000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float SupercruiseActorRepresentationScale = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Flight|Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionBounceRestitution = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Flight|Collision", meta = (ClampMin = "0.0"))
	float CollisionSlideDamping = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "1.0", Units = "deg/s"))
	float SteeringForceDegrees = 115.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "1.0", Units = "deg/s"))
	float RollForceDegrees = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.01"))
	float SteeringResponseExponent = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.0"))
	float SteeringRateInterpSpeed = 9.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.0"))
	float MaxAngularSpeedDegrees = 165.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float CursorDeadZone = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float CursorAxisIsolationDeadZone = 0.14f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CursorAxisIsolationRatio = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Steering", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
	float MaxPitchDegrees = 88.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Movement", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float NormalFlightDragScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Movement", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float FlightAssistLateralDamping = 42000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EngineKillDragScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxStrafeSpeed = 12000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Arcade Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float EngineKillBrakeSpeed = 18000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "1.0"))
	float CameraArmLength = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraRotationLagSpeed = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera")
	FVector CameraTargetOffset = FVector(0.0f, 0.0f, 220.0f);

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraAccelerationArmExtension = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "1.0"))
	float CameraAccelerationReference = 4200.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraResponseInterpSpeed = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraBrakeForwardOffset = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CameraRollInfluence = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals")
	FVector ShipVisualOffset = FVector(0.0f, 0.0f, -70.0f);

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float MaxVisualBankDegrees = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float MouseTurnBankDegrees = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualBankStrafeDegrees = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualPitchThrottleDegrees = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualRotationInterpSpeed = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "1.0"))
	float StellarCautionRadiusMultiplier = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "1.0"))
	float StellarDamageRadiusMultiplier = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "1.0"))
	float StellarExtremeRadiusMultiplier = 16.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "1.0"))
	float StellarKillRadiusMultiplier = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "0.0"))
	float StellarShieldDamagePerSecond = 0.055f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "0.0"))
	float StellarHullDamagePerSecond = 0.035f;

	UPROPERTY(EditAnywhere, Category = "Flight|Environment", meta = (ClampMin = "0.05"))
	float StellarDamageTickIntervalSeconds = 0.25f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float ThrottlePercent = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LogicalSystemPositionCm = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LastAcceleration = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float ForwardSpeedCmPerSec = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LocalSlipVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float BaseThrustAcceleration = 30000.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float BaseStrafeAcceleration = 16000.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float BaseNormalMaxSpeed = 24000.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FRotator AngularVelocityDegrees = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector2D MouseSteeringInput = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Docking")
	FDockingOperationState DockingOperation;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Docking")
	FTransform DockingAssistStartLocalTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Docking")
	FTransform DockingAssistTargetLocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = "Flight|Docking", meta = (ClampMin = "0.1", Units = "s"))
	float DockingFinalAssistSeconds = 2.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float ThrottleInput = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float StrafeRightInput = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float StrafeUpInput = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float RollInput = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	bool bSteeringEnabled = true;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	bool bEngineKill = false;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	bool bPrimaryMouseHeld = false;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	bool bSecondaryMouseHeld = false;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FRotator ShipVisualRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float CurrentCameraArmLength = 650.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector CurrentCameraTargetOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Environment")
	FStellarEnvironmentTelemetry StellarEnvironment;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Combat")
	float Shield01 = 1.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|Combat")
	float Hull01 = 1.0f;

	float StellarEnvironmentDamageAccumulator = 0.0f;

};

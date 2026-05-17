#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Pawn.h"
#include "SpaceFlightPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UShipFlightModeComponent;

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

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	float GetThrottlePercent() const { return ThrottlePercent; }

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	EShipFlightMode GetFlightMode() const;

	UFUNCTION(BlueprintPure, Category = "Flight|Telemetry")
	FVector GetLinearVelocityCmPerSec() const { return LinearVelocity; }

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

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool RequestDocking(FName StationId, FName PortId);

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool RestoreDockedAt(FName StationId, FName PortId, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Flight|Docking")
	bool Undock();

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void SetFlightTestVelocity(FVector NewVelocityCmPerSec);

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void SetFlightTestTransformAndVelocity(const FTransform& NewTransform, FVector NewVelocityCmPerSec);

	UFUNCTION(BlueprintCallable, Category = "Flight|Testing")
	void TickDockingForTest(float DeltaSeconds);

	UFUNCTION(Exec)
	void CycleNavigationTarget();

	UFUNCTION(Exec)
	void RequestSupercruise();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void Throttle(float Value);
	void StrafeRight(float Value);
	void StrafeUp(float Value);
	void Roll(float Value);
	void ToggleSteering();
	void ToggleEngineKill();
	void StartPrimaryMouse();
	void StopPrimaryMouse();
	void StartSecondaryMouse();
	void StopSecondaryMouse();
	void UpdateThrottle(float DeltaSeconds);
	void UpdateSteering(float DeltaSeconds);
	void UpdateNormalFlight(float DeltaSeconds);
	void UpdateSupercruise(float DeltaSeconds);
	void UpdateDocking(float DeltaSeconds);
	void UpdateShipVisuals(float DeltaSeconds);
	void UpdateCameraResponse(float DeltaSeconds, const FVector& PreviousVelocity);
	void InitializeAtmosphericDust();
	void UpdateAtmosphericDust(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> AtmosphericDust;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UShipFlightModeComponent> FlightModeComponent;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.01"))
	float ThrottleRiseRate = 1.45f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.01"))
	float ThrottleFallRate = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float ThrustAcceleration = 16800.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float StrafeAcceleration = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float ZeroThrottleBrakeAcceleration = 4800.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float NormalMaxSpeed = 24000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float SupercruiseActorRepresentationScale = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Flight|Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionBounceRestitution = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Flight|Collision", meta = (ClampMin = "0.0"))
	float CollisionSlideDamping = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float SteeringForceDegrees = 780.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "1.0"))
	float RollForceDegrees = 780.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.0"))
	float AngularDrag = 6.5f;

	UPROPERTY(EditAnywhere, Category = "Flight|Tuning", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float CursorDeadZone = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "1.0"))
	float CameraArmLength = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraRotationLagSpeed = 14.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera")
	FVector CameraTargetOffset = FVector(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraAccelerationArmExtension = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "1.0"))
	float CameraAccelerationReference = 4200.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraResponseInterpSpeed = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Camera", meta = (ClampMin = "0.0"))
	float CameraBrakeForwardOffset = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals")
	FVector ShipVisualOffset = FVector(0.0f, 0.0f, -70.0f);

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float MaxVisualBankDegrees = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualBankYawFactor = 0.075f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualBankStrafeDegrees = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualPitchThrottleDegrees = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Visuals", meta = (ClampMin = "0.0"))
	float VisualRotationInterpSpeed = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "0"))
	int32 AtmosphericDustCount = 90;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "1.0", Units = "cm/s"))
	float AtmosphericDustMinSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "1.0", Units = "cm"))
	float AtmosphericDustForwardRange = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "1.0", Units = "cm"))
	float AtmosphericDustRearRange = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "1.0", Units = "cm"))
	float AtmosphericDustHorizontalSpread = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "1.0", Units = "cm"))
	float AtmosphericDustVerticalSpread = 560.0f;

	UPROPERTY(EditAnywhere, Category = "Flight|Local Particles", meta = (ClampMin = "0.0"))
	float AtmosphericDustTravelMultiplier = 1.25f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	float ThrottlePercent = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LogicalSystemPositionCm = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FVector LastAcceleration = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Flight|State")
	FRotator AngularVelocityDegrees = FRotator::ZeroRotator;

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

	UPROPERTY()
	TArray<FVector> AtmosphericDustPositions;

	UPROPERTY()
	TArray<float> AtmosphericDustSizeFactors;
};

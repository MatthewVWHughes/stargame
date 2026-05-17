#include "Flight/SpaceFlightPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Flight/ShipFlightModeComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ASpaceFlightPawn::ASpaceFlightPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	FlightModeComponent = CreateDefaultSubobject<UShipFlightModeComponent>(TEXT("FlightModeComponent"));

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->SetBoxExtent(FVector(140.0f, 52.0f, 36.0f));
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionRoot->SetCollisionObjectType(ECC_Pawn);
	CollisionRoot->SetCollisionResponseToAllChannels(ECR_Block);

	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(CollisionRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShipMesh->SetRelativeLocation(ShipVisualOffset);
	ShipMesh->SetRelativeScale3D(FVector(2.4f, 0.8f, 0.45f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ShipMesh->SetStaticMesh(CubeMesh.Object);
	}

	AtmosphericDust = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("AtmosphericDust"));
	AtmosphericDust->SetupAttachment(CollisionRoot);
	AtmosphericDust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AtmosphericDust->SetMobility(EComponentMobility::Movable);
	if (CubeMesh.Succeeded())
	{
		AtmosphericDust->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		AtmosphericDust->SetMaterial(0, BasicMaterial.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionRoot);
	CameraBoom->TargetArmLength = CameraArmLength;
	CameraBoom->TargetOffset = CameraTargetOffset;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	CameraBoom->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

EShipFlightMode ASpaceFlightPawn::GetFlightMode() const
{
	return FlightModeComponent ? FlightModeComponent->GetFlightMode() : EShipFlightMode::Normal;
}

FSupercruiseTelemetry ASpaceFlightPawn::GetSupercruiseTelemetry() const
{
	return FlightModeComponent ? FlightModeComponent->GetSupercruiseTelemetry() : FSupercruiseTelemetry();
}

FString ASpaceFlightPawn::GetDockingDebugSummary() const
{
	return FString::Printf(
		TEXT("DockingState=%s\nStation=%s\nPort=%s\nClearance=%s\nFailure=%s"),
		*UEnum::GetValueAsString(DockingOperation.DockingState),
		*DockingOperation.StationId.ToString(),
		*DockingOperation.PortId.ToString(),
		*DockingOperation.ClearanceId.ToString(),
		*DockingOperation.LastFailureReason);
}

void ASpaceFlightPawn::BeginPlay()
{
	Super::BeginPlay();

	CurrentCameraArmLength = CameraArmLength;
	CurrentCameraTargetOffset = CameraTargetOffset;
	LogicalSystemPositionCm = GetActorLocation();
	InitializeAtmosphericDust();

	if (const UWorld* World = GetWorld())
	{
		if (const UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>())
		{
			const FStargameScaleContract Scale = StarSystem->GetActiveScaleContract();
			if (Scale.NormalFlightMaxSpeedCmPerSec > 0.0)
			{
				NormalMaxSpeed = static_cast<float>(Scale.NormalFlightMaxSpeedCmPerSec);
				if (FlightModeComponent)
				{
					FlightModeComponent->SetNormalFlightMaxSpeedCmPerSec(Scale.NormalFlightMaxSpeedCmPerSec);
					FlightModeComponent->ConfigureSupercruiseFromScale(Scale);
				}
			}
		}
	}

	if (AtmosphericDust)
	{
		UMaterialInstanceDynamic* DustMaterial = AtmosphericDust->CreateAndSetMaterialInstanceDynamic(0);
		if (DustMaterial)
		{
			const FLinearColor DustColor(0.82f, 0.54f, 0.30f, 1.0f);
			DustMaterial->SetVectorParameterValue(TEXT("Color"), DustColor);
			DustMaterial->SetVectorParameterValue(TEXT("BaseColor"), DustColor);
		}
	}
}

void ASpaceFlightPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateThrottle(DeltaSeconds);
	UpdateSteering(DeltaSeconds);
	const FVector PreviousVelocity = LinearVelocity;
	if (DockingOperation.DockingState == EDockingState::FinalAssist ||
		DockingOperation.DockingState == EDockingState::Docked ||
		DockingOperation.DockingState == EDockingState::Undocking)
	{
		UpdateDocking(DeltaSeconds);
	}
	else if (GetFlightMode() == EShipFlightMode::Supercruise)
	{
		UpdateSupercruise(DeltaSeconds);
	}
	else
	{
		UpdateNormalFlight(DeltaSeconds);
	}
	UpdateShipVisuals(DeltaSeconds);
	UpdateCameraResponse(DeltaSeconds, PreviousVelocity);
	UpdateAtmosphericDust(DeltaSeconds);
}

void ASpaceFlightPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Throttle"), this, &ASpaceFlightPawn::Throttle);
	PlayerInputComponent->BindAxis(TEXT("StrafeRight"), this, &ASpaceFlightPawn::StrafeRight);
	PlayerInputComponent->BindAxis(TEXT("StrafeUp"), this, &ASpaceFlightPawn::StrafeUp);
	PlayerInputComponent->BindAxis(TEXT("Roll"), this, &ASpaceFlightPawn::Roll);
	PlayerInputComponent->BindAction(TEXT("ToggleSteering"), IE_Pressed, this, &ASpaceFlightPawn::ToggleSteering);
	PlayerInputComponent->BindAction(TEXT("EngineKill"), IE_Pressed, this, &ASpaceFlightPawn::ToggleEngineKill);
	PlayerInputComponent->BindAction(TEXT("PrimaryMouse"), IE_Pressed, this, &ASpaceFlightPawn::StartPrimaryMouse);
	PlayerInputComponent->BindAction(TEXT("PrimaryMouse"), IE_Released, this, &ASpaceFlightPawn::StopPrimaryMouse);
	PlayerInputComponent->BindAction(TEXT("SecondaryMouse"), IE_Pressed, this, &ASpaceFlightPawn::StartSecondaryMouse);
	PlayerInputComponent->BindAction(TEXT("SecondaryMouse"), IE_Released, this, &ASpaceFlightPawn::StopSecondaryMouse);
	PlayerInputComponent->BindAction(TEXT("CycleTarget"), IE_Pressed, this, &ASpaceFlightPawn::CycleNavigationTarget);
	PlayerInputComponent->BindAction(TEXT("RequestSupercruise"), IE_Pressed, this, &ASpaceFlightPawn::RequestSupercruise);
}

void ASpaceFlightPawn::Throttle(float Value)
{
	ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ASpaceFlightPawn::StrafeRight(float Value)
{
	StrafeRightInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ASpaceFlightPawn::StrafeUp(float Value)
{
	StrafeUpInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ASpaceFlightPawn::Roll(float Value)
{
	RollInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ASpaceFlightPawn::ToggleSteering()
{
	bSteeringEnabled = !bSteeringEnabled;
}

void ASpaceFlightPawn::ToggleEngineKill()
{
	bEngineKill = !bEngineKill;
	if (bEngineKill)
	{
		ThrottlePercent = 0.0f;
	}
}

void ASpaceFlightPawn::StartPrimaryMouse()
{
	bPrimaryMouseHeld = true;
}

void ASpaceFlightPawn::StopPrimaryMouse()
{
	bPrimaryMouseHeld = false;
}

void ASpaceFlightPawn::StartSecondaryMouse()
{
	bSecondaryMouseHeld = true;
}

void ASpaceFlightPawn::StopSecondaryMouse()
{
	bSecondaryMouseHeld = false;
}

void ASpaceFlightPawn::CycleNavigationTarget()
{
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (Session)
	{
		Session->CycleNavigationTarget();
	}
}

void ASpaceFlightPawn::RequestSupercruise()
{
	if (!FlightModeComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem || !Session)
	{
		return;
	}

	const double SimulationTimeSeconds = Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds;
	FGravityWellQueryResult Gravity;
	StarSystem->QueryNearestGravityWell(LogicalSystemPositionCm, LinearVelocity, GetFlightMode(), SimulationTimeSeconds, Gravity);

	FSupercruiseTargetTelemetry Target;
	StarSystem->BuildSupercruiseTargetTelemetry(Session->GetSelectedTargetId(), LogicalSystemPositionCm, LinearVelocity, SimulationTimeSeconds, Target);
	FlightModeComponent->RequestEnterSupercruise(Gravity, Target);
}

bool ASpaceFlightPawn::RequestDocking(FName StationId, FName PortId)
{
	DockingOperation = FDockingOperationState();
	DockingOperation.StationId = StationId;
	DockingOperation.PortId = PortId;
	DockingOperation.DockingState = EDockingState::Requested;

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem)
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Docking requires an active star system.");
		return false;
	}

	FDockingPortRegistryEntry Port;
	if (!StarSystem->FindDockingPort(StationId, PortId, Port))
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Docking port is not registered.");
		return false;
	}

	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;
	const FSimulationClockSnapshot Clock = Session
		? Session->GetSimulationClockSnapshot()
		: UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), SimulationTimeSeconds);
	FFrameResolvedTransform ApproachFrame;
	FFrameResolvedTransform DockedFrame;
	if (!UOrbitRouteFrameQueryService::ResolveDockingPortTransform(StarSystem->GetActiveSystemDefinition(), StationId, PortId, EDockingPortTransformKind::Approach, Clock, SimulationTimeSeconds, ApproachFrame) ||
		!UOrbitRouteFrameQueryService::ResolveDockingPortTransform(StarSystem->GetActiveSystemDefinition(), StationId, PortId, EDockingPortTransformKind::Docked, Clock, SimulationTimeSeconds, DockedFrame))
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Docking port frame could not be resolved.");
		return false;
	}

	const double DistanceToApproachCm = FVector::Distance(LogicalSystemPositionCm, ApproachFrame.PositionCm);
	if (DistanceToApproachCm > Port.Definition.ActivationRangeCm)
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Ship is outside docking activation range.");
		return false;
	}
	if (LinearVelocity.Size() > Port.Definition.MaxApproachSpeedCmPerSec)
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Approach speed exceeds docking limit.");
		return false;
	}

	const double AlignmentDot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal(), DockedFrame.Rotation.Vector().GetSafeNormal());
	const double AlignmentDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(AlignmentDot, -1.0, 1.0)));
	if (AlignmentDegrees > Port.Definition.RequiredAlignmentDegrees)
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Ship alignment exceeds docking tolerance.");
		return false;
	}

	FDockingPortRuntimeState RuntimeState;
	FString FailureReason;
	if (!StarSystem->TryReserveDockingPort(StationId, PortId, DockingOperation.ShipInstanceId, SimulationTimeSeconds, RuntimeState, FailureReason))
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = FailureReason;
		return false;
	}

	const FTransform LivePortTransform(DockedFrame.Rotation, DockedFrame.PositionCm);
	const FTransform LogicalShipTransform(GetActorRotation(), LogicalSystemPositionCm);
	DockingAssistStartLocalTransform = LogicalShipTransform.GetRelativeTransform(LivePortTransform);
	DockingAssistTargetLocalTransform = Port.Definition.DockedTransform.GetRelativeTransform(Port.Definition.DockedTransform);
	DockingOperation.CapturedPortRelativeTransform = DockingAssistStartLocalTransform;
	DockingOperation.CapturedPortRelativeVelocityCmPerSec = LivePortTransform.InverseTransformVectorNoScale(LinearVelocity);
	DockingOperation.ClearanceId = RuntimeState.ClearanceId;
	DockingOperation.DockingState = EDockingState::FinalAssist;
	DockingOperation.StateStartTimeSeconds = SimulationTimeSeconds;
	DockingOperation.LastStateChangeTimeSeconds = SimulationTimeSeconds;
	LinearVelocity = FVector::ZeroVector;
	LastAcceleration = FVector::ZeroVector;
	ThrottlePercent = 0.0f;
	if (FlightModeComponent)
	{
		FlightModeComponent->SetFlightMode(EShipFlightMode::DockingAssist);
	}
	return true;
}

bool ASpaceFlightPawn::RestoreDockedAt(FName StationId, FName PortId, double SimulationTimeSeconds)
{
	return RestoreDockedAt(StationId, PortId, TEXT("player_ship"), NAME_None, SimulationTimeSeconds);
}

bool ASpaceFlightPawn::RestoreDockedAt(FName StationId, FName PortId, FName ShipInstanceId, FName ClearanceId, double SimulationTimeSeconds)
{
	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem)
	{
		return false;
	}

	const FSimulationClockSnapshot Clock = Session
		? Session->GetSimulationClockSnapshot()
		: UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), SimulationTimeSeconds);
	FFrameResolvedTransform DockedFrame;
	if (!UOrbitRouteFrameQueryService::ResolveDockingPortFrame(StarSystem->GetActiveSystemDefinition(), StationId, PortId, Clock, SimulationTimeSeconds, DockedFrame))
	{
		return false;
	}

	FDockingPortRuntimeState RuntimeState;
	FString FailureReason;
	if (!ClearanceId.IsNone())
	{
		FDockingPortRuntimeState SavedRuntimeState;
		SavedRuntimeState.StationId = StationId;
		SavedRuntimeState.PortId = PortId;
		SavedRuntimeState.OccupyingShipId = ShipInstanceId;
		SavedRuntimeState.ClearanceId = ClearanceId;
		SavedRuntimeState.DockingState = EDockingState::Docked;
		if (!StarSystem->RestoreDockingPortRuntimeState(SavedRuntimeState, RuntimeState, FailureReason))
		{
			return false;
		}
	}
	else if (!StarSystem->OccupyDockingPort(StationId, PortId, ShipInstanceId, RuntimeState, FailureReason))
	{
		return false;
	}

	SetActorTransform(FTransform(DockedFrame.Rotation, DockedFrame.PositionCm), false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = DockedFrame.PositionCm;
	LinearVelocity = DockedFrame.LinearVelocityCmPerSec;
	LastAcceleration = FVector::ZeroVector;
	DockingOperation.ShipInstanceId = ShipInstanceId;
	DockingOperation.StationId = StationId;
	DockingOperation.PortId = PortId;
	DockingOperation.DockingState = EDockingState::Docked;
	DockingOperation.ClearanceId = RuntimeState.ClearanceId;
	DockingOperation.CapturedPortRelativeTransform = FTransform::Identity;
	DockingOperation.StateStartTimeSeconds = SimulationTimeSeconds;
	DockingOperation.LastStateChangeTimeSeconds = SimulationTimeSeconds;
	DockingOperation.LastFailureReason.Reset();
	if (FlightModeComponent)
	{
		FlightModeComponent->SetFlightMode(EShipFlightMode::Docked);
	}
	return true;
}

bool ASpaceFlightPawn::Undock()
{
	if (DockingOperation.DockingState != EDockingState::Docked)
	{
		DockingOperation.LastFailureReason = TEXT("Ship is not docked.");
		return false;
	}

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem)
	{
		DockingOperation.LastFailureReason = TEXT("Undock requires an active star system.");
		return false;
	}

	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : DockingOperation.LastStateChangeTimeSeconds;
	const FSimulationClockSnapshot Clock = Session
		? Session->GetSimulationClockSnapshot()
		: UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), SimulationTimeSeconds);
	FFrameResolvedTransform UndockFrame;
	if (!UOrbitRouteFrameQueryService::ResolveDockingPortTransform(StarSystem->GetActiveSystemDefinition(), DockingOperation.StationId, DockingOperation.PortId, EDockingPortTransformKind::Undock, Clock, SimulationTimeSeconds, UndockFrame))
	{
		DockingOperation.LastFailureReason = TEXT("Undock frame could not be resolved.");
		return false;
	}

	SetActorTransform(FTransform(UndockFrame.Rotation, UndockFrame.PositionCm), false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = UndockFrame.PositionCm;
	LinearVelocity = UndockFrame.LinearVelocityCmPerSec;
	LastAcceleration = FVector::ZeroVector;
	StarSystem->ReleaseDockingPort(DockingOperation.StationId, DockingOperation.PortId, DockingOperation.ShipInstanceId);
	DockingOperation.DockingState = EDockingState::None;
	DockingOperation.StationId = NAME_None;
	DockingOperation.PortId = NAME_None;
	DockingOperation.ClearanceId = NAME_None;
	DockingOperation.LastFailureReason.Reset();
	if (FlightModeComponent)
	{
		FlightModeComponent->SetFlightMode(EShipFlightMode::Normal);
	}
	return true;
}

void ASpaceFlightPawn::SetFlightTestVelocity(FVector NewVelocityCmPerSec)
{
	LinearVelocity = NewVelocityCmPerSec;
}

void ASpaceFlightPawn::SetFlightTestTransformAndVelocity(const FTransform& NewTransform, FVector NewVelocityCmPerSec)
{
	SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = NewTransform.GetLocation();
	LinearVelocity = NewVelocityCmPerSec;
	LastAcceleration = FVector::ZeroVector;
}

void ASpaceFlightPawn::TickDockingForTest(float DeltaSeconds)
{
	UpdateDocking(DeltaSeconds);
}

void ASpaceFlightPawn::UpdateThrottle(float DeltaSeconds)
{
	if (DockingOperation.DockingState == EDockingState::FinalAssist ||
		DockingOperation.DockingState == EDockingState::Docked ||
		DockingOperation.DockingState == EDockingState::Undocking)
	{
		ThrottleInput = 0.0f;
		ThrottlePercent = 0.0f;
		return;
	}

	if (bEngineKill)
	{
		return;
	}

	if (ThrottleInput > KINDA_SMALL_NUMBER)
	{
		ThrottlePercent = FMath::Min(1.0f, ThrottlePercent + ThrottleRiseRate * DeltaSeconds);
	}
	else if (ThrottleInput < -KINDA_SMALL_NUMBER)
	{
		ThrottlePercent = FMath::Max(0.0f, ThrottlePercent - ThrottleFallRate * DeltaSeconds);
	}
}

void ASpaceFlightPawn::UpdateSteering(float DeltaSeconds)
{
	if (DockingOperation.DockingState == EDockingState::FinalAssist ||
		DockingOperation.DockingState == EDockingState::Docked ||
		DockingOperation.DockingState == EDockingState::Undocking)
	{
		AngularVelocityDegrees = FRotator::ZeroRotator;
		return;
	}

	if (bSteeringEnabled)
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			int32 ViewportX = 0;
			int32 ViewportY = 0;
			PlayerController->GetViewportSize(ViewportX, ViewportY);

			float MouseX = 0.0f;
			float MouseY = 0.0f;
			if (ViewportX > 0 && ViewportY > 0 && PlayerController->GetMousePosition(MouseX, MouseY))
			{
				FVector2D Steer(
					(MouseX - static_cast<float>(ViewportX) * 0.5f) / (static_cast<float>(FMath::Min(ViewportX, ViewportY)) * 0.5f),
					(MouseY - static_cast<float>(ViewportY) * 0.5f) / (static_cast<float>(FMath::Min(ViewportX, ViewportY)) * 0.5f));

				if (Steer.SizeSquared() > 1.0f)
				{
					Steer.Normalize();
				}

				if (FMath::Abs(Steer.X) < CursorDeadZone)
				{
					Steer.X = 0.0f;
				}
				if (FMath::Abs(Steer.Y) < CursorDeadZone)
				{
					Steer.Y = 0.0f;
				}

				AngularVelocityDegrees.Yaw += Steer.X * SteeringForceDegrees * DeltaSeconds;
				AngularVelocityDegrees.Pitch -= Steer.Y * SteeringForceDegrees * DeltaSeconds;
			}
		}
	}

	AngularVelocityDegrees.Roll += RollInput * RollForceDegrees * DeltaSeconds;
	AngularVelocityDegrees = FMath::RInterpTo(AngularVelocityDegrees, FRotator::ZeroRotator, DeltaSeconds, AngularDrag);

	AddActorLocalRotation(FRotator(
		AngularVelocityDegrees.Pitch * DeltaSeconds,
		AngularVelocityDegrees.Yaw * DeltaSeconds,
		AngularVelocityDegrees.Roll * DeltaSeconds));
}

void ASpaceFlightPawn::UpdateNormalFlight(float DeltaSeconds)
{
	const FVector PreviousVelocity = LinearVelocity;
	FVector TotalAcceleration = FVector::ZeroVector;

	if (!bEngineKill)
	{
		if (ThrottlePercent > 0.001f)
		{
			TotalAcceleration += GetActorForwardVector() * ThrustAcceleration * ThrottlePercent;
		}

		TotalAcceleration += GetActorRightVector() * StrafeRightInput * StrafeAcceleration;
		TotalAcceleration += GetActorUpVector() * StrafeUpInput * StrafeAcceleration;
	}

	const float Speed = LinearVelocity.Size();
	if (Speed > 1.0f)
	{
		const float DragCoefficient = ThrustAcceleration / FMath::Square(FMath::Max(NormalMaxSpeed, 1.0f));
		TotalAcceleration += -LinearVelocity.GetSafeNormal() * DragCoefficient * Speed * Speed;
	}

	LinearVelocity += TotalAcceleration * DeltaSeconds;

	if (!bEngineKill && ThrottlePercent <= 0.001f && LinearVelocity.SizeSquared() > 1.0f)
	{
		LinearVelocity = FMath::VInterpConstantTo(LinearVelocity, FVector::ZeroVector, DeltaSeconds, ZeroThrottleBrakeAcceleration);
	}

	LinearVelocity = LinearVelocity.GetClampedToMaxSize(NormalMaxSpeed);

	const FVector RequestedDeltaCm = LinearVelocity * DeltaSeconds;
	const FVector PreviousActorLocation = GetActorLocation();
	FHitResult Hit;
	AddActorWorldOffset(RequestedDeltaCm, true, &Hit);
	const FVector AppliedActorDeltaCm = GetActorLocation() - PreviousActorLocation;
	LogicalSystemPositionCm += AppliedActorDeltaCm;
	if (Hit.bBlockingHit)
	{
		const FVector IncomingVelocity = LinearVelocity;
		const FVector ReflectedVelocity = FMath::GetReflectionVector(IncomingVelocity, Hit.ImpactNormal) * CollisionBounceRestitution;
		const FVector TangentialVelocity = FVector::VectorPlaneProject(IncomingVelocity, Hit.ImpactNormal) * CollisionSlideDamping;
		LinearVelocity = ReflectedVelocity + TangentialVelocity;

		AddActorWorldOffset(Hit.ImpactNormal * 20.0f, false);
	}

	LastAcceleration = DeltaSeconds > SMALL_NUMBER
		? (LinearVelocity - PreviousVelocity) / DeltaSeconds
		: FVector::ZeroVector;
}

void ASpaceFlightPawn::UpdateSupercruise(float DeltaSeconds)
{
	if (!FlightModeComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;

	FGravityWellQueryResult Gravity;
	if (StarSystem)
	{
		StarSystem->QueryNearestGravityWell(LogicalSystemPositionCm, LinearVelocity, GetFlightMode(), SimulationTimeSeconds, Gravity);
	}

	FSupercruiseTargetTelemetry Target;
	if (StarSystem && Session)
	{
		StarSystem->BuildSupercruiseTargetTelemetry(Session->GetSelectedTargetId(), LogicalSystemPositionCm, LinearVelocity, SimulationTimeSeconds, Target);
	}

	const FVector PreviousVelocity = LinearVelocity;
	FlightModeComponent->TickSupercruise(DeltaSeconds, Gravity, Target);

	if (FlightModeComponent->GetSupercruiseState() == ESupercruiseState::DropoutCooldown ||
		FlightModeComponent->GetSupercruiseState() == ESupercruiseState::Inactive)
	{
		LinearVelocity = LinearVelocity.GetClampedToMaxSize(NormalMaxSpeed);
	}
	else
	{
		LinearVelocity = GetActorForwardVector() * FlightModeComponent->GetCurrentSupercruiseSpeedCmPerSec();
	}

	const FVector LogicalDeltaCm = LinearVelocity * DeltaSeconds;
	LogicalSystemPositionCm += LogicalDeltaCm;
	AddActorWorldOffset(LogicalDeltaCm * SupercruiseActorRepresentationScale, false);
	LastAcceleration = DeltaSeconds > SMALL_NUMBER
		? (LinearVelocity - PreviousVelocity) / DeltaSeconds
		: FVector::ZeroVector;
}

void ASpaceFlightPawn::UpdateDocking(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : DockingOperation.LastStateChangeTimeSeconds + DeltaSeconds;
	if (!Session)
	{
		DockingOperation.LastStateChangeTimeSeconds = SimulationTimeSeconds;
	}

	if (!StarSystem)
	{
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Docking update requires active star system.");
		return;
	}

	FFrameResolvedTransform DockedFrame;
	const FSimulationClockSnapshot Clock = Session
		? Session->GetSimulationClockSnapshot()
		: UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), SimulationTimeSeconds);
	if (!UOrbitRouteFrameQueryService::ResolveDockingPortFrame(StarSystem->GetActiveSystemDefinition(), DockingOperation.StationId, DockingOperation.PortId, Clock, SimulationTimeSeconds, DockedFrame))
	{
		StarSystem->ReleaseDockingPort(DockingOperation.StationId, DockingOperation.PortId, DockingOperation.ShipInstanceId);
		DockingOperation.DockingState = EDockingState::Aborted;
		DockingOperation.LastFailureReason = TEXT("Docking frame could not be resolved during update.");
		return;
	}

	if (DockingOperation.DockingState == EDockingState::Docked)
	{
		SetActorTransform(FTransform(DockedFrame.Rotation, DockedFrame.PositionCm), false, nullptr, ETeleportType::TeleportPhysics);
		LogicalSystemPositionCm = DockedFrame.PositionCm;
		LinearVelocity = DockedFrame.LinearVelocityCmPerSec;
		LastAcceleration = FVector::ZeroVector;
		return;
	}

	if (DockingOperation.DockingState != EDockingState::FinalAssist)
	{
		return;
	}

	const double Alpha = FMath::Clamp((SimulationTimeSeconds - DockingOperation.StateStartTimeSeconds) / FMath::Max(static_cast<double>(DockingFinalAssistSeconds), SMALL_NUMBER), 0.0, 1.0);
	FTransform LocalInterpolated = FTransform::Identity;
	LocalInterpolated.Blend(DockingAssistStartLocalTransform, DockingAssistTargetLocalTransform, static_cast<float>(Alpha));

	const FTransform LivePortTransform(DockedFrame.Rotation, DockedFrame.PositionCm);
	const FTransform NewTransform = LocalInterpolated * LivePortTransform;
	SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = NewTransform.GetLocation();
	LinearVelocity = FVector::ZeroVector;
	LastAcceleration = FVector::ZeroVector;

	if (Alpha >= 1.0 - KINDA_SMALL_NUMBER)
	{
		FDockingPortRuntimeState RuntimeState;
		FString FailureReason;
		if (StarSystem->OccupyDockingPort(DockingOperation.StationId, DockingOperation.PortId, DockingOperation.ShipInstanceId, RuntimeState, FailureReason))
		{
			DockingOperation.DockingState = EDockingState::Docked;
			DockingOperation.LastStateChangeTimeSeconds = SimulationTimeSeconds;
			DockingOperation.CapturedPortRelativeTransform = FTransform::Identity;
			DockingOperation.LastFailureReason.Reset();
			if (FlightModeComponent)
			{
				FlightModeComponent->SetFlightMode(EShipFlightMode::Docked);
			}
		}
		else
		{
			StarSystem->ReleaseDockingPort(DockingOperation.StationId, DockingOperation.PortId, DockingOperation.ShipInstanceId);
			DockingOperation.DockingState = EDockingState::Aborted;
			DockingOperation.LastFailureReason = FailureReason;
			if (FlightModeComponent)
			{
				FlightModeComponent->SetFlightMode(EShipFlightMode::Normal);
			}
		}
	}
}

void ASpaceFlightPawn::UpdateShipVisuals(float DeltaSeconds)
{
	const float SteeringBank = AngularVelocityDegrees.Yaw * VisualBankYawFactor;
	const float StrafeBank = StrafeRightInput * VisualBankStrafeDegrees;
	const float TargetBank = FMath::Clamp(SteeringBank + StrafeBank, -MaxVisualBankDegrees, MaxVisualBankDegrees);
	const float TargetPitch = -ThrottlePercent * VisualPitchThrottleDegrees;

	const FRotator TargetRotation(TargetPitch, 0.0f, TargetBank);
	ShipVisualRotation = FMath::RInterpTo(ShipVisualRotation, TargetRotation, DeltaSeconds, VisualRotationInterpSpeed);

	ShipMesh->SetRelativeLocation(ShipVisualOffset);
	ShipMesh->SetRelativeRotation(ShipVisualRotation);
}

void ASpaceFlightPawn::UpdateCameraResponse(float DeltaSeconds, const FVector& PreviousVelocity)
{
	const FVector WorldAcceleration = DeltaSeconds > SMALL_NUMBER
		? (LinearVelocity - PreviousVelocity) / DeltaSeconds
		: FVector::ZeroVector;
	const FVector LocalAcceleration = GetActorTransform().InverseTransformVectorNoScale(WorldAcceleration);

	const float NormalizedForwardAcceleration = FMath::Clamp(
		LocalAcceleration.X / FMath::Max(CameraAccelerationReference, 1.0f),
		-1.0f,
		1.0f);

	const float TargetArmLength = CameraArmLength + FMath::Max(0.0f, NormalizedForwardAcceleration) * CameraAccelerationArmExtension;
	const FVector TargetOffset = CameraTargetOffset + FVector(
		FMath::Max(0.0f, -NormalizedForwardAcceleration) * CameraBrakeForwardOffset,
		0.0f,
		0.0f);

	CurrentCameraArmLength = FMath::FInterpTo(CurrentCameraArmLength, TargetArmLength, DeltaSeconds, CameraResponseInterpSpeed);
	CurrentCameraTargetOffset = FMath::VInterpTo(CurrentCameraTargetOffset, TargetOffset, DeltaSeconds, CameraResponseInterpSpeed);

	CameraBoom->TargetArmLength = CurrentCameraArmLength;
	CameraBoom->TargetOffset = CurrentCameraTargetOffset;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
}

void ASpaceFlightPawn::InitializeAtmosphericDust()
{
	if (!AtmosphericDust)
	{
		return;
	}

	AtmosphericDust->ClearInstances();
	AtmosphericDustPositions.Reset();
	AtmosphericDustSizeFactors.Reset();

	FRandomStream Random(4711);
	for (int32 Index = 0; Index < AtmosphericDustCount; ++Index)
	{
		const FVector LocalPosition(
			Random.FRandRange(-AtmosphericDustRearRange, AtmosphericDustForwardRange),
			Random.FRandRange(-AtmosphericDustHorizontalSpread, AtmosphericDustHorizontalSpread),
			Random.FRandRange(-AtmosphericDustVerticalSpread, AtmosphericDustVerticalSpread));

		AtmosphericDustPositions.Add(LocalPosition);
		AtmosphericDustSizeFactors.Add(Random.FRandRange(0.55f, 1.55f));
		AtmosphericDust->AddInstance(FTransform(FRotator::ZeroRotator, LocalPosition, FVector::ZeroVector));
	}
}

void ASpaceFlightPawn::UpdateAtmosphericDust(float DeltaSeconds)
{
	if (!AtmosphericDust || AtmosphericDustPositions.Num() == 0)
	{
		return;
	}

	const float Speed = LinearVelocity.Size();
	const float SpeedInfluence = FMath::Clamp(
		(Speed - AtmosphericDustMinSpeed) / FMath::Max(NormalMaxSpeed - AtmosphericDustMinSpeed, 1.0f),
		0.0f,
		1.0f);
	const float Visibility = SpeedInfluence;
	const float Travel = FMath::Max(Speed, AtmosphericDustMinSpeed) * AtmosphericDustTravelMultiplier * DeltaSeconds;
	const float LoopLength = AtmosphericDustForwardRange + AtmosphericDustRearRange;

	for (int32 Index = 0; Index < AtmosphericDustPositions.Num(); ++Index)
	{
		FVector& LocalPosition = AtmosphericDustPositions[Index];
		LocalPosition.X -= Travel;
		while (LocalPosition.X < -AtmosphericDustRearRange)
		{
			LocalPosition.X += LoopLength;
		}

		const float SizeFactor = AtmosphericDustSizeFactors.IsValidIndex(Index) ? AtmosphericDustSizeFactors[Index] : 1.0f;
		const float Length = FMath::Lerp(18.0f, 190.0f, SpeedInfluence) * SizeFactor;
		const float Thickness = FMath::Lerp(0.0f, 2.6f, Visibility) * SizeFactor;
		const FVector Scale(Length / 100.0f, Thickness / 100.0f, Thickness / 100.0f);

		AtmosphericDust->UpdateInstanceTransform(
			Index,
			FTransform(FRotator::ZeroRotator, LocalPosition, Scale),
			false,
			false,
			true);
	}

	AtmosphericDust->MarkRenderStateDirty();
}

#include "Flight/SpaceFlightPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Flight/ShipFlightModeComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStargameFlightInput, Log, All);

namespace
{
	double EstimateStellarLuminosity(FName StellarClass)
	{
		const FString Normalized = StellarClass.ToString().TrimStartAndEnd().ToUpper();
		if (Normalized.Contains(TEXT("BLUE"))) { return 90000.0; }
		if (Normalized.Contains(TEXT("RED"))) { return 1800.0; }
		if (Normalized.Contains(TEXT("WHITE"))) { return 0.012; }
		if (Normalized.StartsWith(TEXT("O"))) { return 30000.0; }
		if (Normalized.StartsWith(TEXT("B"))) { return 800.0; }
		if (Normalized.StartsWith(TEXT("A"))) { return 35.0; }
		if (Normalized.StartsWith(TEXT("F"))) { return 4.0; }
		if (Normalized.StartsWith(TEXT("K"))) { return 0.35; }
		if (Normalized.StartsWith(TEXT("M"))) { return 0.04; }
		if (Normalized.StartsWith(TEXT("L"))) { return 0.003; }
		return 1.0;
	}
}

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

	ShipVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipVisualRoot"));
	ShipVisualRoot->SetupAttachment(CollisionRoot);
	ShipVisualRoot->SetRelativeLocation(ShipVisualOffset);

	NativeDebugShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeDebugShipMesh"));
	NativeDebugShipMesh->SetupAttachment(ShipVisualRoot);
	NativeDebugShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NativeDebugShipMesh->SetGenerateOverlapEvents(false);
	NativeDebugShipMesh->SetCanEverAffectNavigation(false);
	NativeDebugShipMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	NativeDebugShipMesh->SetRelativeScale3D(FVector(2.2f, 1.0f, 0.55f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebugShipMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (DebugShipMesh.Succeeded())
	{
		NativeDebugShipMesh->SetStaticMesh(DebugShipMesh.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionRoot);
	CameraBoom->TargetArmLength = CameraArmLength;
	CameraBoom->TargetOffset = FVector::ZeroVector;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	CameraBoom->SetRelativeLocation(CameraTargetOffset);
	CameraBoom->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));
	CameraBoom->SetUsingAbsoluteRotation(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

EShipFlightMode ASpaceFlightPawn::GetFlightMode() const
{
	return FlightModeComponent ? FlightModeComponent->GetFlightMode() : EShipFlightMode::Normal;
}

float ASpaceFlightPawn::GetCameraRollDegrees() const
{
	return CameraBoom ? CameraBoom->GetComponentRotation().Roll : 0.0f;
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

FString ASpaceFlightPawn::GetFlightDebugSummary() const
{
	const FSupercruiseTelemetry Supercruise = GetSupercruiseTelemetry();
	return FString::Printf(
		TEXT("FlightMode=%s\nSupercruiseState=%s\nSpeedCmPerSec=%.0f\nThrottle=%.0f%%\nLogicalPositionCm=%s\nActorPositionCm=%s\nVelocityCmPerSec=%s\nDockingState=%s"),
		*UEnum::GetValueAsString(GetFlightMode()),
		*UEnum::GetValueAsString(Supercruise.SupercruiseState),
		LinearVelocity.Size(),
		ThrottlePercent * 100.0f,
		*LogicalSystemPositionCm.ToCompactString(),
		*GetActorLocation().ToCompactString(),
		*LinearVelocity.ToCompactString(),
		*UEnum::GetValueAsString(DockingOperation.DockingState));
}

void ASpaceFlightPawn::BeginPlay()
{
	Super::BeginPlay();

	CurrentCameraArmLength = CameraArmLength;
	CurrentCameraTargetOffset = CameraTargetOffset;
	LogicalSystemPositionCm = GetActorLocation();
	BaseThrustAcceleration = ThrustAcceleration;
	BaseStrafeAcceleration = StrafeAcceleration;
	BaseNormalMaxSpeed = NormalMaxSpeed;

	if (const UWorld* World = GetWorld())
	{
		if (const UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>())
		{
			const FStargameScaleContract Scale = StarSystem->GetActiveScaleContract();
			if (Scale.NormalFlightMaxSpeedCmPerSec > 0.0)
			{
				BaseNormalMaxSpeed = static_cast<float>(Scale.NormalFlightMaxSpeedCmPerSec);
				NormalMaxSpeed = BaseNormalMaxSpeed;
				if (FlightModeComponent)
				{
					FlightModeComponent->SetNormalFlightMaxSpeedCmPerSec(Scale.NormalFlightMaxSpeedCmPerSec);
					FlightModeComponent->ConfigureSupercruiseFromScale(Scale);
				}
			}
		}
	}
}

void ASpaceFlightPawn::ApplyShipEquipmentFlightStats(float NormalMaxSpeedMultiplier, float ThrustAccelerationMultiplier, float StrafeAccelerationMultiplier)
{
	const float SafeSpeedMultiplier = FMath::Max(NormalMaxSpeedMultiplier, 0.1f);
	const float SafeThrustMultiplier = FMath::Max(ThrustAccelerationMultiplier, 0.1f);
	const float SafeStrafeMultiplier = FMath::Max(StrafeAccelerationMultiplier, 0.1f);

	NormalMaxSpeed = FMath::Max(1.0f, BaseNormalMaxSpeed * SafeSpeedMultiplier);
	ThrustAcceleration = FMath::Max(1.0f, BaseThrustAcceleration * SafeThrustMultiplier);
	StrafeAcceleration = FMath::Max(1.0f, BaseStrafeAcceleration * SafeStrafeMultiplier);

	if (FlightModeComponent)
	{
		FlightModeComponent->SetNormalFlightMaxSpeedCmPerSec(NormalMaxSpeed);
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
	else if (IsSupercruiseUpdateRequired())
	{
		UpdateSupercruise(DeltaSeconds);
	}
	else
	{
		UpdateNormalFlight(DeltaSeconds);
	}
	UpdateStellarEnvironment(DeltaSeconds);
	UpdateShipVisuals(DeltaSeconds);
	UpdateCameraResponse(DeltaSeconds, PreviousVelocity);
}

bool ASpaceFlightPawn::IsDockingControlLocked() const
{
	return DockingOperation.DockingState == EDockingState::FinalAssist ||
		DockingOperation.DockingState == EDockingState::Docked ||
		DockingOperation.DockingState == EDockingState::Undocking;
}

void ASpaceFlightPawn::RebuildArcadeVelocityStateFromLinearVelocity()
{
	ForwardSpeedCmPerSec = FMath::Max(0.0f, FVector::DotProduct(LinearVelocity, GetActorForwardVector()));
	LocalSlipVelocityCmPerSec = GetActorTransform().InverseTransformVectorNoScale(LinearVelocity);
	LocalSlipVelocityCmPerSec.X = 0.0f;
}

void ASpaceFlightPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Throttle"), this, &ASpaceFlightPawn::Throttle);
	PlayerInputComponent->BindAxis(TEXT("StrafeRight"), this, &ASpaceFlightPawn::StrafeRight);
	PlayerInputComponent->BindAxis(TEXT("StrafeUp"), this, &ASpaceFlightPawn::StrafeUp);
	PlayerInputComponent->BindAxis(TEXT("Roll"), this, &ASpaceFlightPawn::Roll);
	PlayerInputComponent->BindAction(TEXT("ToggleSteering"), IE_Pressed, this, &ASpaceFlightPawn::ActivateMouseFlight);
	PlayerInputComponent->BindAction(TEXT("EngineKill"), IE_Pressed, this, &ASpaceFlightPawn::ToggleEngineKill);
	PlayerInputComponent->BindAction(TEXT("PrimaryMouse"), IE_Pressed, this, &ASpaceFlightPawn::StartPrimaryMouse);
	PlayerInputComponent->BindAction(TEXT("PrimaryMouse"), IE_Released, this, &ASpaceFlightPawn::StopPrimaryMouse);
	PlayerInputComponent->BindAction(TEXT("SecondaryMouse"), IE_Pressed, this, &ASpaceFlightPawn::StartSecondaryMouse);
	PlayerInputComponent->BindAction(TEXT("SecondaryMouse"), IE_Released, this, &ASpaceFlightPawn::StopSecondaryMouse);
	PlayerInputComponent->BindAction(TEXT("CycleTarget"), IE_Pressed, this, &ASpaceFlightPawn::CycleNavigationTarget);
	PlayerInputComponent->BindAction(TEXT("InteractTarget"), IE_Pressed, this, &ASpaceFlightPawn::InteractWithSelectedTarget);
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

void ASpaceFlightPawn::ActivateMouseFlight()
{
	bSteeringEnabled = true;

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetIgnoreMoveInput(false);
	}

	UE_LOG(LogStargameFlightInput, Display, TEXT("Mouse flight enabled."));
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
	FirePrimaryWeapon();
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
	UE_LOG(LogStargameFlightInput, Display, TEXT("RequestSupercruise reached flight pawn. Mode=%s State=%s"),
		*UEnum::GetValueAsString(GetFlightMode()),
		FlightModeComponent ? *UEnum::GetValueAsString(FlightModeComponent->GetSupercruiseState()) : TEXT("NoFlightModeComponent"));

	if (!FlightModeComponent)
	{
		return;
	}

	if (FlightModeComponent->GetSupercruiseState() == ESupercruiseState::Spooling ||
		FlightModeComponent->GetSupercruiseState() == ESupercruiseState::Cruising)
	{
		FlightModeComponent->RequestManualDropout();
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
	const bool bHasSelectedTarget = StarSystem->BuildSupercruiseTargetTelemetry(Session->GetSelectedTargetId(), LogicalSystemPositionCm, LinearVelocity, SimulationTimeSeconds, Target);
	const FStargameScaleContract Scale = StarSystem->GetActiveScaleContract();
	if (!bHasSelectedTarget || Target.TargetId.IsNone() || Target.DistanceCm < Scale.SupercruiseTargetDropoutMaxRadiusCm)
	{
		Target = FSupercruiseTargetTelemetry();
		Target.TargetId = TEXT("free_supercruise");
		Target.DistanceCm = FMath::Max(Scale.SupercruiseTargetDropoutMaxRadiusCm * 4.0, Scale.LocalBubbleRadiusCm);
		Target.DistanceToDropoutBandCm = FMath::Max(Scale.SupercruiseTargetDropoutMaxRadiusCm * 3.0, Scale.LocalBubbleRadiusCm);
		Target.ClosingSpeedCmPerSec = 0.0;
		Target.bInsideDropoutBand = false;
		Target.bApproachReady = false;
		UE_LOG(LogStargameFlightInput, Display, TEXT("Supercruise using free-cruise guidance target."));
	}
	const ESupercruiseRequestResult Result = FlightModeComponent->RequestEnterSupercruise(Gravity, Target);
	if (Result != ESupercruiseRequestResult::Success)
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Supercruise request rejected: %s."), *UEnum::GetValueAsString(Result));
	}
	else
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Supercruise request accepted."));
	}
}

void ASpaceFlightPawn::InteractWithSelectedTarget()
{
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!Session || !StarSystem)
	{
		return;
	}

	if (DockingOperation.DockingState == EDockingState::Docked)
	{
		FDockedStationCommandResult Result;
		const bool bEntered = Session->EnterDockedStationInterior(Result);
		UE_LOG(LogStargameFlightInput, Display, TEXT("Enter station interior %s Station=%s Reason=%s"),
			bEntered ? TEXT("accepted") : TEXT("rejected"),
			*Result.StationId.ToString(),
			*Result.FailureReason);
		return;
	}

	FNavigationTargetDefinition SelectedTarget;
	if (!StarSystem->FindNavigationTarget(Session->GetSelectedTargetId(), SelectedTarget))
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Interact target rejected: no selected navigation target."));
		return;
	}

	if (SelectedTarget.TargetType == FName(TEXT("station")))
	{
		RequestDockingWithSelectedStation(Session, StarSystem);
	}
	else if (SelectedTarget.TargetType == FName(TEXT("gate")))
	{
		RequestGateTransitionWithSelectedGate(Session, StarSystem);
	}
	else
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Interact target ignored for target '%s' type '%s'."),
			*SelectedTarget.TargetId.ToString(),
			*SelectedTarget.TargetType.ToString());
	}
}

bool ASpaceFlightPawn::FirePrimaryWeapon()
{
	if (DockingOperation.DockingState == EDockingState::FinalAssist ||
		DockingOperation.DockingState == EDockingState::Docked ||
		DockingOperation.DockingState == EDockingState::Undocking ||
		GetFlightMode() == EShipFlightMode::Supercruise)
	{
		return false;
	}

	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		return false;
	}

	Session->TryUpdateRuntimeEncounterBehavior();
	const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
	if (Encounter.EncounterId.IsNone())
	{
		return false;
	}

	FShipWeaponFireResult Result;
	const bool bAccepted = Session->FireEquippedShipWeaponAtRuntimeEncounter(Encounter.EncounterId, Result);
	UE_LOG(LogStargameFlightInput, Display, TEXT("Ship primary weapon %s Encounter=%s Weapon=%s Target=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Result.EncounterId.ToString(),
		*Result.WeaponItemId.ToString(),
		*Result.TargetCombatantId.ToString(),
		*Result.FailureReason);
	return bAccepted;
}

bool ASpaceFlightPawn::IsSupercruiseUpdateRequired() const
{
	if (!FlightModeComponent)
	{
		return false;
	}

	const ESupercruiseState SupercruiseState = FlightModeComponent->GetSupercruiseState();
	return SupercruiseState == ESupercruiseState::Spooling ||
		SupercruiseState == ESupercruiseState::Cruising ||
		SupercruiseState == ESupercruiseState::DropoutCooldown;
}

void ASpaceFlightPawn::RequestDockingWithSelectedStation(UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem)
{
	if (!Session || !StarSystem)
	{
		return;
	}

	const FName SelectedTargetId = Session->GetSelectedTargetId();
	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FStationDefinition* Station = SystemDefinition.Stations.FindByPredicate([SelectedTargetId](const FStationDefinition& Candidate)
	{
		return Candidate.StationId == SelectedTargetId || Candidate.NavigationTarget.TargetId == SelectedTargetId;
	});
	if (!Station || Station->DockingPorts.IsEmpty())
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Docking rejected: selected station '%s' has no authored docking port."), *SelectedTargetId.ToString());
		return;
	}

	const FDockingPortDefinition& Port = Station->DockingPorts[0];
	const bool bAccepted = RequestDocking(Station->StationId, Port.PortId);
	UE_LOG(LogStargameFlightInput, Display, TEXT("Docking request %s Station=%s Port=%s State=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Station->StationId.ToString(),
		*Port.PortId.ToString(),
		*UEnum::GetValueAsString(DockingOperation.DockingState),
		*DockingOperation.LastFailureReason);
}

void ASpaceFlightPawn::RequestGateTransitionWithSelectedGate(UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem)
{
	if (!Session || !StarSystem)
	{
		return;
	}

	const FName SelectedTargetId = Session->GetSelectedTargetId();
	TArray<FNavigationTargetViewModel> Targets;
	StarSystem->BuildNavigationTargetViewModels(
		SelectedTargetId,
		LogicalSystemPositionCm,
		LinearVelocity,
		Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds,
		Targets);

	const FNavigationTargetViewModel* SelectedView = Targets.FindByPredicate([SelectedTargetId](const FNavigationTargetViewModel& Target)
	{
		return Target.TargetId == SelectedTargetId;
	});
	if (!SelectedView || !SelectedView->bInsideGateActivationRange)
	{
		UE_LOG(LogStargameFlightInput, Display, TEXT("Gate transition rejected: selected gate '%s' is outside activation range."), *SelectedTargetId.ToString());
		return;
	}

	FGateTransitionRequest Request;
	Request.SourceGateId = SelectedTargetId;
	FGateTransitionResult Result;
	const bool bAccepted = Session->RequestGateTransition(Request, Result);
	UE_LOG(LogStargameFlightInput, Display, TEXT("Gate transition %s Source=%s Destination=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Result.SourceGateId.ToString(),
		*Result.DestinationSystemId.ToString(),
		*Result.FailureReason);
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
		DockingOperation.LastFailureReason = FString::Printf(
			TEXT("Ship is outside docking activation range: distance %.1f cm, activation %.1f cm."),
			DistanceToApproachCm,
			Port.Definition.ActivationRangeCm);
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
	RebuildArcadeVelocityStateFromLinearVelocity();
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
	RebuildArcadeVelocityStateFromLinearVelocity();
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
	RebuildArcadeVelocityStateFromLinearVelocity();
}

void ASpaceFlightPawn::SetFlightTestTransformAndVelocity(const FTransform& NewTransform, FVector NewVelocityCmPerSec)
{
	SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = NewTransform.GetLocation();
	LinearVelocity = NewVelocityCmPerSec;
	LastAcceleration = FVector::ZeroVector;
	RebuildArcadeVelocityStateFromLinearVelocity();
}

void ASpaceFlightPawn::SetFlightTestLogicalTransformAndActorLocation(const FTransform& NewLogicalTransform, const FVector& NewActorLocationCm, FVector NewVelocityCmPerSec)
{
	SetActorTransform(FTransform(NewLogicalTransform.GetRotation(), NewActorLocationCm), false, nullptr, ETeleportType::TeleportPhysics);
	LogicalSystemPositionCm = NewLogicalTransform.GetLocation();
	LinearVelocity = NewVelocityCmPerSec;
	LastAcceleration = FVector::ZeroVector;
	RebuildArcadeVelocityStateFromLinearVelocity();
}

void ASpaceFlightPawn::SetFlightTestInputs(float NewThrottleInput, float NewStrafeRightInput, float NewStrafeUpInput, float NewRollInput, FVector2D NewMouseSteeringInput)
{
	ThrottleInput = FMath::Clamp(NewThrottleInput, -1.0f, 1.0f);
	StrafeRightInput = FMath::Clamp(NewStrafeRightInput, -1.0f, 1.0f);
	StrafeUpInput = FMath::Clamp(NewStrafeUpInput, -1.0f, 1.0f);
	RollInput = FMath::Clamp(NewRollInput, -1.0f, 1.0f);
	MouseSteeringInput = ApplySteeringDeadZone(NewMouseSteeringInput);
}

void ASpaceFlightPawn::TickNormalFlightForTest(float DeltaSeconds)
{
	const FVector PreviousVelocity = LinearVelocity;
	UpdateThrottle(DeltaSeconds);
	UpdateSteering(DeltaSeconds);
	UpdateNormalFlight(DeltaSeconds);
	UpdateShipVisuals(DeltaSeconds);
	UpdateCameraResponse(DeltaSeconds, PreviousVelocity);
}

void ASpaceFlightPawn::TickDockingForTest(float DeltaSeconds)
{
	UpdateDocking(DeltaSeconds);
}

void ASpaceFlightPawn::UpdateThrottle(float DeltaSeconds)
{
	if (IsDockingControlLocked())
	{
		ThrottleInput = 0.0f;
		ThrottlePercent = 0.0f;
		return;
	}

	if (bEngineKill)
	{
		ThrottlePercent = FMath::Max(0.0f, ThrottlePercent - ThrottleFallRate * DeltaSeconds);
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

FVector2D ASpaceFlightPawn::ApplySteeringDeadZone(FVector2D RawSteering) const
{
	if (RawSteering.SizeSquared() > 1.0f)
	{
		RawSteering.Normalize();
	}

	if (FMath::Abs(RawSteering.X) < CursorDeadZone)
	{
		RawSteering.X = 0.0f;
	}
	if (FMath::Abs(RawSteering.Y) < CursorDeadZone)
	{
		RawSteering.Y = 0.0f;
	}

	const float AbsX = FMath::Abs(RawSteering.X);
	const float AbsY = FMath::Abs(RawSteering.Y);
	if (AbsY > CursorDeadZone && (AbsX < CursorAxisIsolationDeadZone || AbsX < AbsY * CursorAxisIsolationRatio))
	{
		RawSteering.X = 0.0f;
	}
	if (AbsX > CursorDeadZone && (AbsY < CursorAxisIsolationDeadZone || AbsY < AbsX * CursorAxisIsolationRatio))
	{
		RawSteering.Y = 0.0f;
	}
	return RawSteering;
}

FVector2D ASpaceFlightPawn::ReadMouseSteeringInput() const
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return MouseSteeringInput;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (ViewportX <= 0 || ViewportY <= 0 || !PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return MouseSteeringInput;
	}

	const float HalfReference = static_cast<float>(FMath::Min(ViewportX, ViewportY)) * 0.5f;
	if (HalfReference <= SMALL_NUMBER)
	{
		return MouseSteeringInput;
	}

	return ApplySteeringDeadZone(FVector2D(
		(MouseX - static_cast<float>(ViewportX) * 0.5f) / HalfReference,
		(MouseY - static_cast<float>(ViewportY) * 0.5f) / HalfReference));
}

void ASpaceFlightPawn::UpdateSteering(float DeltaSeconds)
{
	if (IsDockingControlLocked())
	{
		AngularVelocityDegrees = FRotator::ZeroRotator;
		MouseSteeringInput = FVector2D::ZeroVector;
		return;
	}

	if (bSteeringEnabled)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (!PlayerController->bShowMouseCursor)
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetHideCursorDuringCapture(false);
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PlayerController->SetInputMode(InputMode);
				PlayerController->bShowMouseCursor = true;
			}
		}
		MouseSteeringInput = ReadMouseSteeringInput();
	}

	const auto ShapeAxis = [this](float Axis)
	{
		const float Sign = FMath::Sign(Axis);
		return Sign * FMath::Pow(FMath::Abs(Axis), FMath::Max(SteeringResponseExponent, 0.01f));
	};

	const FRotator TargetAngularVelocity(
		ShapeAxis(MouseSteeringInput.Y) * SteeringForceDegrees,
		ShapeAxis(MouseSteeringInput.X) * SteeringForceDegrees,
		RollInput * RollForceDegrees);
	AngularVelocityDegrees = FMath::RInterpTo(AngularVelocityDegrees, TargetAngularVelocity, DeltaSeconds, SteeringRateInterpSpeed);

	AngularVelocityDegrees.Pitch = FMath::Clamp(AngularVelocityDegrees.Pitch, -MaxAngularSpeedDegrees, MaxAngularSpeedDegrees);
	AngularVelocityDegrees.Yaw = FMath::Clamp(AngularVelocityDegrees.Yaw, -MaxAngularSpeedDegrees, MaxAngularSpeedDegrees);
	AngularVelocityDegrees.Roll = FMath::Clamp(AngularVelocityDegrees.Roll, -MaxAngularSpeedDegrees, MaxAngularSpeedDegrees);

	const FQuat CurrentRotation = GetActorQuat();
	const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(AngularVelocityDegrees.Pitch * DeltaSeconds));
	const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(AngularVelocityDegrees.Yaw * DeltaSeconds));
	const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(AngularVelocityDegrees.Roll * DeltaSeconds));
	SetActorRotation((CurrentRotation * PitchDelta * YawDelta * RollDelta).GetNormalized());
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
		const float DragScale = bEngineKill ? EngineKillDragScale : NormalFlightDragScale;
		const float DragCoefficient = (ThrustAcceleration / FMath::Square(FMath::Max(NormalMaxSpeed, 1.0f))) * DragScale;
		TotalAcceleration += -LinearVelocity.GetSafeNormal() * DragCoefficient * Speed * Speed;
	}

	LinearVelocity += TotalAcceleration * DeltaSeconds;

	if (!bEngineKill)
	{
		FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(LinearVelocity);
		const FVector TargetLocalVelocity(
			FMath::Max(0.0f, LocalVelocity.X),
			StrafeRightInput * MaxStrafeSpeed,
			StrafeUpInput * MaxStrafeSpeed);
		LocalVelocity.Y = FMath::FInterpConstantTo(LocalVelocity.Y, TargetLocalVelocity.Y, DeltaSeconds, FlightAssistLateralDamping);
		LocalVelocity.Z = FMath::FInterpConstantTo(LocalVelocity.Z, TargetLocalVelocity.Z, DeltaSeconds, FlightAssistLateralDamping);
		if (ThrottlePercent <= 0.001f)
		{
			LocalVelocity.X = FMath::FInterpConstantTo(LocalVelocity.X, 0.0f, DeltaSeconds, ZeroThrottleBrakeAcceleration);
		}
		LinearVelocity = GetActorTransform().TransformVectorNoScale(LocalVelocity);
	}

	if (!bEngineKill && ThrottlePercent <= 0.001f && LinearVelocity.SizeSquared() > 1.0f)
	{
		LinearVelocity = FMath::VInterpConstantTo(LinearVelocity, FVector::ZeroVector, DeltaSeconds, ZeroThrottleBrakeAcceleration);
	}

	LinearVelocity = LinearVelocity.GetClampedToMaxSize(NormalMaxSpeed);
	RebuildArcadeVelocityStateFromLinearVelocity();

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
		LogicalSystemPositionCm += Hit.ImpactNormal * 20.0f;
		RebuildArcadeVelocityStateFromLinearVelocity();
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

	const bool bUseSupercruiseTranslation =
		FlightModeComponent->GetSupercruiseState() != ESupercruiseState::DropoutCooldown &&
		FlightModeComponent->GetSupercruiseState() != ESupercruiseState::Inactive;

	if (FlightModeComponent->GetSupercruiseState() == ESupercruiseState::DropoutCooldown ||
		FlightModeComponent->GetSupercruiseState() == ESupercruiseState::Inactive)
	{
		LinearVelocity = LinearVelocity.GetClampedToMaxSize(NormalMaxSpeed);
		RebuildArcadeVelocityStateFromLinearVelocity();
	}
	else
	{
		LinearVelocity = GetActorForwardVector() * FlightModeComponent->GetCurrentSupercruiseSpeedCmPerSec();
	}

	const FVector LogicalDeltaCm = LinearVelocity * DeltaSeconds;
	if (bUseSupercruiseTranslation)
	{
		LogicalSystemPositionCm += LogicalDeltaCm;
		AddActorWorldOffset(LogicalDeltaCm * SupercruiseActorRepresentationScale, false);
	}
	else
	{
		const FVector PreviousActorLocation = GetActorLocation();
		FHitResult Hit;
		AddActorWorldOffset(LogicalDeltaCm, true, &Hit);
		LogicalSystemPositionCm += GetActorLocation() - PreviousActorLocation;
		if (Hit.bBlockingHit)
		{
			const FVector IncomingVelocity = LinearVelocity;
			const FVector ReflectedVelocity = FMath::GetReflectionVector(IncomingVelocity, Hit.ImpactNormal) * CollisionBounceRestitution;
			const FVector TangentialVelocity = FVector::VectorPlaneProject(IncomingVelocity, Hit.ImpactNormal) * CollisionSlideDamping;
			LinearVelocity = ReflectedVelocity + TangentialVelocity;
			AddActorWorldOffset(Hit.ImpactNormal * 20.0f, false);
			LogicalSystemPositionCm += Hit.ImpactNormal * 20.0f;
			RebuildArcadeVelocityStateFromLinearVelocity();
		}
	}
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

void ASpaceFlightPawn::UpdateStellarEnvironment(float DeltaSeconds)
{
	StellarEnvironment = FStellarEnvironmentTelemetry();
	if (Hull01 <= 0.0f)
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Kill;
		StellarEnvironment.Hazard01 = 1.0;
		StellarEnvironment.WarningText = TEXT("VESSEL LOST");
		return;
	}

	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || StarSystem->GetActiveSystemId().IsNone())
	{
		return;
	}

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;
	const FSimulationClockSnapshot Clock = Session
		? Session->GetSimulationClockSnapshot()
		: UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), SimulationTimeSeconds);

	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FBodyDefinition* NearestStar = nullptr;
	double NearestDistanceCm = TNumericLimits<double>::Max();
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		if (Body.BodyType != FName(TEXT("star")))
		{
			continue;
		}

		FFrameResolvedTransform StarTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Body.BodyId, Clock, SimulationTimeSeconds, StarTransform))
		{
			continue;
		}

		const double DistanceCm = FVector::Distance(LogicalSystemPositionCm, StarTransform.PositionCm);
		if (DistanceCm < NearestDistanceCm)
		{
			NearestDistanceCm = DistanceCm;
			NearestStar = &Body;
		}
	}

	if (!NearestStar)
	{
		return;
	}

	const double StarRadiusCm = FMath::Max(1.0, NearestStar->PhysicalReferenceRadiusCm > 0.0 ? NearestStar->PhysicalReferenceRadiusCm : NearestStar->VisualRadiusCm);
	const double LuminosityScale = FMath::Sqrt(FMath::Clamp(EstimateStellarLuminosity(NearestStar->StellarClass), 0.05, 10000.0));
	const double CautionRadiusCm = StarRadiusCm * StellarCautionRadiusMultiplier * LuminosityScale;
	const double DamageRadiusCm = StarRadiusCm * StellarDamageRadiusMultiplier * LuminosityScale;
	const double ExtremeRadiusCm = StarRadiusCm * StellarExtremeRadiusMultiplier * LuminosityScale;
	const double KillRadiusCm = StarRadiusCm * StellarKillRadiusMultiplier * LuminosityScale;
	const double SafeDistanceForFlux = FMath::Max(StarRadiusCm, NearestDistanceCm);
	const double Flux01 = FMath::Clamp((DamageRadiusCm * DamageRadiusCm) / FMath::Max(1.0, SafeDistanceForFlux * SafeDistanceForFlux), 0.0, 1.0);

	StellarEnvironment.StarId = NearestStar->BodyId;
	StellarEnvironment.DistanceCm = NearestDistanceCm;
	StellarEnvironment.DistanceInStarRadii = NearestDistanceCm / StarRadiusCm;
	StellarEnvironment.Hazard01 = FMath::Clamp((CautionRadiusCm - NearestDistanceCm) / FMath::Max(1.0, CautionRadiusCm - KillRadiusCm), 0.0, 1.0);
	StellarEnvironment.HeatLoad01 = Flux01;
	StellarEnvironment.RadiationLoad01 = FMath::Clamp(Flux01 * LuminosityScale * 0.35, 0.0, 1.0);

	if (NearestDistanceCm <= KillRadiusCm)
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Kill;
		StellarEnvironment.WarningText = TEXT("FATAL STELLAR FLUX");
		Shield01 = 0.0f;
		Hull01 = 0.0f;
	}
	else if (NearestDistanceCm <= ExtremeRadiusCm)
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Extreme;
		StellarEnvironment.WarningText = TEXT("EXTREME HEAT AND RADIATION");
	}
	else if (NearestDistanceCm <= DamageRadiusCm)
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Damage;
		StellarEnvironment.WarningText = TEXT("STELLAR RADIATION DAMAGE");
	}
	else if (NearestDistanceCm <= CautionRadiusCm)
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Caution;
		StellarEnvironment.WarningText = TEXT("STELLAR RADIATION CAUTION");
	}
	else
	{
		StellarEnvironment.HazardBand = EStellarHazardBand::Safe;
		StellarEnvironment.WarningText = TEXT("");
	}

	if (StellarEnvironment.HazardBand == EStellarHazardBand::Extreme && FlightModeComponent && GetFlightMode() == EShipFlightMode::Supercruise)
	{
		FlightModeComponent->RequestManualDropout();
	}

	if (StellarEnvironment.HazardBand != EStellarHazardBand::Damage && StellarEnvironment.HazardBand != EStellarHazardBand::Extreme)
	{
		StellarEnvironmentDamageAccumulator = 0.0f;
		return;
	}

	StellarEnvironmentDamageAccumulator += DeltaSeconds;
	if (StellarEnvironmentDamageAccumulator < StellarDamageTickIntervalSeconds)
	{
		return;
	}

	const float DamageDeltaSeconds = StellarEnvironmentDamageAccumulator;
	StellarEnvironmentDamageAccumulator = 0.0f;
	const float DamageScale = FMath::Clamp(static_cast<float>(StellarEnvironment.Hazard01), 0.0f, 1.0f);
	const float ShieldDamage = StellarShieldDamagePerSecond * (0.25f + DamageScale * DamageScale) * DamageDeltaSeconds;
	const float HullDamage = StellarHullDamagePerSecond * DamageScale * DamageScale * DamageDeltaSeconds;
	if (Shield01 > 0.0f)
	{
		Shield01 = FMath::Max(0.0f, Shield01 - ShieldDamage);
	}
	else
	{
		Hull01 = FMath::Max(0.0f, Hull01 - HullDamage);
	}
}

void ASpaceFlightPawn::UpdateShipVisuals(float DeltaSeconds)
{
	const float SteeringBank = -MouseSteeringInput.X * MouseTurnBankDegrees;
	const float StrafeBank = StrafeRightInput * VisualBankStrafeDegrees;
	const float TargetBank = FMath::Clamp(SteeringBank + StrafeBank, -MaxVisualBankDegrees, MaxVisualBankDegrees);
	const float TargetPitch = -ThrottlePercent * VisualPitchThrottleDegrees;

	const FRotator TargetRotation(TargetPitch, 0.0f, TargetBank);
	ShipVisualRotation = FMath::RInterpTo(ShipVisualRotation, TargetRotation, DeltaSeconds, VisualRotationInterpSpeed);

	ShipVisualRoot->SetRelativeLocation(ShipVisualOffset);
	ShipVisualRoot->SetRelativeRotation(ShipVisualRotation);
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
	CameraBoom->TargetOffset = FVector::ZeroVector;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->SetUsingAbsoluteRotation(false);
	CameraBoom->SetRelativeLocation(CurrentCameraTargetOffset);
	CameraBoom->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));
}

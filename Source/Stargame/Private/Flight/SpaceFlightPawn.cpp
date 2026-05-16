#include "Flight/SpaceFlightPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ASpaceFlightPawn::ASpaceFlightPawn()
{
	PrimaryActorTick.bCanEverTick = true;

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

void ASpaceFlightPawn::BeginPlay()
{
	Super::BeginPlay();

	CurrentCameraArmLength = CameraArmLength;
	CurrentCameraTargetOffset = CameraTargetOffset;
	InitializeAtmosphericDust();

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
	UpdateNormalFlight(DeltaSeconds);
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
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!StarSystem || !Session)
	{
		return;
	}

	TArray<FNavigationTargetDefinition> Targets;
	StarSystem->GetNavigationTargets(Targets);
	Targets.RemoveAll([](const FNavigationTargetDefinition& Target)
	{
		return !Target.bCanTarget || !Target.bShowInHud;
	});

	if (Targets.Num() == 0)
	{
		Session->SetSelectedTargetId(NAME_None);
		return;
	}

	const FName CurrentTargetId = Session->GetSelectedTargetId();
	int32 NextIndex = 0;
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		if (Targets[Index].TargetId == CurrentTargetId)
		{
			NextIndex = (Index + 1) % Targets.Num();
			break;
		}
	}

	Session->SetSelectedTargetId(Targets[NextIndex].TargetId);
}

void ASpaceFlightPawn::UpdateThrottle(float DeltaSeconds)
{
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

	FHitResult Hit;
	AddActorWorldOffset(LinearVelocity * DeltaSeconds, true, &Hit);
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

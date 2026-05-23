#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Flight/SpaceFlightPawn.h"
#include "Misc/AutomationTest.h"

namespace
{
	UWorld* FindNormalFlightAutomationWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (World && World->PersistentLevel)
			{
				return World;
			}
		}

		return nullptr;
	}

	ASpaceFlightPawn* SpawnNormalFlightTestPawn(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ASpaceFlightPawn* Pawn = World->SpawnActor<ASpaceFlightPawn>(
			ASpaceFlightPawn::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(10000000.0f, 0.0f, 10000000.0f)),
			SpawnParameters))
		{
			Pawn->SetActorEnableCollision(false);
			return Pawn;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightSteeringTurnsDirectlyTest,
	"Stargame.Flight.NormalFlight.DirectMouseSteeringTurnsPawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightSteeringTurnsDirectlyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	const float StartYaw = Pawn->GetActorRotation().Yaw;
	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(1.0f, 0.0f));
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Right steering yaws pawn right without inertial build-up"), Pawn->GetActorRotation().Yaw > StartYaw + 10.0f);
	TestEqual(TEXT("Steering input remains available for HUD/Blueprint"), static_cast<float>(Pawn->GetMouseSteeringInput().X), 1.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightSteeringDeadzoneClampTest,
	"Stargame.Flight.NormalFlight.SteeringDeadzoneAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightSteeringDeadzoneClampTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	const float StartYaw = Pawn->GetActorRotation().Yaw;
	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(0.02f, 0.02f));
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}
	TestTrue(TEXT("Cursor deadzone prevents steering drift"), FMath::Abs(Pawn->GetActorRotation().Yaw - StartYaw) < 0.5f);

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(4.0f, 3.0f));
	TestTrue(TEXT("Oversized steering input clamps to unit circle"), Pawn->GetMouseSteeringInput().Size() <= 1.0f + KINDA_SMALL_NUMBER);

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(0.08f, 1.0f));
	TestTrue(TEXT("Mostly vertical steering suppresses small horizontal wobble"), FMath::Abs(Pawn->GetMouseSteeringInput().X) < KINDA_SMALL_NUMBER);

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(1.0f, 0.08f));
	TestTrue(TEXT("Mostly horizontal steering suppresses small vertical wobble"), FMath::Abs(Pawn->GetMouseSteeringInput().Y) < KINDA_SMALL_NUMBER);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightVelocityAlignsToNoseTest,
	"Stargame.Flight.NormalFlight.VelocityAlignsToForwardVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightVelocityAlignsToNoseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	Pawn->SetFlightTestInputs(1.0f, 0.0f, 0.0f, 0.0f, FVector2D::ZeroVector);
	for (int32 Step = 0; Step < 90; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	const float InitialSpeed = Pawn->GetSpeedCentimetersPerSecond();
	TestTrue(TEXT("Throttle accelerates the pawn"), InitialSpeed > 5000.0f);
	TestTrue(TEXT("Initial velocity follows the nose"), FVector::DotProduct(Pawn->GetLinearVelocityCmPerSec().GetSafeNormal(), Pawn->GetActorForwardVector()) > 0.95f);

	Pawn->SetFlightTestInputs(1.0f, 0.0f, 0.0f, 0.0f, FVector2D(0.9f, 0.0f));
	for (int32 Step = 0; Step < 120; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Velocity bends toward the new forward vector without snapping"), FVector::DotProduct(Pawn->GetLinearVelocityCmPerSec().GetSafeNormal(), Pawn->GetActorForwardVector()) > 0.35f);
	TestTrue(TEXT("Normal speed remains clamped"), Pawn->GetSpeedCentimetersPerSecond() <= Pawn->GetNormalMaxSpeedCentimetersPerSecond() + 1.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightMouseSteeringBanksVisualsTest,
	"Stargame.Flight.NormalFlight.MouseSteeringBanksVisuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightMouseSteeringBanksVisualsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(1.0f, 0.0f));
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Mouse steering banks the ship visual into the turn"), Pawn->GetShipVisualRollDegrees() < -12.0f);
	TestTrue(TEXT("Mouse steering does not force physical actor roll"), FMath::Abs(Pawn->GetActorRotation().Roll) < 1.0f);
	TestTrue(TEXT("Chase camera stays horizon-stable during visual banking"), FMath::Abs(Pawn->GetCameraRollDegrees()) < 1.0f);

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(-0.65f, -0.65f));
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Diagonal up-left steering keeps left bank direction"), Pawn->GetShipVisualRollDegrees() > 8.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightVerticalSteeringStaysCleanTest,
	"Stargame.Flight.NormalFlight.VerticalSteeringStaysClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightVerticalSteeringStaysCleanTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, 0.0f, FVector2D(0.0f, -1.0f));
	for (int32 Step = 0; Step < 240; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Sustained vertical steering pitches the ship"), FMath::Abs(Pawn->GetActorRotation().Pitch) > 15.0f);
	TestTrue(TEXT("Mouse above center pitches the nose up in Unreal rotation space"), Pawn->GetActorRotation().Pitch > 15.0f);
	TestTrue(TEXT("Vertical steering does not create visual bank wobble"), FMath::Abs(Pawn->GetShipVisualRollDegrees()) < 1.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightManualRollCarriesCameraTest,
	"Stargame.Flight.NormalFlight.ManualRollCarriesCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightManualRollCarriesCameraTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	Pawn->SetFlightTestInputs(0.0f, 0.0f, 0.0f, -1.0f, FVector2D::ZeroVector);
	for (int32 Step = 0; Step < 45; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Manual roll rotates the ship actor"), FMath::Abs(Pawn->GetActorRotation().Roll) > 20.0f);
	TestTrue(TEXT("Manual roll carries the chase camera with the ship"), FMath::Abs(Pawn->GetCameraRollDegrees()) > 15.0f);
	TestTrue(TEXT("Manual roll does not add mouse-bank visual telemetry"), FMath::Abs(Pawn->GetShipVisualRollDegrees()) < 1.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightBrakingIsDeterministicTest,
	"Stargame.Flight.NormalFlight.BrakingReducesForwardSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightBrakingIsDeterministicTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* Pawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	Pawn->SetFlightTestTransformAndVelocity(FTransform(FRotator::ZeroRotator, FVector::ZeroVector), FVector(22000.0f, 0.0f, 0.0f));
	const float StartSpeed = Pawn->GetSpeedCentimetersPerSecond();
	Pawn->SetFlightTestInputs(-1.0f, 0.0f, 0.0f, 0.0f, FVector2D::ZeroVector);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Pawn->TickNormalFlightForTest(1.0f / 60.0f);
	}

	TestTrue(TEXT("Braking reduces speed"), Pawn->GetSpeedCentimetersPerSecond() < StartSpeed - 8000.0f);
	TestTrue(TEXT("Braking does not reverse the ship"), Pawn->GetForwardSpeedCentimetersPerSecond() >= 0.0f);
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalFlightStrafeEquipmentTuningTest,
	"Stargame.Flight.NormalFlight.StrafeUsesEquipmentTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNormalFlightStrafeEquipmentTuningTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindNormalFlightAutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	ASpaceFlightPawn* BaselinePawn = SpawnNormalFlightTestPawn(World);
	ASpaceFlightPawn* TunedPawn = SpawnNormalFlightTestPawn(World);
	TestNotNull(TEXT("Baseline flight pawn spawned"), BaselinePawn);
	TestNotNull(TEXT("Tuned flight pawn spawned"), TunedPawn);
	if (!BaselinePawn || !TunedPawn)
	{
		return false;
	}

	TunedPawn->ApplyShipEquipmentFlightStats(1.0f, 1.0f, 0.1f);
	TestTrue(TEXT("Strafe equipment multiplier changes tuned acceleration"), TunedPawn->GetStrafeAccelerationCentimetersPerSecondSquared() < BaselinePawn->GetStrafeAccelerationCentimetersPerSecondSquared() * 0.2f);

	BaselinePawn->Destroy();
	TunedPawn->Destroy();
	return true;
}

#endif

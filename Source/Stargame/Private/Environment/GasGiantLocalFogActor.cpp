#include "Environment/GasGiantLocalFogActor.h"

#include "Components/LocalFogVolumeComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Environment/GasGiantPlanetActor.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr int32 DeckVolumeCount = 14;
constexpr int32 StormVolumeCount = 7;

const FVector2D DeckOffsets[DeckVolumeCount] = {
	FVector2D(-0.86f, -0.32f),
	FVector2D(-0.62f, 0.42f),
	FVector2D(-0.34f, -0.78f),
	FVector2D(-0.18f, 0.05f),
	FVector2D(0.04f, 0.72f),
	FVector2D(0.20f, -0.46f),
	FVector2D(0.38f, 0.18f),
	FVector2D(0.56f, -0.84f),
	FVector2D(0.74f, 0.56f),
	FVector2D(0.92f, -0.08f),
	FVector2D(-0.72f, 0.88f),
	FVector2D(-0.48f, -0.58f),
	FVector2D(0.12f, -0.96f),
	FVector2D(0.68f, 0.04f)
};

const FVector2D StormOffsets[StormVolumeCount] = {
	FVector2D(-0.64f, 0.16f),
	FVector2D(-0.38f, -0.62f),
	FVector2D(-0.08f, 0.72f),
	FVector2D(0.26f, -0.18f),
	FVector2D(0.48f, 0.54f),
	FVector2D(0.76f, -0.48f),
	FVector2D(0.88f, 0.10f)
};
}

AGasGiantLocalFogActor::AGasGiantLocalFogActor()
{
	PrimaryActorTick.bCanEverTick = true;

	FieldRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FieldRoot"));
	SetRootComponent(FieldRoot);
	SetActorEnableCollision(false);

	BaseHazeVolume = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("BaseHazeVolume"));
	BaseHazeVolume->SetupAttachment(FieldRoot);

	for (int32 Index = 0; Index < DeckVolumeCount; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("DeckVolume_%02d"), Index));
		ULocalFogVolumeComponent* DeckVolume = CreateDefaultSubobject<ULocalFogVolumeComponent>(ComponentName);
		DeckVolume->SetupAttachment(FieldRoot);
		DeckVolumes.Add(DeckVolume);
	}

	for (int32 Index = 0; Index < StormVolumeCount; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("StormVolume_%02d"), Index));
		ULocalFogVolumeComponent* StormVolume = CreateDefaultSubobject<ULocalFogVolumeComponent>(ComponentName);
		StormVolume->SetupAttachment(FieldRoot);
		StormVolumes.Add(StormVolume);
	}

	TArray<ULocalFogVolumeComponent*> AllVolumes;
	AllVolumes.Add(BaseHazeVolume);
	for (ULocalFogVolumeComponent* DeckVolume : DeckVolumes)
	{
		AllVolumes.Add(DeckVolume);
	}
	for (ULocalFogVolumeComponent* StormVolume : StormVolumes)
	{
		AllVolumes.Add(StormVolume);
	}

	for (ULocalFogVolumeComponent* Volume : AllVolumes)
	{
		if (!Volume)
		{
			continue;
		}

		Volume->SetMobility(EComponentMobility::Movable);
		Volume->SetRadialFogExtinction(0.0f);
		Volume->SetHeightFogExtinction(0.0f);
		Volume->SetHeightFogFalloff(HeightFalloff);
		Volume->SetFogPhaseG(0.46f);
		Volume->SetFogAlbedo(FogAlbedo);
		Volume->SetFogEmissive(FLinearColor::Black);
	}
}

void AGasGiantLocalFogActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveSceneActors();
}

void AGasGiantLocalFogActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!CachedPlanet || !CachedPawn)
	{
		ResolveSceneActors();
	}

	if (!CachedPlanet || !CachedPawn || !BaseHazeVolume)
	{
		return;
	}

	const FGasGiantAtmosphereState AtmosphereState = CachedPlanet->ComputeAtmosphereState(CachedPawn->GetActorLocation());
	ApplyFogState(AtmosphereState, DeltaSeconds);
	ApplyStructuredVolumes(AtmosphereState, DeltaSeconds);
}

void AGasGiantLocalFogActor::ResolveSceneActors()
{
	CachedPawn = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			CachedPawn = Cast<ASpaceFlightPawn>(PlayerController->GetPawn());
		}

		CachedPlanet = nullptr;
		for (TActorIterator<AGasGiantPlanetActor> It(World); It; ++It)
		{
			CachedPlanet = *It;
			break;
		}
	}
}

void AGasGiantLocalFogActor::ApplyFogState(const FGasGiantAtmosphereState& AtmosphereState, float DeltaSeconds)
{
	const float TargetRadial = AtmosphereState.RenderFogDensity * MaxBaseRadialExtinction;
	const float TargetHeight = AtmosphereState.RenderFogDensity * MaxBaseHeightExtinction;

	CurrentRadialExtinction = FMath::FInterpTo(CurrentRadialExtinction, TargetRadial, DeltaSeconds, FogInterpSpeed);
	CurrentHeightExtinction = FMath::FInterpTo(CurrentHeightExtinction, TargetHeight, DeltaSeconds, FogInterpSpeed);

	SetActorLocation(CachedPawn->GetActorLocation());

	SetVolumeTransform(BaseHazeVolume, CachedPawn->GetActorLocation(), FQuat::Identity, FVector(BaseHazeRadius * 2.0f));
	BaseHazeVolume->SetRadialFogExtinction(CurrentRadialExtinction);
	BaseHazeVolume->SetHeightFogExtinction(CurrentHeightExtinction);
	BaseHazeVolume->SetHeightFogFalloff(HeightFalloff);
	BaseHazeVolume->SetFogAlbedo(FogAlbedo);
	BaseHazeVolume->SetFogEmissive(DeepFogEmissive * AtmosphereState.DeepAlpha);
}

void AGasGiantLocalFogActor::ApplyStructuredVolumes(const FGasGiantAtmosphereState& AtmosphereState, float DeltaSeconds)
{
	const FVector PawnLocation = CachedPawn->GetActorLocation();
	const FVector Up = (PawnLocation - AtmosphereState.PlanetLocation).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector Forward = FVector::VectorPlaneProject(CachedPawn->GetActorForwardVector(), Up).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::CrossProduct(FVector::RightVector, Up).GetSafeNormal();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	Forward = FVector::CrossProduct(Right, Up).GetSafeNormal();
	const FQuat FieldRotation = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();

	StormRotationDegrees = FMath::Fmod(StormRotationDegrees + StormRotationDegreesPerSecond * DeltaSeconds, 360.0f);
	const float StormRotationRadians = FMath::DegreesToRadians(StormRotationDegrees);
	const float SinStorm = FMath::Sin(StormRotationRadians);
	const float CosStorm = FMath::Cos(StormRotationRadians);

	const float DeckBelowShip = FMath::Clamp(
		AtmosphereState.VisualAltitude - 500000.0f,
		DeckMinBelowShip,
		DeckMaxBelowShip);
	const float DeckDensity = FMath::Clamp(
		AtmosphereState.EntryAlpha * 0.12f + AtmosphereState.DenseAlpha * 0.62f + AtmosphereState.DeepAlpha * 0.26f,
		0.0f,
		1.0f);
	const float StormDensity = FMath::Clamp(
		AtmosphereState.DenseAlpha * 0.22f + AtmosphereState.DeepAlpha * 0.78f,
		0.0f,
		1.0f);

	for (int32 Index = 0; Index < DeckVolumes.Num(); ++Index)
	{
		ULocalFogVolumeComponent* DeckVolume = DeckVolumes[Index];
		if (!DeckVolume)
		{
			continue;
		}

		const FVector2D Offset = DeckOffsets[Index % DeckVolumeCount] * DeckFieldRadius;
		const float LayerOffset = (static_cast<float>((Index % 5) - 2) * 36000.0f);
		const FVector Location = PawnLocation + Forward * Offset.X + Right * Offset.Y - Up * (DeckBelowShip + LayerOffset);
		const float WidthScale = 1.0f + static_cast<float>(Index % 4) * 0.16f;
		const FVector Diameter(
			520000.0f * WidthScale,
			320000.0f * (1.08f - static_cast<float>(Index % 3) * 0.08f),
			72000.0f + static_cast<float>(Index % 4) * 18000.0f);

		SetVolumeTransform(DeckVolume, Location, FieldRotation, Diameter);
		DeckVolume->SetRadialFogExtinction(DeckDensity * MaxDeckRadialExtinction);
		DeckVolume->SetHeightFogExtinction(DeckDensity * MaxDeckHeightExtinction);
		DeckVolume->SetHeightFogFalloff(HeightFalloff);
		DeckVolume->SetFogAlbedo(FogAlbedo);
		DeckVolume->SetFogEmissive(DeepFogEmissive * AtmosphereState.DeepAlpha * 0.35f);
	}

	for (int32 Index = 0; Index < StormVolumes.Num(); ++Index)
	{
		ULocalFogVolumeComponent* StormVolume = StormVolumes[Index];
		if (!StormVolume)
		{
			continue;
		}

		const FVector2D RawOffset = StormOffsets[Index % StormVolumeCount] * StormFieldRadius;
		const FVector2D RotatedOffset(
			RawOffset.X * CosStorm - RawOffset.Y * SinStorm,
			RawOffset.X * SinStorm + RawOffset.Y * CosStorm);
		const float HeightBias = static_cast<float>((Index % 3) - 1) * 90000.0f;
		const FVector Location = PawnLocation + Forward * RotatedOffset.X + Right * RotatedOffset.Y - Up * (DeckBelowShip * 0.45f + HeightBias);
		const FVector Diameter(
			180000.0f + static_cast<float>(Index % 3) * 45000.0f,
			150000.0f + static_cast<float>(Index % 4) * 32000.0f,
			560000.0f + static_cast<float>(Index % 5) * 90000.0f);

		SetVolumeTransform(StormVolume, Location, FieldRotation, Diameter);
		StormVolume->SetRadialFogExtinction(StormDensity * MaxStormRadialExtinction);
		StormVolume->SetHeightFogExtinction(StormDensity * MaxStormHeightExtinction);
		StormVolume->SetHeightFogFalloff(FMath::Max(HeightFalloff * 0.65f, 1.0f));
		StormVolume->SetFogAlbedo(FLinearColor(0.28f, 0.62f, 0.18f, 1.0f));
		StormVolume->SetFogEmissive(DeepFogEmissive * AtmosphereState.DeepAlpha * 0.65f);
	}
}

void AGasGiantLocalFogActor::SetVolumeTransform(ULocalFogVolumeComponent* Volume, const FVector& Location, const FQuat& Rotation, const FVector& Diameter) const
{
	if (!Volume)
	{
		return;
	}

	const float BaseDiameter = ULocalFogVolumeComponent::GetBaseVolumeSize();
	Volume->SetWorldLocationAndRotation(Location, Rotation);
	Volume->SetWorldScale3D(FVector(
		Diameter.X / BaseDiameter,
		Diameter.Y / BaseDiameter,
		Diameter.Z / BaseDiameter));
}

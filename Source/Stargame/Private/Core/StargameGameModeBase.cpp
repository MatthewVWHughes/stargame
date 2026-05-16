#include "Core/StargameGameModeBase.h"

#include "Core/StargamePlayerController.h"
#include "Environment/GasGiantLocalFogActor.h"
#include "Environment/GasGiantPlanetActor.h"
#include "Environment/GasGiantVolumetricCloudActor.h"
#include "Environment/StarfieldBackdrop.h"
#include "Flight/SpaceFlightPawn.h"
#include "UI/PrototypeFlightHud.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"

AStargameGameModeBase::AStargameGameModeBase()
{
	PlayerControllerClass = AStargamePlayerController::StaticClass();
	DefaultPawnClass = ASpaceFlightPawn::StaticClass();
	HUDClass = APrototypeFlightHud::StaticClass();
	PrototypeGasGiantClass = AGasGiantPlanetActor::StaticClass();
	PrototypeLocalFogClass = AGasGiantLocalFogActor::StaticClass();
	PrototypeVolumetricCloudClass = AGasGiantVolumetricCloudActor::StaticClass();
	PrototypeSunLightClass = ADirectionalLight::StaticClass();
	PrototypeSkyAtmosphereClass = ASkyAtmosphere::StaticClass();
	PrototypeStarfieldClass = AStarfieldBackdrop::StaticClass();
}

void AStargameGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (!bSpawnPrototypeGasGiantScene || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("PrototypeGasGiant");
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (PrototypeGasGiantClass)
	{
		GetWorld()->SpawnActor<AGasGiantPlanetActor>(
			PrototypeGasGiantClass,
			PrototypeGasGiantLocation,
			FRotator::ZeroRotator,
			SpawnParameters);
	}

	FActorSpawnParameters FogSpawnParameters;
	FogSpawnParameters.Name = TEXT("PrototypeGasGiantLocalFog");
	FogSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (bSpawnPrototypeLocalFog && PrototypeLocalFogClass)
	{
		GetWorld()->SpawnActor<AGasGiantLocalFogActor>(
			PrototypeLocalFogClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			FogSpawnParameters);
	}

	FActorSpawnParameters CloudSpawnParameters;
	CloudSpawnParameters.Name = TEXT("PrototypeGasGiantVolumetricCloud");
	CloudSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (bSpawnPrototypeVolumetricCloud && PrototypeVolumetricCloudClass)
	{
		GetWorld()->SpawnActor<AGasGiantVolumetricCloudActor>(
			PrototypeVolumetricCloudClass,
			PrototypeGasGiantLocation,
			FRotator::ZeroRotator,
			CloudSpawnParameters);
	}

	FActorSpawnParameters LightSpawnParameters;
	LightSpawnParameters.Name = TEXT("PrototypeSunLight");
	LightSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADirectionalLight* SunLight = PrototypeSunLightClass
		? GetWorld()->SpawnActor<ADirectionalLight>(
			PrototypeSunLightClass,
			FVector::ZeroVector,
			PrototypeSunLightRotation,
			LightSpawnParameters)
		: nullptr;

	if (SunLight && SunLight->GetLightComponent())
	{
		SunLight->GetLightComponent()->SetIntensity(PrototypeSunLightIntensity);
		SunLight->GetLightComponent()->SetLightColor(PrototypeSunLightColor);
		if (UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			DirectionalLightComponent->SetAtmosphereSunLight(true);
			DirectionalLightComponent->SetAtmosphereSunLightIndex(0);
		}
	}

	FActorSpawnParameters AtmosphereSpawnParameters;
	AtmosphereSpawnParameters.Name = TEXT("PrototypeGasGiantSkyAtmosphere");
	AtmosphereSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASkyAtmosphere* SkyAtmosphere = PrototypeSkyAtmosphereClass
		? GetWorld()->SpawnActor<ASkyAtmosphere>(
			PrototypeSkyAtmosphereClass,
			PrototypeGasGiantLocation,
			FRotator::ZeroRotator,
			AtmosphereSpawnParameters)
		: nullptr;

	if (SkyAtmosphere && SkyAtmosphere->GetComponent())
	{
		USkyAtmosphereComponent* AtmosphereComponent = SkyAtmosphere->GetComponent();
		AtmosphereComponent->TransformMode = ESkyAtmosphereTransformMode::PlanetCenterAtComponentTransform;
		AtmosphereComponent->SetBottomRadius(50.0f);
		AtmosphereComponent->SetAtmosphereHeight(12.0f);
		AtmosphereComponent->SetGroundAlbedo(FColor(12, 56, 18));
		AtmosphereComponent->SetMultiScatteringFactor(1.7f);
		AtmosphereComponent->SetRayleighScatteringScale(0.06f);
		AtmosphereComponent->SetMieScatteringScale(3.2f);
		AtmosphereComponent->SetMieAbsorptionScale(0.45f);
		AtmosphereComponent->SetMieAnisotropy(0.78f);
		AtmosphereComponent->SetMieExponentialDistribution(2.2f);
		AtmosphereComponent->SetAerialPespectiveViewDistanceScale(2.8f);
		AtmosphereComponent->SetAerialPerspectiveStartDepth(0.001f);
		AtmosphereComponent->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(0.42f, 0.82f, 0.32f, 1.0f));
	}

	FActorSpawnParameters StarfieldSpawnParameters;
	StarfieldSpawnParameters.Name = TEXT("PrototypeStarfield");
	StarfieldSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (PrototypeStarfieldClass)
	{
		GetWorld()->SpawnActor<AStarfieldBackdrop>(
			PrototypeStarfieldClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			StarfieldSpawnParameters);
	}
}

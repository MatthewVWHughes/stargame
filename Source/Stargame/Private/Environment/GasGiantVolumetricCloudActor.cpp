#include "Environment/GasGiantVolumetricCloudActor.h"

#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGasGiantVolumetricCloudActor::AGasGiantVolumetricCloudActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	SetRootComponent(VolumetricCloud);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> JoolCloudMaterial(
		TEXT("/Game/Materials/Environment/MI_JoolVolumetricCloud.MI_JoolVolumetricCloud"));
	if (JoolCloudMaterial.Succeeded())
	{
		VolumetricCloud->SetMaterial(JoolCloudMaterial.Object);
		return;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (CloudMaterial.Succeeded())
	{
		VolumetricCloud->SetMaterial(CloudMaterial.Object);
	}
}

void AGasGiantVolumetricCloudActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCloudSettings();
}

void AGasGiantVolumetricCloudActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyCloudSettings();
}

void AGasGiantVolumetricCloudActor::ApplyCloudSettings()
{
	if (!VolumetricCloud)
	{
		return;
	}

	VolumetricCloud->SetPlanetRadius(PrototypePlanetRadiusKilometers);
	VolumetricCloud->SetLayerBottomAltitude(CloudBottomAltitudeKilometers);
	VolumetricCloud->SetLayerHeight(CloudLayerHeightKilometers);
	VolumetricCloud->SetTracingStartMaxDistance(TracingMaxDistanceKilometers);
	VolumetricCloud->SetTracingStartDistanceFromCamera(0.0f);
	VolumetricCloud->SetTracingMaxDistance(TracingMaxDistanceKilometers);
	VolumetricCloud->SetGroundAlbedo(GroundAlbedo);
	VolumetricCloud->SetSkyLightCloudBottomOcclusion(0.9f);
	VolumetricCloud->SetViewSampleCountScale(ViewSampleCountScale);
	VolumetricCloud->SetShadowViewSampleCountScale(1.2f);
	VolumetricCloud->SetShadowTracingDistance(18.0f);
	VolumetricCloud->SetStopTracingTransmittanceThreshold(0.006f);
	VolumetricCloud->SetbUsePerSampleAtmosphericLightTransmittance(true);
}

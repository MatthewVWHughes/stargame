#include "Environment/GasGiantPlanetActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Environment/GasGiantCloudField.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/ConstructorHelpers.h"

AGasGiantPlanetActor::AGasGiantPlanetActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	SetRootComponent(PlanetMesh);
	PlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlanetMesh->SetCollisionObjectType(ECC_WorldStatic);
	PlanetMesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PlanetMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		PlanetMesh->SetMaterial(0, BasicMaterial.Object);
	}
	CloudFieldClass = nullptr;
}

void AGasGiantPlanetActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ValidateAtmosphereBands();
	ApplyPlanetScale();
}

void AGasGiantPlanetActor::BeginPlay()
{
	Super::BeginPlay();
	ValidateAtmosphereBands();
	ApplyPlanetScale();
	SpawnCloudField();
}

void AGasGiantPlanetActor::ValidateAtmosphereBands()
{
	AtmosphereEntryAltitude = FMath::Max(AtmosphereEntryAltitude, 100000.0f);
	DenseLayerTopAltitude = FMath::Clamp(DenseLayerTopAltitude, 0.0f, AtmosphereEntryAltitude - 100000.0f);
	DeepLayerTopAltitude = FMath::Clamp(DeepLayerTopAltitude, CrushDepthStartAltitude + 100000.0f, DenseLayerTopAltitude - 100000.0f);
	CrushDepthStartAltitude = FMath::Min(CrushDepthStartAltitude, DeepLayerTopAltitude - 100000.0f);
	CrushDepthFatalAltitude = FMath::Min(CrushDepthFatalAltitude, CrushDepthStartAltitude - 100000.0f);
}

void AGasGiantPlanetActor::ApplyPlanetScale()
{
	// Engine basic sphere is 100cm across, so scale by diameter in centimeters.
	const float SphereScale = (VisualRadius * 2.0f) / 100.0f;
	PlanetMesh->SetWorldScale3D(FVector(SphereScale));
	PlanetMesh->SetCollisionEnabled(bEnableDebugCoreCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* DynamicMaterial = PlanetMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), PlanetCoreColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), PlanetCoreColor);
	}
}

FGasGiantAtmosphereState AGasGiantPlanetActor::ComputeAtmosphereState(FVector WorldLocation) const
{
	FGasGiantAtmosphereState State;
	State.PlanetLocation = GetActorLocation();
	State.DistanceFromCenter = FVector::Distance(WorldLocation, State.PlanetLocation);
	State.VisualAltitude = State.DistanceFromCenter - VisualRadius;

	const float SafeEntryAltitude = FMath::Max(AtmosphereEntryAltitude, 1.0f);
	const float SafeDenseSpan = FMath::Max(DenseLayerTopAltitude - DeepLayerTopAltitude, 1.0f);
	const float SafeDeepSpan = FMath::Max(DeepLayerTopAltitude - CrushDepthStartAltitude, 1.0f);
	const float SafeCrushSpan = FMath::Max(CrushDepthStartAltitude - CrushDepthFatalAltitude, 1.0f);

	State.EntryAlpha = 1.0f - FMath::Clamp(State.VisualAltitude / SafeEntryAltitude, 0.0f, 1.0f);
	State.DenseAlpha = 1.0f - FMath::Clamp((State.VisualAltitude - DeepLayerTopAltitude) / SafeDenseSpan, 0.0f, 1.0f);
	State.DeepAlpha = 1.0f - FMath::Clamp((State.VisualAltitude - CrushDepthStartAltitude) / SafeDeepSpan, 0.0f, 1.0f);
	State.CrushAlpha = 1.0f - FMath::Clamp((State.VisualAltitude - CrushDepthFatalAltitude) / SafeCrushSpan, 0.0f, 1.0f);

	if (State.VisualAltitude <= CrushDepthStartAltitude)
	{
		State.Zone = EGasGiantAtmosphereZone::Crush;
	}
	else if (State.VisualAltitude <= DeepLayerTopAltitude)
	{
		State.Zone = EGasGiantAtmosphereZone::Deep;
	}
	else if (State.VisualAltitude <= DenseLayerTopAltitude)
	{
		State.Zone = EGasGiantAtmosphereZone::Dense;
	}
	else if (State.EntryAlpha > 0.0f)
	{
		State.Zone = EGasGiantAtmosphereZone::Entry;
	}
	else
	{
		State.Zone = EGasGiantAtmosphereZone::Far;
	}

	State.RenderFogDensity = FMath::Clamp(
		State.EntryAlpha * 0.10f + State.DenseAlpha * 0.48f + State.DeepAlpha * 0.42f,
		0.0f,
		1.0f);
	return State;
}

void AGasGiantPlanetActor::SpawnCloudField()
{
	if (!bSpawnCloudField || RuntimeCloudField || !CloudFieldClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RuntimeCloudField = GetWorld()->SpawnActor<AGasGiantCloudField>(
		CloudFieldClass,
		GetActorLocation(),
		GetActorRotation(),
		SpawnParameters);
}

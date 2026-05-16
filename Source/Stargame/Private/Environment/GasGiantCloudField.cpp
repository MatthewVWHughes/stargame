#include "Environment/GasGiantCloudField.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGasGiantCloudField::AGasGiantCloudField()
{
	PrimaryActorTick.bCanEverTick = true;

	CloudRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CloudRoot"));
	SetRootComponent(CloudRoot);

	CloudPuffs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CloudPuffs"));
	CloudPuffs->SetupAttachment(CloudRoot);
	CloudPuffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudPuffs->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		CloudPuffs->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		CloudPuffs->SetMaterial(0, BasicMaterial.Object);
	}
}

void AGasGiantCloudField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRebuildEveryConstruction)
	{
		RebuildCloudLayer();
	}
}

void AGasGiantCloudField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyZero(DriftDegreesPerSecond))
	{
		AddActorLocalRotation(FRotator(0.0f, DriftDegreesPerSecond * DeltaSeconds, 0.0f));
	}
}

void AGasGiantCloudField::RebuildCloudLayer()
{
	if (!CloudPuffs)
	{
		return;
	}

	CloudPuffs->ClearInstances();

	UMaterialInstanceDynamic* DynamicMaterial = CloudPuffs->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), CloudColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), CloudColor);
	}

	FRandomStream Random(RandomSeed);
	const float GoldenAngle = PI * (3.0f - FMath::Sqrt(5.0f));
	const int32 InstanceCount = FMath::Max(CloudPuffCount, 0);

	for (int32 Index = 0; Index < InstanceCount; ++Index)
	{
		const float T = InstanceCount > 1 ? static_cast<float>(Index) / static_cast<float>(InstanceCount - 1) : 0.0f;
		const float Z = 1.0f - 2.0f * T;
		const float RadiusAtZ = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
		const float Theta = GoldenAngle * static_cast<float>(Index);

		FVector Direction(
			FMath::Cos(Theta) * RadiusAtZ,
			FMath::Sin(Theta) * RadiusAtZ,
			Z);

		Direction = Direction.RotateAngleAxis(Random.FRandRange(-1.6f, 1.6f), FVector::UpVector).GetSafeNormal();

		const float Altitude = DeepLayerBottomAltitude + Random.FRandRange(0.0f, DeepLayerHeight);
		const float Radius = PlanetRadius + Altitude;
		const float PuffRadius = Random.FRandRange(MinPuffRadius, MaxPuffRadius);
		const FVector Location = Direction * Radius;

		const FQuat Rotation = FRotationMatrix::MakeFromZ(Direction).ToQuat();
		const FVector Scale(
			(PuffRadius * Random.FRandRange(1.7f, 3.8f)) / 100.0f,
			(PuffRadius * Random.FRandRange(0.9f, 1.7f)) / 100.0f,
			(PuffRadius * Random.FRandRange(0.35f, 0.8f)) / 100.0f);

		CloudPuffs->AddInstance(FTransform(Rotation, Location, Scale));
	}
}

float AGasGiantCloudField::ComputeCrushDepthAlpha(FVector WorldLocation) const
{
	if (CrushDepthStartRadius <= CrushDepthFatalRadius)
	{
		return 0.0f;
	}

	const float RadiusFromPlanetCentre = FVector::Distance(WorldLocation, GetActorLocation());
	const float DepthAlpha = (CrushDepthStartRadius - RadiusFromPlanetCentre) / (CrushDepthStartRadius - CrushDepthFatalRadius);
	return FMath::Clamp(DepthAlpha, 0.0f, 1.0f);
}

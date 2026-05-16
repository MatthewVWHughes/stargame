#include "Environment/StarfieldBackdrop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AStarfieldBackdrop::AStarfieldBackdrop()
{
	PrimaryActorTick.bCanEverTick = false;

	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	SetRootComponent(Stars);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StarMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (StarMesh.Succeeded())
	{
		Stars->SetStaticMesh(StarMesh.Object);
	}
}

void AStarfieldBackdrop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildStars();
}

void AStarfieldBackdrop::BuildStars()
{
	if (!Stars)
	{
		return;
	}

	Stars->ClearInstances();

	FRandomStream Random(Seed);
	const float GoldenAngle = PI * (3.0f - FMath::Sqrt(5.0f));

	for (int32 Index = 0; Index < StarCount; ++Index)
	{
		const float T = StarCount > 1 ? static_cast<float>(Index) / static_cast<float>(StarCount - 1) : 0.0f;
		const float Y = 1.0f - 2.0f * T;
		const float RadiusAtY = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Y * Y));
		const float Theta = GoldenAngle * static_cast<float>(Index);

		FVector Direction(
			FMath::Cos(Theta) * RadiusAtY,
			FMath::Sin(Theta) * RadiusAtY,
			Y);

		Direction = Direction.RotateAngleAxis(Random.FRandRange(-1.25f, 1.25f), FVector::UpVector).GetSafeNormal();

		const float StarScale = BaseStarScale * Random.FRandRange(0.5f, 2.2f);
		const FTransform InstanceTransform(
			FRotator::ZeroRotator,
			Direction * StarfieldRadius,
			FVector(StarScale));

		Stars->AddInstance(InstanceTransform);
	}
}

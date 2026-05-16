#include "Space/M0SystemMarkerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AM0SystemMarkerActor::AM0SystemMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	SetRootComponent(MarkerMesh);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MarkerMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		MarkerMesh->SetMaterial(0, BasicMaterial.Object);
	}
}

void AM0SystemMarkerActor::InitializeMarker(FName InGameplayId, FName InEntityType, double VisualRadiusCm)
{
	GameplayId = InGameplayId;
	EntityType = InEntityType;

	const float DiameterCm = static_cast<float>(FMath::Max(VisualRadiusCm * 2.0, 1000.0));
	const float EngineSphereDiameterCm = 100.0f;
	SetActorScale3D(FVector(DiameterCm / EngineSphereDiameterCm));
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("M0_%s_%s"), *EntityType.ToString(), *GameplayId.ToString()));
#endif
}

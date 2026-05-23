#include "Space/M0SystemMarkerActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AM0SystemMarkerActor::AM0SystemMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativePlaceholderMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void AM0SystemMarkerActor::InitializeMarker(FName InGameplayId, FName InEntityType, double VisualRadiusCm)
{
	GameplayId = InGameplayId;
	EntityType = InEntityType;

	const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	if (EntityType == FName(TEXT("station")))
	{
		MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	}
	else if (EntityType == FName(TEXT("gate")))
	{
		MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	}
	else if (EntityType == FName(TEXT("resource")))
	{
		MeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	}

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
	{
		VisualMesh->SetStaticMesh(Mesh);
	}

	const double MeshDiameterCm = 100.0;
	const double UniformScale = FMath::Max(0.1, (VisualRadiusCm * 2.0) / MeshDiameterCm);
	if (EntityType == FName(TEXT("station")))
	{
		VisualMesh->SetRelativeScale3D(FVector(UniformScale * 1.6, UniformScale * 0.55, UniformScale * 0.55));
	}
	else if (EntityType == FName(TEXT("gate")))
	{
		VisualMesh->SetRelativeScale3D(FVector(UniformScale, UniformScale, FMath::Max(0.08, UniformScale * 0.12)));
	}
	else if (EntityType == FName(TEXT("resource")))
	{
		VisualMesh->SetRelativeScale3D(FVector(UniformScale * 0.7, UniformScale * 0.7, UniformScale * 0.7));
	}
	else
	{
		VisualMesh->SetRelativeScale3D(FVector(UniformScale));
	}

	Tags.AddUnique(GameplayId);
	Tags.AddUnique(EntityType);
	OnMarkerInitialized(InGameplayId, InEntityType, VisualRadiusCm);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("M0_%s_%s"), *EntityType.ToString(), *GameplayId.ToString()));
#endif
}

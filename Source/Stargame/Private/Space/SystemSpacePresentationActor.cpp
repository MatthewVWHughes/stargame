#include "Space/SystemSpacePresentationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASystemSpacePresentationActor::ASystemSpacePresentationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SystemOriginMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeSystemOriginMarker"));
	SystemOriginMesh->SetupAttachment(SceneRoot);
	SystemOriginMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SystemOriginMesh->SetGenerateOverlapEvents(false);
	SystemOriginMesh->SetCanEverAffectNavigation(false);
	SystemOriginMesh->SetRelativeScale3D(FVector(12.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		SystemOriginMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void ASystemSpacePresentationActor::ConfigureForSystem(FName InSystemId)
{
	SystemId = InSystemId;
	Tags.AddUnique(SystemId);
	Tags.AddUnique(TEXT("system_presentation"));
	OnConfiguredForSystem(InSystemId);

#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Presentation_%s"), *SystemId.ToString()));
#endif
}

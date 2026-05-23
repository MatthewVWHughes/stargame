#include "Space/CombatShotPresentationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void AddActorTagIfSet(AActor& Actor, FName Tag)
	{
		if (!Tag.IsNone())
		{
			Actor.Tags.AddUnique(Tag);
		}
	}
}

ACombatShotPresentationActor::ACombatShotPresentationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeShotBeamMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ACombatShotPresentationActor::ConfigureShotPresentation(FName InShotPresentationId, FName InPresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double InStartedAtTimeSeconds, double InDurationSeconds)
{
	Tags.Remove(ShotPresentationId);
	Tags.Remove(PresentationType);
	ShotPresentationId = InShotPresentationId;
	PresentationType = InPresentationType;
	StartedAtTimeSeconds = InStartedAtTimeSeconds;
	DurationSeconds = FMath::Max(0.0, InDurationSeconds);
	StoredStartPositionCm = StartPositionCm;
	StoredEndPositionCm = EndPositionCm;

	const FVector TraceVectorCm = EndPositionCm - StartPositionCm;
	const double TraceLengthCm = FMath::Max(TraceVectorCm.Size(), 1.0);
	const FVector MidpointCm = StartPositionCm + TraceVectorCm * 0.5;
	const FQuat Rotation = TraceVectorCm.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector).ToOrientationQuat();

	SetActorLocationAndRotation(MidpointCm, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorScale3D(FVector(TraceLengthCm / 100.0, 0.025, 0.025));

	AddActorTagIfSet(*this, ShotPresentationId);
	AddActorTagIfSet(*this, PresentationType);
	OnShotPresentationConfigured(ShotPresentationId, PresentationType, StartPositionCm, EndPositionCm, StartedAtTimeSeconds, DurationSeconds);
}

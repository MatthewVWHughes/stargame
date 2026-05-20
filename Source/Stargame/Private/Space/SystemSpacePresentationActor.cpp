#include "Space/SystemSpacePresentationActor.h"

#include "Components/SceneComponent.h"

ASystemSpacePresentationActor::ASystemSpacePresentationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ASystemSpacePresentationActor::ConfigureForSystem(FName InSystemId)
{
	SystemId = InSystemId;
	OnConfiguredForSystem(InSystemId);

#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Presentation_%s"), *SystemId.ToString()));
#endif
}

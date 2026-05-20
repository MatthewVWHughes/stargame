#include "Space/M0SystemMarkerActor.h"

#include "Components/SceneComponent.h"

AM0SystemMarkerActor::AM0SystemMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AM0SystemMarkerActor::InitializeMarker(FName InGameplayId, FName InEntityType, double VisualRadiusCm)
{
	GameplayId = InGameplayId;
	EntityType = InEntityType;
	OnMarkerInitialized(InGameplayId, InEntityType, VisualRadiusCm);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("M0_%s_%s"), *EntityType.ToString(), *GameplayId.ToString()));
#endif
}

#include "Station/StationInteriorInteractableActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AStationInteriorInteractableActor::AStationInteriorInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionArea = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(SceneRoot);
	InteractionArea->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	InteractionArea->SetCapsuleRadius(115.0f);
	InteractionArea->SetCapsuleHalfHeight(110.0f);
	InteractionArea->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	ServiceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ServiceMesh"));
	ServiceMesh->SetupAttachment(SceneRoot);
	ServiceMesh->SetRelativeLocation(FVector(0.0, 0.0, 55.0));
	ServiceMesh->SetRelativeScale3D(FVector(0.75, 0.75, 1.1));
	ServiceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ServiceMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AStationInteriorInteractableActor::ConfigureInteractable(FName InStationId, FName InInteractionType, const FText& InDisplayName)
{
	StationId = InStationId;
	InteractionType = InInteractionType;
	DisplayName = InDisplayName.IsEmpty() ? FText::FromName(InInteractionType) : InDisplayName;
	OnInteractableConfigured(StationId, InteractionType, DisplayName);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("StationInteractable_%s"), *InteractionType.ToString()));
#endif
}

void AStationInteriorInteractableActor::ConfigureInteractableFromDefaults(FName InStationId)
{
	ConfigureInteractable(InStationId, DefaultInteractionType, DefaultDisplayName);
}

void AStationInteriorInteractableActor::SetFocusedByPlayer(bool bInFocused)
{
	if (bFocusedByPlayer == bInFocused)
	{
		return;
	}

	bFocusedByPlayer = bInFocused;
	OnFocusedByPlayerChanged(bFocusedByPlayer);
}

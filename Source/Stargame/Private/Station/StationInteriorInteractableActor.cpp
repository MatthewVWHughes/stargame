#include "Station/StationInteriorInteractableActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"

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
}

void AStationInteriorInteractableActor::ConfigureInteractable(FName InStationId, FName InInteractionType, const FText& InDisplayName)
{
	StationId = InStationId;
	InteractionType = InInteractionType;
	DisplayName = InDisplayName.IsEmpty() ? FText::FromName(InInteractionType) : InDisplayName;
	OnInteractableConfigured(StationId, InteractionType, DisplayName);
	SetActorLabel(FString::Printf(TEXT("StationInteractable_%s"), *InteractionType.ToString()));
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

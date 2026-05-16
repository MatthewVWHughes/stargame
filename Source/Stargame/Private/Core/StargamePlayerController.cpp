#include "Core/StargamePlayerController.h"

AStargamePlayerController::AStargamePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AStargamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetIgnoreLookInput(false);
	SetIgnoreMoveInput(false);
	bShowMouseCursor = true;
}

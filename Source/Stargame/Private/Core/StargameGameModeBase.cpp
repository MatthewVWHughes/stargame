#include "Core/StargameGameModeBase.h"

#include "Core/StargamePlayerController.h"
#include "UI/StargameBootMenuWidget.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

AStargameGameModeBase::AStargameGameModeBase()
{
	PlayerControllerClass = AStargamePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void AStargameGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	UStargameBootMenuWidget* BootMenu = BootMenuWidgetClass
		? CreateWidget<UStargameBootMenuWidget>(PlayerController, BootMenuWidgetClass)
		: nullptr;
	if (!BootMenu)
	{
		return;
	}

	BootMenu->AddToViewport(100);
	BootMenu->RefreshContinueState();
	BootMenu->SetIsFocusable(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(BootMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->bShowMouseCursor = true;
}

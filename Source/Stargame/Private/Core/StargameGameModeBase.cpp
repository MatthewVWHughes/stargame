#include "Core/StargameGameModeBase.h"

#include "Core/StargamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "UI/StargameBootMenuWidget.h"

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
		if (bAutoStartWhenBootMenuMissing)
		{
			if (UStargameSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr)
			{
				Session->StartNewSession();
			}
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetIgnoreLookInput(false);
			PlayerController->SetIgnoreMoveInput(false);
			PlayerController->bShowMouseCursor = false;
		}
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

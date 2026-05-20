#include "UI/StargameBootMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Runtime/StargameSessionSubsystem.h"

void UStargameBootMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddUniqueDynamic(this, &UStargameBootMenuWidget::HandleNewGameClicked);
	}
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UStargameBootMenuWidget::HandleContinueClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UStargameBootMenuWidget::HandleQuitClicked);
	}

	RefreshContinueState();
}

void UStargameBootMenuWidget::RefreshContinueState()
{
	UStargameSessionSubsystem* Session = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>()
		: nullptr;

	FString Reason;
	const bool bCanContinue = Session && Session->CanContinueDevelopmentSlot(Reason);
	if (ContinueButton)
	{
		ContinueButton->SetIsEnabled(bCanContinue);
	}
	OnContinueAvailabilityChanged(bCanContinue);

	SetStatusText(Reason.IsEmpty() ? TEXT("Start a new frontier session.") : Reason, !bCanContinue && Session && Session->DoesDevelopmentSaveExist());
}

void UStargameBootMenuWidget::NewGame()
{
	UStargameSessionSubsystem* Session = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>()
		: nullptr;
	if (!Session)
	{
		SetStatusText(TEXT("Session subsystem is not available."), true);
		return;
	}

	const EStartSessionResult Result = Session->StartNewSession();
	if (Result != EStartSessionResult::Success)
	{
		SetStatusText(Session->GetLastSessionError().IsEmpty() ? TEXT("New game failed to start.") : Session->GetLastSessionError(), true);
		RefreshContinueState();
		return;
	}

	FinishBootFlow();
}

void UStargameBootMenuWidget::Continue()
{
	UStargameSessionSubsystem* Session = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>()
		: nullptr;
	if (!Session)
	{
		SetStatusText(TEXT("Session subsystem is not available."), true);
		return;
	}

	FString Reason;
	if (!Session->CanContinueDevelopmentSlot(Reason))
	{
		SetStatusText(Reason, true);
		RefreshContinueState();
		return;
	}

	if (!Session->LoadDevelopmentSlot())
	{
		SetStatusText(Session->GetLastSessionError().IsEmpty() ? TEXT("Save file could not be loaded.") : Session->GetLastSessionError(), true);
		RefreshContinueState();
		return;
	}

	FinishBootFlow();
}

void UStargameBootMenuWidget::Quit()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
}

void UStargameBootMenuWidget::HandleNewGameClicked()
{
	NewGame();
}

void UStargameBootMenuWidget::HandleContinueClicked()
{
	Continue();
}

void UStargameBootMenuWidget::HandleQuitClicked()
{
	Quit();
}

void UStargameBootMenuWidget::FinishBootFlow()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetIgnoreMoveInput(false);
	}

	RemoveFromParent();
}

void UStargameBootMenuWidget::SetStatusText(const FString& Status, bool bIsError)
{
	const FText StatusMessage = FText::FromString(Status);
	if (StatusText)
	{
		StatusText->SetText(StatusMessage);
	}

	OnStatusChanged(StatusMessage, bIsError);
}

#include "UI/StargameDialogWidget.h"

#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

namespace
{
	UStargameSessionSubsystem* ResolveSession(const UUserWidget* Widget)
	{
		return Widget && Widget->GetGameInstance()
			? Widget->GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>()
			: nullptr;
	}
}

void UStargameDialogChoiceAction::Configure(UStargameDialogWidget* InOwner, FName InChoiceId, bool bInCloseAfter)
{
	Owner = InOwner;
	ChoiceId = InChoiceId;
	bCloseAfter = bInCloseAfter;
}

void UStargameDialogChoiceAction::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectChoice(ChoiceId, bCloseAfter);
	}
}

void UStargameDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UStargameDialogWidget::ShowDialog(const FText& InTitle, const FString& InBody, const TArray<FStargameDialogChoiceView>& InChoices)
{
	bHasActiveMissionContact = false;
	LastSelectedChoiceId = NAME_None;
	LastStatusText.Reset();
	if (TitleText)
	{
		TitleText->SetText(InTitle);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(InBody));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::GetEmpty());
	}

	bDialogOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
	SetOwningPlayerInputLocked(true);
	OnDialogShown(InTitle, InBody, InChoices);
}

void UStargameDialogWidget::ShowMissionContact(const FDockedMissionContactOption& Contact)
{
	ActiveMissionContact = Contact;
	bHasActiveMissionContact = true;
	ShowDialog(Contact.MissionTitle.IsEmpty() ? Contact.DisplayName : Contact.MissionTitle, BuildMissionContactBody(Contact), BuildMissionContactChoices(Contact));
	bHasActiveMissionContact = true;
}

void UStargameDialogWidget::CloseDialog()
{
	bDialogOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	SetOwningPlayerInputLocked(false);
	OnDialogClosed();
}

FReply UStargameDialogWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialog();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FString UStargameDialogWidget::BuildMissionContactBody(const FDockedMissionContactOption& Contact)
{
	FString Body = Contact.DialogText;
	if (Body.IsEmpty())
	{
		Body = Contact.ContextText;
	}
	if (!Contact.ObjectiveSummaryText.IsEmpty())
	{
		Body += Body.IsEmpty() ? Contact.ObjectiveSummaryText : FString::Printf(TEXT("\n\n%s"), *Contact.ObjectiveSummaryText);
	}
	if (!Contact.CargoManifestSummaryText.IsEmpty())
	{
		Body += FString::Printf(TEXT("\n%s"), *Contact.CargoManifestSummaryText);
	}
	if (!Contact.RewardText.IsEmpty())
	{
		Body += FString::Printf(TEXT("\n\n%s"), *Contact.RewardText);
	}
	if (Body.IsEmpty())
	{
		Body = FString::Printf(TEXT("Mission state: %s"), *Contact.InteractionState.ToString());
	}
	return Body;
}

TArray<FStargameDialogChoiceView> UStargameDialogWidget::BuildMissionContactChoices(const FDockedMissionContactOption& Contact)
{
	TArray<FStargameDialogChoiceView> Choices;
	if (Contact.bCanAccept && !Contact.OfferId.IsNone())
	{
		Choices.Add({ TEXT("accept"), NSLOCTEXT("StargameDialog", "AcceptMission", "Accept Mission"), true, true });
	}
	if (Contact.bCanTurnIn && !Contact.MissionInstanceId.IsNone())
	{
		Choices.Add({ TEXT("turn_in"), NSLOCTEXT("StargameDialog", "TurnInMission", "Turn In Mission"), true, true });
	}
	if (Contact.bCanContinue)
	{
		Choices.Add({ TEXT("continue"), NSLOCTEXT("StargameDialog", "ContinueMission", "Continue"), true, true });
	}
	Choices.Add({ TEXT("close"), NSLOCTEXT("StargameDialog", "Close", "Close"), true, true });
	return Choices;
}

void UStargameDialogWidget::SelectChoice(FName ChoiceId, bool bCloseAfter)
{
	HandleChoiceSelected(ChoiceId, bCloseAfter);
}

void UStargameDialogWidget::HandleChoiceSelected(FName ChoiceId, bool bCloseAfter)
{
	LastSelectedChoiceId = ChoiceId;
	if (ChoiceId == FName(TEXT("close")) || ChoiceId == FName(TEXT("continue")))
	{
		if (bCloseAfter)
		{
			CloseDialog();
		}
		return;
	}

	if (bHasActiveMissionContact)
	{
		const bool bAccepted = ExecuteMissionChoice(ChoiceId);
		if (!bAccepted)
		{
			if (StatusText)
			{
				StatusText->SetText(FText::FromString(LastStatusText));
			}
			OnDialogStatusChanged(LastStatusText, true);
			return;
		}
	}

	if (bCloseAfter)
	{
		CloseDialog();
	}
}

bool UStargameDialogWidget::ExecuteMissionChoice(FName ChoiceId)
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		LastStatusText = TEXT("Mission action failed: no Stargame session.");
		return false;
	}

	FDockedStationCommandResult CommandResult;
	if (ChoiceId == FName(TEXT("accept")))
	{
		FMissionInstanceState Mission;
		const FName EventId = MakeDialogEventId(TEXT("dialog_accept"));
		const bool bAccepted = Session->AcceptDockedMissionOffer(ActiveMissionContact.OfferId, TEXT("player"), FName(*FString::Printf(TEXT("idem_%s"), *EventId.ToString())), Mission, CommandResult);
		LastStatusText = bAccepted ? TEXT("Mission accepted.") : CommandResult.FailureReason;
		return bAccepted;
	}
	if (ChoiceId == FName(TEXT("turn_in")))
	{
		FProgressionDebugLedgerEntry Completion;
		const FName EventId = MakeDialogEventId(TEXT("dialog_turn_in"));
		const bool bAccepted = Session->CompleteDockedMission(ActiveMissionContact.MissionInstanceId, EventId, FName(*FString::Printf(TEXT("idem_%s"), *EventId.ToString())), Completion, CommandResult);
		LastStatusText = bAccepted ? TEXT("Mission complete.") : CommandResult.FailureReason;
		return bAccepted;
	}

	LastStatusText = FString::Printf(TEXT("Mission choice '%s' has no command."), *ChoiceId.ToString());
	return false;
}

void UStargameDialogWidget::SetOwningPlayerInputLocked(bool bLocked)
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		if (bLocked)
		{
			InputMode.SetWidgetToFocus(TakeWidget());
		}
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetIgnoreLookInput(bLocked);
		PlayerController->SetIgnoreMoveInput(bLocked);
		PlayerController->bShowMouseCursor = true;
	}
}

FName UStargameDialogWidget::MakeDialogEventId(const TCHAR* Prefix)
{
	++DialogSequence;
	return FName(*FString::Printf(TEXT("%s_%d"), Prefix, DialogSequence));
}

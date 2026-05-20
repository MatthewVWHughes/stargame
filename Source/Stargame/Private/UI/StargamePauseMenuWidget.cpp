#include "UI/StargamePauseMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

namespace
{
	const FItemDefinition* FindItem(const FSystemicGameplayState& State, FName ItemId)
	{
		return State.Items.FindByPredicate([ItemId](const FItemDefinition& Item)
		{
			return Item.ItemId == ItemId;
		});
	}

	FString StackDisplay(const FSystemicGameplayState& State, const FItemStackState& Stack)
	{
		const FItemDefinition* Item = FindItem(State, Stack.ItemId);
		const FString Name = Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName.ToString() : Stack.ItemId.ToString();
		return Stack.Quantity > 1 ? FString::Printf(TEXT("%s x%d"), *Name, Stack.Quantity) : Name;
	}

	void AppendContainerSummary(const FSystemicGameplayState& State, FName ContainerId, const TCHAR* Title, FString& OutText, int32& OutCount)
	{
		const FContainerState* Container = State.Containers.FindByPredicate([ContainerId](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == ContainerId;
		});
		OutText += FString::Printf(TEXT("%s\n"), Title);
		if (!Container || Container->Stacks.IsEmpty())
		{
			OutText += TEXT("  (empty)\n");
			return;
		}
		OutCount = Container->Stacks.Num();
		for (const FItemStackState& Stack : Container->Stacks)
		{
			OutText += FString::Printf(TEXT("  %s\n"), *StackDisplay(State, Stack));
		}
	}
}

void UStargamePauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (InventoryTabButton)
	{
		InventoryTabButton->OnClicked.AddUniqueDynamic(this, &UStargamePauseMenuWidget::HandleInventoryTabClicked);
	}
	if (MissionsTabButton)
	{
		MissionsTabButton->OnClicked.AddUniqueDynamic(this, &UStargamePauseMenuWidget::HandleMissionsTabClicked);
	}
	if (SettingsTabButton)
	{
		SettingsTabButton->OnClicked.AddUniqueDynamic(this, &UStargamePauseMenuWidget::HandleSettingsTabClicked);
	}
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddUniqueDynamic(this, &UStargamePauseMenuWidget::HandleResumeClicked);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UStargamePauseMenuWidget::OpenPauseMenu(EStargamePauseMenuTab Tab)
{
	ActiveTab = Tab;
	bPauseMenuOpen = true;
	RefreshPauseMenu();
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
	SetOwningPlayerInputLocked(true);
	OnPauseMenuOpened(ActiveTab, CachedView);
}

void UStargamePauseMenuWidget::ClosePauseMenu()
{
	bPauseMenuOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	SetOwningPlayerInputLocked(false);
	OnPauseMenuClosed();
}

void UStargamePauseMenuWidget::TogglePauseMenu(EStargamePauseMenuTab Tab)
{
	bPauseMenuOpen ? ClosePauseMenu() : OpenPauseMenu(Tab);
}

void UStargamePauseMenuWidget::SetActiveTab(EStargamePauseMenuTab Tab)
{
	ActiveTab = Tab;
	RefreshPauseMenu();
	OnActiveTabChanged(ActiveTab, CachedView);
}

FReply UStargamePauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::I)
	{
		ClosePauseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FStargamePauseMenuView UStargamePauseMenuWidget::BuildPauseMenuView(const UStargameSessionSubsystem* Session)
{
	FStargamePauseMenuView View;
	if (!Session)
	{
		View.InventoryText = TEXT("No session inventory is available.");
		View.MissionsText = TEXT("No session mission state is available.");
		View.SettingsText = TEXT("Resume\nReturn to menu is handled by the boot flow for now.");
		return View;
	}

	const FSystemicGameplayState State = Session->GetSystemicGameplayState();
	AppendContainerSummary(State, TEXT("player_personal_inventory"), TEXT("Personal Inventory"), View.InventoryText, View.PersonalInventoryCount);
	View.InventoryText += TEXT("\n");
	AppendContainerSummary(State, TEXT("player_ship_cargo"), TEXT("Ship Cargo"), View.InventoryText, View.ShipCargoCount);
	View.InventoryText += TEXT("\nPlayer Loadout\n");
	for (const FEquipmentSlotState& Slot : State.EquipmentSlots)
	{
		if (Slot.OwnerId == FName(TEXT("player")))
		{
			View.InventoryText += FString::Printf(TEXT("  %s: %s\n"), *Slot.SlotId.ToString(), Slot.EquippedStack.ItemId.IsNone() ? TEXT("(empty)") : *StackDisplay(State, Slot.EquippedStack));
		}
	}
	View.InventoryText += TEXT("\nShip Loadout\n");
	for (const FEquipmentSlotState& Slot : State.EquipmentSlots)
	{
		if (Slot.OwnerId == FName(TEXT("player_ship")))
		{
			View.InventoryText += FString::Printf(TEXT("  %s: %s\n"), *Slot.SlotId.ToString(), Slot.EquippedStack.ItemId.IsNone() ? TEXT("(empty)") : *StackDisplay(State, Slot.EquippedStack));
		}
	}

	FMissionWaypointViewModel Waypoint;
	const bool bHasWaypoint = Session->GetActiveMissionWaypoint(Waypoint);
	for (const FMissionInstanceState& Mission : State.MissionInstances)
	{
		if (Mission.CurrentState == FName(TEXT("active")) || Mission.CurrentState == FName(TEXT("ready_to_turn_in")))
		{
			++View.ActiveMissionCount;
			View.MissionsText += FString::Printf(TEXT("%s\n  State: %s\n"), *Mission.MissionDefinitionId.ToString(), *Mission.CurrentState.ToString());
			if (bHasWaypoint && Waypoint.MissionInstanceId == Mission.MissionInstanceId)
			{
				View.MissionsText += FString::Printf(TEXT("  Objective: %s -> %s\n  Distance: %.1f km\n"), *Waypoint.ObjectiveType.ToString(), *Waypoint.TargetId.ToString(), Waypoint.DistanceCm / 100000.0);
				if (Waypoint.bReadyToTurnIn)
				{
					View.MissionsText += TEXT("  Ready to turn in.\n");
				}
			}
			View.MissionsText += TEXT("\n");
		}
	}
	if (View.MissionsText.IsEmpty())
	{
		View.MissionsText = TEXT("No active missions.");
	}

	View.SettingsText = TEXT("Settings\n  Resume returns to the current scene.\n  Main-menu return remains in the boot/menu flow until the broader frontend pass.");
	return View;
}

void UStargamePauseMenuWidget::RefreshPauseMenu()
{
	const UStargameSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	CachedView = BuildPauseMenuView(Session);
	if (!BodyText || !TitleText)
	{
		return;
	}

	if (ActiveTab == EStargamePauseMenuTab::Inventory)
	{
		TitleText->SetText(NSLOCTEXT("StargamePause", "InventoryTitle", "Inventory"));
		BodyText->SetText(FText::FromString(CachedView.InventoryText));
	}
	else if (ActiveTab == EStargamePauseMenuTab::Missions)
	{
		TitleText->SetText(NSLOCTEXT("StargamePause", "MissionsTitle", "Missions"));
		BodyText->SetText(FText::FromString(CachedView.MissionsText));
	}
	else
	{
		TitleText->SetText(NSLOCTEXT("StargamePause", "SettingsTitle", "Settings"));
		BodyText->SetText(FText::FromString(CachedView.SettingsText));
	}
}

void UStargamePauseMenuWidget::HandleInventoryTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Inventory);
}

void UStargamePauseMenuWidget::HandleMissionsTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Missions);
}

void UStargamePauseMenuWidget::HandleSettingsTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Settings);
}

void UStargamePauseMenuWidget::HandleResumeClicked()
{
	ClosePauseMenu();
}

void UStargamePauseMenuWidget::SetOwningPlayerInputLocked(bool bLocked)
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
		SetIsEnabled(true);
	}
}

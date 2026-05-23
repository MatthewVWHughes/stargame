#include "UI/StargamePauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

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

	const FLinearColor MenuBackground(0.012f, 0.018f, 0.028f, 0.94f);
	const FLinearColor PanelBackground(0.035f, 0.048f, 0.067f, 0.96f);
	const FLinearColor NavBackground(0.021f, 0.031f, 0.045f, 0.98f);
	const FLinearColor Accent(0.18f, 0.72f, 0.76f, 1.0f);
	const FLinearColor TextPrimary(0.86f, 0.91f, 0.94f, 1.0f);
	const FLinearColor TextMuted(0.48f, 0.57f, 0.62f, 1.0f);
	const FLinearColor Warning(0.86f, 0.62f, 0.22f, 1.0f);
}

TSharedRef<SWidget> UStargamePauseMenuWidget::RebuildWidget()
{
	auto MakeButton = [this](const FText& Label, const FText& Hint, TFunction<void()> Action, TAttribute<bool> IsEnabled = true)
	{
		return SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.055f, 0.073f, 0.096f, 1.0f))
			.ForegroundColor(FSlateColor(TextPrimary))
			.ContentPadding(FMargin(14.0f, 10.0f))
			.IsEnabled(IsEnabled)
			.ToolTipText(Hint)
			.OnClicked_Lambda([Action]()
			{
				Action();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				.ColorAndOpacity(FSlateColor(TextPrimary))
			];
	};

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(MenuBackground)
		.Padding(FMargin(42.0f))
		[
			SNew(SBox)
			.MaxDesiredWidth(1040.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(NavBackground)
					.Padding(FMargin(22.0f))
					[
						SNew(SBox)
						.WidthOverride(260.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 24.0f)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("StargamePause", "PauseTitle", "STARGAME"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
								.ColorAndOpacity(FSlateColor(Accent))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "ResumeButton", "Resume"), NSLOCTEXT("StargamePause", "ResumeHint", "Return to the current flight or station scene."), [this]() { ClosePauseMenu(); })
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "SaveButton", "Save Game"), NSLOCTEXT("StargamePause", "SaveHint", "Manual saves are available only while docked."), [this]() { HandleSaveClicked(); }, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(this, &UStargamePauseMenuWidget::IsSaveAllowed)))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "LoadButton", "Load Last Save"), NSLOCTEXT("StargamePause", "LoadHint", "Load the development slot if one exists."), [this]() { HandleLoadClicked(); }, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(this, &UStargamePauseMenuWidget::IsLoadAllowed)))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "SettingsButton", "Settings"), NSLOCTEXT("StargamePause", "SettingsHint", "View current display, audio, and input settings."), [this]() { ShowSettingsPage(); })
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "NewGameButton", "New Game"), NSLOCTEXT("StargamePause", "NewGameHint", "Restart from the default frontier test start."), [this]() { HandleNewGameClicked(); })
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SSpacer)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								MakeButton(NSLOCTEXT("StargamePause", "ExitButton", "Exit Game"), NSLOCTEXT("StargamePause", "ExitHint", "Close the running game instance."), [this]() { ExitGame(); })
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(18.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(PanelBackground)
					.Padding(FMargin(28.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(NativeTitleText, STextBlock)
							.Text(NSLOCTEXT("StargamePause", "MainPanelTitle", "Paused"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
							.ColorAndOpacity(FSlateColor(TextPrimary))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 6.0f, 0.0f, 16.0f)
						[
							SAssignNew(NativeContextText, STextBlock)
							.Text(NSLOCTEXT("StargamePause", "Context", "Session"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
							.ColorAndOpacity(FSlateColor(TextMuted))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 18.0f)
						[
							SNew(SSeparator)
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(NativeBodyText, STextBlock)
								.Text(FText::GetEmpty())
								.AutoWrapText(true)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
								.LineHeightPercentage(1.12f)
								.ColorAndOpacity(FSlateColor(TextPrimary))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 18.0f, 0.0f, 0.0f)
						[
							SAssignNew(NativeStatusText, STextBlock)
							.Text(NSLOCTEXT("StargamePause", "StatusReady", "Escape resumes. Manual saves require a docked ship."))
							.AutoWrapText(true)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
							.ColorAndOpacity(FSlateColor(Warning))
						]
					]
				]
			]
		];
}

void UStargamePauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplySettingsMenuChrome();
	EnsureRuntimeExitButton();
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
	if (ExitButton)
	{
		ExitButton->OnClicked.AddUniqueDynamic(this, &UStargamePauseMenuWidget::HandleExitClicked);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UStargamePauseMenuWidget::OpenPauseMenu(EStargamePauseMenuTab Tab)
{
	ActiveTab = EStargamePauseMenuTab::Settings;
	ActivePage = EStargamePauseMenuPage::Main;
	bPauseMenuOpen = true;
	RefreshPauseMenu();
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
	SetOwningPlayerInputLocked(true);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(true);
	}
	OnPauseMenuOpened(ActiveTab, CachedView);
}

void UStargamePauseMenuWidget::ClosePauseMenu()
{
	bPauseMenuOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
	}
	SetOwningPlayerInputLocked(false);
	OnPauseMenuClosed();
}

void UStargamePauseMenuWidget::TogglePauseMenu(EStargamePauseMenuTab Tab)
{
	bPauseMenuOpen ? ClosePauseMenu() : OpenPauseMenu(Tab);
}

void UStargamePauseMenuWidget::SetActiveTab(EStargamePauseMenuTab Tab)
{
	ActiveTab = EStargamePauseMenuTab::Settings;
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
		View.SettingsText = TEXT("Settings\n  Resume returns to the current scene.\n  Exit Game closes the game.");
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

	View.SettingsText = TEXT("Settings\n  Resume returns to the current scene.\n  Exit Game closes the game.");
	return View;
}

void UStargamePauseMenuWidget::RefreshPauseMenu()
{
	const UStargameSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	CachedView = BuildPauseMenuView(Session);
	if (NativeTitleText && NativeContextText && NativeBodyText && NativeStatusText)
	{
		if (ActivePage == EStargamePauseMenuPage::Settings)
		{
			NativeTitleText->SetText(NSLOCTEXT("StargamePause", "SettingsTitleNative", "Settings"));
			NativeBodyText->SetText(FText::FromString(BuildSettingsPanelText(Session)));
		}
		else
		{
			NativeTitleText->SetText(NSLOCTEXT("StargamePause", "MainTitleNative", "Paused"));
			NativeBodyText->SetText(FText::FromString(BuildMainPanelText(Session)));
		}

		FString Context = TEXT("No active session");
		if (Session)
		{
			Context = FString::Printf(TEXT("System %s  /  Start %s"),
				*Session->GetCurrentSystemId().ToString(),
				*Session->GetStartProfileId().ToString());
			FDockedStationContext DockedContext;
			if (Session->GetDockedStationContext(DockedContext) && DockedContext.bDocked)
			{
				Context += FString::Printf(TEXT("  /  Docked at %s"), *DockedContext.DisplayName.ToString());
			}
		}
		NativeContextText->SetText(FText::FromString(Context));

		if (NativeStatusText->GetText().IsEmpty())
		{
			SetMenuStatus(NSLOCTEXT("StargamePause", "StatusDefault", "Escape resumes. Manual saves require a docked ship."));
		}
	}
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

	if (FooterText)
	{
		FooterText->SetText(NSLOCTEXT("StargamePause", "SettingsFooter", "Esc/I: resume"));
	}
}

void UStargamePauseMenuWidget::HandleInventoryTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Settings);
}

void UStargamePauseMenuWidget::HandleMissionsTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Settings);
}

void UStargamePauseMenuWidget::HandleSettingsTabClicked()
{
	SetActiveTab(EStargamePauseMenuTab::Settings);
}

void UStargamePauseMenuWidget::HandleResumeClicked()
{
	ClosePauseMenu();
}

void UStargamePauseMenuWidget::HandleExitClicked()
{
	ExitGame();
}

UStargameSessionSubsystem* UStargamePauseMenuWidget::ResolveSession() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
}

bool UStargamePauseMenuWidget::IsSaveAllowed() const
{
	const UStargameSessionSubsystem* Session = ResolveSession();
	FDockedStationContext Context;
	return Session && Session->GetDockedStationContext(Context) && Context.bDocked;
}

bool UStargamePauseMenuWidget::IsLoadAllowed() const
{
	const UStargameSessionSubsystem* Session = ResolveSession();
	return Session && Session->DoesDevelopmentSaveExist();
}

FString UStargamePauseMenuWidget::BuildMainPanelText(const UStargameSessionSubsystem* Session) const
{
	if (!Session)
	{
		return TEXT("No session is active.\n\nStart a new game or load the development slot.");
	}

	const bool bCanSave = IsSaveAllowed();
	FString ContinueReason;
	const bool bCanLoad = Session->CanContinueDevelopmentSlot(ContinueReason);

	FString Text;
	Text += TEXT("Game Menu\n");
	Text += TEXT("  Resume returns to the current scene and unpauses simulation.\n");
	Text += bCanSave
		? TEXT("  Save Game is available because the ship is docked.\n")
		: TEXT("  Save Game is locked until the ship is docked.\n");
	Text += bCanLoad
		? TEXT("  Load Last Save is available.\n")
		: FString::Printf(TEXT("  Load Last Save is unavailable: %s\n"), ContinueReason.IsEmpty() ? TEXT("no compatible save found") : *ContinueReason);
	Text += TEXT("\nSession\n");
	Text += FString::Printf(TEXT("  Clock: %.1f seconds\n"), Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds);
	Text += FString::Printf(TEXT("  Last save: %s\n"), Session->GetLastAutosaveStatus().IsEmpty() ? TEXT("(none this session)") : *Session->GetLastAutosaveStatus());
	Text += TEXT("\nControls\n");
	Text += TEXT("  Escape / I: resume\n");
	return Text;
}

FString UStargamePauseMenuWidget::BuildSettingsPanelText(const UStargameSessionSubsystem* Session) const
{
	FString Text;
	Text += TEXT("Settings\n");
	Text += TEXT("  Display: windowed test build\n");
	Text += TEXT("  Resolution: launcher controlled\n");
	Text += TEXT("  Audio: launcher controlled\n");
	Text += TEXT("  Input: mouse and keyboard\n");
	Text += TEXT("\nPlanned\n");
	Text += TEXT("  Sliders for master audio, look sensitivity, and UI scale.\n");
	Text += TEXT("  Toggles for flight assist helpers and HUD verbosity.\n");
	Text += TEXT("  Rebindable controls once input moves to the final Enhanced Input layer.\n");
	Text += TEXT("\nThis page is intentionally calm and readable for now; the controls are placeholders until the settings backend exists.");
	return Text;
}

void UStargamePauseMenuWidget::SetMenuStatus(const FText& StatusText)
{
	if (NativeStatusText)
	{
		NativeStatusText->SetText(StatusText);
	}
}

void UStargamePauseMenuWidget::ShowMainPage()
{
	ActivePage = EStargamePauseMenuPage::Main;
	SetMenuStatus(NSLOCTEXT("StargamePause", "StatusMain", "Escape resumes. Manual saves require a docked ship."));
	RefreshPauseMenu();
}

void UStargamePauseMenuWidget::ShowSettingsPage()
{
	ActivePage = EStargamePauseMenuPage::Settings;
	SetMenuStatus(NSLOCTEXT("StargamePause", "StatusSettings", "Settings are display-only until the settings backend is added."));
	RefreshPauseMenu();
}

void UStargamePauseMenuWidget::HandleSaveClicked()
{
	UStargameSessionSubsystem* Session = ResolveSession();
	if (!Session || !IsSaveAllowed())
	{
		SetMenuStatus(NSLOCTEXT("StargamePause", "SaveBlocked", "Save blocked: dock at a station before making a manual save."));
		return;
	}

	const bool bSaved = Session->SaveDevelopmentSlot();
	SetMenuStatus(bSaved
		? FText::FromString(Session->GetLastAutosaveStatus())
		: FText::FromString(Session->GetLastSessionError().IsEmpty() ? TEXT("Save failed.") : Session->GetLastSessionError()));
	RefreshPauseMenu();
}

void UStargamePauseMenuWidget::HandleLoadClicked()
{
	UStargameSessionSubsystem* Session = ResolveSession();
	if (!Session || !Session->DoesDevelopmentSaveExist())
	{
		SetMenuStatus(NSLOCTEXT("StargamePause", "LoadBlocked", "Load blocked: no development save exists."));
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
	}
	const bool bLoaded = Session->LoadDevelopmentSlot();
	if (bLoaded)
	{
		ClosePauseMenu();
	}
	else
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->SetPause(true);
		}
		SetMenuStatus(FText::FromString(Session->GetLastSessionError().IsEmpty() ? TEXT("Load failed.") : Session->GetLastSessionError()));
		RefreshPauseMenu();
	}
}

void UStargamePauseMenuWidget::HandleNewGameClicked()
{
	UStargameSessionSubsystem* Session = ResolveSession();
	if (!Session)
	{
		SetMenuStatus(NSLOCTEXT("StargamePause", "NewGameBlocked", "New game blocked: no session subsystem is available."));
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
	}
	const EStartSessionResult Result = Session->StartNewSession();
	if (Result == EStartSessionResult::Success)
	{
		ClosePauseMenu();
	}
	else
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->SetPause(true);
		}
		SetMenuStatus(FText::FromString(Session->GetLastSessionError().IsEmpty() ? TEXT("New game failed.") : Session->GetLastSessionError()));
		RefreshPauseMenu();
	}
}

void UStargamePauseMenuWidget::ApplySettingsMenuChrome()
{
	if (InventoryTabButton)
	{
		InventoryTabButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MissionsTabButton)
	{
		MissionsTabButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SettingsTabButton)
	{
		SettingsTabButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UStargamePauseMenuWidget::EnsureRuntimeExitButton()
{
	if (ExitButton || !WidgetTree || !ResumeButton)
	{
		return;
	}

	UPanelWidget* ParentPanel = Cast<UPanelWidget>(ResumeButton->GetParent());
	if (!ParentPanel)
	{
		return;
	}

	UButton* CreatedExitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExitButton"));
	UTextBlock* CreatedExitLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExitButtonLabel"));
	if (!CreatedExitButton || !CreatedExitLabel)
	{
		return;
	}

	CreatedExitLabel->SetText(NSLOCTEXT("StargamePause", "RuntimeExitButton", "Exit Game"));
	CreatedExitButton->SetContent(CreatedExitLabel);
	ParentPanel->AddChild(CreatedExitButton);
	ExitButton = CreatedExitButton;
}

void UStargamePauseMenuWidget::ExitGame()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
	}
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
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

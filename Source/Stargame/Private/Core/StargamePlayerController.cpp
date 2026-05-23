#include "Core/StargamePlayerController.h"

#include "Components/InputComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Flight/SpaceFlightPawn.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UI/StargameBootMenuWidget.h"
#include "UI/StargameCommsWidget.h"
#include "UI/StargameFlightHUDWidget.h"
#include "UI/StargamePauseMenuWidget.h"
#include "UI/StargameSystemMapWidget.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugStationHotkeys(
		TEXT("stargame.DebugStationHotkeys"),
		0,
		TEXT("Enables legacy docked-station number-key service, market, mission, and undock shortcuts for debugging."));

	UStargameSessionSubsystem* ResolveSession(const APlayerController* PlayerController)
	{
		const UGameInstance* GameInstance = PlayerController ? PlayerController->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}

	void LogStationCommandResult(const TCHAR* CommandName, bool bAccepted, const FDockedStationCommandResult& Result)
	{
		UE_LOG(LogTemp, Display, TEXT("%s %s Station=%s Command=%s Reason=%s"),
			CommandName,
			bAccepted ? TEXT("accepted") : TEXT("rejected"),
			*Result.StationId.ToString(),
			*Result.CommandId.ToString(),
			*Result.FailureReason);
	}

	FDockedStationCommandResult MakeRejectedDockedCommandResult(FName StationId, FName CommandId, const FString& FailureReason)
	{
		FDockedStationCommandResult Result;
		Result.bAccepted = false;
		Result.StationId = StationId;
		Result.CommandId = CommandId;
		Result.FailureReason = FailureReason;
		return Result;
	}

	void RemoveBootMenuWidgets(APlayerController* PlayerController)
	{
		if (!PlayerController)
		{
			return;
		}

		TArray<UUserWidget*> BootMenus;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, BootMenus, UStargameBootMenuWidget::StaticClass(), false);
		for (UUserWidget* Widget : BootMenus)
		{
			if (Widget)
			{
				Widget->RemoveFromParent();
			}
		}
	}
}

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

	SyncFlightHUDForPossessedPawn();
}

void AStargamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SyncFlightHUDForPossessedPawn();
}

void AStargamePlayerController::OnUnPossess()
{
	Super::OnUnPossess();
	SyncFlightHUDForPossessedPawn();
}

void AStargamePlayerController::SyncFlightHUDForPossessedPawn()
{
	const bool bHasFlightPawn = Cast<ASpaceFlightPawn>(GetPawn()) != nullptr;
	if (!bHasFlightPawn)
	{
		if (FlightHUDWidget)
		{
			FlightHUDWidget->RemoveFromParent();
			FlightHUDWidget = nullptr;
		}
		return;
	}

	if (!FlightHUDWidget && FlightHUDWidgetClass)
	{
		FlightHUDWidget = CreateWidget<UStargameFlightHUDWidget>(this, FlightHUDWidgetClass);
		if (FlightHUDWidget)
		{
			FlightHUDWidget->AddToViewport(20);
			FlightHUDWidget->RefreshFlightHUD();
		}
	}
}

void AStargamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAction(TEXT("StationRepair"), IE_Pressed, this, &AStargamePlayerController::StationRepair);
	InputComponent->BindAction(TEXT("StationRefuel"), IE_Pressed, this, &AStargamePlayerController::StationRefuel);
	InputComponent->BindAction(TEXT("StationBuyOre"), IE_Pressed, this, &AStargamePlayerController::StationBuyOre);
	InputComponent->BindAction(TEXT("StationAcceptMission"), IE_Pressed, this, &AStargamePlayerController::StationAcceptMission);
	InputComponent->BindAction(TEXT("StationCompleteMission"), IE_Pressed, this, &AStargamePlayerController::StationCompleteMission);
	InputComponent->BindAction(TEXT("StationUndock"), IE_Pressed, this, &AStargamePlayerController::StationUndock);
	InputComponent->BindAction(TEXT("EncounterEscape"), IE_Pressed, this, &AStargamePlayerController::EncounterEscape);
	InputComponent->BindAction(TEXT("EncounterSurrender"), IE_Pressed, this, &AStargamePlayerController::EncounterSurrender);
	InputComponent->BindAction(TEXT("EncounterPatrolResponse"), IE_Pressed, this, &AStargamePlayerController::EncounterPatrolResponse);
	InputComponent->BindAction(TEXT("EncounterFireWeapon"), IE_Pressed, this, &AStargamePlayerController::EncounterFireWeapon);
	InputComponent->BindAction(TEXT("ToggleSystemMap"), IE_Pressed, this, &AStargamePlayerController::ToggleSystemMap);
	InputComponent->BindAction(TEXT("ToggleComms"), IE_Pressed, this, &AStargamePlayerController::ToggleComms);
	InputComponent->BindAction(TEXT("TogglePauseMenu"), IE_Pressed, this, &AStargamePlayerController::TogglePauseMenu);
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AStargamePlayerController::RequestSupercruise);
}

void AStargamePlayerController::RecordDockedStationCommandResult(const FDockedStationCommandResult& Result)
{
	LastDockedStationCommandResult = Result;
	bHasLastDockedStationCommandResult = true;
}

void AStargamePlayerController::NewGame()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("NewGame rejected: session subsystem is unavailable."));
		return;
	}

	const EStartSessionResult Result = Session->StartNewSession();
	UE_LOG(LogTemp, Display, TEXT("NewGame %s System=%s Spawn=%s Reason=%s"),
		Result == EStartSessionResult::Success ? TEXT("accepted") : TEXT("rejected"),
		*Session->GetCurrentSystemId().ToString(),
		*Session->GetCurrentSpawnZoneId().ToString(),
		*Session->GetLastSessionError());

	if (Result == EStartSessionResult::Success)
	{
		RemoveBootMenuWidgets(this);
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);
		bShowMouseCursor = false;
		SyncFlightHUDForPossessedPawn();
	}
}

void AStargamePlayerController::ContinueGame()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("ContinueGame rejected: session subsystem is unavailable."));
		return;
	}

	const bool bLoaded = Session->LoadDevelopmentSlot();
	UE_LOG(LogTemp, Display, TEXT("ContinueGame %s System=%s Spawn=%s Reason=%s"),
		bLoaded ? TEXT("accepted") : TEXT("rejected"),
		*Session->GetCurrentSystemId().ToString(),
		*Session->GetCurrentSpawnZoneId().ToString(),
		*Session->GetLastSessionError());

	if (bLoaded)
	{
		RemoveBootMenuWidgets(this);
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);
		bShowMouseCursor = false;
		SyncFlightHUDForPossessedPawn();
	}
}

void AStargamePlayerController::SaveGame()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveGame rejected: session subsystem is unavailable."));
		return;
	}

	const bool bSaved = Session->SaveDevelopmentSlot();
	UE_LOG(LogTemp, Display, TEXT("SaveGame %s Status=%s Reason=%s"),
		bSaved ? TEXT("accepted") : TEXT("rejected"),
		*Session->GetLastAutosaveStatus(),
		*Session->GetLastSessionError());
}

void AStargamePlayerController::RequestSupercruise()
{
	ASpaceFlightPawn* FlightPawn = Cast<ASpaceFlightPawn>(GetPawn());
	if (!FlightPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestSupercruise ignored: possessed pawn is not a space flight pawn."));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("RequestSupercruise key accepted by player controller."));
	FlightPawn->RequestSupercruise();
}

void AStargamePlayerController::EnterStation()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->EnterDockedStationInterior(CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("EnterStation"), bAccepted, CommandResult);
}

bool AStargamePlayerController::AllowDebugStationHotkey(const TCHAR* CommandName)
{
	if (CVarDebugStationHotkeys.GetValueOnGameThread() != 0)
	{
		return true;
	}

	FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(NAME_None, MakeDockedCommandId(TEXT("cmd_station_debug_hotkey")), TEXT("Station hotkeys are debug-only. Use the station interior walk-up interactions."));
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(CommandName, false, CommandResult);
	return false;
}

FName AStargamePlayerController::MakeDockedCommandId(const TCHAR* Prefix)
{
	++DockedCommandSequence;
	return FName(*FString::Printf(TEXT("%s_%d"), Prefix, DockedCommandSequence));
}

void AStargamePlayerController::ExecuteDockedServiceByType(FName ServiceType, double Amount)
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session || ServiceType.IsNone())
	{
		return;
	}

	FDockedStationCommandPanelView Panel;
	if (!Session->GetDockedStationCommandPanel(Panel) || !Panel.bDocked)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(NAME_None, MakeDockedCommandId(TEXT("cmd_station_service")), TEXT("No docked station service panel is available."));
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationService"), false, CommandResult);
		return;
	}

	const FDockedStationCommandOption* ServiceCommand = Panel.Commands.FindByPredicate([ServiceType](const FDockedStationCommandOption& Command)
	{
		return Command.CommandType == FName(TEXT("service")) && Command.TargetId == ServiceType && Command.bAvailable;
	});
	if (!ServiceCommand)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(Panel.StationId, MakeDockedCommandId(TEXT("cmd_station_service")), FString::Printf(TEXT("No available %s service at the docked station."), *ServiceType.ToString()));
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationService"), false, CommandResult);
		return;
	}

	const FName RequestId = MakeDockedCommandId(TEXT("tx_station_service"));
	const FSystemicGameplayState State = Session->GetSystemicGameplayState();
	const FStationServiceEndpointDefinition* Endpoint = State.StationServiceEndpoints.FindByPredicate([ServiceCommand](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == ServiceCommand->SourceId;
	});
	const int64 TotalCost = Endpoint ? static_cast<int64>(FMath::RoundToDouble(Amount * static_cast<double>(Endpoint->UnitPriceCredits))) : 0;
	FStationServiceRequest Request;
	Request.RequestId = RequestId;
	Request.ServiceEndpointId = ServiceCommand->SourceId;
	Request.ServiceType = ServiceType;
	Request.TargetShipId = TEXT("player_ship");
	Request.DebitAccountId = TEXT("account_player");
	Request.CreditAccountId = Endpoint ? Endpoint->CreditAccountId : FName(TEXT("account_brink_watch_services"));
	Request.Amount = Amount;
	Request.TotalCost = TotalCost;
	Request.SourceEventId = FName(*FString::Printf(TEXT("event_%s"), *RequestId.ToString()));
	Request.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *RequestId.ToString()));

	FStationServiceResultRecord Result;
	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->ExecuteDockedStationService(Request, Result, CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("StationService"), bAccepted, CommandResult);
}

void AStargamePlayerController::StationRepair()
{
	if (!AllowDebugStationHotkey(TEXT("StationRepair")))
	{
		return;
	}
	ExecuteDockedServiceByType(TEXT("repair"), 10.0);
}

void AStargamePlayerController::ToggleSystemMap()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session || Session->IsInStationInterior())
	{
		return;
	}

	if (!SystemMapWidget && SystemMapWidgetClass)
	{
		SystemMapWidget = CreateWidget<UStargameSystemMapWidget>(this, SystemMapWidgetClass);
		if (SystemMapWidget)
		{
			SystemMapWidget->AddToViewport(60);
		}
	}

	if (SystemMapWidget)
	{
		SystemMapWidget->ToggleMap();
	}
}

void AStargamePlayerController::ToggleComms()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session || Session->IsInStationInterior())
	{
		return;
	}

	if (!CommsWidget && CommsWidgetClass)
	{
		CommsWidget = CreateWidget<UStargameCommsWidget>(this, CommsWidgetClass);
		if (CommsWidget)
		{
			CommsWidget->AddToViewport(58);
		}
	}

	if (CommsWidget)
	{
		CommsWidget->ToggleComms();
	}
}

void AStargamePlayerController::TogglePauseMenu()
{
	if (!PauseMenuWidget && PauseMenuWidgetClass)
	{
		PauseMenuWidget = CreateWidget<UStargamePauseMenuWidget>(this, PauseMenuWidgetClass);
		if (PauseMenuWidget)
		{
			PauseMenuWidget->AddToViewport(70);
		}
	}

	if (PauseMenuWidget)
	{
		PauseMenuWidget->TogglePauseMenu(EStargamePauseMenuTab::Settings);
	}
}

void AStargamePlayerController::StationRefuel()
{
	if (!AllowDebugStationHotkey(TEXT("StationRefuel")))
	{
		return;
	}
	ExecuteDockedServiceByType(TEXT("refuel"), 10.0);
}

void AStargamePlayerController::StationBuyOre()
{
	if (!AllowDebugStationHotkey(TEXT("StationBuyOre")))
	{
		return;
	}

	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	FDockedMarketView Market;
	if (!Session->GetDockedMarketView(Market) || !Market.bAvailable)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(NAME_None, MakeDockedCommandId(TEXT("cmd_market_buy")), Market.DebugReason.IsEmpty() ? TEXT("No docked market is available.") : Market.DebugReason);
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationBuyMarketItem"), false, CommandResult);
		return;
	}

	const FDockedMarketCommodityView* Quote = Market.Commodities.FindByPredicate([](const FDockedMarketCommodityView& Commodity)
	{
		return Commodity.bCanBuy;
	});
	if (!Quote)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(Market.StationId, MakeDockedCommandId(TEXT("cmd_market_buy")), TEXT("No market commodity can be bought right now."));
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationBuyMarketItem"), false, CommandResult);
		return;
	}

	const FName TransactionId = MakeDockedCommandId(TEXT("tx_station_buy"));
	FMarketTransactionRequest Request;
	Request.TransactionId = TransactionId;
	Request.MarketId = Market.MarketId;
	Request.BuyerId = TEXT("player");
	Request.SellerId = Market.StationId;
	Request.CommodityId = Quote->CommodityId;
	Request.CommodityItemBridgeId = Quote->CommodityItemBridgeId;
	Request.Quantity = 1;
	Request.QuotedUnitPrice = Quote->UnitPrice;
	Request.SourceContainerId = Market.SourceContainerId;
	Request.DestinationContainerId = Quote->BuyDestinationContainerId;
	Request.DebitAccountId = TEXT("account_player");
	Request.CreditAccountId = Market.MarketCreditAccountId;
	Request.LegalContextId = Market.LegalContextId;
	Request.SourceEventId = FName(*FString::Printf(TEXT("event_%s"), *TransactionId.ToString()));
	Request.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *TransactionId.ToString()));

	FMarketTransactionResult Result;
	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->ExecuteDockedMarketTransaction(Request, Result, CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("StationBuyMarketItem"), bAccepted, CommandResult);
}

void AStargamePlayerController::StationAcceptMission()
{
	if (!AllowDebugStationHotkey(TEXT("StationAcceptMission")))
	{
		return;
	}

	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	FDockedMissionContactPanelView ContactPanel;
	if (!Session->GetDockedMissionContactPanel(ContactPanel) || !ContactPanel.bAvailable)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(NAME_None, MakeDockedCommandId(TEXT("cmd_accept_mission")), ContactPanel.DebugReason.IsEmpty() ? TEXT("No docked mission contact panel is available.") : ContactPanel.DebugReason);
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationAcceptMission"), false, CommandResult);
		return;
	}

	const FDockedMissionContactOption* Contact = ContactPanel.Contacts.FindByPredicate([](const FDockedMissionContactOption& Candidate)
	{
		return Candidate.bCanAccept && !Candidate.OfferId.IsNone();
	});
	if (!Contact)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(ContactPanel.StationId, MakeDockedCommandId(TEXT("cmd_accept_mission")), TEXT("No mission offer is available to accept at this station."));
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationAcceptMission"), false, CommandResult);
		return;
	}

	const FName CommandId = MakeDockedCommandId(TEXT("tx_accept_mission"));
	FMissionInstanceState Mission;
	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->AcceptDockedMissionOffer(Contact->OfferId, TEXT("player"), FName(*FString::Printf(TEXT("idem_%s"), *CommandId.ToString())), Mission, CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("StationAcceptMission"), bAccepted, CommandResult);
}

void AStargamePlayerController::StationCompleteMission()
{
	if (!AllowDebugStationHotkey(TEXT("StationCompleteMission")))
	{
		return;
	}

	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	FDockedMissionContactPanelView ContactPanel;
	if (!Session->GetDockedMissionContactPanel(ContactPanel) || !ContactPanel.bAvailable)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(NAME_None, MakeDockedCommandId(TEXT("cmd_complete_mission")), ContactPanel.DebugReason.IsEmpty() ? TEXT("No docked mission contact panel is available.") : ContactPanel.DebugReason);
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationCompleteMission"), false, CommandResult);
		return;
	}

	const FDockedMissionContactOption* Contact = ContactPanel.Contacts.FindByPredicate([](const FDockedMissionContactOption& Candidate)
	{
		return Candidate.bCanTurnIn && !Candidate.MissionInstanceId.IsNone();
	});
	if (!Contact)
	{
		FDockedStationCommandResult CommandResult = MakeRejectedDockedCommandResult(ContactPanel.StationId, MakeDockedCommandId(TEXT("cmd_complete_mission")), TEXT("No mission is ready to turn in at this station."));
		RecordDockedStationCommandResult(CommandResult);
		LogStationCommandResult(TEXT("StationCompleteMission"), false, CommandResult);
		return;
	}

	const FName CommandId = MakeDockedCommandId(TEXT("tx_complete_mission"));
	FProgressionDebugLedgerEntry CompletionEntry;
	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->CompleteDockedMission(Contact->MissionInstanceId, FName(*FString::Printf(TEXT("event_%s"), *CommandId.ToString())), FName(*FString::Printf(TEXT("idem_%s"), *CommandId.ToString())), CompletionEntry, CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("StationCompleteMission"), bAccepted, CommandResult);
}

void AStargamePlayerController::StationUndock()
{
	if (!AllowDebugStationHotkey(TEXT("StationUndock")))
	{
		return;
	}

	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->RequestDockedUndock(CommandResult);
	RecordDockedStationCommandResult(CommandResult);
	LogStationCommandResult(TEXT("StationUndock"), bAccepted, CommandResult);
}

void AStargamePlayerController::EncounterEscape()
{
	ApplyEncounterOutcome(TEXT("escape"));
}

void AStargamePlayerController::EncounterSurrender()
{
	ApplyEncounterOutcome(TEXT("surrender"));
}

void AStargamePlayerController::EncounterPatrolResponse()
{
	ApplyEncounterOutcome(TEXT("patrol_response"));
}

void AStargamePlayerController::EncounterFireWeapon()
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session)
	{
		return;
	}

	Session->TryUpdateRuntimeEncounterBehavior();
	const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
	FShipWeaponFireResult Result;
	const bool bAccepted = Session->FireEquippedShipWeaponAtRuntimeEncounter(Encounter.EncounterId, Result);
	UE_LOG(LogTemp, Display, TEXT("EncounterFireWeapon %s Encounter=%s Weapon=%s Target=%s Damage=%.1f State=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Result.EncounterId.ToString(),
		*Result.WeaponItemId.ToString(),
		*Result.TargetCombatantId.ToString(),
		Result.DamageAmount,
		*Result.TargetDurabilityState.ToString(),
		*Result.FailureReason);
}

void AStargamePlayerController::ApplyEncounterOutcome(FName OutcomeType)
{
	UStargameSessionSubsystem* Session = ResolveSession(this);
	if (!Session || OutcomeType.IsNone())
	{
		return;
	}

	Session->TryUpdateRuntimeEncounterBehavior();
	const FRuntimeEncounterViewModel Encounter = Session->GetRuntimeEncounterView();
	if (!Encounter.bPirateAmbushActive || Encounter.EncounterId.IsNone())
	{
		UE_LOG(LogTemp, Display, TEXT("EncounterOutcome rejected Outcome=%s Reason=%s"),
			*OutcomeType.ToString(),
			*Encounter.DebugReason);
		return;
	}

	const bool bAccepted = Session->ApplyRuntimeEncounterOutcome(Encounter.EncounterId, OutcomeType);
	const FRuntimeEncounterViewModel Result = Session->GetRuntimeEncounterView();
	UE_LOG(LogTemp, Display, TEXT("EncounterOutcome %s Encounter=%s Outcome=%s State=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Encounter.EncounterId.ToString(),
		*OutcomeType.ToString(),
		*Result.TargetDurabilityState.ToString(),
		*Result.DebugReason);
}

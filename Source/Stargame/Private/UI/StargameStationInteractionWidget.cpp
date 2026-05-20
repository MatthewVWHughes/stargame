#include "UI/StargameStationInteractionWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Station/StationInteriorPawn.h"

namespace
{
	UStargameSessionSubsystem* ResolveSession(const UUserWidget* Widget)
	{
		return Widget && Widget->GetGameInstance()
			? Widget->GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>()
			: nullptr;
	}

	FString BuildCommandPanelSummary(const FDockedStationCommandPanelView& Panel)
	{
		FString Summary = Panel.SummaryText;
		if (Summary.IsEmpty())
		{
			Summary = FString::Printf(TEXT("Docked at %s."), *Panel.StationId.ToString());
		}

		Summary += FString::Printf(
			TEXT("\nCredits: %lld\nHull: %.0f / %.0f\nShields: %.0f / %.0f\nServices: %d  Markets: %d  Missions: %d"),
			Panel.PlayerCredits,
			Panel.Hull,
			Panel.MaxHull,
			Panel.Shields,
			Panel.MaxShields,
			Panel.AvailableServiceCount,
			Panel.AvailableMarketCount,
			Panel.AvailableMissionCount);
		return Summary;
	}

}

void UStargameStationMarketAction::Configure(UStargameStationInteractionWidget* InOwner, FName InCommodityId, bool bInBuy)
{
	Owner = InOwner;
	CommodityId = InCommodityId;
	bBuy = bInBuy;
}

void UStargameStationMarketAction::HandleClicked()
{
	if (Owner)
	{
		Owner->ExecuteMarketAction(CommodityId, bBuy);
	}
}

void UStargameStationInteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (PrimaryButton)
	{
		PrimaryButton->OnClicked.AddUniqueDynamic(this, &UStargameStationInteractionWidget::HandlePrimaryClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UStargameStationInteractionWidget::HandleCloseClicked);
	}
}

FReply UStargameStationInteractionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePanel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UStargameStationInteractionWidget::ShowStationObject(AStationInteriorPawn* InStationPawn, FName InInteractionType, const FText& InDisplayName)
{
	StationPawn = InStationPawn;
	InteractionType = InInteractionType;
	bShowingMissionContact = false;
	GiverNpcId = NAME_None;

	RefreshStationObject();
	if (TitleText && !InDisplayName.IsEmpty())
	{
		TitleText->SetText(InDisplayName);
	}
}

void UStargameStationInteractionWidget::ShowMissionContact(AStationInteriorPawn* InStationPawn, FName InGiverNpcId, const FText& InDisplayName)
{
	StationPawn = InStationPawn;
	GiverNpcId = InGiverNpcId;
	bShowingMissionContact = true;
	InteractionType = NAME_None;

	RefreshMissionContact();
	if (TitleText && !InDisplayName.IsEmpty())
	{
		TitleText->SetText(InDisplayName);
	}
}

void UStargameStationInteractionWidget::RefreshStationObject()
{
	ClearActionButtons();

	UStargameSessionSubsystem* Session = ResolveSession(this);
	FDockedStationCommandPanelView Panel;
	if (!Session || !Session->GetDockedStationCommandPanel(Panel) || !Panel.bDocked)
	{
		SetPanelText(NSLOCTEXT("StationInteraction", "StationUnavailable", "Station Services"), TEXT("Station services are not available."), TEXT("No docked station context."), true);
		AddActionButton(NSLOCTEXT("StationInteraction", "Unavailable", "Unavailable"), false);
		return;
	}

	if (InteractionType == FName(TEXT("repair")))
	{
		SetPanelText(NSLOCTEXT("StationInteraction", "RepairTitle", "Repair Service"), BuildCommandPanelSummary(Panel));
		AddActionButton(NSLOCTEXT("StationInteraction", "RepairAction", "Repair hull and shields"), true);
		return;
	}
	if (InteractionType == FName(TEXT("refuel")))
	{
		SetPanelText(NSLOCTEXT("StationInteraction", "RefuelTitle", "Refuel Service"), BuildCommandPanelSummary(Panel));
		AddActionButton(NSLOCTEXT("StationInteraction", "RefuelAction", "Buy fuel"), true);
		return;
	}
	if (InteractionType == FName(TEXT("market")))
	{
		FDockedMarketView Market;
		FString Body = TEXT("Commodity desk is not available.");
		if (Session->GetDockedMarketView(Market) && Market.bAvailable)
		{
			Body = Market.SummaryText.IsEmpty()
				? FString::Printf(TEXT("Market %s. Credits: %lld."), *Market.MarketId.ToString(), Market.PlayerCredits)
				: Market.SummaryText;
			Body += FString::Printf(TEXT("\nCredits: %lld  Cargo stacks: %d"), Market.PlayerCredits, Market.ShipCargoStacks);
			for (const FDockedMarketCommodityView& Commodity : Market.Commodities)
			{
				const FText DisplayName = Commodity.DisplayName.IsEmpty() ? FText::FromName(Commodity.CommodityId) : Commodity.DisplayName;
				Body += FString::Printf(
					TEXT("\n%s | stock %d | price %lld cr | cargo %d | inventory %d | %s"),
					*DisplayName.ToString(),
					Commodity.Stock,
					Commodity.UnitPrice,
					Commodity.PlayerCargoQuantity,
					Commodity.PlayerPersonalInventoryQuantity,
					*Commodity.LocalMarketState.ToString());

				AddMarketActionButton(
					FText::Format(NSLOCTEXT("StationInteraction", "BuyCommodity", "Buy 1 {0}"), DisplayName),
					Commodity.bCanBuy,
					Commodity.CommodityId,
					true);
				AddMarketActionButton(
					FText::Format(NSLOCTEXT("StationInteraction", "SellCommodity", "Sell 1 {0}"), DisplayName),
					Commodity.bCanSell,
					Commodity.CommodityId,
					false);
			}
		}
		SetPanelText(NSLOCTEXT("StationInteraction", "MarketTitle", "Commodity Desk"), Body);
		return;
	}
	if (InteractionType == FName(TEXT("launch")))
	{
		SetPanelText(NSLOCTEXT("StationInteraction", "LaunchTitle", "Airlock / Departure"), BuildCommandPanelSummary(Panel));
		AddActionButton(NSLOCTEXT("StationInteraction", "LaunchAction", "Launch"), true);
		return;
	}

	SetPanelText(NSLOCTEXT("StationInteraction", "UnknownTitle", "Station Service"), BuildCommandPanelSummary(Panel), FString::Printf(TEXT("Unsupported station service: %s"), *InteractionType.ToString()), true);
	AddActionButton(NSLOCTEXT("StationInteraction", "Unsupported", "Unsupported"), false);
}

void UStargameStationInteractionWidget::RefreshMissionContact()
{
	ClearActionButtons();

	UStargameSessionSubsystem* Session = ResolveSession(this);
	FMissionContactInteractionView Interaction;
	if (!Session || !Session->GetDockedMissionContactInteraction(GiverNpcId, Interaction))
	{
		SetPanelText(NSLOCTEXT("StationInteraction", "ContactUnavailable", "Mission Contact"), TEXT("Nothing for you right now."), TEXT("No mission interaction available."), false);
		AddActionButton(NSLOCTEXT("StationInteraction", "Continue", "Continue"), false);
		return;
	}

	FString Body = Interaction.DialogText;
	if (Body.IsEmpty())
	{
		Body = Interaction.ContextText;
	}
	if (Body.IsEmpty())
	{
		Body = FString::Printf(TEXT("Mission state: %s"), *Interaction.InteractionState.ToString());
	}

	FText ButtonLabel = NSLOCTEXT("StationInteraction", "Continue", "Continue");
	bool bCanRunCommand = false;
	if (Interaction.AvailableCommand == FName(TEXT("accept")))
	{
		ButtonLabel = NSLOCTEXT("StationInteraction", "AcceptMission", "Accept mission");
		bCanRunCommand = true;
	}
	else if (Interaction.AvailableCommand == FName(TEXT("turn_in")))
	{
		ButtonLabel = NSLOCTEXT("StationInteraction", "TurnInMission", "Turn in mission");
		bCanRunCommand = true;
	}

	const FText Title = Interaction.MissionTitle.IsEmpty()
		? NSLOCTEXT("StationInteraction", "MissionContactTitle", "Mission Contact")
		: Interaction.MissionTitle;
	SetPanelText(Title, Body);
	AddActionButton(ButtonLabel, bCanRunCommand);
}

void UStargameStationInteractionWidget::HandlePrimaryClicked()
{
	ExecutePrimaryAction();
}

void UStargameStationInteractionWidget::ExecutePrimaryAction()
{
	if (!StationPawn)
	{
		SetPanelText(FText::GetEmpty(), TEXT("Station pawn is not available."), TEXT("Action failed."), true);
		return;
	}

	const bool bAccepted = bShowingMissionContact
		? StationPawn->ExecuteMissionContactCommand(GiverNpcId)
		: StationPawn->ExecuteStationObjectCommand(InteractionType);

	if (bAccepted)
	{
		SetPanelText(TitleText ? TitleText->GetText() : FText::GetEmpty(), BodyText ? BodyText->GetText().ToString() : FString(), TEXT("Action accepted."));
		if (bShowingMissionContact)
		{
			RefreshMissionContact();
		}
		else if (InteractionType != FName(TEXT("launch")))
		{
			RefreshStationObject();
		}
		return;
	}

	SetPanelText(TitleText ? TitleText->GetText() : FText::GetEmpty(), BodyText ? BodyText->GetText().ToString() : FString(), TEXT("Action was not accepted."), true);
}

void UStargameStationInteractionWidget::ExecuteMarketAction(FName CommodityId, bool bBuy)
{
	if (!StationPawn)
	{
		SetPanelText(TitleText ? TitleText->GetText() : FText::GetEmpty(), BodyText ? BodyText->GetText().ToString() : FString(), TEXT("Action failed."), true);
		return;
	}

	const bool bAccepted = StationPawn->ExecuteMarketCommodityTransaction(CommodityId, bBuy);
	RefreshStationObject();
	SetPanelText(
		TitleText ? TitleText->GetText() : NSLOCTEXT("StationInteraction", "MarketTitle", "Commodity Desk"),
		BodyText ? BodyText->GetText().ToString() : FString(),
		bAccepted ? TEXT("Trade accepted.") : TEXT("Trade was not accepted."),
		!bAccepted);
}

void UStargameStationInteractionWidget::HandleCloseClicked()
{
	ClosePanel();
}

void UStargameStationInteractionWidget::ClosePanel()
{
	OnStationInteractionClosed();
	if (AStationInteriorPawn* Pawn = StationPawn.Get())
	{
		Pawn->CloseStationInteractionPanel();
		return;
	}

	RemoveFromParent();
}

void UStargameStationInteractionWidget::ClearActionButtons()
{
	MarketActions.Reset();
	CurrentActions.Reset();
}

void UStargameStationInteractionWidget::AddActionButton(const FText& Label, bool bEnabled)
{
	FStargameStationInteractionActionView Action;
	Action.ActionId = TEXT("primary");
	Action.Label = Label;
	Action.bEnabled = bEnabled;
	CurrentActions.Add(Action);
	if (PrimaryButton)
	{
		PrimaryButton->SetIsEnabled(bEnabled);
	}
}

void UStargameStationInteractionWidget::AddMarketActionButton(const FText& Label, bool bEnabled, FName CommodityId, bool bBuy)
{
	FStargameStationInteractionActionView Action;
	Action.ActionId = bBuy ? TEXT("market_buy") : TEXT("market_sell");
	Action.Label = Label;
	Action.bEnabled = bEnabled;
	Action.CommodityId = CommodityId;
	Action.bBuy = bBuy;
	CurrentActions.Add(Action);
}

void UStargameStationInteractionWidget::SetPanelText(const FText& InTitle, const FString& InBody, const FString& InStatus, bool bStatusIsError)
{
	if (TitleText && !InTitle.IsEmpty())
	{
		TitleText->SetText(InTitle);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(InBody));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(InStatus));
	}
	OnStationInteractionChanged(InTitle, InBody, InStatus, bStatusIsError, CurrentActions);
}

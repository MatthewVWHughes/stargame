#include "UI/StargameCommsWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Flight/SpaceFlightPawn.h"
#include "InputCoreTypes.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"

void UStargameCommsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (RequestDockingButton)
	{
		RequestDockingButton->OnClicked.AddUniqueDynamic(this, &UStargameCommsWidget::HandleRequestDockingClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UStargameCommsWidget::HandleCloseClicked);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UStargameCommsWidget::OpenComms()
{
	bCommsOpen = true;
	RefreshComms();
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
	SetOwningPlayerInputLocked(true);
	OnCommsOpened(CachedView);
}

void UStargameCommsWidget::CloseComms()
{
	bCommsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	SetOwningPlayerInputLocked(false);
	OnCommsClosed();
}

void UStargameCommsWidget::ToggleComms()
{
	bCommsOpen ? CloseComms() : OpenComms();
}

void UStargameCommsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bCommsOpen)
	{
		RefreshComms();
	}
}

FReply UStargameCommsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseComms();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FStargameCommsView UStargameCommsWidget::BuildCommsView(const UWorld* World, const ASpaceFlightPawn* FlightPawn, const FString& LastStatus)
{
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	return BuildCommsView(Session, StarSystem, FlightPawn, LastStatus);
}

FStargameCommsView UStargameCommsWidget::BuildCommsView(const UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem, const ASpaceFlightPawn* FlightPawn, const FString& LastStatus)
{
	FStargameCommsView View;
	View.StatusText = LastStatus;
	if (!Session || !StarSystem || !FlightPawn)
	{
		View.BodyText = TEXT("No ship comms link is available.");
		return View;
	}

	const FName SelectedTargetId = Session->GetSelectedTargetId();
	View.TargetId = SelectedTargetId;
	View.bHasSelectedTarget = !SelectedTargetId.IsNone();
	if (SelectedTargetId.IsNone())
	{
		View.BodyText = TEXT("No station selected.\nSelect a station target before opening comms.");
		return View;
	}

	TArray<FNavigationTargetViewModel> Targets;
	StarSystem->BuildNavigationTargetViewModels(
		SelectedTargetId,
		FlightPawn->GetLogicalSystemPositionCm(),
		FlightPawn->GetLinearVelocityCmPerSec(),
		Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds,
		Targets);
	const FNavigationTargetViewModel* SelectedView = Targets.FindByPredicate([SelectedTargetId](const FNavigationTargetViewModel& Target)
	{
		return Target.TargetId == SelectedTargetId;
	});

	FNavigationTargetDefinition TargetDefinition;
	const bool bFoundNavigationTarget = StarSystem->FindNavigationTarget(SelectedTargetId, TargetDefinition);

	View.DisplayName = SelectedView && !SelectedView->DisplayName.IsEmpty() ? SelectedView->DisplayName : FText::FromName(SelectedTargetId);
	View.DistanceMeters = SelectedView ? SelectedView->DistanceCm / 100.0 : 0.0;

	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FStationDefinition* Station = SystemDefinition.Stations.FindByPredicate([SelectedTargetId](const FStationDefinition& Candidate)
	{
		return Candidate.StationId == SelectedTargetId || Candidate.NavigationTarget.TargetId == SelectedTargetId;
	});
	View.bStationSelected = Station != nullptr || (bFoundNavigationTarget && TargetDefinition.TargetType == FName(TEXT("station")));
	if (!View.bStationSelected)
	{
		View.BodyText = FString::Printf(TEXT("%s\nThis target is not a station.\nDistance: %.0f m"), *View.DisplayName.ToString(), View.DistanceMeters);
		return View;
	}
	if (!Station || Station->DockingPorts.IsEmpty())
	{
		View.BodyText = FString::Printf(TEXT("%s\nNo docking port is authored for this station."), *View.DisplayName.ToString());
		return View;
	}

	if (!Station->DisplayName.IsEmpty())
	{
		View.DisplayName = Station->DisplayName;
	}
	View.StationId = Station->StationId;
	View.PortId = Station->DockingPorts[0].PortId;
	View.bCanRequestDocking = SelectedView ? SelectedView->bInsideStationApproachBubble : false;
	if (!SelectedView)
	{
		const double SimulationTimeSeconds = Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds;
		const FSimulationClockSnapshot Clock = Session->GetSimulationClockSnapshot();
		FFrameResolvedTransform ApproachFrame;
		if (UOrbitRouteFrameQueryService::ResolveDockingPortTransform(StarSystem->GetActiveSystemDefinition(), Station->StationId, View.PortId, EDockingPortTransformKind::Approach, Clock, SimulationTimeSeconds, ApproachFrame))
		{
			const double DistanceCm = FVector::Distance(FlightPawn->GetLogicalSystemPositionCm(), ApproachFrame.PositionCm);
			View.DistanceMeters = DistanceCm / 100.0;
			View.bCanRequestDocking = DistanceCm <= Station->DockingPorts[0].ActivationRangeCm;
		}
	}
	View.BodyText = View.bCanRequestDocking
		? FString::Printf(TEXT("%s\nRequest docking clearance?\nDistance: %.0f m"), *View.DisplayName.ToString(), View.DistanceMeters)
		: FString::Printf(TEXT("%s\nNo docking clearance available at this range.\nDistance: %.0f m"), *View.DisplayName.ToString(), View.DistanceMeters);
	return View;
}

void UStargameCommsWidget::RefreshComms()
{
	const ASpaceFlightPawn* FlightPawn = GetOwningPlayer() ? Cast<ASpaceFlightPawn>(GetOwningPlayer()->GetPawn()) : nullptr;
	CachedView = BuildCommsView(GetWorld(), FlightPawn, LastStatusText);
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(CachedView.BodyText));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(CachedView.StatusText));
	}
	if (RequestDockingButton)
	{
		RequestDockingButton->SetIsEnabled(CachedView.bCanRequestDocking);
	}
	OnCommsViewChanged(CachedView);
}

void UStargameCommsWidget::HandleRequestDockingClicked()
{
	const bool bAccepted = ExecuteDockingRequest();
	LastStatusText = bAccepted ? TEXT("Docking request accepted.") : LastStatusText;
	RefreshComms();
}

void UStargameCommsWidget::HandleCloseClicked()
{
	CloseComms();
}

bool UStargameCommsWidget::ExecuteDockingRequest()
{
	ASpaceFlightPawn* FlightPawn = GetOwningPlayer() ? Cast<ASpaceFlightPawn>(GetOwningPlayer()->GetPawn()) : nullptr;
	if (!FlightPawn)
	{
		LastStatusText = TEXT("Docking request failed: no active ship.");
		return false;
	}

	RefreshComms();
	if (!CachedView.bCanRequestDocking || CachedView.StationId.IsNone() || CachedView.PortId.IsNone())
	{
		LastStatusText = CachedView.BodyText;
		return false;
	}

	const bool bAccepted = FlightPawn->RequestDocking(CachedView.StationId, CachedView.PortId);
	const FDockingOperationState Docking = FlightPawn->GetDockingOperationState();
	LastStatusText = bAccepted ? TEXT("Docking request accepted.") : Docking.LastFailureReason;
	return bAccepted;
}

void UStargameCommsWidget::SetOwningPlayerInputLocked(bool bLocked)
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

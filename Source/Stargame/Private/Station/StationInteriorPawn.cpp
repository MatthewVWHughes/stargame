#include "Station/StationInteriorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Station/StationInteriorHostileActor.h"
#include "Station/StationInteriorInteractableActor.h"
#include "Station/StationInteriorRoomActor.h"
#include "Station/StationMissionContactActor.h"
#include "UI/StargameStationInteractionWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogStargameStationInterior, Log, All);

namespace
{
	FText MakeMissionContactPromptText(const FMissionContactInteractionView& Interaction)
	{
		if (Interaction.AvailableCommand == FName(TEXT("accept")))
		{
			return NSLOCTEXT("StationInterior", "StationAcceptMissionPrompt", "Accept mission");
		}
		if (Interaction.AvailableCommand == FName(TEXT("turn_in")))
		{
			return NSLOCTEXT("StationInterior", "StationTurnInMissionPrompt", "Turn in mission");
		}
		if (Interaction.AvailableCommand == FName(TEXT("continue")))
		{
			return NSLOCTEXT("StationInterior", "StationMissionInProgressPrompt", "Mission in progress");
		}
		return NSLOCTEXT("StationInterior", "StationTalkPromptShort", "Talk");
	}
}

AStationInteriorPawn::AStationInteriorPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->MaxWalkSpeed = 520.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2048.0f;
}

void AStationInteriorPawn::ConfigureStationInteriorPawn(UStargameSessionSubsystem* InSession, AStationInteriorRoomActor* InRoom)
{
	OwningSession = InSession;
	OwningRoom = InRoom;
}

void AStationInteriorPawn::ApplyCombatProfile(const FStationInteriorCombatProfileDefinition& Profile)
{
	MaxOnFootHealth = FMath::Max(1.0f, static_cast<float>(Profile.PlayerMaxHealth));
	OnFootWeaponDamage = FMath::Max(0.0f, static_cast<float>(Profile.PlayerWeaponDamage));
	OnFootWeaponRangeCm = FMath::Max(0.0f, static_cast<float>(Profile.PlayerWeaponRangeCm));
	OnFootWeaponCooldownSeconds = FMath::Max(0.0f, static_cast<float>(Profile.PlayerWeaponCooldownSeconds));
	OnFootHealth = FMath::Clamp(OnFootHealth <= 0.0f ? MaxOnFootHealth : OnFootHealth, 0.0f, MaxOnFootHealth);
	OnFootWeaponCooldownRemainingSeconds = FMath::Min(OnFootWeaponCooldownRemainingSeconds, OnFootWeaponCooldownSeconds);
}

void AStationInteriorPawn::SetHostileBoardingEnabled(bool bInEnabled)
{
	bHostileBoardingEnabled = bInEnabled;
	if (bHostileBoardingEnabled && OwningRoom)
	{
		FStationInteriorCombatProfileDefinition Profile;
		if (UStargameSessionSubsystem* Session = OwningSession.Get())
		{
			Session->ResolveStationInteriorCombatProfile(OwningRoom->GetCombatProfileId(), Profile);
		}
		ApplyCombatProfile(Profile);
	}
	OnFootHealth = MaxOnFootHealth;
	if (bHostileBoardingEnabled && OwningRoom)
	{
		OwningRoom->ArmHostileBoarding(this);
	}
}

void AStationInteriorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AStationInteriorPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AStationInteriorPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AStationInteriorPawn::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AStationInteriorPawn::LookUp);
	PlayerInputComponent->BindAction(TEXT("InteractTarget"), IE_Pressed, this, &AStationInteriorPawn::RequestInteract);
	PlayerInputComponent->BindAction(TEXT("StationExit"), IE_Pressed, this, &AStationInteriorPawn::RequestExitStation);
	PlayerInputComponent->BindAction(TEXT("PrimaryMouse"), IE_Pressed, this, &AStationInteriorPawn::RequestPrimaryFire);
}

void AStationInteriorPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OnFootWeaponCooldownRemainingSeconds = FMath::Max(0.0f, OnFootWeaponCooldownRemainingSeconds - DeltaSeconds);
	RefreshFocusedStationActor();
}

void AStationInteriorPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StationInteractionPanel)
	{
		StationInteractionPanel->RemoveFromParent();
		StationInteractionPanel = nullptr;
	}
	SetStationPanelInputLocked(false);
	ClearFocusedStationActors();

	Super::EndPlay(EndPlayReason);
}

void AStationInteriorPawn::MoveForward(float Value)
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AStationInteriorPawn::MoveRight(float Value)
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AStationInteriorPawn::Turn(float Value)
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}
	AddControllerYawInput(Value);
}

void AStationInteriorPawn::LookUp(float Value)
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}
	AddControllerPitchInput(Value);
}

void AStationInteriorPawn::RequestExitStation()
{
	if (bStationInteractionPanelOpen)
	{
		CloseStationInteractionPanel();
		return;
	}
	if (!TryLaunchFromInterior())
	{
		return;
	}
}

void AStationInteriorPawn::RequestPrimaryFire()
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}
	FireOnFootWeapon();
}

bool AStationInteriorPawn::TryLaunchFromInterior()
{
	if (AStationInteriorRoomActor* Room = OwningRoom.Get())
	{
		if (Room->IsHostileBoarding() && !Room->AreHostilesCleared())
		{
			UE_LOG(LogStargameStationInterior, Display, TEXT("Station launch rejected: hostiles remain (%d)."), Room->GetLiveHostileCount());
			return false;
		}
	}

	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			Session = GameInstance->GetSubsystem<UStargameSessionSubsystem>();
		}
	}
	if (Session)
	{
		FDockedStationCommandResult Result;
		Session->ExitStationInteriorAndUndock(Result);
		return Result.bAccepted;
	}
	return false;
}

void AStationInteriorPawn::RequestInteract()
{
	if (bStationInteractionPanelOpen)
	{
		return;
	}

	RefreshFocusedStationActor();
	if (FocusedMissionContact)
	{
		OpenStationInteractionPanelForContact(FocusedMissionContact);
		return;
	}
	if (FocusedStationObject)
	{
		OpenStationInteractionPanelForObject(FocusedStationObject);
	}
}

bool AStationInteriorPawn::OpenStationInteractionPanelForContact(AStationMissionContactActor* Contact)
{
	if (!Contact)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return false;
	}

	if (!StationInteractionPanel)
	{
		if (!StationInteractionWidgetClass)
		{
			UE_LOG(LogStargameStationInterior, Warning, TEXT("Station interaction panel requires an authored widget Blueprint class."));
			return false;
		}
		StationInteractionPanel = PlayerController->IsLocalController()
			? CreateWidget<UStargameStationInteractionWidget>(PlayerController, StationInteractionWidgetClass)
			: CreateWidget<UStargameStationInteractionWidget>(GetWorld(), StationInteractionWidgetClass);
	}
	if (!StationInteractionPanel)
	{
		return false;
	}

	if (!StationInteractionPanel->IsInViewport())
	{
		StationInteractionPanel->AddToViewport(40);
	}
	StationInteractionPanel->ShowMissionContact(this, Contact->GetNpcId(), Contact->GetDisplayName());
	SetStationPanelInputLocked(true);
	return true;
}

bool AStationInteriorPawn::OpenStationInteractionPanelForObject(AStationInteriorInteractableActor* Interactable)
{
	if (!Interactable)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return false;
	}

	if (!StationInteractionPanel)
	{
		if (!StationInteractionWidgetClass)
		{
			UE_LOG(LogStargameStationInterior, Warning, TEXT("Station interaction panel requires an authored widget Blueprint class."));
			return false;
		}
		StationInteractionPanel = PlayerController->IsLocalController()
			? CreateWidget<UStargameStationInteractionWidget>(PlayerController, StationInteractionWidgetClass)
			: CreateWidget<UStargameStationInteractionWidget>(GetWorld(), StationInteractionWidgetClass);
	}
	if (!StationInteractionPanel)
	{
		return false;
	}

	if (!StationInteractionPanel->IsInViewport())
	{
		StationInteractionPanel->AddToViewport(40);
	}
	StationInteractionPanel->ShowStationObject(this, Interactable->GetInteractionType(), Interactable->GetDisplayName());
	SetStationPanelInputLocked(true);
	return true;
}

void AStationInteriorPawn::CloseStationInteractionPanel()
{
	if (StationInteractionPanel)
	{
		StationInteractionPanel->RemoveFromParent();
		StationInteractionPanel = nullptr;
	}
	SetStationPanelInputLocked(false);
}

void AStationInteriorPawn::SetStationPanelInputLocked(bool bLocked)
{
	if (bStationInteractionPanelOpen == bLocked)
	{
		return;
	}

	bStationInteractionPanelOpen = bLocked;

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController && !bLocked)
	{
		PlayerController = StationPanelPlayerController.Get();
	}
	if (!PlayerController)
	{
		return;
	}
	if (bLocked)
	{
		StationPanelPlayerController = PlayerController;
	}

	PlayerController->SetIgnoreMoveInput(bLocked);
	PlayerController->SetIgnoreLookInput(bLocked);
	PlayerController->bShowMouseCursor = bLocked;
	if (bLocked)
	{
		FInputModeGameAndUI InputMode;
		if (StationInteractionPanel)
		{
			InputMode.SetWidgetToFocus(StationInteractionPanel->TakeWidget());
		}
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		if (StationInteractionPanel)
		{
			StationInteractionPanel->SetKeyboardFocus();
		}
	}
	else
	{
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		StationPanelPlayerController.Reset();
	}
}

void AStationInteriorPawn::ClearFocusedStationActors()
{
	if (IsValid(FocusedMissionContact))
	{
		FocusedMissionContact->SetFocusedByPlayer(false);
	}
	FocusedMissionContact = nullptr;

	if (IsValid(FocusedStationObject))
	{
		FocusedStationObject->SetFocusedByPlayer(false);
	}
	FocusedStationObject = nullptr;
}

void AStationInteriorPawn::RefreshFocusedStationActor()
{
	AStationMissionContactActor* CandidateContact = FindFocusedMissionContact();
	AStationInteriorInteractableActor* CandidateObject = FindFocusedStationObject();
	AStationMissionContactActor* NewFocusedContact = nullptr;
	AStationInteriorInteractableActor* NewFocusedObject = nullptr;

	if (CandidateContact && CandidateObject)
	{
		const double ContactDistanceSquared = FVector::DistSquared(GetActorLocation(), CandidateContact->GetActorLocation());
		const double ObjectDistanceSquared = FVector::DistSquared(GetActorLocation(), CandidateObject->GetActorLocation());
		if (ContactDistanceSquared <= ObjectDistanceSquared)
		{
			NewFocusedContact = CandidateContact;
		}
		else
		{
			NewFocusedObject = CandidateObject;
		}
	}
	else
	{
		NewFocusedContact = CandidateContact;
		NewFocusedObject = CandidateObject;
	}

	if (FocusedMissionContact != NewFocusedContact)
	{
		if (IsValid(FocusedMissionContact))
		{
			FocusedMissionContact->SetFocusedByPlayer(false);
		}
		FocusedMissionContact = NewFocusedContact;
		if (FocusedMissionContact)
		{
			UpdateMissionContactPrompt(FocusedMissionContact);
			FocusedMissionContact->SetFocusedByPlayer(true);
		}
	}
	else if (FocusedMissionContact)
	{
		UpdateMissionContactPrompt(FocusedMissionContact);
	}

	if (FocusedStationObject != NewFocusedObject)
	{
		if (IsValid(FocusedStationObject))
		{
			FocusedStationObject->SetFocusedByPlayer(false);
		}
		FocusedStationObject = NewFocusedObject;
		if (FocusedStationObject)
		{
			FocusedStationObject->SetFocusedByPlayer(true);
		}
	}
}

void AStationInteriorPawn::UpdateMissionContactPrompt(AStationMissionContactActor* Contact) const
{
	if (!Contact)
	{
		return;
	}

	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}

	FMissionContactInteractionView Interaction;
	if (Session && Session->GetDockedMissionContactInteraction(Contact->GetNpcId(), Interaction))
	{
		Contact->SetPromptText(MakeMissionContactPromptText(Interaction));
		return;
	}

	Contact->SetPromptText(FText::Format(NSLOCTEXT("StationInterior", "StationTalkPrompt", "Talk: {0}"), Contact->GetDisplayName()));
}

FName AStationInteriorPawn::MakeInteractionEventId(const TCHAR* Prefix)
{
	++InteractionSequence;
	return FName(*FString::Printf(TEXT("%s_%d"), Prefix, InteractionSequence));
}

AStationMissionContactActor* AStationInteriorPawn::FindFocusedMissionContact() const
{
	AStationInteriorRoomActor* Room = OwningRoom.Get();
	if (!Room)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
		Room = Session ? Session->GetActiveStationInteriorRoom() : nullptr;
	}
	if (!Room)
	{
		return nullptr;
	}

	return Room->FindNearestMissionContact(GetActorLocation(), MissionContactInteractionRangeCm);
}

AStationInteriorInteractableActor* AStationInteriorPawn::FindFocusedStationObject() const
{
	AStationInteriorRoomActor* Room = OwningRoom.Get();
	if (!Room)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UStargameSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
		Room = Session ? Session->GetActiveStationInteriorRoom() : nullptr;
	}
	if (!Room)
	{
		return nullptr;
	}

	return Room->FindNearestInteractable(GetActorLocation(), StationObjectInteractionRangeCm);
}

bool AStationInteriorPawn::InteractWithFocusedMissionContact()
{
	AStationMissionContactActor* Contact = FindFocusedMissionContact();
	if (!Contact)
	{
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact interaction rejected: no mission contact in range."));
		return false;
	}

	return ExecuteMissionContactCommand(Contact->GetNpcId());
}

bool AStationInteriorPawn::ExecuteMissionContactCommand(FName GiverNpcId)
{
	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		UGameInstance* GameInstance = GetGameInstance();
		Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}
	if (!Session)
	{
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact interaction rejected: missing session subsystem."));
		return false;
	}

	FMissionContactInteractionView Interaction;
	if (!Session->GetDockedMissionContactInteraction(GiverNpcId, Interaction))
	{
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact %s has no available mission interaction. Reason=%s"),
			*GiverNpcId.ToString(),
			*Interaction.DebugReason);
		return false;
	}

	FDockedStationCommandResult CommandResult;
	if (Interaction.AvailableCommand == FName(TEXT("accept")) && !Interaction.OfferId.IsNone())
	{
		FMissionInstanceState AcceptedMission;
		const FName IdempotencyKey(*FString::Printf(TEXT("station_contact_accept_%s"), *Interaction.OfferId.ToString()));
		const bool bAccepted = Session->AcceptDockedMissionOffer(Interaction.OfferId, TEXT("player"), IdempotencyKey, AcceptedMission, CommandResult);
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact accept %s NPC=%s Offer=%s Reason=%s"),
			bAccepted ? TEXT("accepted") : TEXT("rejected"),
			*GiverNpcId.ToString(),
			*Interaction.OfferId.ToString(),
			*CommandResult.FailureReason);
		return bAccepted;
	}

	if (Interaction.AvailableCommand == FName(TEXT("turn_in")) && !Interaction.MissionInstanceId.IsNone())
	{
		FProgressionDebugLedgerEntry CompletionEntry;
		const FName SourceEventId(*FString::Printf(TEXT("event_station_contact_turn_in_%s"), *Interaction.MissionInstanceId.ToString()));
		const FName IdempotencyKey(*FString::Printf(TEXT("station_contact_turn_in_%s"), *Interaction.MissionInstanceId.ToString()));
		const bool bCompleted = Session->CompleteDockedMission(Interaction.MissionInstanceId, SourceEventId, IdempotencyKey, CompletionEntry, CommandResult);
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact turn-in %s NPC=%s Mission=%s Reason=%s"),
			bCompleted ? TEXT("accepted") : TEXT("rejected"),
			*GiverNpcId.ToString(),
			*Interaction.MissionInstanceId.ToString(),
			*CommandResult.FailureReason);
		return bCompleted;
	}

	UE_LOG(LogStargameStationInterior, Display, TEXT("Station contact %s interaction state=%s command=%s objective=%s"),
		*GiverNpcId.ToString(),
		*Interaction.InteractionState.ToString(),
		*Interaction.AvailableCommand.ToString(),
		*Interaction.ActiveObjectiveStateId.ToString());
	return Interaction.bHasMission;
}

bool AStationInteriorPawn::InteractWithFocusedStationObject()
{
	AStationInteriorInteractableActor* Interactable = FindFocusedStationObject();
	if (!Interactable)
	{
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station object interaction rejected: no service point in range."));
		return false;
	}

	return ExecuteStationObjectCommand(Interactable->GetInteractionType());
}

bool AStationInteriorPawn::ExecuteStationObjectCommand(FName InteractionType)
{
	if (InteractionType == FName(TEXT("repair")))
	{
		return ExecuteStationService(TEXT("repair"), 10.0);
	}
	if (InteractionType == FName(TEXT("refuel")))
	{
		return ExecuteStationService(TEXT("refuel"), 10.0);
	}
	if (InteractionType == FName(TEXT("market")))
	{
		return ExecuteMarketBuy();
	}
	if (InteractionType == FName(TEXT("launch")))
	{
		return TryLaunchFromInterior();
	}

	UE_LOG(LogStargameStationInterior, Display, TEXT("Station object interaction ignored: unsupported type %s."), *InteractionType.ToString());
	return false;
}

bool AStationInteriorPawn::ExecuteStationService(FName ServiceType, double Amount)
{
	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		UGameInstance* GameInstance = GetGameInstance();
		Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}
	if (!Session)
	{
		return false;
	}

	FDockedStationCommandPanelView Panel;
	if (!Session->GetDockedStationCommandPanel(Panel) || !Panel.bDocked)
	{
		return false;
	}

	const FDockedStationCommandOption* ServiceCommand = Panel.Commands.FindByPredicate([ServiceType](const FDockedStationCommandOption& Command)
	{
		return Command.CommandType == FName(TEXT("service")) && Command.TargetId == ServiceType && Command.bAvailable;
	});
	if (!ServiceCommand)
	{
		return false;
	}

	const FSystemicGameplayState State = Session->GetSystemicGameplayState();
	const FStationServiceEndpointDefinition* Endpoint = State.StationServiceEndpoints.FindByPredicate([ServiceCommand](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == ServiceCommand->SourceId;
	});
	const int64 TotalCost = Endpoint ? static_cast<int64>(FMath::RoundToDouble(Amount * static_cast<double>(Endpoint->UnitPriceCredits))) : 0;

	const FName RequestId = MakeInteractionEventId(TEXT("tx_interior_service"));
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
	UE_LOG(LogStargameStationInterior, Display, TEXT("Station service interaction %s Type=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*ServiceType.ToString(),
		*CommandResult.FailureReason);
	return bAccepted;
}

void AStationInteriorPawn::TakeOnFootDamage(float Amount)
{
	if (!bHostileBoardingEnabled || OnFootHealth <= 0.0f)
	{
		return;
	}

	OnFootHealth = FMath::Max(0.0f, OnFootHealth - FMath::Max(0.0f, Amount));
	UE_LOG(LogStargameStationInterior, Display, TEXT("Station on-foot damage %.1f HP=%.1f"), Amount, OnFootHealth);
}

bool AStationInteriorPawn::FireOnFootWeapon()
{
	if (!bHostileBoardingEnabled || OnFootHealth <= 0.0f || OnFootWeaponCooldownRemainingSeconds > 0.0f || !Camera)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StationOnFootWeapon), true);
	QueryParams.AddIgnoredActor(this);
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * OnFootWeaponRangeCm;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);
	OnFootWeaponCooldownRemainingSeconds = OnFootWeaponCooldownSeconds;

	AStationInteriorHostileActor* Hostile = bHit ? Cast<AStationInteriorHostileActor>(Hit.GetActor()) : nullptr;
	if (Hostile)
	{
		Hostile->ApplyOnFootDamage(OnFootWeaponDamage);
		UE_LOG(LogStargameStationInterior, Display, TEXT("Station on-foot weapon hit %s"), *Hostile->GetHostileId().ToString());
		return true;
	}

	UE_LOG(LogStargameStationInterior, Display, TEXT("Station on-foot weapon fired without hostile hit."));
	return true;
}

bool AStationInteriorPawn::ExecuteMarketBuy()
{
	FDockedMarketView Market;
	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		UGameInstance* GameInstance = GetGameInstance();
		Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}
	if (!Session || !Session->GetDockedMarketView(Market) || !Market.bAvailable)
	{
		return false;
	}

	const FDockedMarketCommodityView* Quote = Market.Commodities.FindByPredicate([](const FDockedMarketCommodityView& Commodity)
	{
		return Commodity.bCanBuy;
	});
	return Quote ? ExecuteMarketCommodityTransaction(Quote->CommodityId, true) : false;
}

bool AStationInteriorPawn::ExecuteMarketCommodityTransaction(FName CommodityId, bool bBuy)
{
	UStargameSessionSubsystem* Session = OwningSession.Get();
	if (!Session)
	{
		UGameInstance* GameInstance = GetGameInstance();
		Session = GameInstance ? GameInstance->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	}
	if (!Session)
	{
		return false;
	}

	FDockedMarketView Market;
	if (!Session->GetDockedMarketView(Market) || !Market.bAvailable)
	{
		return false;
	}

	const FDockedMarketCommodityView* Quote = Market.Commodities.FindByPredicate([CommodityId](const FDockedMarketCommodityView& Commodity)
	{
		return Commodity.CommodityId == CommodityId;
	});
	if (!Quote)
	{
		return false;
	}
	if (bBuy && !Quote->bCanBuy)
	{
		return false;
	}
	if (!bBuy && !Quote->bCanSell)
	{
		return false;
	}

	const FName TransactionId = MakeInteractionEventId(bBuy ? TEXT("tx_interior_market_buy") : TEXT("tx_interior_market_sell"));
	FMarketTransactionRequest Request;
	Request.TransactionId = TransactionId;
	Request.MarketId = Market.MarketId;
	Request.BuyerId = bBuy ? FName(TEXT("player")) : Market.StationId;
	Request.SellerId = bBuy ? Market.StationId : FName(TEXT("player"));
	Request.CommodityId = Quote->CommodityId;
	Request.CommodityItemBridgeId = Quote->CommodityItemBridgeId;
	Request.Quantity = 1;
	Request.QuotedUnitPrice = Quote->UnitPrice;
	Request.SourceContainerId = bBuy ? Market.SourceContainerId : Quote->BuyDestinationContainerId;
	Request.DestinationContainerId = bBuy ? Quote->BuyDestinationContainerId : Market.SourceContainerId;
	Request.DebitAccountId = bBuy ? FName(TEXT("account_player")) : Market.MarketCreditAccountId;
	Request.CreditAccountId = bBuy ? Market.MarketCreditAccountId : FName(TEXT("account_player"));
	Request.LegalContextId = Market.LegalContextId;
	Request.SourceEventId = FName(*FString::Printf(TEXT("event_%s"), *TransactionId.ToString()));
	Request.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *TransactionId.ToString()));

	FMarketTransactionResult Result;
	FDockedStationCommandResult CommandResult;
	const bool bAccepted = Session->ExecuteDockedMarketTransaction(Request, Result, CommandResult);
	UE_LOG(LogStargameStationInterior, Display, TEXT("Station market interaction %s Commodity=%s Reason=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		*Quote->CommodityId.ToString(),
		*CommandResult.FailureReason);
	return bAccepted;
}

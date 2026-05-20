#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Space/StarSystemSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "StargameSessionSubsystem.generated.h"

class UStarCatalogSubsystem;
class UWorld;
class ASpaceFlightPawn;
class AStationInteriorPawn;
class AStationInteriorRoomActor;

UENUM(BlueprintType)
enum class EGateTransitionResultCode : uint8
{
	Success,
	MissingActiveSystem,
	MissingSourceGate,
	MissingDestinationSystem,
	MissingDestinationGate,
	MissingDestinationArrival,
	InvalidArrivalFrame,
	SystemBuildFailed,
	PlayerSpawnFailed
};

USTRUCT(BlueprintType)
struct STARGAME_API FGateTransitionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Transition")
	FName SourceGateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Transition")
	FName ShipInstanceId = TEXT("player_ship");
};

USTRUCT(BlueprintType)
struct STARGAME_API FGateTransitionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	EGateTransitionResultCode ResultCode = EGateTransitionResultCode::MissingActiveSystem;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName SourceSystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName SourceGateId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationSystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationGateId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationArrivalId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName ArrivalFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	double ElapsedTransitionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FShipSaveLocation ArrivalLocation;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedStationContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	bool bDocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName SystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName PortId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName OwnerFactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FText OwnerFactionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName StationRole;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName RegionName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName SecurityProfile;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName MarketProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TSoftClassPtr<AActor> InteriorRoomClass;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TSoftClassPtr<APawn> InteriorPawnClass;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FName> MissionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FName> QuestGiverNpcIds;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FStationQuestGiverDefinition> QuestGivers;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName ShipInstanceId = TEXT("player_ship");

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FStationServiceEndpointDefinition> ServiceEndpoints;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FStationMarketState> Markets;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FMissionOfferRecord> MissionOffers;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedStationCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName CommandId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FString FailureReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedStationCommandOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName CommandId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName CommandType;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FString UnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedStationCommandPanelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	bool bDocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FText StationDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FName PortId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FText OwnerFactionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int64 PlayerCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	double Hull = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	double MaxHull = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	double Shields = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	double MaxShields = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 PersonalInventorySlotsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 PersonalInventorySlotsMax = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 ShipCargoStacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 AvailableServiceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 AvailableMarketCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 AvailableMissionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	int32 AvailableContactCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	TArray<FDockedStationCommandOption> Commands;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Station")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedMarketCommodityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName CommodityId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName CommodityItemBridgeId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName Category;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 Stock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 ReservedStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 MaxStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	double FillRatio = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int64 UnitPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName LocalMarketState;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 PlayerCargoQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 PlayerPersonalInventoryQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	bool bCanBuy = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FString BuyUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	bool bCanSell = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FString SellUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	bool bRestricted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	bool bCargoHoldStorable = true;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName BuyDestinationContainerId;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedMarketView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName MarketId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName MarketProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName SourceContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName MarketCreditAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FName LegalContextId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int64 PlayerCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 ShipCargoStacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 PersonalInventorySlotsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	int32 PersonalInventorySlotsMax = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	TArray<FDockedMarketCommodityView> Commodities;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Market")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedMissionContactOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName GiverNpcId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName OfferId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName MissionInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName MissionDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FText MissionTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName InteractionState = TEXT("unavailable");

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName AvailableCommand = TEXT("leave");

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName IssuerFactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FText IssuerFactionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName RegionName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName ActiveObjectiveStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName ActiveObjectiveType;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName ActiveObjectiveTargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	bool bHasMission = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	bool bCanAccept = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	bool bCanContinue = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	bool bCanTurnIn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString ContextText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString DialogText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString ObjectiveSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString CargoManifestSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	int64 RewardCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString RewardText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedMissionContactPanelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FText StationDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	int32 ContactCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	int32 AvailableMissionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	int32 ActiveMissionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	int32 TurnInCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	TArray<FDockedMissionContactOption> Contacts;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Mission")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedInventoryStackView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName ContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName StackId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName ItemType;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 StackLimit = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int64 BaseValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	double TotalMassKg = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	double TotalVolumeM3 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	bool bStackable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	bool bEquippable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FName> CompatibleSlotIds;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedEquipmentSlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName SlotId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName OwnerId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName SlotType;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FName> AcceptedItemTypes;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName EquippedStackId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName EquippedItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FText EquippedDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName EquippedItemType;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 EquippedQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	bool bOccupied = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDockedInventoryEquipmentPanelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName PersonalInventoryContainerId = TEXT("player_personal_inventory");

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FName ShipCargoContainerId = TEXT("player_ship_cargo");

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 PersonalInventorySlotsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 PersonalInventorySlotsMax = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	int32 ShipCargoStacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	double PersonalInventoryMassKg = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	double ShipCargoMassKg = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FDockedInventoryStackView> PersonalInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FDockedInventoryStackView> ShipCargo;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FDockedEquipmentSlotView> PlayerLoadout;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	TArray<FDockedEquipmentSlotView> ShipLoadout;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Docked Inventory")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FMissionWaypointViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bHasActiveMission = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName MissionInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName ObjectiveStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName ObjectiveType;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName TargetType;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName ObjectiveState;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bTargetSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bTargetResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bReadyToTurnIn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRuntimeEncounterViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bHasEncounterActor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName Role;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName IntentType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ThreatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName TargetShipId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName BehaviorVariantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName CommsVariantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName LocalBehaviorStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FString CommsLine;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PlayerWeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PlayerWeaponStatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PlayerWeaponDisplayNameId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PlayerWeaponTargetCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponDamageAmount = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double PlayerWeaponRangeCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	double PlayerWeaponProjectileSpeedCmPerSec = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PlayerWeaponPresentationType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponFireAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponCooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponCooldownRemainingSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponReadyAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponMaxEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergyCost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergyRegenPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponEnergyReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponMinAlignment = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponAlignmentDot = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponAligned = true;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponInRange = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerWeaponHasLeadPoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPlayerFireSolutionReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector PlayerWeaponLeadPointCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FString PlayerFireSolutionText;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileResponseStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileManeuverStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bHostileWeaponReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponCooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponCooldownRemainingSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponMaxEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergyCost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergyRegenPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotStartPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotEndPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationStartedAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationDurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bShotPresentationActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bRenderedShotActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileSourceCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileTargetCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bProjectileInFlight = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	double ProjectileSpeedCmPerSec = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector ProjectileInheritedVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector ProjectileEffectiveVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double ProjectileTravelledCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double ProjectileTargetDistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ProjectileImpactAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double DistanceToDesiredCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double DistanceTrendCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bSteeringActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPirateAmbushActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bPatrolResponseActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bResolvedEncounter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EngagementId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageEventId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageResultState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName TargetDurabilityState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName OutcomeType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HazardId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName PatrolReservationId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double DynamicSelectionScore = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FString SelectionDebugReason;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FShipWeaponFireResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName WeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName WeaponStatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName TargetCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ThreatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageEventId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageResultState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName TargetDurabilityState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double DamageAmount = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double RangeCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	double ProjectileSpeedCmPerSec = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector ProjectileInheritedVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector ProjectileEffectiveVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName WeaponPresentationType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double CooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double CooldownRemainingSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ReadyAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponMaxEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponEnergyCost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponEnergyRegenPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponMinAlignment = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double WeaponAlignmentDot = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bWeaponAligned = true;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileResponseStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileManeuverStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotStartPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotEndPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationStartedAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationDurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileState;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	bool bProjectileInFlight = false;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double ProjectileTravelledCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double ProjectileTargetDistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ProjectileImpactAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FString FailureReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemMapRouteView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName RouteSegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName SourceAnchorId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName DestinationAnchorId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map", meta = (Units = "cm"))
	FVector SourcePositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map", meta = (Units = "cm"))
	FVector DestinationPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map", meta = (Units = "cm"))
	FVector MidpointPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FVector2D SourceMapPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FVector2D DestinationMapPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FVector2D MidpointMapPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double SecurityRating = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double RiskScore = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	bool bSupportsPatrolCoverage = false;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	bool bSupportsPirateAmbush = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemMapMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName MarkerId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName MarkerType;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	EShipGoalKind GoalKind = EShipGoalKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName RouteSegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double RouteProgress01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FVector2D MapPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName State;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double DynamicSelectionScore = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FString SelectionDebugReason;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	bool bRealized = false;
};

USTRUCT(BlueprintType)
struct STARGAME_API FSystemMapOverviewView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName SystemId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double SimulationTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double MapDistanceScaleCmPerUnit = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName SelectedTargetId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName ActiveMissionTargetId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	TArray<FSystemMapEntryViewModel> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	TArray<FSystemMapRouteView> Routes;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	TArray<FSystemMapMarkerView> Markers;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDistantSectorTrafficView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName ShipInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName ActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName SnapshotCategory;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	EShipGoalKind GoalKind = EShipGoalKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	ELogicalTrafficTier TrafficTier = ELogicalTrafficTier::Tier2Logical;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName RouteSegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	double RouteProgress01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector", meta = (Units = "cm"))
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	bool bHasRouteSample = false;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	bool bRealized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName RealizationReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FDistantSectorSnapshotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FName SystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	double SimulationTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	int32 DataOnlyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	int32 MapProjectedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	int32 RealizedLocalCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	int32 EncounterActorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	TArray<FDistantSectorTrafficView> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FString SummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Distant Sector")
	FString DebugReason;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRuntimeEncounterProjectileState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ProjectileState = TEXT("inactive");

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageEventId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName SourceCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName TargetCombatantId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ThreatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double DamageAmount = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName IdempotencyKey;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector StartPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	double SpeedCmPerSec = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector InheritedVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm/s"))
	FVector EffectiveVelocityCmPerSec = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double MaxDistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double CollisionRadiusCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double TargetDistanceCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	double TravelledCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double StartedAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ImpactAtTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRuntimeEncounterState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName RuntimeState = TEXT("dormant");

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ThreatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName DamageEventId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName LastPlayerWeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName LastPlayerWeaponStatId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileResponseStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName HostileManeuverStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double LastHostileWeaponFireTimeSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponReadyAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponCooldownSeconds = 0.35;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponMaxEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergyCost = 14.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergyRegenPerSecond = 28.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double HostileWeaponEnergyUpdatedAtTimeSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	int32 HostileWeaponFireSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationType;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ShotPresentationActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotStartPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter", meta = (Units = "cm"))
	FVector ShotEndPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationStartedAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double ShotPresentationDurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	TArray<FRuntimeEncounterProjectileState> Projectiles;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double EngagedAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double LastUpdatedTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double LastPlayerWeaponFireTimeSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponReadyAtTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponCooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponMaxEnergy = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergyCost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergyRegenPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponEnergyUpdatedAtTimeSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponMinAlignment = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	double PlayerWeaponAlignmentDot = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	int32 PlayerWeaponFireSequence = 0;
};

UCLASS()
class STARGAME_API UStargameSessionSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	EStartSessionResult StartNewSession(FName InStartProfileId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetCurrentSystemId() const { return CurrentSystemId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetStartProfileId() const { return StartProfileId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetCurrentSpawnZoneId() const { return CurrentSpawnZoneId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetSelectedTargetId() const { return SelectedTargetId; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetSelectedTargetId(FName TargetId) { SelectedTargetId = TargetId; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool SelectNavigationTargetById(FName TargetId);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool CycleNavigationTarget();

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	EStartSessionResult GetLastStartSessionResult() const { return LastStartSessionResult; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FString GetLastSessionError() const { return LastSessionError; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FSimulationClockSnapshot GetSimulationClockSnapshot() const { return ClockSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void AdvanceSimulationClock(double DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool SaveDevelopmentSlot();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool LoadDevelopmentSlot();

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	bool DoesDevelopmentSaveExist() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	bool CanContinueDevelopmentSlot(FString& OutReason) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool RequestGateTransition(const FGateTransitionRequest& Request, FGateTransitionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	bool HasPendingGateArrival() const { return bHasPendingGateArrival; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FShipSaveLocation GetPendingGateArrivalLocation() const { return PendingGateArrivalLocation; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FGateTransitionResult GetLastGateTransitionResult() const { return LastGateTransitionResult; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void CompleteGateArrivalSafetyWindow();

	bool SaveDevelopmentSlot(const FStargameM0SaveState& State);
	bool LoadDevelopmentSlot(FStargameM0SaveState& OutState);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FStargameM0SaveState MakeCurrentM0SaveState() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FActiveTrafficSimulationState GetActiveTrafficState() const { return ActiveTrafficState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetActiveTrafficState(const FActiveTrafficSimulationState& NewTrafficState) { ActiveTrafficState = NewTrafficState; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FSystemicGameplayState GetSystemicGameplayState() const { return SystemicGameplayState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetSystemicGameplayState(const FSystemicGameplayState& NewSystemicState) { SystemicGameplayState = NewSystemicState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool GetDockedStationContext(FDockedStationContext& OutContext) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool GetDockedStationCommandPanel(FDockedStationCommandPanelView& OutPanel) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Market")
	bool GetDockedMarketView(FDockedMarketView& OutMarket) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool ExecuteDockedStationService(const FStationServiceRequest& Request, FStationServiceResultRecord& OutResult, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool ExecuteDockedMarketTransaction(const FMarketTransactionRequest& Request, FMarketTransactionResult& OutResult, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool AcceptDockedMissionOffer(FName OfferId, FName PlayerId, FName IdempotencyKey, FMissionInstanceState& OutMission, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool GetDockedMissionContactInteraction(FName GiverNpcId, FMissionContactInteractionView& OutInteraction) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool GetDockedMissionContactPanel(FDockedMissionContactPanelView& OutPanel) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool GetDockedInventoryEquipmentPanel(FDockedInventoryEquipmentPanelView& OutPanel) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool EquipDockedInventoryItem(FName ContainerId, FName StackId, FName SlotId, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool UnequipDockedInventoryItem(FName SlotId, FName ContainerId, FName StackIdPrefix, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool CompleteDockedMission(FName MissionInstanceId, FName SourceEventId, FName IdempotencyKey, FProgressionDebugLedgerEntry& OutCompletionEntry, FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool RequestDockedUndock(FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool EnterDockedStationInterior(FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	bool ExitStationInteriorAndUndock(FDockedStationCommandResult& OutCommandResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool ResolveStationInteriorCombatProfile(FName ProfileId, FStationInteriorCombatProfileDefinition& OutProfile) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool GetStationInteriorCombatView(FStationInteriorCombatView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Mission")
	bool GetActiveMissionWaypoint(FMissionWaypointViewModel& OutWaypoint) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Mission")
	bool TryProgressActiveMissionWaypoint();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Mission")
	bool TryCompleteActiveHostileBoardingObjective(FName StationId);

	UFUNCTION(BlueprintPure, Category = "Stargame|Encounter")
	FRuntimeEncounterViewModel GetRuntimeEncounterView() const { return LastRuntimeEncounterView; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool TryUpdateRuntimeEncounterBehavior();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool ResolveRuntimeEncounter(FName EncounterId);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool ApplyRuntimeEncounterOutcome(FName EncounterId, FName OutcomeType);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool FireEquippedShipWeaponAtRuntimeEncounter(FName EncounterId, FShipWeaponFireResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Map")
	bool GetSystemMapOverview(FSystemMapOverviewView& OutMap) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|World Life")
	bool GetDistantSectorSnapshot(FDistantSectorSnapshotView& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool IsInStationInterior() const { return ActiveStationInteriorPawn != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	AStationInteriorRoomActor* GetActiveStationInteriorRoom() const { return ActiveStationInteriorRoom; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FString GetM0DebugSummary() const;

	static constexpr const TCHAR* DevelopmentSlotName = TEXT("m0_development");

#if WITH_DEV_AUTOMATION_TESTS
	void ConfigureAutomationTestContext(UWorld* InWorld, UStarCatalogSubsystem* InCatalog);
#endif

private:
	UWorld* ResolveSessionWorld() const;
	UStarCatalogSubsystem* ResolveCatalogSubsystem() const;
	bool BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile);
	void ClearSessionState();
	void ReportStartupError(const FString& Error);
	void ReportSessionFailure(const FString& Error);
	bool ValidateLoadedTrafficState(const FStarSystemDefinition& SystemDefinition, const FActiveTrafficSimulationState& TrafficState, FString& OutError) const;
	bool ValidateLoadedSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& LoadedSystemicState, FString& OutError) const;
	bool ValidateLoadedSaveHeader(const FStargameM0SaveState& LoadedState, FString& OutError) const;
	bool ResolveRuntimeSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState* SavedSystemicState, FSystemicGameplayState& OutSystemicState, FString& OutError) const;
	bool RestoreSavedShipLocation(const FShipSaveLocation& ShipLocation, APlayerController* PlayerController, FString& OutError);
	bool ResolveGateArrivalLocation(const FStarSystemDefinition& SystemDefinition, const FShipSaveLocation& GateArrivalLocation, FTransform& OutTransform, FVector& OutVelocityCmPerSec, FString& OutError) const;
	bool ResolveCurrentDockedStation(FDockedStationContext& OutContext, ASpaceFlightPawn** OutFlightPawn = nullptr) const;
	bool RejectDockedStationCommand(const FString& FailureReason, FDockedStationCommandResult& OutCommandResult) const;
	bool ResolveActiveMissionAndPrimaryObjective(const FMissionInstanceState*& OutMission, const FObjectiveState*& OutObjective) const;
	bool ResolveMutableActiveMissionAndPrimaryObjective(FMissionInstanceState*& OutMission, FObjectiveState*& OutObjective);
	bool IsMissionReadyForTurnIn(FName MissionInstanceId) const;
	void AutoSelectMissionWaypoint(const FMissionInstanceState& Mission);
	void CleanupStationInterior();
	bool UpdateRuntimeEncounterProjectiles(FRuntimeEncounterState& RuntimeState, const FRealizedEncounterActorEntry* EncounterActor, double SimulationTimeSeconds);
	bool TryQueueHostileProjectileAtPlayer(FRuntimeEncounterState& RuntimeState, const FRealizedEncounterActorEntry& EncounterActor, const ASpaceFlightPawn& PlayerPawn, double SimulationTimeSeconds);

	UPROPERTY()
	FName StartProfileId = TEXT("frontier_test_start");

	UPROPERTY()
	FName CurrentSystemId;

	UPROPERTY()
	FName CurrentSpawnZoneId;

	UPROPERTY()
	FName SelectedTargetId;

	UPROPERTY()
	FSimulationClockSnapshot ClockSnapshot;

	UPROPERTY()
	FActiveTrafficSimulationState ActiveTrafficState;

	UPROPERTY()
	FSystemicGameplayState SystemicGameplayState;

	UPROPERTY()
	FRuntimeEncounterViewModel LastRuntimeEncounterView;

	UPROPERTY()
	TArray<FRuntimeEncounterState> RuntimeEncounterStates;

	UPROPERTY()
	bool bHasPendingGateArrival = false;

	UPROPERTY()
	FShipSaveLocation PendingGateArrivalLocation;

	UPROPERTY()
	FGateTransitionResult LastGateTransitionResult;

	UPROPERTY()
	TSubclassOf<APawn> ActivePlayerPawnClass;

	UPROPERTY()
	TObjectPtr<AStationInteriorPawn> ActiveStationInteriorPawn;

	UPROPERTY()
	TObjectPtr<AStationInteriorRoomActor> ActiveStationInteriorRoom;

	UPROPERTY()
	TObjectPtr<ASpaceFlightPawn> StationInteriorSpacePawn;

#if WITH_DEV_AUTOMATION_TESTS
	UWorld* AutomationTestWorld = nullptr;

	UStarCatalogSubsystem* AutomationTestCatalog = nullptr;
#endif

	EStartSessionResult LastStartSessionResult = EStartSessionResult::ValidationFailed;
	FString LastSessionError;
};

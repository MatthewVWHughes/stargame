#include "Runtime/StargameSessionSubsystem.h"

#include "Data/FrontierTestFixtureProvider.h"
#include "Data/StarCatalogSubsystem.h"
#include "Engine/Engine.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/StargameM0SaveGame.h"
#include "Space/LogicalTrafficActor.h"
#include "Space/LogicalTrafficQueryService.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"
#include "Space/SystemicGameplayQueryService.h"
#include "Station/StationInteriorPawn.h"
#include "Station/StationInteriorRoomActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogStargameStartup, Log, All);

namespace
{
	bool HasAuthoredSystemicGameplayState(const FSystemicGameplayState& State)
	{
		return !State.Events.IsEmpty() ||
			!State.EventResults.IsEmpty() ||
			!State.Transactions.IsEmpty() ||
			!State.TransactionJournal.IsEmpty() ||
			!State.CreditAccounts.IsEmpty() ||
			!State.CreditLedger.IsEmpty() ||
			!State.EscrowHolds.IsEmpty() ||
			!State.Factions.IsEmpty() ||
			!State.FactionRelationships.IsEmpty() ||
			!State.FactionOperations.IsEmpty() ||
			!State.Jurisdictions.IsEmpty() ||
			!State.Offenses.IsEmpty() ||
			!State.EvidenceRecords.IsEmpty() ||
			!State.CriminalRecords.IsEmpty() ||
			!State.Items.IsEmpty() ||
			!State.ShipWeaponStats.IsEmpty() ||
			!State.ShipEquipmentStats.IsEmpty() ||
			!State.StationInteriorCombatProfiles.IsEmpty() ||
			!State.Containers.IsEmpty() ||
			!State.EquipmentSlots.IsEmpty() ||
			!State.CargoTransferResults.IsEmpty() ||
			!State.CargoManifests.IsEmpty() ||
			!State.Commodities.IsEmpty() ||
			!State.CommodityItemBridges.IsEmpty() ||
			!State.Markets.IsEmpty() ||
			!State.MarketTransactionResults.IsEmpty() ||
			!State.StationServiceEndpoints.IsEmpty() ||
			!State.CommsEndpoints.IsEmpty() ||
			!State.MessageDefinitions.IsEmpty() ||
			!State.MessageLog.IsEmpty() ||
			!State.MissionDefinitions.IsEmpty() ||
			!State.MissionOffers.IsEmpty() ||
			!State.MissionInstances.IsEmpty() ||
			!State.ObjectiveStates.IsEmpty() ||
			!State.LogicalContactTracks.IsEmpty() ||
			!State.RouteSecuritySnapshots.IsEmpty() ||
			!State.PatrolReservations.IsEmpty() ||
			!State.InterdictionHazards.IsEmpty() ||
			!State.DistressEvents.IsEmpty() ||
			!State.LogicalEncounters.IsEmpty() ||
			!State.AbstractEngagements.IsEmpty() ||
			!State.ActorPromotionAttachments.IsEmpty() ||
			!State.RealizedActorBudgetProfiles.IsEmpty() ||
			!State.RealizedActorMappings.IsEmpty() ||
			!State.RealizedDemotionSnapshots.IsEmpty() ||
			!State.RealizedSteeringIntents.IsEmpty() ||
			!State.RealizedCommsHooks.IsEmpty() ||
			!State.ShipDurabilityStates.IsEmpty() ||
			!State.ThreatRecords.IsEmpty() ||
			!State.DamageEvents.IsEmpty() ||
			!State.ShipResourceStates.IsEmpty() ||
			!State.StationServiceResults.IsEmpty() ||
			!State.ReputationDeltas.IsEmpty() ||
			!State.FollowUpOpportunities.IsEmpty() ||
			!State.MessageArbitrationResults.IsEmpty() ||
			!State.ProgressionDebugLedger.IsEmpty();
	}

	bool ValidateResolvedSystemDefinitionForRuntime(const UStarCatalogSubsystem* Catalog, const FStarSystemDefinition& SystemDefinition, FString& OutError)
	{
		if (Catalog && Catalog->IsUsingAssetCatalog())
		{
			return true;
		}

		return FFrontierTestFixtureProvider::ValidateM0Fixture(SystemDefinition, OutError);
	}

	bool GoalReferencesValidRoute(const FShipGoalState& Goal, const TSet<FName>& RouteIds, FString& OutError)
	{
		if (Goal.GoalKind != EShipGoalKind::TradeRoute)
		{
			return true;
		}

		if (Goal.RouteSegmentId.IsNone() || !RouteIds.Contains(Goal.RouteSegmentId))
		{
			OutError = FString::Printf(TEXT("Saved traffic goal references missing route '%s'."), *Goal.RouteSegmentId.ToString());
			return false;
		}

		return true;
	}

	bool ValidateSavedShipLocationAgainstSystem(const FStarSystemDefinition& SystemDefinition, const FStargameM0SaveState& LoadedState, FString& OutError)
	{
		if (LoadedState.ShipLocation.LocationMode == EShipLocationMode::Respawn)
		{
			return true;
		}

		if (LoadedState.ShipLocation.SystemId != LoadedState.SystemId)
		{
			OutError = FString::Printf(TEXT("Saved ship location system '%s' does not match save system '%s'."),
				*LoadedState.ShipLocation.SystemId.ToString(),
				*LoadedState.SystemId.ToString());
			return false;
		}

		if (LoadedState.ShipLocation.LocationMode == EShipLocationMode::FreeFlight)
		{
			const bool bSystemFrame = LoadedState.ShipLocation.CoordinateFrame.FrameType == FName(TEXT("system_barycentric")) &&
				LoadedState.ShipLocation.CoordinateFrame.AnchorId == LoadedState.SystemId;
			if (!bSystemFrame)
			{
				OutError = TEXT("Saved free-flight location must use a system_barycentric coordinate frame.");
				return false;
			}
			return true;
		}

		if (LoadedState.ShipLocation.LocationMode == EShipLocationMode::GateArrival)
		{
			const bool bGateFrame = LoadedState.ShipLocation.CoordinateFrame.FrameType == FName(TEXT("gate_relative")) &&
				LoadedState.ShipLocation.CoordinateFrame.AnchorId == LoadedState.ShipLocation.AnchorId &&
				!LoadedState.ShipLocation.GateArrivalId.IsNone() &&
				!LoadedState.ShipLocation.AnchorId.IsNone();
			if (!bGateFrame)
			{
				OutError = TEXT("Saved gate arrival location must use a gate_relative frame anchored to the destination gate.");
				return false;
			}

			const bool bArrivalResolves = SystemDefinition.SpawnZones.ContainsByPredicate([&LoadedState](const FSpawnZoneDefinition& SpawnZone)
			{
				return SpawnZone.SpawnZoneId == LoadedState.ShipLocation.GateArrivalId;
			});
			if (!bArrivalResolves)
			{
				OutError = FString::Printf(TEXT("Saved gate arrival marker '%s' does not resolve in system '%s'."),
					*LoadedState.ShipLocation.GateArrivalId.ToString(),
					*LoadedState.SystemId.ToString());
				return false;
			}
			return true;
		}

		if (LoadedState.ShipLocation.LocationMode == EShipLocationMode::StationDocked)
		{
			const FStationDefinition* DockedStation = SystemDefinition.Stations.FindByPredicate([&LoadedState](const FStationDefinition& Station)
			{
				return Station.StationId == LoadedState.ShipLocation.DockedStationId;
			});
			const bool bDockingPortAuthored = DockedStation && DockedStation->DockingPorts.ContainsByPredicate([&LoadedState](const FDockingPortDefinition& Port)
			{
				return Port.PortId == LoadedState.ShipLocation.DockingPortId;
			});
			if (!bDockingPortAuthored)
			{
				OutError = TEXT("Saved docked location does not resolve to an authored station docking port.");
				return false;
			}
			return true;
		}

		OutError = TEXT("Saved ship location mode is unsupported.");
		return false;
	}

	ASpaceFlightPawn* ResolveSpaceFlightPawn(UWorld* World, APlayerController* PlayerController)
	{
		if (ASpaceFlightPawn* PossessedPawn = PlayerController ? Cast<ASpaceFlightPawn>(PlayerController->GetPawn()) : nullptr)
		{
			return PossessedPawn;
		}

		if (!World)
		{
			return nullptr;
		}

		UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
		if (APawn* ActivePlayerPawn = StarSystem ? StarSystem->GetActivePlayerPawn() : nullptr)
		{
			return Cast<ASpaceFlightPawn>(ActivePlayerPawn);
		}

		return nullptr;
	}

	FName EncounterRoleName(EShipGoalKind GoalKind)
	{
		switch (GoalKind)
		{
		case EShipGoalKind::Pirate:
			return TEXT("pirate");
		case EShipGoalKind::Patrol:
			return TEXT("patrol");
		case EShipGoalKind::Escort:
			return TEXT("escort");
		case EShipGoalKind::Flee:
			return TEXT("flee");
		case EShipGoalKind::Fight:
			return TEXT("fight");
		case EShipGoalKind::TradeRoute:
			return TEXT("trade_route");
		default:
			return TEXT("none");
		}
	}

	const FLogicalEncounterRecord* FindRuntimeEncounter(const FSystemicGameplayState& State, FName EncounterId)
	{
		return State.LogicalEncounters.FindByPredicate([EncounterId](const FLogicalEncounterRecord& Candidate)
		{
			return Candidate.EncounterId == EncounterId;
		});
	}

	const FThreatRecord* FindRuntimeThreat(const FSystemicGameplayState& State, FName ThreatId)
	{
		return State.ThreatRecords.FindByPredicate([ThreatId](const FThreatRecord& Candidate)
		{
			return Candidate.ThreatId == ThreatId;
		});
	}

	const FItemDefinition* FindRuntimeItem(const FSystemicGameplayState& State, FName ItemId)
	{
		return State.Items.FindByPredicate([ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		});
	}

	const FShipWeaponStatDefinition* FindRuntimeShipWeaponStats(const FSystemicGameplayState& State, FName ItemId)
	{
		return State.ShipWeaponStats.FindByPredicate([ItemId](const FShipWeaponStatDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		});
	}

	const FShipWeaponStatDefinition* FindRuntimeShipWeaponStatsById(const FSystemicGameplayState& State, FName WeaponStatId)
	{
		return State.ShipWeaponStats.FindByPredicate([WeaponStatId](const FShipWeaponStatDefinition& Candidate)
		{
			return Candidate.WeaponStatId == WeaponStatId;
		});
	}

	const FShipEquipmentStatDefinition* FindRuntimeShipEquipmentStats(const FSystemicGameplayState& State, FName ItemId)
	{
		return State.ShipEquipmentStats.FindByPredicate([ItemId](const FShipEquipmentStatDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		});
	}

	const FShipDurabilityState* FindRuntimeDurability(const FSystemicGameplayState& State, FName CombatantId)
	{
		return State.ShipDurabilityStates.FindByPredicate([CombatantId](const FShipDurabilityState& Candidate)
		{
			return Candidate.CombatantId == CombatantId;
		});
	}

	double ResolveRuntimeShipWeaponDamage(const FItemDefinition& WeaponItem, const FShipWeaponStatDefinition* WeaponStats)
	{
		if (WeaponStats && WeaponStats->DamageAmount > 0.0)
		{
			return WeaponStats->DamageAmount;
		}
		// Godot weapons carry explicit projectile damage. Unreal's current item bridge only has value,
		// so use a stable value-derived pulse-laser damage until weapon stats are authored.
		return FMath::Clamp(static_cast<double>(WeaponItem.BaseValue) / 25.0, 12.0, 40.0);
	}

	double ResolveRuntimeShipWeaponCooldownSeconds(const FItemDefinition& WeaponItem, const FShipWeaponStatDefinition* WeaponStats)
	{
		if (WeaponStats && WeaponStats->CooldownSeconds > 0.0)
		{
			return WeaponStats->CooldownSeconds;
		}
		// Until authored weapon fire-rate records land, derive a stable short cooldown from item value.
		return FMath::Clamp(static_cast<double>(WeaponItem.BaseValue) / 500.0, 0.75, 2.5);
	}

	double ResolveRuntimeShipWeaponEnergyCost(const FShipWeaponStatDefinition* WeaponStats)
	{
		return WeaponStats && WeaponStats->EnergyCost > 0.0
			? WeaponStats->EnergyCost
			: 9.0;
	}

	double ResolveRuntimeShipWeaponEnergyRegenPerSecond(const FShipWeaponStatDefinition* WeaponStats)
	{
		return WeaponStats && WeaponStats->EnergyRegenPerSecond > 0.0
			? WeaponStats->EnergyRegenPerSecond
			: 30.0;
	}

	double ResolveRuntimeShipWeaponMinAlignment(const FShipWeaponStatDefinition* WeaponStats)
	{
		return WeaponStats && WeaponStats->MinAlignment > 0.0
			? WeaponStats->MinAlignment
			: 0.955;
	}

	double ResolveRuntimeShipWeaponProjectileSpeedCmPerSec(const FShipWeaponStatDefinition* WeaponStats)
	{
		return WeaponStats && WeaponStats->ProjectileSpeedCmPerSec > 0.0
			? WeaponStats->ProjectileSpeedCmPerSec
			: 76000.0;
	}

	double ResolveRuntimeShipWeaponProjectileCollisionRadiusCm(const FShipWeaponStatDefinition* WeaponStats)
	{
		return WeaponStats && WeaponStats->ProjectileCollisionRadiusCm > 0.0
			? WeaponStats->ProjectileCollisionRadiusCm
			: 850.0;
	}

	bool SegmentIntersectsSphere(const FVector& SegmentStart, const FVector& SegmentEnd, const FVector& SphereCenter, double RadiusCm)
	{
		const FVector Segment = SegmentEnd - SegmentStart;
		const double SegmentLengthSq = Segment.SizeSquared();
		if (SegmentLengthSq <= UE_SMALL_NUMBER)
		{
			return FVector::DistSquared(SegmentStart, SphereCenter) <= FMath::Square(RadiusCm);
		}

		const double T = FMath::Clamp(FVector::DotProduct(SphereCenter - SegmentStart, Segment) / SegmentLengthSq, 0.0, 1.0);
		const FVector ClosestPoint = SegmentStart + Segment * T;
		return FVector::DistSquared(ClosestPoint, SphereCenter) <= FMath::Square(RadiusCm);
	}

	bool TryGetRuntimeInterceptPoint(
		const FVector& ShooterPositionCm,
		const FVector& ShooterVelocityCmPerSec,
		const FVector& TargetPositionCm,
		const FVector& TargetVelocityCmPerSec,
		double ProjectileSpeedCmPerSec,
		FVector& OutInterceptPointCm)
	{
		const FVector RelativePosition = TargetPositionCm - ShooterPositionCm;
		const FVector RelativeVelocity = TargetVelocityCmPerSec - ShooterVelocityCmPerSec;
		const double A = RelativeVelocity.SizeSquared() - FMath::Square(ProjectileSpeedCmPerSec);
		const double B = 2.0 * FVector::DotProduct(RelativePosition, RelativeVelocity);
		const double C = RelativePosition.SizeSquared();

		double TimeSeconds = 0.0;
		if (FMath::Abs(A) < 0.0001)
		{
			if (FMath::Abs(B) < 0.0001)
			{
				OutInterceptPointCm = TargetPositionCm;
				return false;
			}

			TimeSeconds = -C / B;
		}
		else
		{
			const double Discriminant = B * B - 4.0 * A * C;
			if (Discriminant < 0.0)
			{
				OutInterceptPointCm = TargetPositionCm;
				return false;
			}

			const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
			const double T0 = (-B - SqrtDiscriminant) / (2.0 * A);
			const double T1 = (-B + SqrtDiscriminant) / (2.0 * A);
			const bool bT0Valid = T0 > 0.0 && FMath::IsFinite(T0);
			const bool bT1Valid = T1 > 0.0 && FMath::IsFinite(T1);
			if (bT0Valid && bT1Valid)
			{
				TimeSeconds = FMath::Min(T0, T1);
			}
			else if (bT0Valid)
			{
				TimeSeconds = T0;
			}
			else if (bT1Valid)
			{
				TimeSeconds = T1;
			}
		}

		if (TimeSeconds <= 0.0 || !FMath::IsFinite(TimeSeconds))
		{
			OutInterceptPointCm = TargetPositionCm;
			return false;
		}

		OutInterceptPointCm = TargetPositionCm + TargetVelocityCmPerSec * TimeSeconds;
		return true;
	}

	FString BuildRuntimeFireSolutionText(bool bInRange, bool bHasEnergy, bool bHasLeadPoint, bool bAligned)
	{
		const TCHAR* RangeText = bInRange ? TEXT("RANGE OK") : TEXT("OUT OF RANGE");
		const TCHAR* EnergyText = bHasEnergy ? TEXT("CAP OK") : TEXT("CAP LOW");
		const TCHAR* AimText = bHasLeadPoint ? (bAligned ? TEXT("AIM READY") : TEXT("AIM OFF")) : TEXT("NO LEAD");
		return FString::Printf(TEXT("Solution: %s   |   %s   |   %s"), RangeText, EnergyText, AimText);
	}

	void RegenerateRuntimePlayerWeaponEnergy(FRuntimeEncounterState& RuntimeState, double SimulationTimeSeconds, double MaxEnergy, double RegenPerSecond)
	{
		RuntimeState.PlayerWeaponMaxEnergy = MaxEnergy > UE_SMALL_NUMBER ? MaxEnergy : 100.0;
		RuntimeState.PlayerWeaponEnergyRegenPerSecond = FMath::Max(0.0, RegenPerSecond);
		RuntimeState.PlayerWeaponEnergy = FMath::Clamp(RuntimeState.PlayerWeaponEnergy, 0.0, RuntimeState.PlayerWeaponMaxEnergy);

		if (RuntimeState.PlayerWeaponEnergyUpdatedAtTimeSeconds < 0.0)
		{
			RuntimeState.PlayerWeaponEnergyUpdatedAtTimeSeconds = SimulationTimeSeconds;
			return;
		}

		const double ElapsedSeconds = FMath::Max(0.0, SimulationTimeSeconds - RuntimeState.PlayerWeaponEnergyUpdatedAtTimeSeconds);
		RuntimeState.PlayerWeaponEnergy = FMath::Min(
			RuntimeState.PlayerWeaponMaxEnergy,
			RuntimeState.PlayerWeaponEnergy + RuntimeState.PlayerWeaponEnergyRegenPerSecond * ElapsedSeconds);
		RuntimeState.PlayerWeaponEnergyUpdatedAtTimeSeconds = SimulationTimeSeconds;
	}

	const FRuntimeEncounterProjectileState* FindLastRuntimeProjectile(const FRuntimeEncounterState& RuntimeState)
	{
		return RuntimeState.Projectiles.IsEmpty() ? nullptr : &RuntimeState.Projectiles.Last();
	}

	FName ResolveHostileResponseState(FName RuntimeThreatId, FName LocalBehaviorStateId)
	{
		if (RuntimeThreatId.IsNone())
		{
			return TEXT("hostile_alerted");
		}

		if (LocalBehaviorStateId == FName(TEXT("local_intercept_closing")) ||
			LocalBehaviorStateId == FName(TEXT("local_priority_intercept")))
		{
			return TEXT("hostile_return_fire");
		}

		if (LocalBehaviorStateId == FName(TEXT("local_probe_scan")))
		{
			return TEXT("hostile_break_scan");
		}

		return TEXT("hostile_repositioning");
	}

	FVector2D ProjectSystemMapPosition(const FStargameScaleContract& Scale, const FVector& PositionCm)
	{
		const double Divisor = FMath::Max(Scale.MapDistanceScaleCmPerUnit, SMALL_NUMBER);
		return FVector2D(PositionCm.X, PositionCm.Y) / Divisor;
	}

	void EnsureRuntimeGodotMarketCommodity(FSystemicGameplayState& State, FName CommodityId, const TCHAR* DisplayName, int64 BasePrice, FName Category, int32 Stock, int32 MaxStock)
	{
		if (!State.Items.ContainsByPredicate([CommodityId](const FItemDefinition& Item)
		{
			return Item.ItemId == CommodityId;
		}))
		{
			FItemDefinition Item;
			Item.ItemId = CommodityId;
			Item.DisplayName = FText::FromString(DisplayName);
			Item.ItemType = TEXT("commodity");
			Item.StackLimit = 999;
			Item.MassPerUnitKg = 1.0;
			Item.VolumePerUnitM3 = 1.0;
			Item.BaseValue = BasePrice;
			State.Items.Add(Item);
		}
		if (!State.Commodities.ContainsByPredicate([CommodityId](const FCommodityDefinition& Commodity)
		{
			return Commodity.CommodityId == CommodityId;
		}))
		{
			FCommodityDefinition Commodity;
			Commodity.CommodityId = CommodityId;
			Commodity.DisplayName = FText::FromString(DisplayName);
			Commodity.BasePrice = BasePrice;
			Commodity.MassPerUnitKg = 1.0;
			Commodity.VolumePerUnitM3 = 1.0;
			Commodity.Category = Category;
			State.Commodities.Add(Commodity);
		}
		const FName BridgeId(*FString::Printf(TEXT("bridge_%s"), *CommodityId.ToString()));
		if (!State.CommodityItemBridges.ContainsByPredicate([BridgeId](const FCommodityItemBridge& Bridge)
		{
			return Bridge.CommodityItemBridgeId == BridgeId;
		}))
		{
			FCommodityItemBridge Bridge;
			Bridge.CommodityItemBridgeId = BridgeId;
			Bridge.CommodityId = CommodityId;
			Bridge.ItemId = CommodityId;
			State.CommodityItemBridges.Add(Bridge);
		}
		if (FStationMarketState* Market = State.Markets.FindByPredicate([](const FStationMarketState& Candidate)
		{
			return Candidate.MarketId == FName(TEXT("brink_watch"));
		}))
		{
			Market->StockByCommodity.FindOrAdd(CommodityId) = Stock;
			Market->MaxStockByCommodity.FindOrAdd(CommodityId) = MaxStock;
		}
		if (FContainerState* MarketCargo = State.Containers.FindByPredicate([](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == FName(TEXT("brink_watch_market_inventory"));
		}))
		{
			if (!MarketCargo->Stacks.ContainsByPredicate([CommodityId](const FItemStackState& Stack)
			{
				return Stack.ItemId == CommodityId;
			}))
			{
				FItemStackState Stack;
				Stack.StackId = FName(*FString::Printf(TEXT("stack_brink_%s_01"), *CommodityId.ToString()));
				Stack.ItemId = CommodityId;
				Stack.Quantity = Stock;
				Stack.OwnerFactionId = TEXT("frontier_local_authority");
				MarketCargo->Stacks.Add(Stack);
			}
		}
	}

	void EnsureRuntimeGodotFrontierMarket(FSystemicGameplayState& State)
	{
		if (!State.Markets.ContainsByPredicate([](const FStationMarketState& Market)
		{
			return Market.MarketId == FName(TEXT("brink_watch"));
		}))
		{
			return;
		}
		EnsureRuntimeGodotMarketCommodity(State, TEXT("food"), TEXT("Food"), 10, TEXT("consumer"), 80, 120);
		EnsureRuntimeGodotMarketCommodity(State, TEXT("water"), TEXT("Water"), 6, TEXT("life_support"), 45, 100);
		EnsureRuntimeGodotMarketCommodity(State, TEXT("iron"), TEXT("Iron"), 14, TEXT("ore"), 160, 220);
		EnsureRuntimeGodotMarketCommodity(State, TEXT("synthetic_parts"), TEXT("Synthetic Parts"), 40, TEXT("industrial"), 18, 60);
	}

	FRuntimeEncounterState* FindRuntimeEncounterState(TArray<FRuntimeEncounterState>& States, FName EncounterId)
	{
		return States.FindByPredicate([EncounterId](const FRuntimeEncounterState& Candidate)
		{
			return Candidate.EncounterId == EncounterId;
		});
	}

	const FRuntimeEncounterState* FindRuntimeEncounterState(const TArray<FRuntimeEncounterState>& States, FName EncounterId)
	{
		return States.FindByPredicate([EncounterId](const FRuntimeEncounterState& Candidate)
		{
			return Candidate.EncounterId == EncounterId;
		});
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UStargameSessionSubsystem::ConfigureAutomationTestContext(UWorld* InWorld, UStarCatalogSubsystem* InCatalog)
{
	AutomationTestWorld = InWorld;
	AutomationTestCatalog = InCatalog;
}
#endif

UWorld* UStargameSessionSubsystem::ResolveSessionWorld() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (AutomationTestWorld)
	{
		return AutomationTestWorld;
	}
#endif

	return GetWorld();
}

UStarCatalogSubsystem* UStargameSessionSubsystem::ResolveCatalogSubsystem() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (AutomationTestCatalog)
	{
		return AutomationTestCatalog;
	}
#endif

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
}

void UStargameSessionSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime > 0.0f)
	{
		AdvanceSimulationClock(DeltaTime);
	}
}

TStatId UStargameSessionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UStargameSessionSubsystem, STATGROUP_Tickables);
}

bool UStargameSessionSubsystem::IsTickable() const
{
	return !IsTemplate() && !CurrentSystemId.IsNone();
}

EStartSessionResult UStargameSessionSubsystem::StartNewSession(FName InStartProfileId)
{
	LastSessionError.Reset();
	LastStartSessionResult = EStartSessionResult::ValidationFailed;

	const FName RequestedStartProfileId = InStartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: InStartProfileId;

	FStartProfileDefinition StartProfile;
	UStarCatalogSubsystem* Catalog = ResolveCatalogSubsystem();
	const bool bResolvedStartProfile = Catalog && Catalog->ResolveStartProfile(RequestedStartProfileId, StartProfile);
	if (!bResolvedStartProfile)
	{
		LastStartSessionResult = EStartSessionResult::MissingStartProfile;
		ReportStartupError(FString::Printf(TEXT("Missing start profile '%s'."), *RequestedStartProfileId.ToString()));
		return LastStartSessionResult;
	}

	if (!BuildAndSpawnFromStartProfile(StartProfile))
	{
		return LastStartSessionResult;
	}

	LastStartSessionResult = EStartSessionResult::Success;
	UE_LOG(LogStargameStartup, Display, TEXT("Started session '%s' in system '%s' at spawn zone '%s'."),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString());
	return LastStartSessionResult;
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot()
{
	return SaveDevelopmentSlot(MakeCurrentM0SaveState());
}

bool UStargameSessionSubsystem::SaveDockedAutosave(const TCHAR* Reason)
{
	const bool bSaved = SaveDevelopmentSlot();
	bLastAutosaveSuccessful = bSaved;
	LastAutosaveStatus = bSaved
		? FString::Printf(TEXT("Autosaved while docked: %s."), Reason ? Reason : TEXT("station action"))
		: FString::Printf(TEXT("Docked autosave failed after %s."), Reason ? Reason : TEXT("station action"));
	UE_LOG(LogStargameStartup, Display, TEXT("%s"), *LastAutosaveStatus);
	return bSaved;
}

void UStargameSessionSubsystem::SetSystemicGameplayState(const FSystemicGameplayState& NewSystemicState)
{
	SystemicGameplayState = NewSystemicState;
	RefreshPlayerShipEquipmentEffects();
}

FResolvedShipEquipmentStats UStargameSessionSubsystem::ResolvePlayerShipEquipmentStats() const
{
	FResolvedShipEquipmentStats Stats;
	Stats.bResolved = true;
	Stats.BaseMaxShield = 100.0;
	Stats.ResolvedMaxShield = Stats.BaseMaxShield;

	for (const FEquipmentSlotState& Slot : SystemicGameplayState.EquipmentSlots)
	{
		if (Slot.OwnerId != FName(TEXT("player_ship")) || Slot.EquippedStack.ItemId.IsNone() || Slot.EquippedStack.Quantity <= 0)
		{
			continue;
		}

		const FShipEquipmentStatDefinition* EquipmentStats = FindRuntimeShipEquipmentStats(SystemicGameplayState, Slot.EquippedStack.ItemId);
		if (!EquipmentStats)
		{
			continue;
		}

		Stats.EquippedItemIds.AddUnique(Slot.EquippedStack.ItemId);
		Stats.MaxShieldBonus += FMath::Max(0.0, EquipmentStats->MaxShieldBonus);
		if (EquipmentStats->NormalMaxSpeedMultiplier > 0.0)
		{
			Stats.NormalMaxSpeedMultiplier *= EquipmentStats->NormalMaxSpeedMultiplier;
		}
		if (EquipmentStats->ThrustAccelerationMultiplier > 0.0)
		{
			Stats.ThrustAccelerationMultiplier *= EquipmentStats->ThrustAccelerationMultiplier;
		}
		if (EquipmentStats->StrafeAccelerationMultiplier > 0.0)
		{
			Stats.StrafeAccelerationMultiplier *= EquipmentStats->StrafeAccelerationMultiplier;
		}
	}

	Stats.ResolvedMaxShield = Stats.BaseMaxShield + Stats.MaxShieldBonus;
	Stats.DebugReason = FString::Printf(
		TEXT("Resolved %d installed ship equipment stat records: shield %.0f, speed x%.2f, thrust x%.2f, strafe x%.2f."),
		Stats.EquippedItemIds.Num(),
		Stats.ResolvedMaxShield,
		Stats.NormalMaxSpeedMultiplier,
		Stats.ThrustAccelerationMultiplier,
		Stats.StrafeAccelerationMultiplier);
	return Stats;
}

void UStargameSessionSubsystem::RefreshPlayerShipEquipmentEffects()
{
	const FResolvedShipEquipmentStats Stats = ResolvePlayerShipEquipmentStats();
	ApplyPlayerShipEquipmentEffectsToDurability(Stats);
	ApplyPlayerShipEquipmentEffectsToPawn(Stats);
}

void UStargameSessionSubsystem::ApplyPlayerShipEquipmentEffectsToDurability(const FResolvedShipEquipmentStats& Stats)
{
	FShipDurabilityState* PlayerDurability = SystemicGameplayState.ShipDurabilityStates.FindByPredicate([](const FShipDurabilityState& Candidate)
	{
		return Candidate.CombatantId == FName(TEXT("player_ship"));
	});
	if (!PlayerDurability)
	{
		return;
	}

	const double PreviousShield = PlayerDurability->Shield;
	PlayerDurability->MaxShield = FMath::Max(1.0, Stats.ResolvedMaxShield);
	PlayerDurability->Shield = FMath::Clamp(PreviousShield, 0.0, PlayerDurability->MaxShield);
}

void UStargameSessionSubsystem::ApplyPlayerShipEquipmentEffectsToPawn(const FResolvedShipEquipmentStats& Stats)
{
	UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	ASpaceFlightPawn* PlayerPawn = StarSystem ? Cast<ASpaceFlightPawn>(StarSystem->GetActivePlayerPawn()) : nullptr;
	if (!PlayerPawn)
	{
		return;
	}

	PlayerPawn->ApplyShipEquipmentFlightStats(
		static_cast<float>(Stats.NormalMaxSpeedMultiplier),
		static_cast<float>(Stats.ThrustAccelerationMultiplier),
		static_cast<float>(Stats.StrafeAccelerationMultiplier));
}

bool UStargameSessionSubsystem::DoesDevelopmentSaveExist() const
{
	constexpr int32 UserIndex = 0;
	return UGameplayStatics::DoesSaveGameExist(DevelopmentSlotName, UserIndex);
}

bool UStargameSessionSubsystem::CanContinueDevelopmentSlot(FString& OutReason) const
{
	OutReason.Reset();
	constexpr int32 UserIndex = 0;
	if (!UGameplayStatics::DoesSaveGameExist(DevelopmentSlotName, UserIndex))
	{
		OutReason = TEXT("No save found yet. Start a new game.");
		return false;
	}

	const UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::LoadGameFromSlot(DevelopmentSlotName, UserIndex));
	if (!SaveGame)
	{
		OutReason = FString::Printf(TEXT("Save slot '%s' could not be read."), DevelopmentSlotName);
		return false;
	}

	FString ValidationError;
	if (!ValidateLoadedSaveHeader(SaveGame->State, ValidationError))
	{
		OutReason = ValidationError;
		return false;
	}

	if (SaveGame->State.SystemId.IsNone() || SaveGame->State.SpawnZoneId.IsNone())
	{
		OutReason = TEXT("Save is missing its system or spawn location.");
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = ResolveCatalogSubsystem();
	bool bResolvedSystem = Catalog && Catalog->ResolveSystemDefinition(SaveGame->State.SystemId, SystemDefinition);
	if (!bResolvedSystem && Catalog && SaveGame->State.bSavedSystemIsGenerated)
	{
		FStarSystemDefinition RegeneratedSystem;
		if (Catalog->GenerateSeededPhysicalSystem(SaveGame->State.GeneratedSystemSourceMetadata.GeneratedSeed, RegeneratedSystem) &&
			RegeneratedSystem.SystemId == SaveGame->State.SystemId &&
			RegeneratedSystem.GeneratedSourceMetadata.GeneratorVersion == SaveGame->State.GeneratedSystemSourceMetadata.GeneratorVersion &&
			RegeneratedSystem.GeneratedSourceMetadata.SourceFingerprint == SaveGame->State.GeneratedSystemSourceMetadata.SourceFingerprint)
		{
			SystemDefinition = RegeneratedSystem;
			bResolvedSystem = true;
		}
	}
	if (!bResolvedSystem)
	{
		OutReason = FString::Printf(TEXT("Saved system '%s' could not be resolved."), *SaveGame->State.SystemId.ToString());
		return false;
	}
	if (!ValidateResolvedSystemDefinitionForRuntime(Catalog, SystemDefinition, ValidationError))
	{
		OutReason = ValidationError;
		return false;
	}
	if (!SystemDefinition.SpawnZones.ContainsByPredicate([&SaveGame](const FSpawnZoneDefinition& SpawnZone)
	{
		return SpawnZone.SpawnZoneId == SaveGame->State.SpawnZoneId;
	}))
	{
		OutReason = FString::Printf(TEXT("Saved spawn zone '%s' does not exist in system '%s'."),
			*SaveGame->State.SpawnZoneId.ToString(),
			*SaveGame->State.SystemId.ToString());
		return false;
	}
	if (!ValidateSavedShipLocationAgainstSystem(SystemDefinition, SaveGame->State, ValidationError))
	{
		OutReason = ValidationError;
		return false;
	}

	OutReason = FString::Printf(TEXT("Continue is available: %s."), *SaveGame->State.SystemId.ToString());
	return true;
}

void UStargameSessionSubsystem::AdvanceSimulationClock(double DeltaSeconds)
{
	const double ClampedDeltaSeconds = FMath::Max(0.0, DeltaSeconds);
	ClockSnapshot.AuthoritativeSimulationTimeSeconds += ClampedDeltaSeconds * ClockSnapshot.TimeScale;
	RuntimePresentationAccumulatorSeconds += ClampedDeltaSeconds;
	MissionProgressAccumulatorSeconds += ClampedDeltaSeconds;

	if (MissionProgressAccumulatorSeconds >= MissionProgressUpdateIntervalSeconds)
	{
		MissionProgressAccumulatorSeconds = 0.0;
		TryProgressActiveMissionWaypoint();
	}

	if (RuntimePresentationAccumulatorSeconds < RuntimePresentationUpdateIntervalSeconds)
	{
		return;
	}
	RuntimePresentationAccumulatorSeconds = 0.0;

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (StarSystem && StarSystem->GetActiveSystemId() == CurrentSystemId)
	{
		StarSystem->RefreshRegisteredEntityTransforms(ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		ULogicalTrafficQueryService::RefreshTransientRouteSamples(
			StarSystem->GetActiveSystemDefinition(),
			ClockSnapshot,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds,
			ActiveTrafficState);
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController))
		{
			ULogicalTrafficQueryService::ApplyPlayerRelevanceRealization(
				StarSystem->GetActiveSystemDefinition(),
				SystemicGameplayState,
				TEXT("actor_budget_m11_low"),
				FlightPawn->GetLogicalSystemPositionCm(),
				ClockSnapshot,
				ClockSnapshot.AuthoritativeSimulationTimeSeconds,
				ActiveTrafficState);
		}
		StarSystem->RefreshRealizedTrafficActors(ActiveTrafficState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		StarSystem->RealizeSystemicEncounterActors(SystemicGameplayState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		StarSystem->RefreshCombatShotPresentations(ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		TryUpdateRuntimeEncounterBehavior();
	}
}

bool UStargameSessionSubsystem::UpdateRuntimeEncounterProjectiles(FRuntimeEncounterState& RuntimeState, const FRealizedEncounterActorEntry* EncounterActor, double SimulationTimeSeconds)
{
	bool bUpdated = false;
	if (RuntimeState.Projectiles.IsEmpty())
	{
		return false;
	}

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const ASpaceFlightPawn* PlayerPawn = World ? ResolveSpaceFlightPawn(World, World->GetFirstPlayerController()) : nullptr;

	for (FRuntimeEncounterProjectileState& Projectile : RuntimeState.Projectiles)
	{
		if (Projectile.ProjectileState != FName(TEXT("in_flight")))
		{
			continue;
		}

		FVector EffectiveVelocityCmPerSec = Projectile.EffectiveVelocityCmPerSec;
		if (EffectiveVelocityCmPerSec.IsNearlyZero())
		{
			const FVector ProjectileDirection = Projectile.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			EffectiveVelocityCmPerSec = ProjectileDirection * Projectile.SpeedCmPerSec + Projectile.InheritedVelocityCmPerSec;
			Projectile.EffectiveVelocityCmPerSec = EffectiveVelocityCmPerSec;
		}
		const double EffectiveSpeedCmPerSec = EffectiveVelocityCmPerSec.Size();
		if (EffectiveSpeedCmPerSec <= UE_SMALL_NUMBER)
		{
			continue;
		}

		const FVector EffectiveDirection = EffectiveVelocityCmPerSec.GetSafeNormal(UE_SMALL_NUMBER, Projectile.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
		const double PreviousTravelledCm = Projectile.TravelledCm;
		const double ElapsedSeconds = FMath::Max(0.0, SimulationTimeSeconds - Projectile.StartedAtTimeSeconds);
		Projectile.TravelledCm = FMath::Min(Projectile.MaxDistanceCm, EffectiveSpeedCmPerSec * ElapsedSeconds);
		const FVector SegmentStart = Projectile.StartPositionCm + EffectiveDirection * PreviousTravelledCm;
		const FVector SegmentEnd = Projectile.StartPositionCm + EffectiveDirection * Projectile.TravelledCm;
		FVector TargetPositionCm = FVector::ZeroVector;
		bool bCanHitTarget = false;
		if (Projectile.TargetCombatantId == FName(TEXT("player_ship")) && PlayerPawn)
		{
			TargetPositionCm = PlayerPawn->GetLogicalSystemPositionCm();
			bCanHitTarget = true;
		}
		else if (const FThreatRecord* ProjectileThreat = FindRuntimeThreat(SystemicGameplayState, Projectile.ThreatId);
			ProjectileThreat && EncounterActor && EncounterActor->Actor.IsValid() && Projectile.TargetCombatantId == ProjectileThreat->DefenderId)
		{
			TargetPositionCm = EncounterActor->Actor->GetActorLocation();
			bCanHitTarget = true;
		}
		else if (EncounterActor && EncounterActor->Actor.IsValid() && Projectile.TargetCombatantId == EncounterActor->TargetShipId)
		{
			TargetPositionCm = EncounterActor->Actor->GetActorLocation();
			bCanHitTarget = true;
		}
		else if (EncounterActor && EncounterActor->Actor.IsValid() && Projectile.TargetCombatantId == EncounterActor->ActorId)
		{
			TargetPositionCm = EncounterActor->Actor->GetActorLocation();
			bCanHitTarget = true;
		}
		const bool bHitTarget = bCanHitTarget && SegmentIntersectsSphere(SegmentStart, SegmentEnd, TargetPositionCm, Projectile.CollisionRadiusCm);
		if (bHitTarget)
		{
			FDamageEventRecord WeaponDamage;
			WeaponDamage.DamageEventId = Projectile.DamageEventId;
			WeaponDamage.SourceCombatantId = Projectile.SourceCombatantId;
			WeaponDamage.TargetCombatantId = Projectile.TargetCombatantId;
			WeaponDamage.DamageType = Projectile.DamageType;
			WeaponDamage.Amount = Projectile.DamageAmount;
			WeaponDamage.AuthorityTimeSeconds = SimulationTimeSeconds;
			WeaponDamage.ThreatId = Projectile.ThreatId;
			WeaponDamage.IdempotencyKey = Projectile.IdempotencyKey;

			FString FailureReason;
			FDamageEventRecord DamageResult;
			if (USystemicGameplayQueryService::ApplyDamageEventOnce(SystemicGameplayState, WeaponDamage, DamageResult, FailureReason))
			{
				RuntimeState.DamageEventId = DamageResult.DamageEventId;
				RuntimeState.HostileResponseStateId = ResolveHostileResponseState(RuntimeState.ThreatId, LastRuntimeEncounterView.LocalBehaviorStateId);
				RuntimeState.HostileAIStateId = TEXT("engage");
				FVector ResponseStartPositionCm = FVector::ZeroVector;
				FVector ResponseEndPositionCm = FVector::ZeroVector;
				FName HostileManeuverStateId;
				if (StarSystem)
				{
					StarSystem->ApplyEncounterResponseManeuver(
						RuntimeState.EncounterId,
						RuntimeState.HostileResponseStateId,
						SimulationTimeSeconds,
						ResponseStartPositionCm,
						ResponseEndPositionCm,
						HostileManeuverStateId);
				}
				RuntimeState.HostileManeuverStateId = HostileManeuverStateId;
				RuntimeState.LastUpdatedTimeSeconds = SimulationTimeSeconds;
				Projectile.ProjectileState = TEXT("hit");
				Projectile.ImpactAtTimeSeconds = SimulationTimeSeconds;
				if (Projectile.SourceCombatantId == FName(TEXT("player_ship")))
				{
					RuntimeState.HostileWeaponCooldownSeconds = RuntimeState.HostileWeaponCooldownSeconds > UE_SMALL_NUMBER
						? RuntimeState.HostileWeaponCooldownSeconds
						: 0.35;
					RuntimeState.HostileWeaponReadyAtTimeSeconds = FMath::Max(
						RuntimeState.HostileWeaponReadyAtTimeSeconds,
						SimulationTimeSeconds + RuntimeState.HostileWeaponCooldownSeconds);
				}
				bUpdated = true;
			}
			else
			{
				LastRuntimeEncounterView.DebugReason = FailureReason;
			}
			continue;
		}

		if (Projectile.TravelledCm >= Projectile.MaxDistanceCm - UE_SMALL_NUMBER)
		{
			Projectile.ProjectileState = TEXT("expired");
			Projectile.ImpactAtTimeSeconds = SimulationTimeSeconds;
			RuntimeState.LastUpdatedTimeSeconds = SimulationTimeSeconds;
			bUpdated = true;
		}
	}

	return bUpdated;
}

bool UStargameSessionSubsystem::TryQueueHostileProjectileAtPlayer(
	FRuntimeEncounterState& RuntimeState,
	const FRealizedEncounterActorEntry& EncounterActor,
	const ASpaceFlightPawn& PlayerPawn,
	double SimulationTimeSeconds)
{
	if (RuntimeState.RuntimeState != FName(TEXT("engaged")) ||
		RuntimeState.HostileResponseStateId.IsNone() ||
		RuntimeState.HostileWeaponFireSequence >= RuntimeState.PlayerWeaponFireSequence ||
		!EncounterActor.Actor.IsValid())
	{
		return false;
	}

	const bool bHostileProjectileInFlight = RuntimeState.Projectiles.ContainsByPredicate([](const FRuntimeEncounterProjectileState& Projectile)
	{
		return Projectile.ProjectileState == FName(TEXT("in_flight")) &&
			Projectile.SourceCombatantId != FName(TEXT("player_ship"));
	});
	if (bHostileProjectileInFlight)
	{
		return false;
	}

	const FShipWeaponStatDefinition* HostileWeaponStats = FindRuntimeShipWeaponStatsById(SystemicGameplayState, TEXT("weapon_stat_hostile_pulse_laser"));
	const double HostileWeaponCooldownSeconds = HostileWeaponStats && HostileWeaponStats->CooldownSeconds > 0.0 ? HostileWeaponStats->CooldownSeconds : 0.35;
	const double HostileWeaponDamage = HostileWeaponStats && HostileWeaponStats->DamageAmount > 0.0 ? HostileWeaponStats->DamageAmount : 12.0;
	const double HostileProjectileSpeedCmPerSec = HostileWeaponStats && HostileWeaponStats->ProjectileSpeedCmPerSec > 0.0 ? HostileWeaponStats->ProjectileSpeedCmPerSec : 70000.0;
	const double HostileProjectileMaxDistanceCm = HostileWeaponStats && HostileWeaponStats->RangeCm > 0.0 ? HostileWeaponStats->RangeCm : 90000.0;
	const double HostileFireRangeCm = FMath::Min(56000.0, HostileProjectileMaxDistanceCm);
	const double HostileWeaponEnergyCost = HostileWeaponStats && HostileWeaponStats->EnergyCost > 0.0 ? HostileWeaponStats->EnergyCost : 14.0;
	const double HostileWeaponMaxEnergy = 100.0;
	const double HostileWeaponEnergyRegenPerSecond = HostileWeaponStats && HostileWeaponStats->EnergyRegenPerSecond > 0.0 ? HostileWeaponStats->EnergyRegenPerSecond : 28.0;
	const double HostileProjectileCollisionRadiusCm = HostileWeaponStats && HostileWeaponStats->ProjectileCollisionRadiusCm > 0.0 ? HostileWeaponStats->ProjectileCollisionRadiusCm : 850.0;
	const FName HostileDamageType = HostileWeaponStats && !HostileWeaponStats->DamageType.IsNone() ? HostileWeaponStats->DamageType : FName(TEXT("energy_weapon"));
	const FName HostilePresentationType = HostileWeaponStats && !HostileWeaponStats->PresentationType.IsNone() ? HostileWeaponStats->PresentationType : FName(TEXT("projectile_bolt"));

	RuntimeState.HostileWeaponCooldownSeconds = HostileWeaponCooldownSeconds;
	RuntimeState.HostileWeaponMaxEnergy = HostileWeaponMaxEnergy;
	RuntimeState.HostileWeaponEnergyCost = HostileWeaponEnergyCost;
	RuntimeState.HostileWeaponEnergyRegenPerSecond = HostileWeaponEnergyRegenPerSecond;
	RuntimeState.HostileWeaponEnergy = FMath::Clamp(RuntimeState.HostileWeaponEnergy, 0.0, RuntimeState.HostileWeaponMaxEnergy);
	if (RuntimeState.HostileWeaponEnergyUpdatedAtTimeSeconds < 0.0)
	{
		RuntimeState.HostileWeaponEnergyUpdatedAtTimeSeconds = SimulationTimeSeconds;
	}
	const double EnergyElapsedSeconds = FMath::Max(0.0, SimulationTimeSeconds - RuntimeState.HostileWeaponEnergyUpdatedAtTimeSeconds);
	RuntimeState.HostileWeaponEnergy = FMath::Min(
		RuntimeState.HostileWeaponMaxEnergy,
		RuntimeState.HostileWeaponEnergy + RuntimeState.HostileWeaponEnergyRegenPerSecond * EnergyElapsedSeconds);
	RuntimeState.HostileWeaponEnergyUpdatedAtTimeSeconds = SimulationTimeSeconds;

	if (SimulationTimeSeconds + UE_SMALL_NUMBER < RuntimeState.HostileWeaponReadyAtTimeSeconds ||
		RuntimeState.HostileWeaponEnergy + 0.001 < RuntimeState.HostileWeaponEnergyCost)
	{
		return false;
	}

	const FThreatRecord* RuntimeThreat = FindRuntimeThreat(SystemicGameplayState, RuntimeState.ThreatId);
	const FName HostileCombatantId = RuntimeThreat && !RuntimeThreat->AttackerId.IsNone()
		? RuntimeThreat->AttackerId
		: EncounterActor.ActorId;
	if (HostileCombatantId.IsNone())
	{
		return false;
	}

	const FVector ShotStartPositionCm = EncounterActor.Actor->GetActorLocation();
	const FVector HostileVelocityCmPerSec = EncounterActor.Actor->GetVelocity();
	const FVector PlayerPositionCm = PlayerPawn.GetLogicalSystemPositionCm();
	const FVector PlayerVelocityCmPerSec = PlayerPawn.GetLinearVelocityCmPerSec();
	const double TargetDistanceCm = FVector::Distance(ShotStartPositionCm, PlayerPositionCm);
	if (TargetDistanceCm > HostileFireRangeCm || TargetDistanceCm <= UE_SMALL_NUMBER)
	{
		return false;
	}

	FVector AimPointCm = PlayerPositionCm;
	TryGetRuntimeInterceptPoint(
		ShotStartPositionCm,
		HostileVelocityCmPerSec,
		PlayerPositionCm,
		PlayerVelocityCmPerSec,
		HostileProjectileSpeedCmPerSec,
		AimPointCm);

	const FVector ProjectileDirection = (AimPointCm - ShotStartPositionCm).GetSafeNormal(UE_SMALL_NUMBER, (PlayerPositionCm - ShotStartPositionCm).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	if (ProjectileDirection.IsNearlyZero())
	{
		return false;
	}

	const FName HostileThreatId(*FString::Printf(TEXT("threat_runtime_hostile_%s"), *RuntimeState.EncounterId.ToString()));
	FThreatRecord* HostileThreat = SystemicGameplayState.ThreatRecords.FindByPredicate([HostileThreatId](const FThreatRecord& Candidate)
	{
		return Candidate.ThreatId == HostileThreatId;
	});
	if (!HostileThreat)
	{
		FThreatRecord NewThreat;
		NewThreat.ThreatId = HostileThreatId;
		NewThreat.AttackerId = HostileCombatantId;
		NewThreat.DefenderId = TEXT("player_ship");
		NewThreat.LastKnownTarget.TargetId = TEXT("player_ship");
		NewThreat.LastKnownTarget.TargetType = TEXT("ship");
		NewThreat.LastKnownTarget.LocalOffsetCm = PlayerPositionCm;
		NewThreat.Severity = RuntimeThreat ? FMath::Max(0.45, RuntimeThreat->Severity) : 0.45;
		NewThreat.Confidence = RuntimeThreat ? RuntimeThreat->Confidence : 1.0;
		NewThreat.ExpiresAtTimeSeconds = RuntimeThreat ? RuntimeThreat->ExpiresAtTimeSeconds : SimulationTimeSeconds + 30.0;
		NewThreat.SourceEventId = FName(*FString::Printf(TEXT("event_runtime_hostile_fire_%s"), *RuntimeState.EncounterId.ToString()));
		NewThreat.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *NewThreat.ThreatId.ToString()));
		SystemicGameplayState.ThreatRecords.Add(NewThreat);
		HostileThreat = &SystemicGameplayState.ThreatRecords.Last();
	}

	RuntimeState.HostileWeaponEnergy = FMath::Max(0.0, RuntimeState.HostileWeaponEnergy - RuntimeState.HostileWeaponEnergyCost);
	RuntimeState.HostileWeaponEnergyUpdatedAtTimeSeconds = SimulationTimeSeconds;
	RuntimeState.LastHostileWeaponFireTimeSeconds = SimulationTimeSeconds;
	RuntimeState.HostileWeaponReadyAtTimeSeconds = SimulationTimeSeconds + RuntimeState.HostileWeaponCooldownSeconds;
	RuntimeState.HostileWeaponFireSequence += 1;
	RuntimeState.LastUpdatedTimeSeconds = SimulationTimeSeconds;

	const FName PendingDamageEventId(*FString::Printf(TEXT("damage_runtime_%s_hostile_%03d"), *RuntimeState.EncounterId.ToString(), RuntimeState.HostileWeaponFireSequence));
	const FVector ProjectileEffectiveVelocityCmPerSec = ProjectileDirection * HostileProjectileSpeedCmPerSec + HostileVelocityCmPerSec;
	const double ProjectileEffectiveSpeedCmPerSec = ProjectileEffectiveVelocityCmPerSec.Size();
	const FVector ProjectileTravelDirection = ProjectileEffectiveVelocityCmPerSec.GetSafeNormal(UE_SMALL_NUMBER, ProjectileDirection);
	const double ProjectileLifetimeSeconds = ProjectileEffectiveSpeedCmPerSec > UE_SMALL_NUMBER ? HostileProjectileMaxDistanceCm / ProjectileEffectiveSpeedCmPerSec : 0.0;
	const double ProjectileImpactDelaySeconds = ProjectileEffectiveSpeedCmPerSec > UE_SMALL_NUMBER ? TargetDistanceCm / ProjectileEffectiveSpeedCmPerSec : 0.0;

	FRuntimeEncounterProjectileState Projectile;
	Projectile.ProjectileId = FName(*FString::Printf(TEXT("projectile_%s"), *PendingDamageEventId.ToString()));
	Projectile.ProjectileState = TEXT("in_flight");
	Projectile.DamageEventId = PendingDamageEventId;
	Projectile.SourceCombatantId = HostileCombatantId;
	Projectile.TargetCombatantId = TEXT("player_ship");
	Projectile.ThreatId = HostileThreat->ThreatId;
	Projectile.DamageType = HostileDamageType;
	Projectile.DamageAmount = HostileWeaponDamage;
	Projectile.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *PendingDamageEventId.ToString()));
	Projectile.StartPositionCm = ShotStartPositionCm;
	Projectile.Direction = ProjectileDirection;
	Projectile.SpeedCmPerSec = HostileProjectileSpeedCmPerSec;
	Projectile.InheritedVelocityCmPerSec = HostileVelocityCmPerSec;
	Projectile.EffectiveVelocityCmPerSec = ProjectileEffectiveVelocityCmPerSec;
	Projectile.MaxDistanceCm = HostileProjectileMaxDistanceCm;
	Projectile.CollisionRadiusCm = HostileProjectileCollisionRadiusCm;
	Projectile.TargetDistanceCm = TargetDistanceCm;
	Projectile.StartedAtTimeSeconds = SimulationTimeSeconds;
	Projectile.ImpactAtTimeSeconds = SimulationTimeSeconds + ProjectileImpactDelaySeconds;
	RuntimeState.Projectiles.Add(Projectile);

	RuntimeState.ShotPresentationId = FName(*FString::Printf(TEXT("shot_%s"), *PendingDamageEventId.ToString()));
	RuntimeState.ShotPresentationType = HostilePresentationType;
	RuntimeState.ShotStartPositionCm = ShotStartPositionCm;
	RuntimeState.ShotEndPositionCm = ShotStartPositionCm + ProjectileTravelDirection * FMath::Min(HostileProjectileMaxDistanceCm, FMath::Max(TargetDistanceCm, 1.0));
	RuntimeState.ShotPresentationStartedAtTimeSeconds = SimulationTimeSeconds;
	RuntimeState.ShotPresentationDurationSeconds = FMath::Max(0.05, ProjectileLifetimeSeconds);
	RuntimeState.ShotPresentationActorId = NAME_None;
	if (UWorld* World = ResolveSessionWorld())
	{
		if (UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>())
		{
			StarSystem->SpawnCombatShotPresentation(
				RuntimeState.ShotPresentationId,
				RuntimeState.ShotPresentationType,
				RuntimeState.ShotStartPositionCm,
				RuntimeState.ShotEndPositionCm,
				RuntimeState.ShotPresentationStartedAtTimeSeconds,
				RuntimeState.ShotPresentationDurationSeconds,
				RuntimeState.ShotPresentationActorId);
		}
	}

	RuntimeState.DamageEventId = PendingDamageEventId;
	return true;
}

void UStargameSessionSubsystem::ClearSessionState()
{
	CleanupStationInterior();
	CurrentSystemId = NAME_None;
	CurrentSpawnZoneId = NAME_None;
	SelectedTargetId = NAME_None;
	ClockSnapshot = FSimulationClockSnapshot();
	ActiveTrafficState = FActiveTrafficSimulationState();
	SystemicGameplayState = FSystemicGameplayState();
	LastRuntimeEncounterView = FRuntimeEncounterViewModel();
	RuntimeEncounterStates.Reset();
	RuntimePresentationAccumulatorSeconds = RuntimePresentationUpdateIntervalSeconds;
	MissionProgressAccumulatorSeconds = MissionProgressUpdateIntervalSeconds;
	bHasPendingGateArrival = false;
	PendingGateArrivalLocation = FShipSaveLocation();
	LastGateTransitionResult = FGateTransitionResult();
	ActivePlayerPawnClass = nullptr;
}

void UStargameSessionSubsystem::CleanupStationInterior()
{
	if (StationInteriorSpacePawn)
	{
		StationInteriorSpacePawn->SetActorHiddenInGame(false);
		StationInteriorSpacePawn->SetActorEnableCollision(true);
	}

	if (ActiveStationInteriorPawn)
	{
		ActiveStationInteriorPawn->Destroy();
	}
	if (ActiveStationInteriorRoom)
	{
		ActiveStationInteriorRoom->Destroy();
	}

	ActiveStationInteriorPawn = nullptr;
	ActiveStationInteriorRoom = nullptr;
	StationInteriorSpacePawn = nullptr;
}

bool UStargameSessionSubsystem::SelectNavigationTargetById(FName TargetId)
{
	if (TargetId.IsNone())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	const UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	FNavigationTargetDefinition Target;
	if (!StarSystem || !StarSystem->FindNavigationTarget(TargetId, Target) || !Target.bCanTarget)
	{
		return false;
	}

	SelectedTargetId = TargetId;
	return true;
}

bool UStargameSessionSubsystem::CycleNavigationTarget()
{
	const UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		return false;
	}

	TArray<FNavigationTargetDefinition> Targets;
	StarSystem->GetNavigationTargets(Targets);
	Targets.RemoveAll([](const FNavigationTargetDefinition& Target)
	{
		return !Target.bCanTarget || !Target.bShowInHud;
	});

	if (Targets.IsEmpty())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	int32 NextIndex = 0;
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		if (Targets[Index].TargetId == SelectedTargetId)
		{
			NextIndex = (Index + 1) % Targets.Num();
			break;
		}
	}

	SelectedTargetId = Targets[NextIndex].TargetId;
	return true;
}

bool UStargameSessionSubsystem::ResolveActiveMissionAndPrimaryObjective(const FMissionInstanceState*& OutMission, const FObjectiveState*& OutObjective) const
{
	OutMission = nullptr;
	OutObjective = nullptr;

	for (const FMissionInstanceState& Mission : SystemicGameplayState.MissionInstances)
	{
		if (Mission.CurrentState != FName(TEXT("active")))
		{
			continue;
		}

		OutMission = &Mission;
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			const FObjectiveState* Objective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			});
			if (Objective && Objective->State == FName(TEXT("active")) && !Objective->TargetId.IsNone())
			{
				OutObjective = Objective;
				return true;
			}
		}

		return true;
	}

	return false;
}

bool UStargameSessionSubsystem::ResolveMutableActiveMissionAndPrimaryObjective(FMissionInstanceState*& OutMission, FObjectiveState*& OutObjective)
{
	OutMission = nullptr;
	OutObjective = nullptr;

	for (FMissionInstanceState& Mission : SystemicGameplayState.MissionInstances)
	{
		if (Mission.CurrentState != FName(TEXT("active")))
		{
			continue;
		}

		OutMission = &Mission;
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			FObjectiveState* Objective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			});
			if (Objective && Objective->State == FName(TEXT("active")) && !Objective->TargetId.IsNone())
			{
				OutObjective = Objective;
				return true;
			}
		}

		return true;
	}

	return false;
}

bool UStargameSessionSubsystem::IsMissionReadyForTurnIn(FName MissionInstanceId) const
{
	const FMissionInstanceState* Mission = SystemicGameplayState.MissionInstances.FindByPredicate([MissionInstanceId](const FMissionInstanceState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	if (!Mission || Mission->ObjectiveStateIds.IsEmpty())
	{
		return false;
	}

	for (const FName ObjectiveStateId : Mission->ObjectiveStateIds)
	{
		const FObjectiveState* Objective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
		{
			return Candidate.ObjectiveStateId == ObjectiveStateId;
		});
		if (!Objective || Objective->State != FName(TEXT("completed")))
		{
			return false;
		}
	}

	return true;
}

void UStargameSessionSubsystem::AutoSelectMissionWaypoint(const FMissionInstanceState& Mission)
{
	const UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		return;
	}

	for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
	{
		const FObjectiveState* Objective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
		{
			return Candidate.ObjectiveStateId == ObjectiveStateId;
		});
		if (!Objective || Objective->State != FName(TEXT("active")) || Objective->TargetId.IsNone())
		{
			continue;
		}

		FNavigationTargetDefinition Target;
		if (StarSystem->FindNavigationTarget(Objective->TargetId, Target) && Target.bCanTarget)
		{
			SelectNavigationTargetById(Objective->TargetId);
			return;
		}
	}
}

bool UStargameSessionSubsystem::GetActiveMissionWaypoint(FMissionWaypointViewModel& OutWaypoint) const
{
	OutWaypoint = FMissionWaypointViewModel();

	const FMissionInstanceState* Mission = nullptr;
	const FObjectiveState* Objective = nullptr;
	if (!ResolveActiveMissionAndPrimaryObjective(Mission, Objective) || !Mission)
	{
		OutWaypoint.DebugReason = TEXT("No active mission.");
		return false;
	}

	OutWaypoint.bHasActiveMission = true;
	OutWaypoint.MissionInstanceId = Mission->MissionInstanceId;
	OutWaypoint.bReadyToTurnIn = IsMissionReadyForTurnIn(Mission->MissionInstanceId);

	if (!Objective)
	{
		OutWaypoint.DebugReason = OutWaypoint.bReadyToTurnIn ? TEXT("Mission objectives complete.") : TEXT("No active mission waypoint.");
		return true;
	}

	OutWaypoint.ObjectiveStateId = Objective->ObjectiveStateId;
	OutWaypoint.ObjectiveType = Objective->ObjectiveType;
	OutWaypoint.TargetType = Objective->TargetType;
	OutWaypoint.TargetId = Objective->TargetId;
	OutWaypoint.ObjectiveState = Objective->State;
	OutWaypoint.bTargetSelected = SelectedTargetId == Objective->TargetId;

	UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (!StarSystem || !FlightPawn)
	{
		OutWaypoint.DebugReason = TEXT("Mission waypoint target cannot resolve without active space flight.");
		return true;
	}

	FNavigationTargetViewModel TargetViewModel;
	OutWaypoint.bTargetResolved = StarSystem->BuildNavigationTargetViewModel(
		Objective->TargetId,
		SelectedTargetId,
		FlightPawn->GetLogicalSystemPositionCm(),
		FlightPawn->GetLinearVelocityCmPerSec(),
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		TargetViewModel);
	if (OutWaypoint.bTargetResolved)
	{
		OutWaypoint.DistanceCm = TargetViewModel.DistanceCm;
	}
	else
	{
		OutWaypoint.DebugReason = TEXT("Mission waypoint navigation target is not registered.");
	}

	return true;
}

bool UStargameSessionSubsystem::TryProgressActiveMissionWaypoint()
{
	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	bool bProgressed = false;

	const int32 MaxObjectivePasses = FMath::Max(1, SystemicGameplayState.ObjectiveStates.Num());
	for (int32 PassIndex = 0; PassIndex < MaxObjectivePasses; ++PassIndex)
	{
		FMissionInstanceState* Mission = nullptr;
		FObjectiveState* Objective = nullptr;
		if (!ResolveMutableActiveMissionAndPrimaryObjective(Mission, Objective) || !Mission || !Objective)
		{
			break;
		}

		const FName ObjectiveStateId = Objective->ObjectiveStateId;
		const FName ObjectiveType = Objective->ObjectiveType;
		const FName TargetType = Objective->TargetType;
		const FName TargetId = Objective->TargetId;

		if (TargetType == FName(TEXT("encounter")))
		{
			FString FailureReason;
			FLogicalEncounterResolutionResult EncounterResult;
			if (!USystemicGameplayQueryService::ResolveLogicalEncounterOnce(SystemicGameplayState, TargetId, ClockSnapshot.AuthoritativeSimulationTimeSeconds, EncounterResult, FailureReason))
			{
				break;
			}

			FProgressionDebugLedgerEntry ProgressionEntry;
			const FName EncounterProgressionIdempotencyKey(*FString::Printf(TEXT("idem_session_%s"), *ObjectiveStateId.ToString()));
			if (!USystemicGameplayQueryService::ApplyEncounterProgressionOutcomeOnce(SystemicGameplayState, TargetId, EncounterProgressionIdempotencyKey, ProgressionEntry, FailureReason))
			{
				break;
			}

			if (FObjectiveState* ResolvedObjective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			}))
			{
				ResolvedObjective->State = TEXT("completed");
				bProgressed = true;
				continue;
			}

			break;
		}

		const bool bRequiresHostileBoardingClear =
			ObjectiveType == FName(TEXT("clear_station_hostiles")) ||
			ObjectiveType == FName(TEXT("clear_hostiles")) ||
			ObjectiveType == FName(TEXT("hostile_boarding")) ||
			ObjectiveType == FName(TEXT("boarding_clearance"));
		if (bRequiresHostileBoardingClear)
		{
			break;
		}

		if (!StarSystem || !FlightPawn)
		{
			break;
		}

		FNavigationTargetViewModel TargetViewModel;
		if (!StarSystem->BuildNavigationTargetViewModel(
			TargetId,
			SelectedTargetId,
			FlightPawn->GetLogicalSystemPositionCm(),
			FlightPawn->GetLinearVelocityCmPerSec(),
			ClockSnapshot.AuthoritativeSimulationTimeSeconds,
			TargetViewModel))
		{
			break;
		}

		const bool bReachedWaypoint = TargetType == FName(TEXT("station"))
			? TargetViewModel.bInsideStationApproachBubble
			: TargetViewModel.DistanceCm <= StarSystem->GetActiveScaleContract().StationApproachBubbleRadiusCm;
		if (!bReachedWaypoint)
		{
			break;
		}

		if (FObjectiveState* ReachedObjective = SystemicGameplayState.ObjectiveStates.FindByPredicate([ObjectiveStateId](const FObjectiveState& Candidate)
		{
			return Candidate.ObjectiveStateId == ObjectiveStateId;
		}))
		{
			ReachedObjective->State = TEXT("completed");
			bProgressed = true;
			continue;
		}

		break;
	}

	return bProgressed;
}

bool UStargameSessionSubsystem::TryCompleteActiveHostileBoardingObjective(FName StationId)
{
	bool bCompletedAny = false;
	for (FObjectiveState& Objective : SystemicGameplayState.ObjectiveStates)
	{
		const bool bStationMatches = Objective.TargetId.IsNone() || Objective.TargetId == StationId;
		const bool bClearHostilesObjective =
			Objective.ObjectiveType == FName(TEXT("clear_station_hostiles")) ||
			Objective.ObjectiveType == FName(TEXT("clear_hostiles")) ||
			Objective.ObjectiveType == FName(TEXT("hostile_boarding")) ||
			Objective.ObjectiveType == FName(TEXT("boarding_clearance"));
		if (Objective.State == FName(TEXT("active")) && bStationMatches && bClearHostilesObjective)
		{
			Objective.State = TEXT("completed");
			bCompletedAny = true;
		}
	}
	if (bCompletedAny)
	{
		SaveDockedAutosave(TEXT("hostile boarding objective"));
	}
	return bCompletedAny;
}

bool UStargameSessionSubsystem::TryUpdateRuntimeEncounterBehavior()
{
	LastRuntimeEncounterView = FRuntimeEncounterViewModel();

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (!StarSystem || !FlightPawn)
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Runtime encounter behavior requires active space flight.");
		return false;
	}

	TArray<FRealizedEncounterActorEntry> EncounterActors;
	StarSystem->GetRealizedEncounterActors(EncounterActors);
	if (EncounterActors.IsEmpty())
	{
		LastRuntimeEncounterView.DebugReason = TEXT("No realized encounter actors are active.");
		return false;
	}

	const FVector PlayerPositionCm = FlightPawn->GetLogicalSystemPositionCm();
	constexpr double PirateActivationRangeCm = 320000.0;
	constexpr double PatrolActivationRangeCm = 320000.0;
	const FRealizedEncounterActorEntry* ClosestEntry = nullptr;
	double ClosestDistanceCm = TNumericLimits<double>::Max();
	for (const FRealizedEncounterActorEntry& Entry : EncounterActors)
	{
		if (Entry.EncounterId.IsNone() || !Entry.Actor.IsValid())
		{
			continue;
		}

		const double DistanceCm = FVector::Distance(PlayerPositionCm, Entry.Actor->GetActorLocation());
		if (DistanceCm < ClosestDistanceCm)
		{
			ClosestDistanceCm = DistanceCm;
			ClosestEntry = &Entry;
		}
	}

	if (!ClosestEntry)
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Realized encounter actors exist but none have a live actor and encounter ID.");
		return false;
	}

	LastRuntimeEncounterView.bHasEncounterActor = true;
	LastRuntimeEncounterView.EncounterId = ClosestEntry->EncounterId;
	LastRuntimeEncounterView.ActorId = ClosestEntry->ActorId;
	LastRuntimeEncounterView.Role = EncounterRoleName(ClosestEntry->Role);
	LastRuntimeEncounterView.IntentType = ClosestEntry->IntentType;
	LastRuntimeEncounterView.ThreatId = ClosestEntry->ThreatId;
	LastRuntimeEncounterView.TargetShipId = ClosestEntry->TargetShipId;
	LastRuntimeEncounterView.BehaviorVariantId = ClosestEntry->BehaviorVariantId;
	LastRuntimeEncounterView.CommsVariantId = ClosestEntry->CommsVariantId;
	LastRuntimeEncounterView.LocalBehaviorStateId = ClosestEntry->LocalBehaviorStateId;
	LastRuntimeEncounterView.HostileAIStateId = ClosestEntry->Role == EShipGoalKind::Pirate
		? (ClosestDistanceCm <= PirateActivationRangeCm ? FName(TEXT("interdict")) : FName(TEXT("intercept")))
		: (ClosestEntry->Role == EShipGoalKind::Patrol ? FName(TEXT("patrol")) : NAME_None);
	LastRuntimeEncounterView.CommsLine = ClosestEntry->CommsLine;
	LastRuntimeEncounterView.HazardId = ClosestEntry->HazardId;
	LastRuntimeEncounterView.PatrolReservationId = ClosestEntry->PatrolReservationId;
	LastRuntimeEncounterView.DistanceCm = ClosestDistanceCm;
	LastRuntimeEncounterView.DistanceToDesiredCm = ClosestEntry->DistanceToDesiredCm;
	LastRuntimeEncounterView.DistanceTrendCm = ClosestEntry->DistanceTrendCm;
	LastRuntimeEncounterView.bSteeringActive = ClosestEntry->bSteeringActive;
	if (!ClosestEntry->HazardId.IsNone())
	{
		if (const FInterdictionHazardRecord* Hazard = SystemicGameplayState.InterdictionHazards.FindByPredicate([ClosestEntry](const FInterdictionHazardRecord& Candidate)
		{
			return Candidate.HazardId == ClosestEntry->HazardId;
		}))
		{
			LastRuntimeEncounterView.DynamicSelectionScore = Hazard->DynamicPirateAmbushScore;
			LastRuntimeEncounterView.SelectionDebugReason = Hazard->SelectionDebugReason;
		}
	}
	if (!ClosestEntry->PatrolReservationId.IsNone())
	{
		if (const FPatrolReservationRecord* Reservation = SystemicGameplayState.PatrolReservations.FindByPredicate([ClosestEntry](const FPatrolReservationRecord& Candidate)
		{
			return Candidate.ReservationId == ClosestEntry->PatrolReservationId;
		}))
		{
			LastRuntimeEncounterView.DynamicSelectionScore = Reservation->DynamicPatrolScore;
			LastRuntimeEncounterView.SelectionDebugReason = Reservation->SelectionDebugReason;
		}
	}
	if (FRuntimeEncounterState* MutableRuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, ClosestEntry->EncounterId))
	{
		if (const FEquipmentSlotState* WeaponSlot = SystemicGameplayState.EquipmentSlots.FindByPredicate([](const FEquipmentSlotState& Candidate)
		{
			return Candidate.SlotId == FName(TEXT("ship_weapon_hardpoint_1")) &&
				Candidate.OwnerId == FName(TEXT("player_ship"));
		}))
		{
			if (const FItemDefinition* WeaponItem = FindRuntimeItem(SystemicGameplayState, WeaponSlot->EquippedStack.ItemId))
			{
				if (WeaponItem->ItemType == FName(TEXT("ship_weapon")))
				{
					const FShipWeaponStatDefinition* WeaponStats = FindRuntimeShipWeaponStats(SystemicGameplayState, WeaponItem->ItemId);
					const double DamageAmount = ResolveRuntimeShipWeaponDamage(*WeaponItem, WeaponStats);
					const double CooldownSeconds = ResolveRuntimeShipWeaponCooldownSeconds(*WeaponItem, WeaponStats);
					const double EnergyCost = ResolveRuntimeShipWeaponEnergyCost(WeaponStats);
					const double EnergyRegenPerSecond = ResolveRuntimeShipWeaponEnergyRegenPerSecond(WeaponStats);
					const double MinAlignment = ResolveRuntimeShipWeaponMinAlignment(WeaponStats);
					const double RangeCm = WeaponStats ? WeaponStats->RangeCm : 300000.0;
					const double ProjectileSpeedCmPerSec = ResolveRuntimeShipWeaponProjectileSpeedCmPerSec(WeaponStats);
					const FName WeaponPresentationType = WeaponStats && !WeaponStats->PresentationType.IsNone()
						? WeaponStats->PresentationType
						: FName(TEXT("projectile_bolt"));

					RegenerateRuntimePlayerWeaponEnergy(
						*MutableRuntimeState,
						ClockSnapshot.AuthoritativeSimulationTimeSeconds,
						MutableRuntimeState->PlayerWeaponMaxEnergy,
						EnergyRegenPerSecond);

					MutableRuntimeState->LastPlayerWeaponItemId = WeaponItem->ItemId;
					MutableRuntimeState->LastPlayerWeaponStatId = WeaponStats ? WeaponStats->WeaponStatId : NAME_None;
					MutableRuntimeState->PlayerWeaponCooldownSeconds = CooldownSeconds;
					MutableRuntimeState->PlayerWeaponEnergyCost = EnergyCost;
					MutableRuntimeState->PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
					MutableRuntimeState->PlayerWeaponMinAlignment = MinAlignment;

					const FVector TargetPositionCm = ClosestEntry->Actor.IsValid()
						? ClosestEntry->Actor->GetActorLocation()
						: PlayerPositionCm;
					FVector LeadPointCm = TargetPositionCm;
					const FVector TargetVelocityCmPerSec = ClosestEntry->Actor.IsValid()
						? ClosestEntry->Actor->GetVelocity()
						: FVector::ZeroVector;
					const bool bHasLeadPoint = TryGetRuntimeInterceptPoint(
						PlayerPositionCm,
						FlightPawn->GetLinearVelocityCmPerSec(),
						TargetPositionCm,
						TargetVelocityCmPerSec,
						ProjectileSpeedCmPerSec,
						LeadPointCm);
					const FVector AimDirection = (bHasLeadPoint ? LeadPointCm : TargetPositionCm) - PlayerPositionCm;
					const double AlignmentDot = bHasLeadPoint
						? FVector::DotProduct(FlightPawn->GetActorForwardVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector), AimDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector))
						: 0.0;
					MutableRuntimeState->PlayerWeaponAlignmentDot = AlignmentDot;

					LastRuntimeEncounterView.PlayerWeaponItemId = WeaponItem->ItemId;
					LastRuntimeEncounterView.PlayerWeaponStatId = WeaponStats ? WeaponStats->WeaponStatId : NAME_None;
					LastRuntimeEncounterView.PlayerWeaponDamageAmount = DamageAmount;
					LastRuntimeEncounterView.PlayerWeaponRangeCm = RangeCm;
					LastRuntimeEncounterView.PlayerWeaponProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
					LastRuntimeEncounterView.PlayerWeaponPresentationType = WeaponPresentationType;
					LastRuntimeEncounterView.PlayerWeaponTargetCombatantId = ClosestEntry->TargetShipId;
					LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = CooldownSeconds;
					LastRuntimeEncounterView.PlayerWeaponEnergy = MutableRuntimeState->PlayerWeaponEnergy;
					LastRuntimeEncounterView.PlayerWeaponMaxEnergy = MutableRuntimeState->PlayerWeaponMaxEnergy;
					LastRuntimeEncounterView.PlayerWeaponEnergyCost = EnergyCost;
					LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
					LastRuntimeEncounterView.PlayerWeaponMinAlignment = MinAlignment;
					LastRuntimeEncounterView.PlayerWeaponAlignmentDot = AlignmentDot;
					LastRuntimeEncounterView.bPlayerWeaponHasLeadPoint = bHasLeadPoint;
					LastRuntimeEncounterView.PlayerWeaponLeadPointCm = LeadPointCm;
					LastRuntimeEncounterView.bPlayerWeaponInRange = ClosestDistanceCm <= RangeCm * 0.9;
					LastRuntimeEncounterView.bPlayerWeaponEnergyReady = MutableRuntimeState->PlayerWeaponEnergy + 0.001 >= EnergyCost;
					LastRuntimeEncounterView.bPlayerWeaponAligned = bHasLeadPoint && AlignmentDot + UE_SMALL_NUMBER >= MinAlignment;
					LastRuntimeEncounterView.bPlayerFireSolutionReady =
						LastRuntimeEncounterView.bPlayerWeaponInRange &&
						LastRuntimeEncounterView.bPlayerWeaponEnergyReady &&
						LastRuntimeEncounterView.bPlayerWeaponAligned;
					LastRuntimeEncounterView.PlayerFireSolutionText = BuildRuntimeFireSolutionText(
						LastRuntimeEncounterView.bPlayerWeaponInRange,
						LastRuntimeEncounterView.bPlayerWeaponEnergyReady,
						LastRuntimeEncounterView.bPlayerWeaponHasLeadPoint,
						LastRuntimeEncounterView.bPlayerWeaponAligned);
				}
			}
		}
		RegenerateRuntimePlayerWeaponEnergy(
			*MutableRuntimeState,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds,
			MutableRuntimeState->PlayerWeaponMaxEnergy,
			MutableRuntimeState->PlayerWeaponEnergyRegenPerSecond > 0.0 ? MutableRuntimeState->PlayerWeaponEnergyRegenPerSecond : 30.0);
		UpdateRuntimeEncounterProjectiles(*MutableRuntimeState, ClosestEntry, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		TryQueueHostileProjectileAtPlayer(*MutableRuntimeState, *ClosestEntry, *FlightPawn, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		}
		if (const FRuntimeEncounterState* RuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, ClosestEntry->EncounterId))
		{
			LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = RuntimeState->PlayerWeaponCooldownSeconds;
			LastRuntimeEncounterView.PlayerWeaponReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
			LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds = FMath::Max(0.0, RuntimeState->PlayerWeaponReadyAtTimeSeconds - ClockSnapshot.AuthoritativeSimulationTimeSeconds);
			LastRuntimeEncounterView.PlayerWeaponEnergy = RuntimeState->PlayerWeaponEnergy;
			LastRuntimeEncounterView.PlayerWeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
			LastRuntimeEncounterView.PlayerWeaponEnergyCost = RuntimeState->PlayerWeaponEnergyCost;
			LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = RuntimeState->PlayerWeaponEnergyRegenPerSecond;
			LastRuntimeEncounterView.PlayerWeaponMinAlignment = RuntimeState->PlayerWeaponMinAlignment;
			LastRuntimeEncounterView.PlayerWeaponAlignmentDot = RuntimeState->PlayerWeaponAlignmentDot;
			LastRuntimeEncounterView.bPlayerWeaponAligned = RuntimeState->PlayerWeaponAlignmentDot + UE_SMALL_NUMBER >= RuntimeState->PlayerWeaponMinAlignment;
			LastRuntimeEncounterView.bPlayerWeaponEnergyReady = RuntimeState->PlayerWeaponEnergyCost <= UE_SMALL_NUMBER ||
				RuntimeState->PlayerWeaponEnergy + 0.001 >= RuntimeState->PlayerWeaponEnergyCost;
			LastRuntimeEncounterView.bPlayerWeaponReady = LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds <= UE_SMALL_NUMBER &&
				LastRuntimeEncounterView.bPlayerWeaponEnergyReady &&
				LastRuntimeEncounterView.bPlayerWeaponAligned;
			LastRuntimeEncounterView.HostileResponseStateId = RuntimeState->HostileResponseStateId;
			LastRuntimeEncounterView.HostileManeuverStateId = RuntimeState->HostileManeuverStateId;
			LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId.IsNone()
				? LastRuntimeEncounterView.HostileAIStateId
				: RuntimeState->HostileAIStateId;
			LastRuntimeEncounterView.HostileWeaponCooldownSeconds = RuntimeState->HostileWeaponCooldownSeconds;
			LastRuntimeEncounterView.HostileWeaponCooldownRemainingSeconds = FMath::Max(0.0, RuntimeState->HostileWeaponReadyAtTimeSeconds - ClockSnapshot.AuthoritativeSimulationTimeSeconds);
			LastRuntimeEncounterView.HostileWeaponEnergy = RuntimeState->HostileWeaponEnergy;
			LastRuntimeEncounterView.HostileWeaponMaxEnergy = RuntimeState->HostileWeaponMaxEnergy;
			LastRuntimeEncounterView.HostileWeaponEnergyCost = RuntimeState->HostileWeaponEnergyCost;
			LastRuntimeEncounterView.HostileWeaponEnergyRegenPerSecond = RuntimeState->HostileWeaponEnergyRegenPerSecond;
			LastRuntimeEncounterView.bHostileWeaponReady = LastRuntimeEncounterView.HostileWeaponCooldownRemainingSeconds <= UE_SMALL_NUMBER &&
				(RuntimeState->HostileWeaponEnergyCost <= UE_SMALL_NUMBER || RuntimeState->HostileWeaponEnergy + 0.001 >= RuntimeState->HostileWeaponEnergyCost);
			LastRuntimeEncounterView.DamageEventId = RuntimeState->DamageEventId;
			LastRuntimeEncounterView.PlayerWeaponItemId = RuntimeState->LastPlayerWeaponItemId;
			LastRuntimeEncounterView.PlayerWeaponStatId = RuntimeState->LastPlayerWeaponStatId;
			LastRuntimeEncounterView.ShotPresentationId = RuntimeState->ShotPresentationId;
			LastRuntimeEncounterView.ShotPresentationType = RuntimeState->ShotPresentationType;
			LastRuntimeEncounterView.ShotPresentationActorId = RuntimeState->ShotPresentationActorId;
			LastRuntimeEncounterView.ShotStartPositionCm = RuntimeState->ShotStartPositionCm;
			LastRuntimeEncounterView.ShotEndPositionCm = RuntimeState->ShotEndPositionCm;
			LastRuntimeEncounterView.ShotPresentationStartedAtTimeSeconds = RuntimeState->ShotPresentationStartedAtTimeSeconds;
			LastRuntimeEncounterView.ShotPresentationDurationSeconds = RuntimeState->ShotPresentationDurationSeconds;
			LastRuntimeEncounterView.bShotPresentationActive = !RuntimeState->ShotPresentationId.IsNone() &&
				ClockSnapshot.AuthoritativeSimulationTimeSeconds <= RuntimeState->ShotPresentationStartedAtTimeSeconds + RuntimeState->ShotPresentationDurationSeconds;
			LastRuntimeEncounterView.bRenderedShotActive = StarSystem->IsCombatShotPresentationActive(RuntimeState->ShotPresentationId);
			if (const FRuntimeEncounterProjectileState* Projectile = FindLastRuntimeProjectile(*RuntimeState))
			{
				LastRuntimeEncounterView.ProjectileState = Projectile->ProjectileState;
				LastRuntimeEncounterView.ProjectileSourceCombatantId = Projectile->SourceCombatantId;
				LastRuntimeEncounterView.ProjectileTargetCombatantId = Projectile->TargetCombatantId;
				LastRuntimeEncounterView.bProjectileInFlight = Projectile->ProjectileState == FName(TEXT("in_flight"));
				LastRuntimeEncounterView.ProjectileSpeedCmPerSec = Projectile->SpeedCmPerSec;
				LastRuntimeEncounterView.ProjectileInheritedVelocityCmPerSec = Projectile->InheritedVelocityCmPerSec;
				LastRuntimeEncounterView.ProjectileEffectiveVelocityCmPerSec = Projectile->EffectiveVelocityCmPerSec;
				LastRuntimeEncounterView.ProjectileTravelledCm = Projectile->TravelledCm;
				LastRuntimeEncounterView.ProjectileTargetDistanceCm = Projectile->TargetDistanceCm;
				LastRuntimeEncounterView.ProjectileImpactAtTimeSeconds = Projectile->ImpactAtTimeSeconds;
				if (Projectile->ProjectileState == FName(TEXT("hit")))
				{
					LastRuntimeEncounterView.DamageResultState = TEXT("applied");
					if (const FShipDurabilityState* Durability = FindRuntimeDurability(SystemicGameplayState, Projectile->TargetCombatantId))
					{
						LastRuntimeEncounterView.TargetDurabilityState = Durability->State;
					}
				}
				else if (Projectile->ProjectileState == FName(TEXT("expired")))
				{
					LastRuntimeEncounterView.DamageResultState = TEXT("expired");
				}
				else if (Projectile->ProjectileState == FName(TEXT("in_flight")))
				{
					LastRuntimeEncounterView.DamageResultState = TEXT("pending_projectile");
				}
			}
		LastRuntimeEncounterView.DebugReason = FString::Printf(TEXT("Runtime encounter state: %s."), *RuntimeState->RuntimeState.ToString());
	}

	const bool bPirateAmbush = ClosestEntry->Role == EShipGoalKind::Pirate && ClosestEntry->IntentType == FName(TEXT("attack")) && ClosestDistanceCm <= PirateActivationRangeCm;
	const bool bPatrolResponse = ClosestEntry->Role == EShipGoalKind::Patrol && ClosestDistanceCm <= PatrolActivationRangeCm;
	LastRuntimeEncounterView.bPirateAmbushActive = bPirateAmbush;
	LastRuntimeEncounterView.bPatrolResponseActive = bPatrolResponse;

	if (!bPirateAmbush && !bPatrolResponse)
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Closest encounter actor is outside its activation bubble.");
		return true;
	}

	const FLogicalEncounterRecord* Encounter = FindRuntimeEncounter(SystemicGameplayState, ClosestEntry->EncounterId);
	if (!Encounter)
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Activated encounter actor has no systemic encounter record.");
		return false;
	}
	if (Encounter->State == FName(TEXT("resolved")))
	{
		LastRuntimeEncounterView.bResolvedEncounter = true;
		LastRuntimeEncounterView.EngagementId = Encounter->EngagementId;
		if (const FShipDurabilityState* Durability = FindRuntimeDurability(SystemicGameplayState, LastRuntimeEncounterView.TargetShipId))
		{
			LastRuntimeEncounterView.TargetDurabilityState = Durability->State;
			LastRuntimeEncounterView.DamageEventId = Durability->LastDamageEventId;
		}
		LastRuntimeEncounterView.DebugReason = TEXT("Runtime encounter was already resolved.");
		return true;
	}

	FString FailureReason;
	FRuntimeEncounterState* RuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, ClosestEntry->EncounterId);
	if (!RuntimeState)
	{
		FRuntimeEncounterState NewState;
		NewState.EncounterId = ClosestEntry->EncounterId;
		NewState.RuntimeState = TEXT("detected");
		NewState.HostileAIStateId = bPirateAmbush ? FName(TEXT("interdict")) : FName(TEXT("patrol"));
		RuntimeEncounterStates.Add(NewState);
		RuntimeState = &RuntimeEncounterStates.Last();
	}
	if (RuntimeState->HostileAIStateId.IsNone())
	{
		RuntimeState->HostileAIStateId = bPirateAmbush ? FName(TEXT("interdict")) : FName(TEXT("patrol"));
	}
	LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId;

	if (bPirateAmbush && !ClosestEntry->ThreatId.IsNone())
	{
		if (const FThreatRecord* Threat = FindRuntimeThreat(SystemicGameplayState, ClosestEntry->ThreatId))
		{
			FDamageEventRecord RuntimeDamage;
			RuntimeDamage.DamageEventId = FName(*FString::Printf(TEXT("damage_runtime_%s_warning_shot"), *ClosestEntry->EncounterId.ToString()));
			RuntimeDamage.SourceCombatantId = Threat->AttackerId;
			RuntimeDamage.TargetCombatantId = Threat->DefenderId;
			RuntimeDamage.DamageType = TEXT("kinetic_warning");
			RuntimeDamage.Amount = 12.0;
			RuntimeDamage.AuthorityTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
			RuntimeDamage.ThreatId = Threat->ThreatId;
			RuntimeDamage.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *RuntimeDamage.DamageEventId.ToString()));

			FDamageEventRecord DamageResult;
			if (USystemicGameplayQueryService::ApplyDamageEventOnce(SystemicGameplayState, RuntimeDamage, DamageResult, FailureReason))
			{
				RuntimeState->RuntimeState = TEXT("engaged");
				RuntimeState->HostileAIStateId = TEXT("engage");
				RuntimeState->ThreatId = ClosestEntry->ThreatId;
				RuntimeState->DamageEventId = DamageResult.DamageEventId;
				RuntimeState->EngagedAtTimeSeconds = RuntimeState->EngagedAtTimeSeconds <= 0.0 ? ClockSnapshot.AuthoritativeSimulationTimeSeconds : RuntimeState->EngagedAtTimeSeconds;
				RuntimeState->LastUpdatedTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
				LastRuntimeEncounterView.DamageEventId = DamageResult.DamageEventId;
				LastRuntimeEncounterView.DamageResultState = DamageResult.ResultState;
				LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId;
				if (const FShipDurabilityState* Durability = FindRuntimeDurability(SystemicGameplayState, RuntimeDamage.TargetCombatantId))
				{
					LastRuntimeEncounterView.TargetDurabilityState = Durability->State;
				}
			}
			else
			{
				LastRuntimeEncounterView.DebugReason = FailureReason;
				return false;
			}
		}
	}

	LastRuntimeEncounterView.DebugReason = RuntimeState && RuntimeState->RuntimeState == FName(TEXT("engaged"))
		? TEXT("Runtime encounter engaged; awaiting explicit combat, escape, or patrol outcome.")
		: LastRuntimeEncounterView.DebugReason;
	return true;
}

bool UStargameSessionSubsystem::ResolveRuntimeEncounter(FName EncounterId)
{
	if (EncounterId.IsNone())
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Runtime encounter resolution requires an encounter ID.");
		return false;
	}

	FRuntimeEncounterState* RuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, EncounterId);
	if (RuntimeState)
	{
		RuntimeState->RuntimeState = TEXT("resolving");
		if (RuntimeState->HostileAIStateId != FName(TEXT("disengage")))
		{
			RuntimeState->HostileAIStateId = TEXT("return_to_anchor");
		}
		RuntimeState->LastUpdatedTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	}

	FString FailureReason;
	FLogicalEncounterResolutionResult EncounterResult;
	if (!USystemicGameplayQueryService::ResolveLogicalEncounterOnceWithOutcome(
		SystemicGameplayState,
		EncounterId,
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		LastRuntimeEncounterView.OutcomeType,
		EncounterResult,
		FailureReason))
	{
		LastRuntimeEncounterView.DebugReason = FailureReason;
		return false;
	}

	FProgressionDebugLedgerEntry ProgressionEntry;
	const FName ProgressionIdempotencyKey(*FString::Printf(TEXT("idem_runtime_encounter_%s"), *EncounterId.ToString()));
	USystemicGameplayQueryService::ApplyEncounterProgressionOutcomeOnce(SystemicGameplayState, EncounterId, ProgressionIdempotencyKey, ProgressionEntry, FailureReason);

	LastRuntimeEncounterView.bResolvedEncounter = true;
	LastRuntimeEncounterView.EncounterId = EncounterId;
	LastRuntimeEncounterView.DebugReason = EncounterResult.DebugReason;
	if (const FLogicalEncounterRecord* ResolvedEncounter = FindRuntimeEncounter(SystemicGameplayState, EncounterId))
	{
		LastRuntimeEncounterView.EngagementId = ResolvedEncounter->EngagementId;
	}
	if (RuntimeState)
	{
		RuntimeState->RuntimeState = TEXT("resolved");
		RuntimeState->LastUpdatedTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
		LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId;
	}

	UWorld* World = ResolveSessionWorld();
	if (UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
	{
		StarSystem->RealizeSystemicEncounterActors(SystemicGameplayState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	}
	return true;
}

bool UStargameSessionSubsystem::ApplyRuntimeEncounterOutcome(FName EncounterId, FName OutcomeType)
{
	if (EncounterId.IsNone() || OutcomeType.IsNone())
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Runtime encounter outcome requires encounter and outcome IDs.");
		return false;
	}

	FRuntimeEncounterState* RuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, EncounterId);
	if (!RuntimeState || RuntimeState->RuntimeState != FName(TEXT("engaged")))
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Runtime encounter outcome requires an engaged encounter.");
		return false;
	}

	const FThreatRecord* Threat = FindRuntimeThreat(SystemicGameplayState, RuntimeState->ThreatId);
	if ((OutcomeType == FName(TEXT("escape")) || OutcomeType == FName(TEXT("surrender"))) && !Threat)
	{
		LastRuntimeEncounterView.DebugReason = TEXT("Escape and surrender outcomes require a runtime threat.");
		return false;
	}

	if (Threat && (OutcomeType == FName(TEXT("escape")) || OutcomeType == FName(TEXT("surrender"))))
	{
		RuntimeState->HostileAIStateId = TEXT("disengage");
		FDamageEventRecord OutcomeDamage;
		OutcomeDamage.DamageEventId = FName(*FString::Printf(TEXT("damage_runtime_%s_%s"), *EncounterId.ToString(), *OutcomeType.ToString()));
		OutcomeDamage.SourceCombatantId = Threat->AttackerId;
		OutcomeDamage.TargetCombatantId = Threat->DefenderId;
		OutcomeDamage.DamageType = OutcomeType;
		OutcomeDamage.Amount = 1.0;
		OutcomeDamage.AuthorityTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
		OutcomeDamage.ThreatId = Threat->ThreatId;
		OutcomeDamage.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *OutcomeDamage.DamageEventId.ToString()));

		FString FailureReason;
		FDamageEventRecord DamageResult;
		if (!USystemicGameplayQueryService::ApplyDamageEventOnce(SystemicGameplayState, OutcomeDamage, DamageResult, FailureReason))
		{
			LastRuntimeEncounterView.DebugReason = FailureReason;
			return false;
		}

		RuntimeState->DamageEventId = DamageResult.DamageEventId;
		LastRuntimeEncounterView.DamageEventId = DamageResult.DamageEventId;
		LastRuntimeEncounterView.DamageResultState = DamageResult.ResultState;
		if (const FShipDurabilityState* Durability = FindRuntimeDurability(SystemicGameplayState, OutcomeDamage.TargetCombatantId))
		{
			LastRuntimeEncounterView.TargetDurabilityState = Durability->State;
		}
	}
	else if (OutcomeType != FName(TEXT("patrol_response")))
	{
		LastRuntimeEncounterView.DebugReason = FString::Printf(TEXT("Unsupported runtime encounter outcome '%s'."), *OutcomeType.ToString());
		return false;
	}

	else
	{
		RuntimeState->HostileAIStateId = TEXT("return_to_anchor");
	}

	RuntimeState->RuntimeState = TEXT("resolving");
	RuntimeState->LastUpdatedTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	LastRuntimeEncounterView.OutcomeType = OutcomeType;
	LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId;

	const bool bResolved = ResolveRuntimeEncounter(EncounterId);
	LastRuntimeEncounterView.OutcomeType = OutcomeType;
	return bResolved;
}

bool UStargameSessionSubsystem::FireEquippedShipWeaponAtRuntimeEncounter(FName EncounterId, FShipWeaponFireResult& OutResult)
{
	OutResult = FShipWeaponFireResult();
	OutResult.EncounterId = EncounterId;
	if (EncounterId.IsNone())
	{
		EncounterId = LastRuntimeEncounterView.EncounterId;
		OutResult.EncounterId = EncounterId;
	}
	if (EncounterId.IsNone())
	{
		OutResult.FailureReason = TEXT("Ship weapon fire requires an active encounter.");
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}

	FRuntimeEncounterState* RuntimeState = FindRuntimeEncounterState(RuntimeEncounterStates, EncounterId);
	if (!RuntimeState || RuntimeState->RuntimeState != FName(TEXT("engaged")))
	{
		OutResult.FailureReason = TEXT("Ship weapon fire requires an engaged encounter.");
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}

	const FThreatRecord* RuntimeThreat = FindRuntimeThreat(SystemicGameplayState, RuntimeState->ThreatId);
	if (!RuntimeThreat)
	{
		OutResult.FailureReason = TEXT("Ship weapon fire requires a runtime threat.");
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}

	const FEquipmentSlotState* WeaponSlot = SystemicGameplayState.EquipmentSlots.FindByPredicate([](const FEquipmentSlotState& Candidate)
	{
		return Candidate.SlotId == FName(TEXT("ship_weapon_hardpoint_1")) &&
			Candidate.OwnerId == FName(TEXT("player_ship"));
	});
	if (!WeaponSlot || WeaponSlot->EquippedStack.ItemId.IsNone() || WeaponSlot->EquippedStack.Quantity <= 0)
	{
		OutResult.FailureReason = TEXT("No ship weapon is equipped in hardpoint 1.");
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}

	const FItemDefinition* WeaponItem = FindRuntimeItem(SystemicGameplayState, WeaponSlot->EquippedStack.ItemId);
	if (!WeaponItem || WeaponItem->ItemType != FName(TEXT("ship_weapon")))
	{
		OutResult.WeaponItemId = WeaponSlot->EquippedStack.ItemId;
		OutResult.FailureReason = TEXT("Equipped hardpoint item is not a ship weapon.");
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}
	const FShipWeaponStatDefinition* WeaponStats = FindRuntimeShipWeaponStats(SystemicGameplayState, WeaponItem->ItemId);

	const FName TargetCombatantId = RuntimeThreat->AttackerId;
	const FName CounterThreatId(*FString::Printf(TEXT("threat_runtime_player_%s"), *EncounterId.ToString()));
	FThreatRecord* CounterThreat = SystemicGameplayState.ThreatRecords.FindByPredicate([CounterThreatId](const FThreatRecord& Candidate)
	{
		return Candidate.ThreatId == CounterThreatId;
	});
	if (!CounterThreat)
	{
		FThreatRecord NewThreat;
		NewThreat.ThreatId = CounterThreatId;
		NewThreat.AttackerId = TEXT("player_ship");
		NewThreat.DefenderId = TargetCombatantId;
		NewThreat.LastKnownTarget = RuntimeThreat->LastKnownTarget;
		NewThreat.Severity = FMath::Max(0.45, RuntimeThreat->Severity);
		NewThreat.Confidence = RuntimeThreat->Confidence;
		NewThreat.ExpiresAtTimeSeconds = RuntimeThreat->ExpiresAtTimeSeconds;
		NewThreat.SourceEventId = FName(*FString::Printf(TEXT("event_runtime_player_fire_%s"), *EncounterId.ToString()));
		NewThreat.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *NewThreat.ThreatId.ToString()));
		SystemicGameplayState.ThreatRecords.Add(NewThreat);
		CounterThreat = &SystemicGameplayState.ThreatRecords.Last();
	}

	const double DamageAmount = ResolveRuntimeShipWeaponDamage(*WeaponItem, WeaponStats);
	const double CooldownSeconds = ResolveRuntimeShipWeaponCooldownSeconds(*WeaponItem, WeaponStats);
	const double EnergyCost = ResolveRuntimeShipWeaponEnergyCost(WeaponStats);
	const double EnergyRegenPerSecond = ResolveRuntimeShipWeaponEnergyRegenPerSecond(WeaponStats);
	const double MinAlignment = ResolveRuntimeShipWeaponMinAlignment(WeaponStats);
	const double MaxWeaponEnergy = 100.0;
	const double RangeCm = WeaponStats ? WeaponStats->RangeCm : 300000.0;
	const double ProjectileSpeedCmPerSec = ResolveRuntimeShipWeaponProjectileSpeedCmPerSec(WeaponStats);
	const double ProjectileCollisionRadiusCm = ResolveRuntimeShipWeaponProjectileCollisionRadiusCm(WeaponStats);
	const FName WeaponStatId = WeaponStats ? WeaponStats->WeaponStatId : NAME_None;
	const FName WeaponPresentationType = WeaponStats && !WeaponStats->PresentationType.IsNone()
		? WeaponStats->PresentationType
		: FName(TEXT("projectile_bolt"));
	const double CurrentTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	UWorld* World = ResolveSessionWorld();
	const ASpaceFlightPawn* FlightPawn = World ? ResolveSpaceFlightPawn(World, World->GetFirstPlayerController()) : nullptr;
	FVector ShotStartPositionCm = FVector::ZeroVector;
	FVector ShipForward = FVector::ForwardVector;
	if (FlightPawn)
	{
		ShotStartPositionCm = FlightPawn->GetLogicalSystemPositionCm();
		ShipForward = FlightPawn->GetActorForwardVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	}
	FVector TargetPositionCm = ShotStartPositionCm + ShipForward * FMath::Max(RangeCm, 1.0);
	if (UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
	{
		TArray<FRealizedEncounterActorEntry> EncounterActors;
		StarSystem->GetRealizedEncounterActors(EncounterActors);
		if (const FRealizedEncounterActorEntry* TargetEntry = EncounterActors.FindByPredicate([EncounterId](const FRealizedEncounterActorEntry& Entry)
		{
			return Entry.EncounterId == EncounterId && Entry.Role == EShipGoalKind::Pirate && Entry.Actor.IsValid();
		}))
		{
			TargetPositionCm = TargetEntry->Actor->GetActorLocation();
		}
	}
	const FVector ToTarget = TargetPositionCm - ShotStartPositionCm;
	const double TargetDistanceCm = ToTarget.Size();
	const FVector ProjectileDirection = ToTarget.GetSafeNormal(UE_SMALL_NUMBER, ShipForward);
	const double AlignmentDot = FVector::DotProduct(ShipForward, ProjectileDirection);
	const bool bWeaponAligned = AlignmentDot + UE_SMALL_NUMBER >= MinAlignment;
	RegenerateRuntimePlayerWeaponEnergy(*RuntimeState, CurrentTimeSeconds, MaxWeaponEnergy, EnergyRegenPerSecond);
	RuntimeState->PlayerWeaponEnergyCost = EnergyCost;
	RuntimeState->PlayerWeaponMinAlignment = MinAlignment;
	RuntimeState->PlayerWeaponAlignmentDot = AlignmentDot;
	const double CooldownRemainingSeconds = FMath::Max(0.0, RuntimeState->PlayerWeaponReadyAtTimeSeconds - CurrentTimeSeconds);
	if (CooldownRemainingSeconds > UE_SMALL_NUMBER)
	{
		OutResult.WeaponItemId = WeaponItem->ItemId;
		OutResult.WeaponStatId = WeaponStatId;
		OutResult.TargetCombatantId = TargetCombatantId;
		OutResult.ThreatId = CounterThreat->ThreatId;
		OutResult.DamageAmount = DamageAmount;
		OutResult.RangeCm = RangeCm;
		OutResult.ProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		OutResult.WeaponPresentationType = WeaponPresentationType;
		OutResult.CooldownSeconds = CooldownSeconds;
		OutResult.CooldownRemainingSeconds = CooldownRemainingSeconds;
		OutResult.ReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		OutResult.WeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		OutResult.WeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		OutResult.WeaponEnergyCost = EnergyCost;
		OutResult.WeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		OutResult.WeaponMinAlignment = MinAlignment;
		OutResult.WeaponAlignmentDot = AlignmentDot;
		OutResult.bWeaponAligned = bWeaponAligned;
		OutResult.FailureReason = FString::Printf(TEXT("%s is cooling down for %.2f seconds."), *WeaponItem->ItemId.ToString(), CooldownRemainingSeconds);
		LastRuntimeEncounterView.PlayerWeaponItemId = WeaponItem->ItemId;
		LastRuntimeEncounterView.PlayerWeaponStatId = WeaponStatId;
		LastRuntimeEncounterView.PlayerWeaponTargetCombatantId = TargetCombatantId;
		LastRuntimeEncounterView.PlayerWeaponDamageAmount = DamageAmount;
		LastRuntimeEncounterView.PlayerWeaponRangeCm = RangeCm;
		LastRuntimeEncounterView.PlayerWeaponProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		LastRuntimeEncounterView.PlayerWeaponPresentationType = WeaponPresentationType;
		LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = CooldownSeconds;
		LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds = CooldownRemainingSeconds;
		LastRuntimeEncounterView.PlayerWeaponReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		LastRuntimeEncounterView.PlayerWeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		LastRuntimeEncounterView.PlayerWeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		LastRuntimeEncounterView.PlayerWeaponEnergyCost = EnergyCost;
		LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		LastRuntimeEncounterView.PlayerWeaponMinAlignment = MinAlignment;
		LastRuntimeEncounterView.PlayerWeaponAlignmentDot = AlignmentDot;
		LastRuntimeEncounterView.bPlayerWeaponAligned = bWeaponAligned;
		LastRuntimeEncounterView.bPlayerWeaponEnergyReady = RuntimeState->PlayerWeaponEnergy + 0.001 >= EnergyCost;
		LastRuntimeEncounterView.bPlayerWeaponReady = false;
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}
	if (RuntimeState->PlayerWeaponEnergy + 0.001 < EnergyCost)
	{
		OutResult.WeaponItemId = WeaponItem->ItemId;
		OutResult.WeaponStatId = WeaponStatId;
		OutResult.TargetCombatantId = TargetCombatantId;
		OutResult.ThreatId = CounterThreat->ThreatId;
		OutResult.DamageAmount = DamageAmount;
		OutResult.RangeCm = RangeCm;
		OutResult.ProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		OutResult.WeaponPresentationType = WeaponPresentationType;
		OutResult.CooldownSeconds = CooldownSeconds;
		OutResult.CooldownRemainingSeconds = 0.0;
		OutResult.ReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		OutResult.WeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		OutResult.WeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		OutResult.WeaponEnergyCost = EnergyCost;
		OutResult.WeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		OutResult.WeaponMinAlignment = MinAlignment;
		OutResult.WeaponAlignmentDot = AlignmentDot;
		OutResult.bWeaponAligned = bWeaponAligned;
		OutResult.FailureReason = FString::Printf(TEXT("%s capacitor is low: %.1f / %.1f energy."), *WeaponItem->ItemId.ToString(), RuntimeState->PlayerWeaponEnergy, EnergyCost);
		LastRuntimeEncounterView.PlayerWeaponItemId = WeaponItem->ItemId;
		LastRuntimeEncounterView.PlayerWeaponStatId = WeaponStatId;
		LastRuntimeEncounterView.PlayerWeaponTargetCombatantId = TargetCombatantId;
		LastRuntimeEncounterView.PlayerWeaponDamageAmount = DamageAmount;
		LastRuntimeEncounterView.PlayerWeaponRangeCm = RangeCm;
		LastRuntimeEncounterView.PlayerWeaponProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		LastRuntimeEncounterView.PlayerWeaponPresentationType = WeaponPresentationType;
		LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = CooldownSeconds;
		LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds = 0.0;
		LastRuntimeEncounterView.PlayerWeaponReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		LastRuntimeEncounterView.PlayerWeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		LastRuntimeEncounterView.PlayerWeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		LastRuntimeEncounterView.PlayerWeaponEnergyCost = EnergyCost;
		LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		LastRuntimeEncounterView.PlayerWeaponMinAlignment = MinAlignment;
		LastRuntimeEncounterView.PlayerWeaponAlignmentDot = AlignmentDot;
		LastRuntimeEncounterView.bPlayerWeaponAligned = bWeaponAligned;
		LastRuntimeEncounterView.bPlayerWeaponEnergyReady = false;
		LastRuntimeEncounterView.bPlayerWeaponReady = false;
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}
	if (!bWeaponAligned)
	{
		OutResult.WeaponItemId = WeaponItem->ItemId;
		OutResult.WeaponStatId = WeaponStatId;
		OutResult.TargetCombatantId = TargetCombatantId;
		OutResult.ThreatId = CounterThreat->ThreatId;
		OutResult.DamageAmount = DamageAmount;
		OutResult.RangeCm = RangeCm;
		OutResult.ProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		OutResult.WeaponPresentationType = WeaponPresentationType;
		OutResult.CooldownSeconds = CooldownSeconds;
		OutResult.CooldownRemainingSeconds = 0.0;
		OutResult.ReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		OutResult.WeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		OutResult.WeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		OutResult.WeaponEnergyCost = EnergyCost;
		OutResult.WeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		OutResult.WeaponMinAlignment = MinAlignment;
		OutResult.WeaponAlignmentDot = AlignmentDot;
		OutResult.bWeaponAligned = false;
		OutResult.FailureReason = FString::Printf(TEXT("%s muzzle is misaligned: %.3f / %.3f."), *WeaponItem->ItemId.ToString(), AlignmentDot, MinAlignment);
		LastRuntimeEncounterView.PlayerWeaponItemId = WeaponItem->ItemId;
		LastRuntimeEncounterView.PlayerWeaponStatId = WeaponStatId;
		LastRuntimeEncounterView.PlayerWeaponTargetCombatantId = TargetCombatantId;
		LastRuntimeEncounterView.PlayerWeaponDamageAmount = DamageAmount;
		LastRuntimeEncounterView.PlayerWeaponRangeCm = RangeCm;
		LastRuntimeEncounterView.PlayerWeaponProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
		LastRuntimeEncounterView.PlayerWeaponPresentationType = WeaponPresentationType;
		LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = CooldownSeconds;
		LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds = 0.0;
		LastRuntimeEncounterView.PlayerWeaponReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
		LastRuntimeEncounterView.PlayerWeaponEnergy = RuntimeState->PlayerWeaponEnergy;
		LastRuntimeEncounterView.PlayerWeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
		LastRuntimeEncounterView.PlayerWeaponEnergyCost = EnergyCost;
		LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
		LastRuntimeEncounterView.PlayerWeaponMinAlignment = MinAlignment;
		LastRuntimeEncounterView.PlayerWeaponAlignmentDot = AlignmentDot;
		LastRuntimeEncounterView.bPlayerWeaponAligned = false;
		LastRuntimeEncounterView.bPlayerWeaponEnergyReady = true;
		LastRuntimeEncounterView.bPlayerWeaponReady = false;
		LastRuntimeEncounterView.DebugReason = OutResult.FailureReason;
		return false;
	}

	const int32 NextFireSequence = RuntimeState->PlayerWeaponFireSequence + 1;
	const FName PendingDamageEventId(*FString::Printf(TEXT("damage_runtime_%s_player_%s_%03d"), *EncounterId.ToString(), *WeaponItem->ItemId.ToString(), NextFireSequence));
	RuntimeState->LastPlayerWeaponItemId = WeaponItem->ItemId;
	RuntimeState->LastPlayerWeaponStatId = WeaponStatId;
	RuntimeState->LastPlayerWeaponFireTimeSeconds = CurrentTimeSeconds;
	RuntimeState->PlayerWeaponReadyAtTimeSeconds = CurrentTimeSeconds + CooldownSeconds;
	RuntimeState->PlayerWeaponCooldownSeconds = CooldownSeconds;
	RuntimeState->PlayerWeaponEnergy = FMath::Max(0.0, RuntimeState->PlayerWeaponEnergy - EnergyCost);
	RuntimeState->PlayerWeaponEnergyCost = EnergyCost;
	RuntimeState->PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
	RuntimeState->PlayerWeaponEnergyUpdatedAtTimeSeconds = CurrentTimeSeconds;
	RuntimeState->PlayerWeaponMinAlignment = MinAlignment;
	RuntimeState->PlayerWeaponAlignmentDot = AlignmentDot;
	RuntimeState->PlayerWeaponFireSequence = NextFireSequence;
	RuntimeState->LastUpdatedTimeSeconds = CurrentTimeSeconds;

	const FVector ProjectileInheritedVelocityCmPerSec = FlightPawn->GetLinearVelocityCmPerSec();
	const FVector ProjectileEffectiveVelocityCmPerSec = ProjectileDirection * ProjectileSpeedCmPerSec + ProjectileInheritedVelocityCmPerSec;
	const double ProjectileEffectiveSpeedCmPerSec = ProjectileEffectiveVelocityCmPerSec.Size();
	const FVector ProjectileTravelDirection = ProjectileEffectiveVelocityCmPerSec.GetSafeNormal(UE_SMALL_NUMBER, ProjectileDirection);
	const double ProjectileLifetimeSeconds = ProjectileEffectiveSpeedCmPerSec > UE_SMALL_NUMBER ? RangeCm / ProjectileEffectiveSpeedCmPerSec : 0.0;
	const double ProjectileImpactDelaySeconds = ProjectileEffectiveSpeedCmPerSec > UE_SMALL_NUMBER ? TargetDistanceCm / ProjectileEffectiveSpeedCmPerSec : 0.0;

	FRuntimeEncounterProjectileState Projectile;
	Projectile.ProjectileId = FName(*FString::Printf(TEXT("projectile_%s"), *PendingDamageEventId.ToString()));
	Projectile.ProjectileState = TEXT("in_flight");
	Projectile.DamageEventId = PendingDamageEventId;
	Projectile.SourceCombatantId = TEXT("player_ship");
	Projectile.TargetCombatantId = TargetCombatantId;
	Projectile.ThreatId = CounterThreat->ThreatId;
	Projectile.DamageType = WeaponStats && !WeaponStats->DamageType.IsNone() ? WeaponStats->DamageType : FName(TEXT("energy_weapon"));
	Projectile.DamageAmount = DamageAmount;
	Projectile.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *PendingDamageEventId.ToString()));
	Projectile.StartPositionCm = ShotStartPositionCm;
	Projectile.Direction = ProjectileDirection;
	Projectile.SpeedCmPerSec = ProjectileSpeedCmPerSec;
	Projectile.InheritedVelocityCmPerSec = ProjectileInheritedVelocityCmPerSec;
	Projectile.EffectiveVelocityCmPerSec = ProjectileEffectiveVelocityCmPerSec;
	Projectile.MaxDistanceCm = RangeCm;
	Projectile.CollisionRadiusCm = ProjectileCollisionRadiusCm;
	Projectile.TargetDistanceCm = TargetDistanceCm;
	Projectile.StartedAtTimeSeconds = CurrentTimeSeconds;
	Projectile.ImpactAtTimeSeconds = CurrentTimeSeconds + ProjectileImpactDelaySeconds;
	RuntimeState->Projectiles.Add(Projectile);

	RuntimeState->ShotPresentationId = FName(*FString::Printf(TEXT("shot_%s"), *PendingDamageEventId.ToString()));
	RuntimeState->ShotPresentationType = WeaponPresentationType;
	RuntimeState->ShotStartPositionCm = ShotStartPositionCm;
	RuntimeState->ShotEndPositionCm = ShotStartPositionCm + ProjectileTravelDirection * FMath::Min(RangeCm, FMath::Max(TargetDistanceCm, 1.0));
	RuntimeState->ShotPresentationStartedAtTimeSeconds = CurrentTimeSeconds;
	RuntimeState->ShotPresentationDurationSeconds = FMath::Max(0.05, ProjectileLifetimeSeconds);
	RuntimeState->ShotPresentationActorId = NAME_None;
	if (UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
	{
		StarSystem->SpawnCombatShotPresentation(
			RuntimeState->ShotPresentationId,
			RuntimeState->ShotPresentationType,
			RuntimeState->ShotStartPositionCm,
			RuntimeState->ShotEndPositionCm,
			RuntimeState->ShotPresentationStartedAtTimeSeconds,
			RuntimeState->ShotPresentationDurationSeconds,
			RuntimeState->ShotPresentationActorId);
	}

	OutResult.bAccepted = true;
	OutResult.WeaponItemId = WeaponItem->ItemId;
	OutResult.WeaponStatId = WeaponStatId;
	OutResult.TargetCombatantId = TargetCombatantId;
	OutResult.ThreatId = CounterThreat->ThreatId;
	OutResult.DamageEventId = PendingDamageEventId;
	OutResult.DamageResultState = TEXT("pending_projectile");
	OutResult.DamageAmount = DamageAmount;
	OutResult.RangeCm = RangeCm;
	OutResult.ProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
	OutResult.ProjectileInheritedVelocityCmPerSec = Projectile.InheritedVelocityCmPerSec;
	OutResult.ProjectileEffectiveVelocityCmPerSec = Projectile.EffectiveVelocityCmPerSec;
	OutResult.WeaponPresentationType = WeaponPresentationType;
	OutResult.CooldownSeconds = CooldownSeconds;
	OutResult.CooldownRemainingSeconds = CooldownSeconds;
	OutResult.ReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
	OutResult.WeaponEnergy = RuntimeState->PlayerWeaponEnergy;
	OutResult.WeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
	OutResult.WeaponEnergyCost = EnergyCost;
	OutResult.WeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
	OutResult.WeaponMinAlignment = MinAlignment;
	OutResult.WeaponAlignmentDot = AlignmentDot;
	OutResult.bWeaponAligned = true;
	OutResult.HostileResponseStateId = RuntimeState->HostileResponseStateId;
	OutResult.HostileManeuverStateId = RuntimeState->HostileManeuverStateId;
	OutResult.ShotPresentationId = RuntimeState->ShotPresentationId;
	OutResult.ShotPresentationType = RuntimeState->ShotPresentationType;
	OutResult.ShotPresentationActorId = RuntimeState->ShotPresentationActorId;
	OutResult.ShotStartPositionCm = RuntimeState->ShotStartPositionCm;
	OutResult.ShotEndPositionCm = RuntimeState->ShotEndPositionCm;
	OutResult.ShotPresentationStartedAtTimeSeconds = RuntimeState->ShotPresentationStartedAtTimeSeconds;
	OutResult.ShotPresentationDurationSeconds = RuntimeState->ShotPresentationDurationSeconds;
	OutResult.ProjectileState = Projectile.ProjectileState;
	OutResult.bProjectileInFlight = true;
	OutResult.ProjectileTravelledCm = Projectile.TravelledCm;
	OutResult.ProjectileTargetDistanceCm = Projectile.TargetDistanceCm;
	OutResult.ProjectileImpactAtTimeSeconds = Projectile.ImpactAtTimeSeconds;
	LastRuntimeEncounterView.bPlayerWeaponFireAccepted = true;
	LastRuntimeEncounterView.PlayerWeaponItemId = WeaponItem->ItemId;
	LastRuntimeEncounterView.PlayerWeaponStatId = WeaponStatId;
	LastRuntimeEncounterView.PlayerWeaponTargetCombatantId = TargetCombatantId;
	LastRuntimeEncounterView.PlayerWeaponDamageAmount = DamageAmount;
	LastRuntimeEncounterView.PlayerWeaponRangeCm = RangeCm;
	LastRuntimeEncounterView.PlayerWeaponProjectileSpeedCmPerSec = ProjectileSpeedCmPerSec;
	LastRuntimeEncounterView.PlayerWeaponPresentationType = WeaponPresentationType;
	LastRuntimeEncounterView.PlayerWeaponCooldownSeconds = CooldownSeconds;
	LastRuntimeEncounterView.PlayerWeaponCooldownRemainingSeconds = CooldownSeconds;
	LastRuntimeEncounterView.PlayerWeaponReadyAtTimeSeconds = RuntimeState->PlayerWeaponReadyAtTimeSeconds;
	LastRuntimeEncounterView.PlayerWeaponEnergy = RuntimeState->PlayerWeaponEnergy;
	LastRuntimeEncounterView.PlayerWeaponMaxEnergy = RuntimeState->PlayerWeaponMaxEnergy;
	LastRuntimeEncounterView.PlayerWeaponEnergyCost = EnergyCost;
	LastRuntimeEncounterView.PlayerWeaponEnergyRegenPerSecond = EnergyRegenPerSecond;
	LastRuntimeEncounterView.PlayerWeaponMinAlignment = MinAlignment;
	LastRuntimeEncounterView.PlayerWeaponAlignmentDot = AlignmentDot;
	LastRuntimeEncounterView.bPlayerWeaponAligned = true;
	LastRuntimeEncounterView.bPlayerWeaponEnergyReady = RuntimeState->PlayerWeaponEnergy + 0.001 >= EnergyCost;
	LastRuntimeEncounterView.bPlayerWeaponReady = false;
	LastRuntimeEncounterView.HostileResponseStateId = RuntimeState->HostileResponseStateId;
	LastRuntimeEncounterView.HostileManeuverStateId = RuntimeState->HostileManeuverStateId;
	LastRuntimeEncounterView.HostileAIStateId = RuntimeState->HostileAIStateId;
	LastRuntimeEncounterView.ShotPresentationId = RuntimeState->ShotPresentationId;
	LastRuntimeEncounterView.ShotPresentationType = RuntimeState->ShotPresentationType;
	LastRuntimeEncounterView.ShotPresentationActorId = RuntimeState->ShotPresentationActorId;
	LastRuntimeEncounterView.ShotStartPositionCm = RuntimeState->ShotStartPositionCm;
	LastRuntimeEncounterView.ShotEndPositionCm = RuntimeState->ShotEndPositionCm;
	LastRuntimeEncounterView.ShotPresentationStartedAtTimeSeconds = RuntimeState->ShotPresentationStartedAtTimeSeconds;
	LastRuntimeEncounterView.ShotPresentationDurationSeconds = RuntimeState->ShotPresentationDurationSeconds;
	LastRuntimeEncounterView.bShotPresentationActive = true;
	LastRuntimeEncounterView.bRenderedShotActive = !RuntimeState->ShotPresentationActorId.IsNone();
	LastRuntimeEncounterView.ProjectileState = Projectile.ProjectileState;
	LastRuntimeEncounterView.ProjectileSourceCombatantId = Projectile.SourceCombatantId;
	LastRuntimeEncounterView.ProjectileTargetCombatantId = Projectile.TargetCombatantId;
	LastRuntimeEncounterView.bProjectileInFlight = true;
	LastRuntimeEncounterView.ProjectileSpeedCmPerSec = Projectile.SpeedCmPerSec;
	LastRuntimeEncounterView.ProjectileInheritedVelocityCmPerSec = Projectile.InheritedVelocityCmPerSec;
	LastRuntimeEncounterView.ProjectileEffectiveVelocityCmPerSec = Projectile.EffectiveVelocityCmPerSec;
	LastRuntimeEncounterView.ProjectileTravelledCm = Projectile.TravelledCm;
	LastRuntimeEncounterView.ProjectileTargetDistanceCm = Projectile.TargetDistanceCm;
	LastRuntimeEncounterView.ProjectileImpactAtTimeSeconds = Projectile.ImpactAtTimeSeconds;
	LastRuntimeEncounterView.TargetShipId = TargetCombatantId;
	LastRuntimeEncounterView.DamageEventId = PendingDamageEventId;
	LastRuntimeEncounterView.DamageResultState = TEXT("pending_projectile");
	LastRuntimeEncounterView.DebugReason = FString::Printf(TEXT("Fired %s projectile at %s."), *WeaponItem->ItemId.ToString(), *TargetCombatantId.ToString());
	return true;
}

bool UStargameSessionSubsystem::GetSystemMapOverview(FSystemMapOverviewView& OutMap) const
{
	OutMap = FSystemMapOverviewView();
	UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || !StarSystem->IsSystemBuildComplete() || StarSystem->GetActiveSystemId().IsNone())
	{
		OutMap.DebugReason = TEXT("System map overview requires a built active star system.");
		return false;
	}

	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FStargameScaleContract& Scale = SystemDefinition.Scale;
	const double SimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	OutMap.bAvailable = true;
	OutMap.SystemId = SystemDefinition.SystemId;
	OutMap.SimulationTimeSeconds = SimulationTimeSeconds;
	OutMap.MapDistanceScaleCmPerUnit = Scale.MapDistanceScaleCmPerUnit;
	OutMap.SelectedTargetId = SelectedTargetId;

	UOrbitRouteFrameQueryService::BuildSystemMapViewModel(SystemDefinition, ClockSnapshot, SimulationTimeSeconds, OutMap.Entries);

	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		FRouteSample MidpointSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route.RouteSegmentId, 0.5, ClockSnapshot, SimulationTimeSeconds, MidpointSample))
		{
			continue;
		}

		FSystemMapRouteView RouteView;
		RouteView.RouteSegmentId = Route.RouteSegmentId;
		RouteView.SourceAnchorId = Route.SourceAnchorId;
		RouteView.DestinationAnchorId = Route.DestinationAnchorId;
		RouteView.SourcePositionCm = MidpointSample.SourceAnchorTransform.PositionCm;
		RouteView.DestinationPositionCm = MidpointSample.DestinationAnchorTransform.PositionCm;
		RouteView.MidpointPositionCm = MidpointSample.ResolvedTransform.PositionCm;
		RouteView.SourceMapPosition = ProjectSystemMapPosition(Scale, RouteView.SourcePositionCm);
		RouteView.DestinationMapPosition = ProjectSystemMapPosition(Scale, RouteView.DestinationPositionCm);
		RouteView.MidpointMapPosition = ProjectSystemMapPosition(Scale, RouteView.MidpointPositionCm);
		RouteView.SecurityRating = Route.SecurityRating;
		RouteView.RiskScore = MidpointSample.RiskScore;
		RouteView.bSupportsPatrolCoverage = Route.bSupportsPatrolCoverage;
		RouteView.bSupportsPirateAmbush = Route.bSupportsPirateAmbush;
		OutMap.Routes.Add(RouteView);
	}

	auto AddMarker = [&OutMap, &Scale](FName MarkerId, FName MarkerType, FName SourceId, FName TargetId, EShipGoalKind GoalKind, const FRouteSample& Sample, FName State, bool bRealized, double DynamicSelectionScore = 0.0, const FString& SelectionDebugReason = FString())
	{
		if (MarkerId.IsNone() || Sample.RouteSegmentId.IsNone())
		{
			return;
		}

		FSystemMapMarkerView Marker;
		Marker.MarkerId = MarkerId;
		Marker.MarkerType = MarkerType;
		Marker.SourceId = SourceId;
		Marker.TargetId = TargetId;
		Marker.GoalKind = GoalKind;
		Marker.RouteSegmentId = Sample.RouteSegmentId;
		Marker.RouteProgress01 = Sample.RouteProgress01;
		Marker.PositionCm = Sample.ResolvedTransform.PositionCm;
		Marker.MapPosition = ProjectSystemMapPosition(Scale, Marker.PositionCm);
		Marker.State = State;
		Marker.DynamicSelectionScore = DynamicSelectionScore;
		Marker.SelectionDebugReason = SelectionDebugReason;
		Marker.bRealized = bRealized;
		OutMap.Markers.Add(Marker);
	};

	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController))
	{
		FSystemMapMarkerView PlayerMarker;
		PlayerMarker.MarkerId = TEXT("player_ship");
		PlayerMarker.MarkerType = TEXT("player");
		PlayerMarker.SourceId = TEXT("player_ship");
		PlayerMarker.PositionCm = FlightPawn->GetLogicalSystemPositionCm();
		PlayerMarker.MapPosition = ProjectSystemMapPosition(Scale, PlayerMarker.PositionCm);
		PlayerMarker.bRealized = true;
		OutMap.Markers.Add(PlayerMarker);
	}

	FMissionWaypointViewModel MissionWaypoint;
	if (GetActiveMissionWaypoint(MissionWaypoint) && MissionWaypoint.bHasActiveMission && !MissionWaypoint.TargetId.IsNone())
	{
		OutMap.ActiveMissionTargetId = MissionWaypoint.TargetId;
		FFrameResolvedTransform MissionTransform;
		if (UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, MissionWaypoint.TargetId, ClockSnapshot, SimulationTimeSeconds, MissionTransform))
		{
			FSystemMapMarkerView MissionMarker;
			MissionMarker.MarkerId = MissionWaypoint.ObjectiveStateId;
			MissionMarker.MarkerType = TEXT("mission_target");
			MissionMarker.SourceId = MissionWaypoint.TargetId;
			MissionMarker.PositionCm = MissionTransform.PositionCm;
			MissionMarker.MapPosition = ProjectSystemMapPosition(Scale, MissionMarker.PositionCm);
			MissionMarker.State = MissionWaypoint.ObjectiveState;
			OutMap.Markers.Add(MissionMarker);
		}
	}

	if (!SelectedTargetId.IsNone())
	{
		FFrameResolvedTransform SelectedTransform;
		if (UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, SelectedTargetId, ClockSnapshot, SimulationTimeSeconds, SelectedTransform))
		{
			FSystemMapMarkerView SelectedMarker;
			SelectedMarker.MarkerId = SelectedTargetId;
			SelectedMarker.MarkerType = TEXT("selected_target");
			SelectedMarker.SourceId = SelectedTargetId;
			SelectedMarker.PositionCm = SelectedTransform.PositionCm;
			SelectedMarker.MapPosition = ProjectSystemMapPosition(Scale, SelectedMarker.PositionCm);
			OutMap.Markers.Add(SelectedMarker);
		}
	}

	TSet<FName> RealizedTrafficShipIds;
	TArray<FRealizedTrafficActorEntry> RealizedTrafficActors;
	StarSystem->GetRealizedTrafficActors(RealizedTrafficActors);
	for (const FRealizedTrafficActorEntry& ActorEntry : RealizedTrafficActors)
	{
		RealizedTrafficShipIds.Add(ActorEntry.ShipInstanceId);
	}

	for (const FShipTrafficInstance& Ship : ActiveTrafficState.Ships)
	{
		if (Ship.CurrentGoal.RouteSegmentId.IsNone())
		{
			continue;
		}

		FRouteSample ShipSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Ship.CurrentGoal.RouteSegmentId, Ship.CurrentGoal.RouteProgress01, ClockSnapshot, SimulationTimeSeconds, ShipSample))
		{
			continue;
		}

		const bool bRealized = Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized || RealizedTrafficShipIds.Contains(Ship.ShipInstanceId);
		AddMarker(Ship.ShipInstanceId, TEXT("traffic_ship"), Ship.ShipInstanceId, Ship.CurrentGoal.TargetFrame.TargetId, Ship.CurrentGoal.GoalKind, ShipSample, Ship.LastDecisionReason, bRealized);
	}

	for (const FPatrolReservationRecord& Reservation : SystemicGameplayState.PatrolReservations)
	{
		if (Reservation.RouteSegmentId.IsNone() || Reservation.State == FName(TEXT("consumed")))
		{
			continue;
		}

		FRouteSample PatrolSample;
		if (UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Reservation.RouteSegmentId, 0.5, ClockSnapshot, SimulationTimeSeconds, PatrolSample))
		{
			AddMarker(Reservation.ReservationId, TEXT("patrol_reservation"), Reservation.PatrolAssetId, NAME_None, EShipGoalKind::Patrol, PatrolSample, Reservation.State, false, Reservation.DynamicPatrolScore, Reservation.SelectionDebugReason);
		}
	}

	for (const FInterdictionHazardRecord& Hazard : SystemicGameplayState.InterdictionHazards)
	{
		if (Hazard.RouteSegmentId.IsNone() || Hazard.State == FName(TEXT("resolved")))
		{
			continue;
		}

		const double Progress = Hazard.RouteSample.RouteSegmentId.IsNone() ? 0.5 : Hazard.RouteSample.RouteProgress01;
		FRouteSample HazardSample;
		if (UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Hazard.RouteSegmentId, Progress, ClockSnapshot, SimulationTimeSeconds, HazardSample))
		{
			AddMarker(Hazard.HazardId, TEXT("ambush_risk"), Hazard.OwnerGroupId, Hazard.TargetShipId, EShipGoalKind::Pirate, HazardSample, Hazard.State, false, Hazard.DynamicPirateAmbushScore, Hazard.SelectionDebugReason);
		}
	}

	TArray<FRealizedEncounterActorEntry> EncounterActors;
	StarSystem->GetRealizedEncounterActors(EncounterActors);
	for (const FRealizedEncounterActorEntry& EncounterActor : EncounterActors)
	{
		FRouteSample EncounterSample = EncounterActor.RouteSample;
		if (EncounterSample.RouteSegmentId.IsNone() && !EncounterActor.RouteSegmentId.IsNone())
		{
			UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, EncounterActor.RouteSegmentId, EncounterActor.RouteProgress01, ClockSnapshot, SimulationTimeSeconds, EncounterSample);
		}
		double DynamicSelectionScore = 0.0;
		FString SelectionDebugReason;
		if (!EncounterActor.HazardId.IsNone())
		{
			if (const FInterdictionHazardRecord* Hazard = SystemicGameplayState.InterdictionHazards.FindByPredicate([&EncounterActor](const FInterdictionHazardRecord& Candidate)
			{
				return Candidate.HazardId == EncounterActor.HazardId;
			}))
			{
				DynamicSelectionScore = Hazard->DynamicPirateAmbushScore;
				SelectionDebugReason = Hazard->SelectionDebugReason;
			}
		}
		else if (!EncounterActor.PatrolReservationId.IsNone())
		{
			if (const FPatrolReservationRecord* Reservation = SystemicGameplayState.PatrolReservations.FindByPredicate([&EncounterActor](const FPatrolReservationRecord& Candidate)
			{
				return Candidate.ReservationId == EncounterActor.PatrolReservationId;
			}))
			{
				DynamicSelectionScore = Reservation->DynamicPatrolScore;
				SelectionDebugReason = Reservation->SelectionDebugReason;
			}
		}
		AddMarker(EncounterActor.ActorId, TEXT("encounter_actor"), EncounterActor.EncounterId, EncounterActor.TargetShipId, EncounterActor.Role, EncounterSample, EncounterActor.IntentType, EncounterActor.Actor.IsValid(), DynamicSelectionScore, SelectionDebugReason);
	}

	OutMap.Entries.Sort([](const FSystemMapEntryViewModel& Left, const FSystemMapEntryViewModel& Right)
	{
		return Left.EntryId.LexicalLess(Right.EntryId);
	});
	OutMap.Routes.Sort([](const FSystemMapRouteView& Left, const FSystemMapRouteView& Right)
	{
		return Left.RouteSegmentId.LexicalLess(Right.RouteSegmentId);
	});
	OutMap.Markers.Sort([](const FSystemMapMarkerView& Left, const FSystemMapMarkerView& Right)
	{
		return Left.MarkerId.LexicalLess(Right.MarkerId);
	});
	OutMap.SummaryText = FString::Printf(
		TEXT("%s map: %d entries, %d routes, %d markers at %.1fs."),
		*OutMap.SystemId.ToString(),
		OutMap.Entries.Num(),
		OutMap.Routes.Num(),
		OutMap.Markers.Num(),
		OutMap.SimulationTimeSeconds);
	return true;
}

bool UStargameSessionSubsystem::GetDistantSectorSnapshot(FDistantSectorSnapshotView& OutSnapshot) const
{
	OutSnapshot = FDistantSectorSnapshotView();
	UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || !StarSystem->IsSystemBuildComplete() || StarSystem->GetActiveSystemId().IsNone())
	{
		OutSnapshot.DebugReason = TEXT("Distant-sector snapshot requires a built active star system.");
		return false;
	}

	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const double SimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	const FSimulationClockSnapshot RouteClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
	OutSnapshot.bAvailable = true;
	OutSnapshot.SystemId = SystemDefinition.SystemId;
	OutSnapshot.SimulationTimeSeconds = SimulationTimeSeconds;

	TArray<FRealizedTrafficActorEntry> RealizedTrafficActors;
	StarSystem->GetRealizedTrafficActors(RealizedTrafficActors);
	TSet<FName> RealizedTrafficShipIds;
	for (const FRealizedTrafficActorEntry& ActorEntry : RealizedTrafficActors)
	{
		RealizedTrafficShipIds.Add(ActorEntry.ShipInstanceId);
	}

	auto AddTrafficEntry = [&OutSnapshot](FDistantSectorTrafficView Entry)
	{
		if (Entry.SnapshotCategory == FName(TEXT("data_only")))
		{
			++OutSnapshot.DataOnlyCount;
		}
		else if (Entry.SnapshotCategory == FName(TEXT("map_projected")))
		{
			++OutSnapshot.MapProjectedCount;
		}
		else if (Entry.SnapshotCategory == FName(TEXT("realized_local")))
		{
			++OutSnapshot.RealizedLocalCount;
		}
		else if (Entry.SnapshotCategory == FName(TEXT("encounter_actor")))
		{
			++OutSnapshot.EncounterActorCount;
		}
		OutSnapshot.Entries.Add(MoveTemp(Entry));
	};

	for (const FShipTrafficInstance& Ship : ActiveTrafficState.Ships)
	{
		FDistantSectorTrafficView Entry;
		Entry.ShipInstanceId = Ship.ShipInstanceId;
		Entry.GoalKind = Ship.CurrentGoal.GoalKind;
		Entry.TrafficTier = Ship.TrafficTier;
		Entry.RouteSegmentId = Ship.CurrentGoal.RouteSegmentId;
		Entry.RouteProgress01 = Ship.CurrentGoal.RouteProgress01;
		Entry.RealizationReason = Ship.LastDecisionReason;
		Entry.bRealized = Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized || RealizedTrafficShipIds.Contains(Ship.ShipInstanceId);
		Entry.SnapshotCategory = Entry.bRealized ? FName(TEXT("realized_local")) : FName(TEXT("data_only"));
		if (!Entry.RouteSegmentId.IsNone())
		{
			FRouteSample RouteSample;
			if (UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Entry.RouteSegmentId, Entry.RouteProgress01, RouteClock, SimulationTimeSeconds, RouteSample))
			{
				Entry.bHasRouteSample = true;
				Entry.PositionCm = RouteSample.ResolvedTransform.PositionCm;
				Entry.RouteProgress01 = RouteSample.RouteProgress01;
				if (!Entry.bRealized)
				{
					Entry.SnapshotCategory = TEXT("map_projected");
				}
			}
		}
		AddTrafficEntry(MoveTemp(Entry));
	}

	TArray<FRealizedEncounterActorEntry> EncounterActors;
	StarSystem->GetRealizedEncounterActors(EncounterActors);
	for (const FRealizedEncounterActorEntry& EncounterActor : EncounterActors)
	{
		FDistantSectorTrafficView Entry;
		Entry.ActorId = EncounterActor.ActorId;
		Entry.EncounterId = EncounterActor.EncounterId;
		Entry.ShipInstanceId = EncounterActor.TargetShipId;
		Entry.SnapshotCategory = TEXT("encounter_actor");
		Entry.GoalKind = EncounterActor.Role;
		Entry.TrafficTier = ELogicalTrafficTier::Tier1Realized;
		Entry.RouteSegmentId = EncounterActor.RouteSegmentId;
		Entry.RouteProgress01 = EncounterActor.RouteProgress01;
		Entry.PositionCm = EncounterActor.RouteSample.ResolvedTransform.PositionCm;
		Entry.bHasRouteSample = !EncounterActor.RouteSample.RouteSegmentId.IsNone();
		Entry.bRealized = EncounterActor.Actor.IsValid();
		Entry.RealizationReason = EncounterActor.IntentType;
		AddTrafficEntry(MoveTemp(Entry));
	}

	OutSnapshot.Entries.Sort([](const FDistantSectorTrafficView& Left, const FDistantSectorTrafficView& Right)
	{
		if (Left.SnapshotCategory != Right.SnapshotCategory)
		{
			return Left.SnapshotCategory.LexicalLess(Right.SnapshotCategory);
		}
		const FName LeftId = !Left.ShipInstanceId.IsNone() ? Left.ShipInstanceId : Left.ActorId;
		const FName RightId = !Right.ShipInstanceId.IsNone() ? Right.ShipInstanceId : Right.ActorId;
		return LeftId.LexicalLess(RightId);
	});
	OutSnapshot.SummaryText = FString::Printf(
		TEXT("%s distant sector at %.1fs: data=%d projected=%d realized=%d encounters=%d"),
		*OutSnapshot.SystemId.ToString(),
		OutSnapshot.SimulationTimeSeconds,
		OutSnapshot.DataOnlyCount,
		OutSnapshot.MapProjectedCount,
		OutSnapshot.RealizedLocalCount,
		OutSnapshot.EncounterActorCount);
	OutSnapshot.DebugReason = TEXT("Distant-sector snapshot separates data-only logical traffic, map-projected logical traffic, realized local traffic, and encounter actors.");
	return true;
}

bool UStargameSessionSubsystem::RejectDockedStationCommand(const FString& FailureReason, FDockedStationCommandResult& OutCommandResult) const
{
	OutCommandResult.bAccepted = false;
	OutCommandResult.FailureReason = FailureReason;
	return false;
}

bool UStargameSessionSubsystem::ResolveCurrentDockedStation(FDockedStationContext& OutContext, ASpaceFlightPawn** OutFlightPawn) const
{
	OutContext = FDockedStationContext();
	if (OutFlightPawn)
	{
		*OutFlightPawn = nullptr;
	}
	if (CurrentSystemId.IsNone())
	{
		OutContext.DebugReason = TEXT("No active session system.");
		return false;
	}

	UWorld* World = ResolveSessionWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (!FlightPawn)
	{
		OutContext.DebugReason = TEXT("Docked station context requires the active space flight pawn.");
		return false;
	}

	const FDockingOperationState Docking = FlightPawn->GetDockingOperationState();
	if (Docking.DockingState != EDockingState::Docked || Docking.StationId.IsNone() || Docking.PortId.IsNone())
	{
		OutContext.DebugReason = TEXT("Player ship is not docked at a station port.");
		return false;
	}

	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || StarSystem->GetActiveSystemId() != CurrentSystemId)
	{
		OutContext.DebugReason = TEXT("Docked station context requires the active star system.");
		return false;
	}

	const FStarSystemDefinition& SystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FStationDefinition* DockedStation = SystemDefinition.Stations.FindByPredicate([&Docking](const FStationDefinition& Station)
	{
		return Station.StationId == Docking.StationId && Station.DockingPorts.ContainsByPredicate([&Docking](const FDockingPortDefinition& Port)
		{
			return Port.PortId == Docking.PortId;
		});
	});
	if (!DockedStation)
	{
		OutContext.DebugReason = TEXT("Docked station or port no longer resolves in the active system definition.");
		return false;
	}

	OutContext.bDocked = true;
	OutContext.SystemId = CurrentSystemId;
	OutContext.StationId = Docking.StationId;
	OutContext.PortId = Docking.PortId;
	OutContext.DisplayName = DockedStation->DisplayName;
	OutContext.OwnerFactionId = DockedStation->OwnerFactionId;
	OutContext.StationRole = DockedStation->StationRole;
	OutContext.RegionName = DockedStation->RegionName;
	OutContext.SecurityProfile = DockedStation->SecurityProfile;
	OutContext.MarketProfileId = DockedStation->MarketProfileId;
	OutContext.InteriorRoomClass = DockedStation->InteriorRoomClass;
	OutContext.InteriorPawnClass = DockedStation->InteriorPawnClass;
	OutContext.MissionTags = DockedStation->MissionTags;
	OutContext.QuestGiverNpcIds = DockedStation->QuestGiverNpcIds;
	OutContext.QuestGivers = DockedStation->QuestGivers;
	OutContext.ShipInstanceId = Docking.ShipInstanceId;
	if (!OutContext.OwnerFactionId.IsNone())
	{
		if (const FFactionDefinition* OwnerFaction = SystemicGameplayState.Factions.FindByPredicate([&OutContext](const FFactionDefinition& Faction)
		{
			return Faction.FactionId == OutContext.OwnerFactionId;
		}))
		{
			OutContext.OwnerFactionDisplayName = OwnerFaction->DisplayName;
		}
	}
	for (const FStationServiceEndpointDefinition& Endpoint : SystemicGameplayState.StationServiceEndpoints)
	{
		if (Endpoint.StationId == Docking.StationId)
		{
			OutContext.ServiceEndpoints.Add(Endpoint);
		}
	}
	for (const FStationMarketState& Market : SystemicGameplayState.Markets)
	{
		if (Market.StationId == Docking.StationId)
		{
			OutContext.Markets.Add(Market);
		}
	}
	for (const FMissionOfferRecord& Offer : SystemicGameplayState.MissionOffers)
	{
		if (Offer.SourceStationId == Docking.StationId && Offer.State != FName(TEXT("expired")) && Offer.State != FName(TEXT("cancelled")))
		{
			OutContext.MissionOffers.Add(Offer);
		}
	}
	OutContext.DebugReason = FString::Printf(TEXT("Docked at station %s port %s with %d service endpoints, %d markets, and %d mission offers."),
		*OutContext.StationId.ToString(),
		*OutContext.PortId.ToString(),
		OutContext.ServiceEndpoints.Num(),
		OutContext.Markets.Num(),
		OutContext.MissionOffers.Num());
	if (OutFlightPawn)
	{
		*OutFlightPawn = FlightPawn;
	}
	return true;
}

bool UStargameSessionSubsystem::GetDockedStationContext(FDockedStationContext& OutContext) const
{
	return ResolveCurrentDockedStation(OutContext);
}

bool UStargameSessionSubsystem::GetDockedStationCommandPanel(FDockedStationCommandPanelView& OutPanel) const
{
	OutPanel = FDockedStationCommandPanelView();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		OutPanel.DebugReason = Context.DebugReason;
		return false;
	}

	OutPanel.bDocked = Context.bDocked;
	OutPanel.StationId = Context.StationId;
	OutPanel.StationDisplayName = Context.DisplayName;
	OutPanel.PortId = Context.PortId;
	OutPanel.OwnerFactionDisplayName = Context.OwnerFactionDisplayName;
	OutPanel.AvailableServiceCount = Context.ServiceEndpoints.Num();
	OutPanel.AvailableMarketCount = Context.Markets.Num();
	OutPanel.AvailableMissionCount = Context.MissionOffers.Num();
	OutPanel.AvailableContactCount = Context.QuestGiverNpcIds.Num();

	if (const FCreditAccountRecord* PlayerAccount = SystemicGameplayState.CreditAccounts.FindByPredicate([](const FCreditAccountRecord& Account)
	{
		return Account.AccountId == FName(TEXT("account_player")) || (Account.OwnerType == FName(TEXT("player")) && Account.OwnerId == FName(TEXT("player")));
	}))
	{
		OutPanel.PlayerCredits = PlayerAccount->AvailableBalance;
	}

	if (const FShipDurabilityState* PlayerDurability = SystemicGameplayState.ShipDurabilityStates.FindByPredicate([&Context](const FShipDurabilityState& Durability)
	{
		return Durability.CombatantId == Context.ShipInstanceId || Durability.CombatantId == FName(TEXT("player_ship"));
	}))
	{
		OutPanel.Hull = PlayerDurability->Hull;
		OutPanel.Shields = PlayerDurability->Shield;
		OutPanel.MaxHull = PlayerDurability->MaxHull > UE_SMALL_NUMBER ? PlayerDurability->MaxHull : 100.0;
		OutPanel.MaxShields = PlayerDurability->MaxShield > UE_SMALL_NUMBER ? PlayerDurability->MaxShield : 100.0;
	}

	if (const FContainerState* PersonalInventory = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_personal_inventory"));
	}))
	{
		OutPanel.PersonalInventorySlotsUsed = PersonalInventory->Stacks.Num();
		OutPanel.PersonalInventorySlotsMax = PersonalInventory->MaxSlotCount;
	}

	if (const FContainerState* ShipCargo = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_ship_cargo"));
	}))
	{
		OutPanel.ShipCargoStacks = ShipCargo->Stacks.Num();
	}

	auto AddCommand = [&OutPanel](FName CommandId, FName CommandType, const FText& DisplayName, bool bAvailable, const FString& UnavailableReason, FName SourceId = NAME_None, FName TargetId = NAME_None, int32 Count = 0)
	{
		FDockedStationCommandOption Option;
		Option.CommandId = CommandId;
		Option.CommandType = CommandType;
		Option.DisplayName = DisplayName;
		Option.bAvailable = bAvailable;
		Option.UnavailableReason = UnavailableReason;
		Option.SourceId = SourceId;
		Option.TargetId = TargetId;
		Option.Count = Count;
		OutPanel.Commands.Add(Option);
	};

	for (const FStationServiceEndpointDefinition& Endpoint : Context.ServiceEndpoints)
	{
		FString ServiceName = Endpoint.ServiceType.ToString();
		ServiceName.ReplaceInline(TEXT("_"), TEXT(" "));
		AddCommand(
			FName(*FString::Printf(TEXT("cmd_service_%s"), *Endpoint.ServiceEndpointId.ToString())),
			TEXT("service"),
			FText::FromString(ServiceName.ToUpper()),
			true,
			FString(),
			Endpoint.ServiceEndpointId,
			Endpoint.ServiceType);
	}

	for (const FStationMarketState& Market : Context.Markets)
	{
		AddCommand(
			FName(*FString::Printf(TEXT("cmd_market_%s"), *Market.MarketId.ToString())),
			TEXT("market"),
			FText::FromString(TEXT("COMMODITY DESK")),
			true,
			FString(),
			Market.MarketId,
			Market.MarketProfileId,
			Market.StockByCommodity.Num());
	}

	for (const FName GiverNpcId : Context.QuestGiverNpcIds)
	{
		FMissionContactInteractionView Interaction;
		const bool bHasInteraction = GetDockedMissionContactInteraction(GiverNpcId, Interaction);
		const FStationQuestGiverDefinition* QuestGiver = Context.QuestGivers.FindByPredicate([GiverNpcId](const FStationQuestGiverDefinition& Candidate)
		{
			return Candidate.NpcId == GiverNpcId;
		});
		const FText DisplayName = QuestGiver && !QuestGiver->DisplayName.IsEmpty()
			? QuestGiver->DisplayName
			: FText::FromString(GiverNpcId.ToString());
		const FName TargetId = !Interaction.OfferId.IsNone() ? Interaction.OfferId : Interaction.MissionInstanceId;
		AddCommand(
			FName(*FString::Printf(TEXT("cmd_contact_%s"), *GiverNpcId.ToString())),
			TEXT("mission_contact"),
			DisplayName,
			bHasInteraction && Interaction.bHasMission,
			bHasInteraction ? FString() : TEXT("No available mission interaction at this station."),
			GiverNpcId,
			TargetId);
	}

	const bool bHasInventoryOrEquipment = OutPanel.PersonalInventorySlotsMax > 0 || !SystemicGameplayState.EquipmentSlots.IsEmpty();
	AddCommand(
		TEXT("cmd_inventory_equipment"),
		TEXT("inventory_equipment"),
		FText::FromString(TEXT("INVENTORY / EQUIPMENT")),
		bHasInventoryOrEquipment,
		bHasInventoryOrEquipment ? FString() : TEXT("No personal inventory or equipment slots are available."),
		TEXT("player_personal_inventory"),
		TEXT("player"),
		OutPanel.PersonalInventorySlotsUsed);

	AddCommand(
		TEXT("cmd_enter_interior"),
		TEXT("enter_interior"),
		FText::FromString(TEXT("ENTER STATION")),
		!IsInStationInterior(),
		IsInStationInterior() ? TEXT("Already inside station interior.") : FString(),
		Context.StationId,
		Context.PortId,
		Context.QuestGiverNpcIds.Num());

	AddCommand(
		TEXT("cmd_undock"),
		TEXT("undock"),
		FText::FromString(TEXT("LAUNCH / UNDOCK")),
		true,
		FString(),
		Context.StationId,
		Context.PortId);

	const FString StationName = Context.DisplayName.IsEmpty() ? Context.StationId.ToString() : Context.DisplayName.ToString();
	const FString OwnerName = Context.OwnerFactionDisplayName.IsEmpty() ? Context.OwnerFactionId.ToString() : Context.OwnerFactionDisplayName.ToString();
	OutPanel.SummaryText = FString::Printf(
		TEXT("%s / %s | Owner %s | Credits %lld | Hull %.0f/%.0f | Shields %.0f/%.0f | Cargo stacks %d | Inventory %d/%d | Services %d Markets %d Missions %d Contacts %d"),
		*StationName,
		*Context.PortId.ToString(),
		*OwnerName,
		static_cast<long long>(OutPanel.PlayerCredits),
		OutPanel.Hull,
		OutPanel.MaxHull,
		OutPanel.Shields,
		OutPanel.MaxShields,
		OutPanel.ShipCargoStacks,
		OutPanel.PersonalInventorySlotsUsed,
		OutPanel.PersonalInventorySlotsMax,
		OutPanel.AvailableServiceCount,
		OutPanel.AvailableMarketCount,
		OutPanel.AvailableMissionCount,
		OutPanel.AvailableContactCount);
	OutPanel.DebugReason = Context.DebugReason;
	return true;
}

bool UStargameSessionSubsystem::GetDockedMarketView(FDockedMarketView& OutMarket) const
{
	OutMarket = FDockedMarketView();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		OutMarket.DebugReason = Context.DebugReason;
		return false;
	}

	if (Context.Markets.IsEmpty())
	{
		OutMarket.StationId = Context.StationId;
		OutMarket.DebugReason = TEXT("Docked station has no market.");
		return false;
	}

	const FStationMarketState& Market = Context.Markets[0];
	const FStationServiceEndpointDefinition* MarketEndpoint = Context.ServiceEndpoints.FindByPredicate([&Market](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.MarketId == Market.MarketId && Endpoint.ServiceType == FName(TEXT("market"));
	});
	if (!MarketEndpoint)
	{
		MarketEndpoint = Context.ServiceEndpoints.FindByPredicate([&Market](const FStationServiceEndpointDefinition& Endpoint)
		{
			return Endpoint.MarketId == Market.MarketId;
		});
	}

	OutMarket.bAvailable = true;
	OutMarket.StationId = Context.StationId;
	OutMarket.MarketId = Market.MarketId;
	OutMarket.MarketProfileId = Market.MarketProfileId;
	OutMarket.SourceContainerId = MarketEndpoint ? MarketEndpoint->InventoryContainerId : NAME_None;
	OutMarket.MarketCreditAccountId = MarketEndpoint ? MarketEndpoint->CreditAccountId : NAME_None;
	OutMarket.LegalContextId = MarketEndpoint ? MarketEndpoint->LegalPolicyId : NAME_None;

	if (const FCreditAccountRecord* PlayerAccount = SystemicGameplayState.CreditAccounts.FindByPredicate([](const FCreditAccountRecord& Account)
	{
		return Account.AccountId == FName(TEXT("account_player"));
	}))
	{
		OutMarket.PlayerCredits = PlayerAccount->AvailableBalance;
	}

	const FContainerState* PlayerCargo = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_ship_cargo"));
	});
	const FContainerState* PlayerInventory = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_personal_inventory"));
	});
	OutMarket.ShipCargoStacks = PlayerCargo ? PlayerCargo->Stacks.Num() : 0;
	OutMarket.PersonalInventorySlotsUsed = PlayerInventory ? PlayerInventory->Stacks.Num() : 0;
	OutMarket.PersonalInventorySlotsMax = PlayerInventory ? PlayerInventory->MaxSlotCount : 0;

	auto CountContainerItem = [](const FContainerState* Container, FName ItemId)
	{
		int32 Count = 0;
		if (!Container)
		{
			return Count;
		}
		for (const FItemStackState& Stack : Container->Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				Count += Stack.Quantity;
			}
		}
		return Count;
	};

	auto IsRestrictedName = [](FName Value)
	{
		const FString Text = Value.ToString().ToLower();
		return Text.Contains(TEXT("contraband")) || Text.Contains(TEXT("restricted")) || Text.Contains(TEXT("illegal"));
	};

	auto LocalMarketState = [](double FillRatio)
	{
		if (FillRatio <= 0.18)
		{
			return FName(TEXT("scarce"));
		}
		if (FillRatio <= 0.42)
		{
			return FName(TEXT("tight"));
		}
		if (FillRatio >= 0.82)
		{
			return FName(TEXT("glut"));
		}
		if (FillRatio >= 0.62)
		{
			return FName(TEXT("surplus"));
		}
		return FName(TEXT("steady"));
	};

	for (const TPair<FName, int32>& StockPair : Market.StockByCommodity)
	{
		const FName CommodityId = StockPair.Key;
		const FCommodityDefinition* Commodity = SystemicGameplayState.Commodities.FindByPredicate([CommodityId](const FCommodityDefinition& Candidate)
		{
			return Candidate.CommodityId == CommodityId;
		});
		const FCommodityItemBridge* Bridge = SystemicGameplayState.CommodityItemBridges.FindByPredicate([CommodityId](const FCommodityItemBridge& Candidate)
		{
			return Candidate.CommodityId == CommodityId && Candidate.bMarketTradable;
		});
		if (!Commodity || !Bridge)
		{
			continue;
		}

		const FItemDefinition* Item = SystemicGameplayState.Items.FindByPredicate([Bridge](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == Bridge->ItemId;
		});
		const int32 ReservedStock = Market.ReservedStockByCommodity.FindRef(CommodityId);
		const int32 AuthoredMaxStock = Market.MaxStockByCommodity.FindRef(CommodityId);
		const int32 MaxStock = FMath::Max(AuthoredMaxStock, FMath::Max(StockPair.Value + ReservedStock, 1));
		const double FillRatio = FMath::Clamp(static_cast<double>(StockPair.Value) / static_cast<double>(MaxStock), 0.0, 1.0);
		const double RawUnitPrice = static_cast<double>(Commodity->BasePrice) * (2.0 - FillRatio * 1.5);
		const int64 UnitPrice = FMath::Max<int64>(1, static_cast<int64>(FMath::RoundToDouble(RawUnitPrice)));
		const int32 PlayerCargoQuantity = CountContainerItem(PlayerCargo, Bridge->ItemId);
		const int32 PlayerInventoryQuantity = CountContainerItem(PlayerInventory, Bridge->ItemId);
		const bool bRestricted = IsRestrictedName(Commodity->Category) || (Item && IsRestrictedName(Item->ItemType));

		FDockedMarketCommodityView Quote;
		Quote.CommodityId = CommodityId;
		Quote.CommodityItemBridgeId = Bridge->CommodityItemBridgeId;
		Quote.ItemId = Bridge->ItemId;
		Quote.DisplayName = Commodity->DisplayName.IsEmpty() ? FText::FromString(CommodityId.ToString()) : Commodity->DisplayName;
		Quote.Category = Commodity->Category;
		Quote.Stock = StockPair.Value;
		Quote.ReservedStock = ReservedStock;
		Quote.MaxStock = MaxStock;
		Quote.FillRatio = FillRatio;
		Quote.UnitPrice = UnitPrice;
		Quote.LocalMarketState = LocalMarketState(FillRatio);
		Quote.PlayerCargoQuantity = PlayerCargoQuantity;
		Quote.PlayerPersonalInventoryQuantity = PlayerInventoryQuantity;
		Quote.bRestricted = bRestricted;
		Quote.bCargoHoldStorable = Bridge->bCargoHoldStorable;
		Quote.BuyDestinationContainerId = Bridge->bCargoHoldStorable ? FName(TEXT("player_ship_cargo")) : FName(TEXT("player_personal_inventory"));
		Quote.bCanBuy = StockPair.Value > 0 && UnitPrice <= OutMarket.PlayerCredits && (!bRestricted || !OutMarket.LegalContextId.IsNone());
		if (!Quote.bCanBuy)
		{
			Quote.BuyUnavailableReason = StockPair.Value <= 0
				? TEXT("Market stock is unavailable.")
				: UnitPrice > OutMarket.PlayerCredits
					? TEXT("Not enough credits for one unit.")
					: TEXT("Restricted commodity requires a legal market context.");
		}
		const int32 PlayerHolding = Bridge->bCargoHoldStorable ? PlayerCargoQuantity : PlayerInventoryQuantity;
		Quote.bCanSell = PlayerHolding > 0;
		if (!Quote.bCanSell)
		{
			Quote.SellUnavailableReason = Bridge->bCargoHoldStorable ? TEXT("No matching cargo in ship hold.") : TEXT("No matching item in personal inventory.");
		}
		OutMarket.Commodities.Add(Quote);
	}

	OutMarket.Commodities.Sort([](const FDockedMarketCommodityView& Left, const FDockedMarketCommodityView& Right)
	{
		return Left.DisplayName.ToString() < Right.DisplayName.ToString();
	});
	OutMarket.SummaryText = FString::Printf(
		TEXT("Market %s | Credits %lld | Cargo stacks %d | Inventory %d/%d | Commodities %d"),
		*OutMarket.MarketId.ToString(),
		static_cast<long long>(OutMarket.PlayerCredits),
		OutMarket.ShipCargoStacks,
		OutMarket.PersonalInventorySlotsUsed,
		OutMarket.PersonalInventorySlotsMax,
		OutMarket.Commodities.Num());
	OutMarket.DebugReason = FString::Printf(TEXT("Docked market '%s' resolved from Godot-style stock, price, cargo, and inventory state."), *OutMarket.MarketId.ToString());
	return true;
}

bool UStargameSessionSubsystem::GetDockedMissionContactPanel(FDockedMissionContactPanelView& OutPanel) const
{
	OutPanel = FDockedMissionContactPanelView();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		OutPanel.DebugReason = Context.DebugReason;
		return false;
	}

	OutPanel.bAvailable = true;
	OutPanel.StationId = Context.StationId;
	OutPanel.StationDisplayName = Context.DisplayName;
	OutPanel.ContactCount = Context.QuestGiverNpcIds.Num();

	for (const FName GiverNpcId : Context.QuestGiverNpcIds)
	{
		FDockedMissionContactOption Contact;
		Contact.GiverNpcId = GiverNpcId;

		if (const FStationQuestGiverDefinition* QuestGiver = Context.QuestGivers.FindByPredicate([GiverNpcId](const FStationQuestGiverDefinition& Candidate)
		{
			return Candidate.NpcId == GiverNpcId;
		}))
		{
			Contact.DisplayName = QuestGiver->DisplayName.IsEmpty()
				? FText::FromString(GiverNpcId.ToString())
				: QuestGiver->DisplayName;
		}
		else
		{
			Contact.DisplayName = FText::FromString(GiverNpcId.ToString());
		}

		FMissionContactInteractionView Interaction;
		if (GetDockedMissionContactInteraction(GiverNpcId, Interaction))
		{
			Contact.bHasMission = Interaction.bHasMission;
			Contact.OfferId = Interaction.OfferId;
			Contact.MissionInstanceId = Interaction.MissionInstanceId;
			Contact.MissionDefinitionId = Interaction.MissionDefinitionId;
			Contact.MissionTitle = Interaction.MissionTitle;
			Contact.InteractionState = Interaction.InteractionState;
			Contact.AvailableCommand = Interaction.AvailableCommand;
			Contact.IssuerFactionId = Interaction.IssuerFactionId;
			Contact.IssuerFactionDisplayName = Interaction.IssuerFactionDisplayName;
			Contact.RegionName = Interaction.RegionName;
			Contact.ActiveObjectiveStateId = Interaction.ActiveObjectiveStateId;
			Contact.ActiveObjectiveType = Interaction.ActiveObjectiveType;
			Contact.ActiveObjectiveTargetId = Interaction.ActiveObjectiveTargetId;
			Contact.ContextText = Interaction.ContextText;
			Contact.DialogText = Interaction.DialogText;
			Contact.ObjectiveSummaryText = Interaction.ObjectiveSummaryText;
			Contact.CargoManifestSummaryText = Interaction.CargoManifestSummaryText;
			Contact.RewardCredits = Interaction.RewardCredits;
			Contact.RewardText = Interaction.RewardText;
			Contact.DebugReason = Interaction.DebugReason;
		}
		else
		{
			Contact.DebugReason = Interaction.DebugReason;
		}

		Contact.bCanAccept = Contact.bHasMission && Contact.AvailableCommand == FName(TEXT("accept"));
		Contact.bCanContinue = Contact.bHasMission && Contact.AvailableCommand == FName(TEXT("continue"));
		Contact.bCanTurnIn = Contact.bHasMission && Contact.AvailableCommand == FName(TEXT("turn_in"));
		if (Contact.bCanAccept)
		{
			++OutPanel.AvailableMissionCount;
		}
		if (Contact.bCanContinue)
		{
			++OutPanel.ActiveMissionCount;
		}
		if (Contact.bCanTurnIn)
		{
			++OutPanel.TurnInCount;
		}

		OutPanel.Contacts.Add(Contact);
	}

	OutPanel.Contacts.Sort([](const FDockedMissionContactOption& Left, const FDockedMissionContactOption& Right)
	{
		if (Left.bCanTurnIn != Right.bCanTurnIn)
		{
			return Left.bCanTurnIn;
		}
		if (Left.bCanAccept != Right.bCanAccept)
		{
			return Left.bCanAccept;
		}
		if (Left.bCanContinue != Right.bCanContinue)
		{
			return Left.bCanContinue;
		}
		return Left.DisplayName.ToString() < Right.DisplayName.ToString();
	});

	const FString StationName = Context.DisplayName.IsEmpty() ? Context.StationId.ToString() : Context.DisplayName.ToString();
	OutPanel.SummaryText = FString::Printf(
		TEXT("%s mission contacts | Contacts %d Offers %d Active %d Turn-in %d"),
		*StationName,
		OutPanel.ContactCount,
		OutPanel.AvailableMissionCount,
		OutPanel.ActiveMissionCount,
		OutPanel.TurnInCount);
	OutPanel.DebugReason = TEXT("Docked mission contact panel resolved from station-authored quest givers and mission interaction state.");
	return true;
}

bool UStargameSessionSubsystem::GetDockedInventoryEquipmentPanel(FDockedInventoryEquipmentPanelView& OutPanel) const
{
	OutPanel = FDockedInventoryEquipmentPanelView();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		OutPanel.DebugReason = Context.DebugReason;
		return false;
	}

	OutPanel.bAvailable = true;
	OutPanel.StationId = Context.StationId;

	auto FindItem = [this](FName ItemId) -> const FItemDefinition*
	{
		return SystemicGameplayState.Items.FindByPredicate([ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		});
	};

	auto BuildStackView = [this, &FindItem](FName ContainerId, const FItemStackState& Stack)
	{
		FDockedInventoryStackView View;
		View.ContainerId = ContainerId;
		View.StackId = Stack.StackId;
		View.ItemId = Stack.ItemId;
		View.Quantity = Stack.Quantity;

		const FItemDefinition* Item = FindItem(Stack.ItemId);
		if (Item)
		{
			View.DisplayName = Item->DisplayName.IsEmpty() ? FText::FromString(Stack.ItemId.ToString()) : Item->DisplayName;
			View.ItemType = Item->ItemType;
			View.StackLimit = Item->StackLimit;
			View.BaseValue = Item->BaseValue;
			View.TotalMassKg = Item->MassPerUnitKg * static_cast<double>(Stack.Quantity);
			View.TotalVolumeM3 = Item->VolumePerUnitM3 * static_cast<double>(Stack.Quantity);
			View.bStackable = Item->StackLimit > 1;
		}
		else
		{
			View.DisplayName = FText::FromString(Stack.ItemId.ToString());
		}

		for (const FEquipmentSlotState& Slot : SystemicGameplayState.EquipmentSlots)
		{
			if (!View.ItemType.IsNone() && Slot.AcceptedItemTypes.Contains(View.ItemType))
			{
				View.CompatibleSlotIds.Add(Slot.SlotId);
			}
		}
		View.bEquippable = !View.CompatibleSlotIds.IsEmpty();
		return View;
	};

	auto AddContainerStacks = [&BuildStackView](const FContainerState* Container, TArray<FDockedInventoryStackView>& OutStacks, double& OutMassKg)
	{
		if (!Container)
		{
			return;
		}
		for (const FItemStackState& Stack : Container->Stacks)
		{
			FDockedInventoryStackView View = BuildStackView(Container->ContainerId, Stack);
			OutMassKg += View.TotalMassKg;
			OutStacks.Add(View);
		}
		OutStacks.Sort([](const FDockedInventoryStackView& Left, const FDockedInventoryStackView& Right)
		{
			if (Left.bEquippable != Right.bEquippable)
			{
				return Left.bEquippable;
			}
			return Left.DisplayName.ToString() < Right.DisplayName.ToString();
		});
	};

	auto BuildSlotView = [&FindItem](const FEquipmentSlotState& Slot)
	{
		FDockedEquipmentSlotView View;
		View.SlotId = Slot.SlotId;
		View.OwnerId = Slot.OwnerId;
		View.SlotType = Slot.SlotType;
		View.AcceptedItemTypes = Slot.AcceptedItemTypes;
		View.EquippedStackId = Slot.EquippedStack.StackId;
		View.EquippedItemId = Slot.EquippedStack.ItemId;
		View.EquippedQuantity = Slot.EquippedStack.Quantity;
		View.bOccupied = !Slot.EquippedStack.ItemId.IsNone() && Slot.EquippedStack.Quantity > 0;
		if (const FItemDefinition* Item = FindItem(Slot.EquippedStack.ItemId))
		{
			View.EquippedDisplayName = Item->DisplayName.IsEmpty() ? FText::FromString(Slot.EquippedStack.ItemId.ToString()) : Item->DisplayName;
			View.EquippedItemType = Item->ItemType;
		}
		return View;
	};

	const FContainerState* PersonalInventory = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_personal_inventory"));
	});
	const FContainerState* ShipCargo = SystemicGameplayState.Containers.FindByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("player_ship_cargo"));
	});

	if (PersonalInventory)
	{
		OutPanel.PersonalInventoryContainerId = PersonalInventory->ContainerId;
		OutPanel.PersonalInventorySlotsUsed = PersonalInventory->Stacks.Num();
		OutPanel.PersonalInventorySlotsMax = PersonalInventory->MaxSlotCount;
		AddContainerStacks(PersonalInventory, OutPanel.PersonalInventory, OutPanel.PersonalInventoryMassKg);
	}
	if (ShipCargo)
	{
		OutPanel.ShipCargoContainerId = ShipCargo->ContainerId;
		OutPanel.ShipCargoStacks = ShipCargo->Stacks.Num();
		AddContainerStacks(ShipCargo, OutPanel.ShipCargo, OutPanel.ShipCargoMassKg);
	}

	for (const FEquipmentSlotState& Slot : SystemicGameplayState.EquipmentSlots)
	{
		if (Slot.OwnerId == FName(TEXT("player")))
		{
			OutPanel.PlayerLoadout.Add(BuildSlotView(Slot));
		}
		else if (Slot.OwnerId == FName(TEXT("player_ship")))
		{
			OutPanel.ShipLoadout.Add(BuildSlotView(Slot));
		}
	}
	auto SortSlots = [](TArray<FDockedEquipmentSlotView>& Slots)
	{
		Slots.Sort([](const FDockedEquipmentSlotView& Left, const FDockedEquipmentSlotView& Right)
		{
			return Left.SlotId.LexicalLess(Right.SlotId);
		});
	};
	SortSlots(OutPanel.PlayerLoadout);
	SortSlots(OutPanel.ShipLoadout);

	OutPanel.SummaryText = FString::Printf(
		TEXT("Inventory %d/%d | Cargo stacks %d | Player slots %d | Ship slots %d"),
		OutPanel.PersonalInventorySlotsUsed,
		OutPanel.PersonalInventorySlotsMax,
		OutPanel.ShipCargoStacks,
		OutPanel.PlayerLoadout.Num(),
		OutPanel.ShipLoadout.Num());
	OutPanel.DebugReason = TEXT("Docked inventory and equipment panel resolved from Godot item category, stack, and equipment slot intent.");
	return true;
}

bool UStargameSessionSubsystem::EquipDockedInventoryItem(FName ContainerId, FName StackId, FName SlotId, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = FName(*FString::Printf(TEXT("equip_%s_to_%s"), *StackId.ToString(), *SlotId.ToString()));

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	const FContainerState* Container = SystemicGameplayState.Containers.FindByPredicate([ContainerId](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == ContainerId;
	});
	const FEquipmentSlotState* Slot = SystemicGameplayState.EquipmentSlots.FindByPredicate([SlotId](const FEquipmentSlotState& Candidate)
	{
		return Candidate.SlotId == SlotId;
	});
	if (!Container || Container->OwnerId != FName(TEXT("player")) || Container->ContainerId != FName(TEXT("player_personal_inventory")))
	{
		return RejectDockedStationCommand(TEXT("Docked equipment changes must use the player's personal inventory."), OutCommandResult);
	}
	if (!Slot || (Slot->OwnerId != FName(TEXT("player")) && Slot->OwnerId != FName(TEXT("player_ship"))))
	{
		return RejectDockedStationCommand(TEXT("Docked equipment changes can only target player or player ship slots."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::EquipItemFromContainer(SystemicGameplayState, ContainerId, StackId, SlotId, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	RefreshPlayerShipEquipmentEffects();
	SaveDockedAutosave(TEXT("equipment change"));
	return true;
}

bool UStargameSessionSubsystem::UnequipDockedInventoryItem(FName SlotId, FName ContainerId, FName StackIdPrefix, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = FName(*FString::Printf(TEXT("unequip_%s"), *SlotId.ToString()));

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	const FContainerState* Container = SystemicGameplayState.Containers.FindByPredicate([ContainerId](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == ContainerId;
	});
	const FEquipmentSlotState* Slot = SystemicGameplayState.EquipmentSlots.FindByPredicate([SlotId](const FEquipmentSlotState& Candidate)
	{
		return Candidate.SlotId == SlotId;
	});
	if (!Container || Container->OwnerId != FName(TEXT("player")) || Container->ContainerId != FName(TEXT("player_personal_inventory")))
	{
		return RejectDockedStationCommand(TEXT("Docked equipment changes must return equipment to the player's personal inventory."), OutCommandResult);
	}
	if (!Slot || (Slot->OwnerId != FName(TEXT("player")) && Slot->OwnerId != FName(TEXT("player_ship"))))
	{
		return RejectDockedStationCommand(TEXT("Docked equipment changes can only target player or player ship slots."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::UnequipItemToContainer(SystemicGameplayState, SlotId, ContainerId, StackIdPrefix, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	RefreshPlayerShipEquipmentEffects();
	SaveDockedAutosave(TEXT("equipment change"));
	return true;
}

bool UStargameSessionSubsystem::ExecuteDockedStationService(const FStationServiceRequest& Request, FStationServiceResultRecord& OutResult, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = Request.RequestId;
	OutResult = FStationServiceResultRecord();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	const FStationServiceEndpointDefinition* Endpoint = Context.ServiceEndpoints.FindByPredicate([&Request](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == Request.ServiceEndpointId && Candidate.ServiceType == Request.ServiceType;
	});
	if (!Endpoint)
	{
		return RejectDockedStationCommand(TEXT("Requested service endpoint is not available at the docked station."), OutCommandResult);
	}
	if (!Endpoint->CreditAccountId.IsNone() && Endpoint->CreditAccountId != Request.CreditAccountId)
	{
		return RejectDockedStationCommand(TEXT("Requested service credit account does not match the docked station endpoint."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::ExecuteStationServiceRequestOnce(SystemicGameplayState, Request, OutResult, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	SaveDockedAutosave(TEXT("station service"));
	return true;
}

bool UStargameSessionSubsystem::ExecuteDockedMarketTransaction(const FMarketTransactionRequest& Request, FMarketTransactionResult& OutResult, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = Request.TransactionId;
	OutResult = FMarketTransactionResult();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	if (!Context.Markets.ContainsByPredicate([&Request](const FStationMarketState& Candidate)
	{
		return Candidate.MarketId == Request.MarketId;
	}))
	{
		return RejectDockedStationCommand(TEXT("Requested market is not available at the docked station."), OutCommandResult);
	}
	const bool bPlayerBuy = Request.BuyerId == FName(TEXT("player")) && Request.DebitAccountId == FName(TEXT("account_player"));
	const bool bPlayerSell = Request.SellerId == FName(TEXT("player")) && Request.CreditAccountId == FName(TEXT("account_player"));
	if (!bPlayerBuy && !bPlayerSell)
	{
		return RejectDockedStationCommand(TEXT("Docked market transactions must buy from or sell to the player account through the station facade."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::ExecuteProgressionMarketTransactionOnce(SystemicGameplayState, Request, OutResult, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	SaveDockedAutosave(TEXT("market transaction"));
	return true;
}

bool UStargameSessionSubsystem::AcceptDockedMissionOffer(FName OfferId, FName PlayerId, FName IdempotencyKey, FMissionInstanceState& OutMission, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = OfferId;
	OutMission = FMissionInstanceState();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	const FMissionOfferRecord* Offer = Context.MissionOffers.FindByPredicate([OfferId](const FMissionOfferRecord& Candidate)
	{
		return Candidate.OfferId == OfferId;
	});
	if (!Offer)
	{
		return RejectDockedStationCommand(TEXT("Requested mission offer is not available at the docked station."), OutCommandResult);
	}
	const bool bMissionBoardAvailable = Context.ServiceEndpoints.ContainsByPredicate([Offer](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.ServiceEndpointId == Offer->SourceServiceEndpointId &&
			Endpoint.ServiceType == FName(TEXT("mission_board")) &&
			Endpoint.StationId == Offer->SourceStationId;
	});
	if (!bMissionBoardAvailable)
	{
		return RejectDockedStationCommand(TEXT("Mission offer source service endpoint is not available at the docked station."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::AcceptMissionOfferOnce(SystemicGameplayState, OfferId, PlayerId, IdempotencyKey, OutMission, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	AutoSelectMissionWaypoint(OutMission);
	OutCommandResult.bAccepted = true;
	SaveDockedAutosave(TEXT("mission accepted"));
	return true;
}

bool UStargameSessionSubsystem::GetDockedMissionContactInteraction(FName GiverNpcId, FMissionContactInteractionView& OutInteraction) const
{
	OutInteraction = FMissionContactInteractionView();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		OutInteraction.DebugReason = Context.DebugReason;
		return false;
	}

	UWorld* World = ResolveSessionWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || StarSystem->GetActiveSystemId() != CurrentSystemId)
	{
		OutInteraction.DebugReason = TEXT("Docked mission contact lookup requires the active star system.");
		return false;
	}

	return USystemicGameplayQueryService::ResolveMissionContactInteraction(
		StarSystem->GetActiveSystemDefinition(),
		SystemicGameplayState,
		Context.StationId,
		GiverNpcId,
		OutInteraction);
}

bool UStargameSessionSubsystem::CompleteDockedMission(FName MissionInstanceId, FName SourceEventId, FName IdempotencyKey, FProgressionDebugLedgerEntry& OutCompletionEntry, FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = MissionInstanceId;
	OutCompletionEntry = FProgressionDebugLedgerEntry();

	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	const FMissionInstanceState* Mission = SystemicGameplayState.MissionInstances.FindByPredicate([MissionInstanceId](const FMissionInstanceState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	const FMissionOfferRecord* Offer = Mission ? SystemicGameplayState.MissionOffers.FindByPredicate([Mission](const FMissionOfferRecord& Candidate)
	{
		return Candidate.OfferId == Mission->OfferId;
	}) : nullptr;
	if (!Mission || !Offer || Offer->SourceStationId != Context.StationId)
	{
		return RejectDockedStationCommand(TEXT("Mission turn-in is not available at the docked station."), OutCommandResult);
	}
	if (!IsMissionReadyForTurnIn(MissionInstanceId))
	{
		return RejectDockedStationCommand(TEXT("Mission objectives are not ready for turn-in."), OutCommandResult);
	}

	FString FailureReason;
	if (!USystemicGameplayQueryService::CompleteMissionOnce(SystemicGameplayState, MissionInstanceId, SourceEventId, IdempotencyKey, OutCompletionEntry, FailureReason))
	{
		return RejectDockedStationCommand(FailureReason, OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	SaveDockedAutosave(TEXT("mission completed"));
	return true;
}

bool UStargameSessionSubsystem::RequestDockedUndock(FDockedStationCommandResult& OutCommandResult)
{
	if (ActiveStationInteriorPawn || StationInteriorSpacePawn)
	{
		return ExitStationInteriorAndUndock(OutCommandResult);
	}

	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = TEXT("undock");

	ASpaceFlightPawn* FlightPawn = nullptr;
	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context, &FlightPawn))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;
	if (!FlightPawn || !FlightPawn->Undock())
	{
		return RejectDockedStationCommand(TEXT("Space flight pawn rejected undock."), OutCommandResult);
	}

	OutCommandResult.bAccepted = true;
	return true;
}

bool UStargameSessionSubsystem::EnterDockedStationInterior(FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = TEXT("enter_station_interior");

	if (ActiveStationInteriorPawn)
	{
		OutCommandResult.bAccepted = true;
		if (StationInteriorSpacePawn)
		{
			FDockedStationContext ExistingContext;
			ResolveCurrentDockedStation(ExistingContext);
			OutCommandResult.StationId = ExistingContext.StationId;
		}
		return true;
	}

	ASpaceFlightPawn* FlightPawn = nullptr;
	FDockedStationContext Context;
	if (!ResolveCurrentDockedStation(Context, &FlightPawn))
	{
		return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
	}
	OutCommandResult.StationId = Context.StationId;

	UWorld* World = ResolveSessionWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController && FlightPawn)
	{
		PlayerController = Cast<APlayerController>(FlightPawn->GetController());
	}
	if (!World || !PlayerController || !FlightPawn)
	{
		return RejectDockedStationCommand(TEXT("Station interior requires a world, player controller, and docked flight pawn."), OutCommandResult);
	}

	const FVector RoomLocation(0.0, 0.0, 500000.0);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* RoomClass = Context.InteriorRoomClass.LoadSynchronous();
	if (!RoomClass || !RoomClass->IsChildOf(AStationInteriorRoomActor::StaticClass()))
	{
		RoomClass = AStationInteriorRoomActor::StaticClass();
	}
	UClass* InteriorPawnClass = Context.InteriorPawnClass.LoadSynchronous();
	if (!InteriorPawnClass || !InteriorPawnClass->IsChildOf(AStationInteriorPawn::StaticClass()))
	{
		InteriorPawnClass = AStationInteriorPawn::StaticClass();
	}

	ActiveStationInteriorRoom = World->SpawnActor<AStationInteriorRoomActor>(RoomClass, FTransform(RoomLocation), SpawnParameters);
	if (!ActiveStationInteriorRoom)
	{
		return RejectDockedStationCommand(TEXT("Failed to spawn station interior room."), OutCommandResult);
	}
	const bool bHostileBoarding =
		Context.SecurityProfile == FName(TEXT("hostile_contested_space")) ||
		Context.StationRole == FName(TEXT("hostile_boarding")) ||
		Context.MissionTags.Contains(FName(TEXT("boarding"))) ||
		Context.MissionTags.Contains(FName(TEXT("clearance"))) ||
		Context.MissionTags.Contains(FName(TEXT("hostile_boarding")));
	ActiveStationInteriorRoom->ConfigureStationInterior(Context.StationId, Context.QuestGivers, bHostileBoarding, this);

	ActiveStationInteriorPawn = World->SpawnActor<AStationInteriorPawn>(InteriorPawnClass, ActiveStationInteriorRoom->GetPlayerStartTransform(), SpawnParameters);
	if (!ActiveStationInteriorPawn)
	{
		CleanupStationInterior();
		return RejectDockedStationCommand(TEXT("Failed to spawn station interior pawn."), OutCommandResult);
	}
	ActiveStationInteriorPawn->ConfigureStationInteriorPawn(this, ActiveStationInteriorRoom);
	ActiveStationInteriorPawn->SetHostileBoardingEnabled(bHostileBoarding);

	StationInteriorSpacePawn = FlightPawn;
	StationInteriorSpacePawn->SetActorHiddenInGame(true);
	StationInteriorSpacePawn->SetActorEnableCollision(false);
	PlayerController->Possess(ActiveStationInteriorPawn);

	OutCommandResult.bAccepted = true;
	return true;
}

bool UStargameSessionSubsystem::ResolveStationInteriorCombatProfile(FName ProfileId, FStationInteriorCombatProfileDefinition& OutProfile) const
{
	const FName RequestedProfileId = ProfileId.IsNone() ? FName(TEXT("profile_godot_hostile_boarding_basic")) : ProfileId;
	if (const FStationInteriorCombatProfileDefinition* Profile = SystemicGameplayState.StationInteriorCombatProfiles.FindByPredicate([RequestedProfileId](const FStationInteriorCombatProfileDefinition& Candidate)
	{
		return Candidate.ProfileId == RequestedProfileId;
	}))
	{
		OutProfile = *Profile;
		return true;
	}

	OutProfile = FStationInteriorCombatProfileDefinition();
	OutProfile.ProfileId = RequestedProfileId;
	OutProfile.DisplayName = FText::FromString(TEXT("Godot Hostile Boarding Basic"));
	OutProfile.PlayerMaxHealth = 100.0;
	OutProfile.PlayerWeaponDamage = 20.0;
	OutProfile.PlayerWeaponRangeCm = 6000.0;
	OutProfile.PlayerWeaponCooldownSeconds = 0.35;
	OutProfile.HostileMaxHealth = 45.0;
	OutProfile.HostileFireDamage = 8.0;
	OutProfile.HostileDetectionRangeCm = 3500.0;
	OutProfile.HostileFireRangeCm = 2800.0;
	OutProfile.HostileFireCooldownSeconds = 1.3;
	OutProfile.HostileInaccuracyDegrees = FMath::RadiansToDegrees(0.04);
	OutProfile.HostileInitialFireDelaySeconds = 0.4;
	OutProfile.HostileCount = 3;
	return false;
}

bool UStargameSessionSubsystem::GetStationInteriorCombatView(FStationInteriorCombatView& OutView) const
{
	OutView = FStationInteriorCombatView();
	AStationInteriorRoomActor* Room = ActiveStationInteriorRoom.Get();
	AStationInteriorPawn* Pawn = ActiveStationInteriorPawn.Get();
	if (!Room || !Pawn)
	{
		return false;
	}

	OutView.bInStationInterior = true;
	OutView.StationId = Room->GetStationId();
	OutView.CombatProfileId = Room->GetCombatProfileId();
	OutView.bHostileBoarding = Room->IsHostileBoarding();
	OutView.bHostilesCleared = Room->AreHostilesCleared();
	OutView.HostileCount = Room->GetHostileCount();
	OutView.LiveHostileCount = Room->GetLiveHostileCount();
	OutView.PlayerMaxHealth = Pawn->GetMaxOnFootHealth();
	OutView.PlayerHealth = Pawn->GetOnFootHealth();
	OutView.bPlayerAlive = Pawn->IsOnFootAlive();
	OutView.PlayerWeaponDamage = Pawn->GetOnFootWeaponDamage();
	OutView.PlayerWeaponRangeCm = Pawn->GetOnFootWeaponRangeCm();
	OutView.PlayerWeaponCooldownSeconds = Pawn->GetOnFootWeaponCooldownSeconds();
	OutView.PlayerWeaponCooldownRemainingSeconds = Pawn->GetOnFootWeaponCooldownRemainingSeconds();
	OutView.bPlayerWeaponReady = Pawn->IsOnFootWeaponReady();
	Room->BuildHostileCombatViews(OutView.Hostiles);
	return true;
}

bool UStargameSessionSubsystem::ExitStationInteriorAndUndock(FDockedStationCommandResult& OutCommandResult)
{
	OutCommandResult = FDockedStationCommandResult();
	OutCommandResult.CommandId = TEXT("undock");

	ASpaceFlightPawn* FlightPawn = StationInteriorSpacePawn.Get();
	if (!FlightPawn)
	{
		FDockedStationContext Context;
		if (!ResolveCurrentDockedStation(Context, &FlightPawn))
		{
			return RejectDockedStationCommand(Context.DebugReason, OutCommandResult);
		}
		OutCommandResult.StationId = Context.StationId;
	}
	else
	{
		const FDockingOperationState Docking = FlightPawn->GetDockingOperationState();
		OutCommandResult.StationId = Docking.StationId;
	}

	UWorld* World = ResolveSessionWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController && ActiveStationInteriorPawn)
	{
		PlayerController = Cast<APlayerController>(ActiveStationInteriorPawn->GetController());
	}
	if (!PlayerController && FlightPawn)
	{
		PlayerController = Cast<APlayerController>(FlightPawn->GetController());
	}
	if (!World || !PlayerController || !FlightPawn)
	{
		return RejectDockedStationCommand(TEXT("Station undock requires a world, player controller, and docked flight pawn."), OutCommandResult);
	}

	FlightPawn->SetActorHiddenInGame(false);
	FlightPawn->SetActorEnableCollision(true);
	PlayerController->Possess(FlightPawn);

	if (!FlightPawn->Undock())
	{
		if (ActiveStationInteriorPawn)
		{
			PlayerController->Possess(ActiveStationInteriorPawn);
		}
		FlightPawn->SetActorHiddenInGame(true);
		FlightPawn->SetActorEnableCollision(false);
		return RejectDockedStationCommand(TEXT("Space flight pawn rejected undock."), OutCommandResult);
	}

	if (ActiveStationInteriorPawn)
	{
		ActiveStationInteriorPawn->Destroy();
	}
	if (ActiveStationInteriorRoom)
	{
		ActiveStationInteriorRoom->Destroy();
	}
	ActiveStationInteriorPawn = nullptr;
	ActiveStationInteriorRoom = nullptr;
	StationInteriorSpacePawn = nullptr;

	OutCommandResult.bAccepted = true;
	return true;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot()
{
	FStargameM0SaveState LoadedState;
	if (!LoadDevelopmentSlot(LoadedState))
	{
		return false;
	}

	FString ValidationError;
	if (!ValidateLoadedSaveHeader(LoadedState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = ResolveCatalogSubsystem();
	bool bResolvedSystem = Catalog && Catalog->ResolveSystemDefinition(LoadedState.SystemId, SystemDefinition);
	if (!bResolvedSystem && Catalog && LoadedState.bSavedSystemIsGenerated)
	{
		FStarSystemDefinition RegeneratedSystem;
		if (Catalog->GenerateSeededPhysicalSystem(LoadedState.GeneratedSystemSourceMetadata.GeneratedSeed, RegeneratedSystem) &&
			RegeneratedSystem.SystemId == LoadedState.SystemId &&
			RegeneratedSystem.GeneratedSourceMetadata.GeneratorVersion == LoadedState.GeneratedSystemSourceMetadata.GeneratorVersion &&
			RegeneratedSystem.GeneratedSourceMetadata.SourceFingerprint == LoadedState.GeneratedSystemSourceMetadata.SourceFingerprint)
		{
			SystemDefinition = RegeneratedSystem;
			bResolvedSystem = true;
		}
	}
	if (!bResolvedSystem)
	{
		ReportStartupError(FString::Printf(TEXT("Saved system '%s' could not be resolved."), *LoadedState.SystemId.ToString()));
		return false;
	}

	if (!ValidateResolvedSystemDefinitionForRuntime(Catalog, SystemDefinition, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	FSimulationClockSnapshot RestoredClock = LoadedState.ClockSnapshot;
	if (RestoredClock.ClockOwner.IsNone())
	{
		ReportStartupError(TEXT("Saved clock snapshot is missing its owner."));
		return false;
	}
	FActiveTrafficSimulationState RestoredTrafficState = LoadedState.ActiveTrafficState;
	FSystemicGameplayState RestoredSystemicState;
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(SystemDefinition, RestoredClock, RestoredClock.AuthoritativeSimulationTimeSeconds, RestoredTrafficState);
	if (!ResolveRuntimeSystemicState(SystemDefinition, &LoadedState.SystemicGameplayState, RestoredSystemicState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}
	if (!ValidateLoadedTrafficState(SystemDefinition, RestoredTrafficState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	if (!SystemDefinition.SpawnZones.ContainsByPredicate([&LoadedState](const FSpawnZoneDefinition& SpawnZone)
	{
		return SpawnZone.SpawnZoneId == LoadedState.SpawnZoneId;
	}))
	{
		ReportStartupError(FString::Printf(TEXT("Saved spawn zone '%s' does not exist in system '%s'."),
			*LoadedState.SpawnZoneId.ToString(),
			*LoadedState.SystemId.ToString()));
		return false;
	}

	TSubclassOf<APawn> PawnClass = nullptr;
	FStartProfileDefinition StartProfile;
	const FName LoadStartProfileId = StartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: StartProfileId;
	const bool bResolvedLoadStartProfile = Catalog && Catalog->ResolveStartProfile(LoadStartProfileId, StartProfile);
	if (bResolvedLoadStartProfile)
	{
		FShipArchetypeDefinition Ship;
		if (Catalog && Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
		{
			PawnClass = Ship.PawnClass.LoadSynchronous();
		}
	}
	if (!PawnClass)
	{
		ReportStartupError(FString::Printf(TEXT("Saved session requires authored pawn class from start profile '%s'."), *LoadStartProfileId.ToString()));
		return false;
	}

	if (LoadedState.ShipLocation.LocationMode == EShipLocationMode::GateArrival)
	{
		FTransform PreflightArrivalTransform;
		FVector PreflightArrivalVelocityCmPerSec = FVector::ZeroVector;
		if (!ResolveGateArrivalLocation(SystemDefinition, LoadedState.ShipLocation, PreflightArrivalTransform, PreflightArrivalVelocityCmPerSec, ValidationError))
		{
			ReportStartupError(ValidationError);
			return false;
		}
	}
	else if (!ValidateSavedShipLocationAgainstSystem(SystemDefinition, LoadedState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	CleanupStationInterior();
	LastRuntimeEncounterView = FRuntimeEncounterViewModel();
	RuntimeEncounterStates.Reset();
	if (!StarSystem || !StarSystem->BuildSystem(SystemDefinition, RestoredClock.AuthoritativeSimulationTimeSeconds))
	{
		ReportStartupError(StarSystem ? StarSystem->GetLastBuildError() : TEXT("Missing star system subsystem."));
		ClearSessionState();
		return false;
	}
	if (!StarSystem->RefreshRegisteredEntityTransforms(RestoredClock.AuthoritativeSimulationTimeSeconds))
	{
		ReportStartupError(TEXT("Saved system could not refresh moving entity transforms at the restored simulation time."));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!StarSystem->SpawnPlayerAtSpawnZone(LoadedState.SpawnZoneId, PlayerController, PawnClass))
	{
		ReportStartupError(StarSystem->GetLastBuildError());
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	CurrentSystemId = LoadedState.SystemId;
	CurrentSpawnZoneId = LoadedState.SpawnZoneId;
	SelectedTargetId = LoadedState.SelectedTargetId;
	ClockSnapshot = RestoredClock;
	ActiveTrafficState = RestoredTrafficState;
	SystemicGameplayState = RestoredSystemicState;
	RefreshPlayerShipEquipmentEffects();
	StarSystem->RealizeSystemicEncounterActors(SystemicGameplayState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	bHasPendingGateArrival = LoadedState.ShipLocation.LocationMode == EShipLocationMode::GateArrival;
	PendingGateArrivalLocation = bHasPendingGateArrival ? LoadedState.ShipLocation : FShipSaveLocation();
	ActivePlayerPawnClass = PawnClass;

	FString RestoreError;
	if (!RestoreSavedShipLocation(LoadedState.ShipLocation, PlayerController, RestoreError))
	{
		ReportStartupError(RestoreError);
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}
	if (const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController))
	{
		ULogicalTrafficQueryService::ApplyPlayerRelevanceRealization(
			SystemDefinition,
			SystemicGameplayState,
			TEXT("actor_budget_m11_low"),
			FlightPawn->GetLogicalSystemPositionCm(),
			ClockSnapshot,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds,
			ActiveTrafficState);
	}
	StarSystem->RealizeTrafficActors(ActiveTrafficState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);

	return true;
}

bool UStargameSessionSubsystem::RequestGateTransition(const FGateTransitionRequest& Request, FGateTransitionResult& OutResult)
{
	OutResult = FGateTransitionResult();
	OutResult.SourceSystemId = CurrentSystemId;
	OutResult.SourceGateId = Request.SourceGateId;

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || StarSystem->GetActiveSystemId() != CurrentSystemId || CurrentSystemId.IsNone())
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingActiveSystem;
		OutResult.FailureReason = TEXT("Gate transition requires an active source system.");
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	const FStarSystemDefinition SourceSystemDefinition = StarSystem->GetActiveSystemDefinition();
	const FGateDefinition* SourceGate = SourceSystemDefinition.Gates.FindByPredicate([&Request](const FGateDefinition& Gate)
	{
		return Gate.GateId == Request.SourceGateId;
	});
	if (!SourceGate)
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingSourceGate;
		OutResult.FailureReason = FString::Printf(TEXT("Source gate '%s' is not registered in the active system."), *Request.SourceGateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	OutResult.DestinationSystemId = SourceGate->DestinationSystemId;
	OutResult.DestinationGateId = SourceGate->DestinationGateId;
	OutResult.DestinationArrivalId = SourceGate->DestinationArrivalId;
	OutResult.ArrivalFrame = SourceGate->DestinationArrivalFrame;
	if (SourceGate->DestinationSystemId.IsNone())
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationSystem;
		OutResult.FailureReason = FString::Printf(TEXT("Source gate '%s' has no destination system."), *SourceGate->GateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}
	if (SourceGate->DestinationGateId.IsNone())
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationGate;
		OutResult.FailureReason = FString::Printf(TEXT("Source gate '%s' has no destination gate."), *SourceGate->GateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}
	if (SourceGate->DestinationArrivalId.IsNone())
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationArrival;
		OutResult.FailureReason = FString::Printf(TEXT("Source gate '%s' has no destination arrival target."), *SourceGate->GateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}
	if (SourceGate->DestinationArrivalFrame != FName(TEXT("gate_relative")))
	{
		OutResult.ResultCode = EGateTransitionResultCode::InvalidArrivalFrame;
		OutResult.FailureReason = FString::Printf(TEXT("Source gate '%s' uses unsupported arrival frame '%s'."),
			*SourceGate->GateId.ToString(),
			*SourceGate->DestinationArrivalFrame.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	FStarSystemDefinition DestinationSystemDefinition;
	UStarCatalogSubsystem* Catalog = ResolveCatalogSubsystem();
	const bool bResolvedDestinationSystem = Catalog && Catalog->ResolveSystemDefinition(SourceGate->DestinationSystemId, DestinationSystemDefinition);
	if (!bResolvedDestinationSystem)
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationSystem;
		OutResult.FailureReason = FString::Printf(TEXT("Destination system '%s' could not be resolved for source gate '%s'."),
			*SourceGate->DestinationSystemId.ToString(),
			*SourceGate->GateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	const FGateDefinition* DestinationGate = DestinationSystemDefinition.Gates.FindByPredicate([SourceGate](const FGateDefinition& Gate)
	{
		return Gate.GateId == SourceGate->DestinationGateId;
	});
	if (!DestinationGate)
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationGate;
		OutResult.FailureReason = FString::Printf(TEXT("Destination gate '%s' could not be resolved in system '%s'."),
			*SourceGate->DestinationGateId.ToString(),
			*DestinationSystemDefinition.SystemId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	const FSpawnZoneDefinition* Arrival = DestinationSystemDefinition.SpawnZones.FindByPredicate([SourceGate](const FSpawnZoneDefinition& SpawnZone)
	{
		return SpawnZone.SpawnZoneId == SourceGate->DestinationArrivalId;
	});
	if (!Arrival || Arrival->FrameType != SourceGate->DestinationArrivalFrame || Arrival->AnchorId != SourceGate->DestinationGateId)
	{
		OutResult.ResultCode = EGateTransitionResultCode::MissingDestinationArrival;
		OutResult.FailureReason = FString::Printf(TEXT("Destination arrival '%s' could not be resolved in gate-relative frame '%s'."),
			*SourceGate->DestinationArrivalId.ToString(),
			*SourceGate->DestinationGateId.ToString());
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	FString SystemicError;
	FSystemicGameplayState DestinationSystemicState;
	if (!ResolveRuntimeSystemicState(DestinationSystemDefinition, nullptr, DestinationSystemicState, SystemicError))
	{
		OutResult.ResultCode = EGateTransitionResultCode::SystemBuildFailed;
		OutResult.FailureReason = SystemicError;
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	FShipSaveLocation ArrivalLocation;
	ArrivalLocation.SystemId = DestinationSystemDefinition.SystemId;
	ArrivalLocation.LocationMode = EShipLocationMode::GateArrival;
	ArrivalLocation.CoordinateFrame.FrameType = SourceGate->DestinationArrivalFrame;
	ArrivalLocation.CoordinateFrame.AnchorId = SourceGate->DestinationGateId;
	ArrivalLocation.AnchorId = SourceGate->DestinationGateId;
	ArrivalLocation.GateArrivalId = SourceGate->DestinationArrivalId;
	ArrivalLocation.PositionCm = SourceGate->DestinationArrivalLocalOffsetCm;
	ArrivalLocation.Rotation = SourceGate->DestinationArrivalRotation;
	ArrivalLocation.VelocityFrame = ArrivalLocation.CoordinateFrame;
	ArrivalLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;

	FTransform ArrivalTransform;
	FVector ArrivalVelocityCmPerSec = FVector::ZeroVector;
	FString ArrivalError;
	if (!ResolveGateArrivalLocation(DestinationSystemDefinition, ArrivalLocation, ArrivalTransform, ArrivalVelocityCmPerSec, ArrivalError))
	{
		OutResult.ResultCode = EGateTransitionResultCode::InvalidArrivalFrame;
		OutResult.FailureReason = ArrivalError;
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	if (!StarSystem->BuildSystem(DestinationSystemDefinition, ClockSnapshot.AuthoritativeSimulationTimeSeconds))
	{
		OutResult.ResultCode = EGateTransitionResultCode::SystemBuildFailed;
		OutResult.FailureReason = StarSystem->GetLastBuildError();
		ClearSessionState();
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!StarSystem->SpawnPlayerAtSpawnZone(SourceGate->DestinationArrivalId, PlayerController, ActivePlayerPawnClass))
	{
		OutResult.ResultCode = EGateTransitionResultCode::PlayerSpawnFailed;
		OutResult.FailureReason = StarSystem->GetLastBuildError();
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		LastGateTransitionResult = OutResult;
		ReportSessionFailure(OutResult.FailureReason);
		return false;
	}
	RefreshPlayerShipEquipmentEffects();

	ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (FlightPawn)
	{
		FlightPawn->SetFlightTestTransformAndVelocity(ArrivalTransform, ArrivalVelocityCmPerSec);
	}
	else if (APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr)
	{
		Pawn->SetActorTransform(ArrivalTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	CurrentSystemId = DestinationSystemDefinition.SystemId;
	CurrentSpawnZoneId = SourceGate->DestinationArrivalId;
	SelectedTargetId = NAME_None;
	ActiveTrafficState.SystemId = CurrentSystemId;
	ActiveTrafficState.Ships = DestinationSystemDefinition.LogicalTraffic;
	ActiveTrafficState.Groups = DestinationSystemDefinition.ShipGroups;
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(DestinationSystemDefinition, ClockSnapshot, ClockSnapshot.AuthoritativeSimulationTimeSeconds, ActiveTrafficState);
	SystemicGameplayState = DestinationSystemicState;
	ULogicalTrafficQueryService::ApplyPlayerRelevanceRealization(
		DestinationSystemDefinition,
		SystemicGameplayState,
		TEXT("actor_budget_m11_low"),
		ArrivalTransform.GetLocation(),
		ClockSnapshot,
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		ActiveTrafficState);
	StarSystem->RealizeTrafficActors(ActiveTrafficState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	StarSystem->RealizeSystemicEncounterActors(SystemicGameplayState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	bHasPendingGateArrival = true;
	PendingGateArrivalLocation = ArrivalLocation;

	OutResult.bAccepted = true;
	OutResult.ResultCode = EGateTransitionResultCode::Success;
	OutResult.ElapsedTransitionSeconds = 0.0;
	OutResult.ArrivalLocation = PendingGateArrivalLocation;
	LastGateTransitionResult = OutResult;
	LastSessionError.Reset();
	return true;
}

void UStargameSessionSubsystem::CompleteGateArrivalSafetyWindow()
{
	bHasPendingGateArrival = false;
	PendingGateArrivalLocation = FShipSaveLocation();
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot(const FStargameM0SaveState& State)
{
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::CreateSaveGameObject(UStargameM0SaveGame::StaticClass()));
	if (!SaveGame)
	{
		ReportStartupError(TEXT("Failed to create M0 save game object."));
		return false;
	}

	SaveGame->State = State;

	constexpr int32 UserIndex = 0;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, DevelopmentSlotName, UserIndex);
	if (!bSaved)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to save development slot '%s'."), DevelopmentSlotName));
	}
	return bSaved;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot(FStargameM0SaveState& OutState)
{
	constexpr int32 UserIndex = 0;
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::LoadGameFromSlot(DevelopmentSlotName, UserIndex));
	if (!SaveGame)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to load development slot '%s'."), DevelopmentSlotName));
		return false;
	}

	OutState = SaveGame->State;
	return true;
}

bool UStargameSessionSubsystem::ValidateLoadedSaveHeader(const FStargameM0SaveState& LoadedState, FString& OutError) const
{
	const FStargameM0SaveState CurrentHeader;
	if (LoadedState.SaveFormatVersion != CurrentHeader.SaveFormatVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported save format version %d; expected %d."),
			LoadedState.SaveFormatVersion,
			CurrentHeader.SaveFormatVersion);
		return false;
	}
	if (LoadedState.GameContentVersion != CurrentHeader.GameContentVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported game content version %d; expected %d."),
			LoadedState.GameContentVersion,
			CurrentHeader.GameContentVersion);
		return false;
	}
	if (LoadedState.BuildCompatibilityId != CurrentHeader.BuildCompatibilityId)
	{
		OutError = FString::Printf(TEXT("Development save compatibility '%s' does not match current build '%s'."),
			*LoadedState.BuildCompatibilityId,
			*CurrentHeader.BuildCompatibilityId);
		return false;
	}
	return true;
}

FStargameM0SaveState UStargameSessionSubsystem::MakeCurrentM0SaveState() const
{
	FStargameM0SaveState State;
	State.SystemId = CurrentSystemId;
	State.SpawnZoneId = CurrentSpawnZoneId;
	State.SelectedTargetId = SelectedTargetId;
	State.ClockSnapshot = ClockSnapshot;
	State.ActiveTrafficState = ULogicalTrafficQueryService::SanitizeTrafficStateForSave(ActiveTrafficState);
	State.SystemicGameplayState = SystemicGameplayState;
	State.ShipLocation.SystemId = CurrentSystemId;
	State.ShipLocation.CoordinateFrame.FrameType = TEXT("system_barycentric");
	State.ShipLocation.CoordinateFrame.AnchorId = CurrentSystemId;
	State.ShipLocation.LocationMode = EShipLocationMode::Respawn;
	State.ShipLocation.AnchorId = CurrentSpawnZoneId;
	State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
	State.ShipLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;

	UWorld* World = ResolveSessionWorld();
	if (const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
	{
		const FStarSystemDefinition& ActiveDefinition = StarSystem->GetActiveSystemDefinition();
		if (ActiveDefinition.SystemId == CurrentSystemId && ActiveDefinition.SourceType == ESystemSourceType::Generated)
		{
			State.bSavedSystemIsGenerated = true;
			State.GeneratedSystemSourceMetadata = ActiveDefinition.GeneratedSourceMetadata;
		}
	}
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (!FlightPawn && StationInteriorSpacePawn)
	{
		FlightPawn = StationInteriorSpacePawn.Get();
	}
	if (FlightPawn)
	{
		State.ShipLocation.PositionCm = FlightPawn->GetLogicalSystemPositionCm();
		State.ShipLocation.Rotation = FlightPawn->GetActorRotation();
		State.ShipLocation.LinearVelocityCmPerSec = FlightPawn->GetLinearVelocityCmPerSec();
		State.ShipLocation.CoordinateFrame.AnchorId = CurrentSystemId;
		State.ShipLocation.LocationMode = EShipLocationMode::FreeFlight;
		State.ShipLocation.AnchorId = CurrentSystemId;
		State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
		const FDockingOperationState Docking = FlightPawn->GetDockingOperationState();
		if (Docking.DockingState == EDockingState::Docked && !Docking.StationId.IsNone() && !Docking.PortId.IsNone())
		{
			State.ShipLocation.CoordinateFrame.FrameType = TEXT("station_relative");
			State.ShipLocation.CoordinateFrame.AnchorId = Docking.StationId;
			State.ShipLocation.LocationMode = EShipLocationMode::StationDocked;
			State.ShipLocation.AnchorId = Docking.StationId;
			State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
			State.ShipLocation.DockedStationId = Docking.StationId;
			State.ShipLocation.DockingPortId = Docking.PortId;
			State.ShipLocation.DockedShipInstanceId = Docking.ShipInstanceId;
			State.ShipLocation.DockingClearanceId = Docking.ClearanceId;
			State.ShipLocation.DockingState = EDockingState::Docked;
			State.ShipLocation.PortRelativeTransform = Docking.CapturedPortRelativeTransform;
			if (const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
			{
				FDockingPortRuntimeState RuntimeState;
				if (StarSystem->FindDockingPortRuntimeState(Docking.StationId, Docking.PortId, RuntimeState))
				{
					State.ShipLocation.DockingReservedShipId = RuntimeState.ReservedShipId;
					State.ShipLocation.DockingOccupyingShipId = RuntimeState.OccupyingShipId;
					State.ShipLocation.DockingReservationExpiryTimeSeconds = RuntimeState.ReservationExpiryTimeSeconds;
					State.ShipLocation.DockingPortRuntimeState = RuntimeState.DockingState;
				}
			}
		}
	}
	if (bHasPendingGateArrival)
	{
		State.ShipLocation = PendingGateArrivalLocation;
		State.ShipLocation.SystemId = CurrentSystemId;
		State.ShipLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
		if (FlightPawn)
		{
			State.ShipLocation.LinearVelocityCmPerSec = FlightPawn->GetLinearVelocityCmPerSec();
		}
	}
	return State;
}

bool UStargameSessionSubsystem::ValidateLoadedTrafficState(const FStarSystemDefinition& SystemDefinition, const FActiveTrafficSimulationState& TrafficState, FString& OutError) const
{
	if (TrafficState.SystemId != SystemDefinition.SystemId)
	{
		OutError = FString::Printf(TEXT("Saved traffic system '%s' does not match loaded system '%s'."),
			*TrafficState.SystemId.ToString(),
			*SystemDefinition.SystemId.ToString());
		return false;
	}

	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}

	TSet<FName> ShipIds;
	for (const FShipTrafficInstance& Ship : TrafficState.Ships)
	{
		if (Ship.ShipInstanceId.IsNone())
		{
			OutError = TEXT("Saved traffic contains an empty ship instance ID.");
			return false;
		}
		if (Ship.SystemId != SystemDefinition.SystemId)
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' belongs to system '%s', expected '%s'."),
				*Ship.ShipInstanceId.ToString(),
				*Ship.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString());
			return false;
		}
		if (ShipIds.Contains(Ship.ShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Saved traffic contains duplicate ship instance ID '%s'."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		if (!GoalReferencesValidRoute(Ship.CurrentGoal, RouteIds, OutError) ||
			(Ship.bHasSavedGoal && !GoalReferencesValidRoute(Ship.SavedGoal, RouteIds, OutError)))
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' has an invalid route goal: %s"),
				*Ship.ShipInstanceId.ToString(),
				*OutError);
			return false;
		}
		if (Ship.CurrentGoal.GoalKind == EShipGoalKind::TradeRoute)
		{
			if (!FMath::IsWithinInclusive(Ship.CurrentGoal.RouteProgress01, 0.0, 1.0))
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' has route progress outside [0,1]."), *Ship.ShipInstanceId.ToString());
				return false;
			}
			if (Ship.CurrentGoal.TargetFrame.RouteSegmentId != Ship.CurrentGoal.RouteSegmentId)
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' target frame route does not match current goal route."), *Ship.ShipInstanceId.ToString());
				return false;
			}
			if (Ship.LogicalLocation.RouteSegmentId != Ship.CurrentGoal.RouteSegmentId ||
				Ship.LogicalLocation.TargetType != FName(TEXT("route_sample")))
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' route goal is missing durable route logical location."), *Ship.ShipInstanceId.ToString());
				return false;
			}
		}
		else if (!Ship.bRouteRecoverable && Ship.LogicalLocation.TargetType != FName(TEXT("off_route_free_flight")))
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' is non-recoverable without an off-route logical location."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		if (Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized || !Ship.RealizationToken.IsNone())
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' contains transient realization state."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		ShipIds.Add(Ship.ShipInstanceId);
	}

	TSet<FName> GroupIds;
	for (const FShipGroupState& Group : TrafficState.Groups)
	{
		if (Group.GroupId.IsNone())
		{
			OutError = TEXT("Saved traffic contains an empty group ID.");
			return false;
		}
		if (GroupIds.Contains(Group.GroupId))
		{
			OutError = FString::Printf(TEXT("Saved traffic contains duplicate group ID '%s'."), *Group.GroupId.ToString());
			return false;
		}
		if (!Group.LeaderShipInstanceId.IsNone() && !ShipIds.Contains(Group.LeaderShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Saved traffic group '%s' references missing leader '%s'."),
				*Group.GroupId.ToString(),
				*Group.LeaderShipInstanceId.ToString());
			return false;
		}
		for (const FName MemberShipInstanceId : Group.MemberShipInstanceIds)
		{
			if (MemberShipInstanceId.IsNone() || !ShipIds.Contains(MemberShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Saved traffic group '%s' references missing member '%s'."),
					*Group.GroupId.ToString(),
					*MemberShipInstanceId.ToString());
				return false;
			}
		}
		for (const FShipFormationSlot& Slot : Group.FormationSlots)
		{
			if (!Slot.ShipInstanceId.IsNone() && !ShipIds.Contains(Slot.ShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Saved traffic group '%s' slot '%s' references missing ship '%s'."),
					*Group.GroupId.ToString(),
					*Slot.SlotId.ToString(),
					*Slot.ShipInstanceId.ToString());
				return false;
			}
		}
		GroupIds.Add(Group.GroupId);
	}

	return true;
}

bool UStargameSessionSubsystem::ValidateLoadedSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& LoadedSystemicState, FString& OutError) const
{
	if (!USystemicGameplayQueryService::ValidateSystemicGameplayState(SystemDefinition, LoadedSystemicState, OutError))
	{
		OutError = FString::Printf(TEXT("Saved systemic gameplay state is invalid: %s"), *OutError);
		return false;
	}
	return true;
}

bool UStargameSessionSubsystem::ResolveRuntimeSystemicState(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState* SavedSystemicState,
	FSystemicGameplayState& OutSystemicState,
	FString& OutError) const
{
	if (SavedSystemicState && HasAuthoredSystemicGameplayState(*SavedSystemicState))
	{
		if (!ValidateLoadedSystemicState(SystemDefinition, *SavedSystemicState, OutError))
		{
			return false;
		}
		OutSystemicState = *SavedSystemicState;
		return true;
	}
	if (SavedSystemicState)
	{
		if (HasAuthoredSystemicGameplayState(SystemDefinition.SystemicGameplay))
		{
			OutError = TEXT("Saved systemic gameplay state is missing required authored records.");
			return false;
		}

		OutSystemicState = FSystemicGameplayState();
		return true;
	}

	if (HasAuthoredSystemicGameplayState(SystemDefinition.SystemicGameplay))
	{
		const bool bFrontierSystemicState = SystemDefinition.SystemicGameplay.Markets.ContainsByPredicate([](const FStationMarketState& Market)
		{
			return Market.MarketId == FName(TEXT("brink_watch"));
		});
		const FSystemicGameplayState AuthoredRuntimeState = bFrontierSystemicState
			? USystemicGameplayQueryService::MakeM12FixtureState(SystemDefinition)
			: SystemDefinition.SystemicGameplay;
		if (!ValidateLoadedSystemicState(SystemDefinition, AuthoredRuntimeState, OutError))
		{
			OutError = FString::Printf(TEXT("Authored systemic gameplay state is invalid: %s"), *OutError);
			return false;
		}
		OutSystemicState = AuthoredRuntimeState;
		return true;
	}

	OutSystemicState = FSystemicGameplayState();
	return true;
}

bool UStargameSessionSubsystem::RestoreSavedShipLocation(const FShipSaveLocation& ShipLocation, APlayerController* PlayerController, FString& OutError)
{
	if (ShipLocation.LocationMode == EShipLocationMode::Respawn)
	{
		return true;
	}

	UWorld* World = ResolveSessionWorld();
	ASpaceFlightPawn* FlightPawn = ResolveSpaceFlightPawn(World, PlayerController);
	if (!FlightPawn)
	{
		OutError = TEXT("Saved ship location requires a space flight pawn.");
		return false;
	}

	if (ShipLocation.LocationMode == EShipLocationMode::StationDocked)
	{
		if (ShipLocation.DockedStationId.IsNone() || ShipLocation.DockingPortId.IsNone())
		{
			OutError = TEXT("Saved docked location is missing station or docking port ID.");
			return false;
		}

		if (!FlightPawn->RestoreDockedAt(
			ShipLocation.DockedStationId,
			ShipLocation.DockingPortId,
			ShipLocation.DockedShipInstanceId,
			ShipLocation.DockingClearanceId,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds))
		{
			OutError = TEXT("Saved docked location could not resolve its live docking port.");
			return false;
		}
		return true;
	}

	if (ShipLocation.LocationMode == EShipLocationMode::GateArrival)
	{
		if (ShipLocation.SystemId != CurrentSystemId)
		{
			OutError = FString::Printf(TEXT("Saved gate arrival system '%s' does not match loaded system '%s'."),
				*ShipLocation.SystemId.ToString(),
				*CurrentSystemId.ToString());
			return false;
		}

		const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
		if (!StarSystem || StarSystem->GetActiveSystemId() != CurrentSystemId)
		{
			OutError = TEXT("Saved gate arrival requires the destination system to be active.");
			return false;
		}

		FTransform ArrivalTransform;
		FVector ArrivalVelocityCmPerSec = FVector::ZeroVector;
		if (!ResolveGateArrivalLocation(StarSystem->GetActiveSystemDefinition(), ShipLocation, ArrivalTransform, ArrivalVelocityCmPerSec, OutError))
		{
			return false;
		}

		FlightPawn->SetFlightTestTransformAndVelocity(ArrivalTransform, ShipLocation.LinearVelocityCmPerSec + ArrivalVelocityCmPerSec);
		bHasPendingGateArrival = true;
		PendingGateArrivalLocation = ShipLocation;
		return true;
	}

	if (ShipLocation.LocationMode == EShipLocationMode::FreeFlight)
	{
		if (ShipLocation.SystemId != CurrentSystemId)
		{
			OutError = FString::Printf(TEXT("Saved ship location system '%s' does not match loaded system '%s'."),
				*ShipLocation.SystemId.ToString(),
				*CurrentSystemId.ToString());
			return false;
		}
		const bool bSystemFrame = ShipLocation.CoordinateFrame.FrameType == FName(TEXT("system_barycentric")) && ShipLocation.CoordinateFrame.AnchorId == CurrentSystemId;
		if (!bSystemFrame)
		{
			OutError = TEXT("Saved free-flight location must use a system_barycentric coordinate frame.");
			return false;
		}

		FlightPawn->SetFlightTestTransformAndVelocity(
			FTransform(ShipLocation.Rotation, ShipLocation.PositionCm),
			ShipLocation.LinearVelocityCmPerSec);
		return true;
	}

	OutError = TEXT("Unsupported saved ship location mode.");
	return false;
}

bool UStargameSessionSubsystem::ResolveGateArrivalLocation(const FStarSystemDefinition& SystemDefinition, const FShipSaveLocation& GateArrivalLocation, FTransform& OutTransform, FVector& OutVelocityCmPerSec, FString& OutError) const
{
	if (GateArrivalLocation.LocationMode != EShipLocationMode::GateArrival)
	{
		OutError = TEXT("Gate arrival resolver requires a gate_arrival save location.");
		return false;
	}
	if (GateArrivalLocation.GateArrivalId.IsNone() || GateArrivalLocation.AnchorId.IsNone())
	{
		OutError = TEXT("Gate arrival save location is missing arrival or gate ID.");
		return false;
	}
	if (GateArrivalLocation.CoordinateFrame.FrameType != FName(TEXT("gate_relative")) || GateArrivalLocation.CoordinateFrame.AnchorId != GateArrivalLocation.AnchorId)
	{
		OutError = TEXT("Gate arrival save location must use a gate_relative frame anchored to the destination gate.");
		return false;
	}

	const FSimulationClockSnapshot ArrivalClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, GateArrivalLocation.AuthoritativeSimulationTimeSeconds);
	FFrameResolvedTransform ArrivalFrame;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, GateArrivalLocation.GateArrivalId, ArrivalClock, ArrivalClock.AuthoritativeSimulationTimeSeconds, ArrivalFrame))
	{
		OutError = FString::Printf(TEXT("Gate arrival marker '%s' could not be resolved in system '%s'."),
			*GateArrivalLocation.GateArrivalId.ToString(),
			*SystemDefinition.SystemId.ToString());
		return false;
	}

	OutTransform = FTransform(ArrivalFrame.Rotation, ArrivalFrame.PositionCm);
	OutVelocityCmPerSec = ArrivalFrame.LinearVelocityCmPerSec;
	OutError.Reset();
	return true;
}

FString UStargameSessionSubsystem::GetM0DebugSummary() const
{
	const FString SaveSlotStatus = UGameplayStatics::DoesSaveGameExist(DevelopmentSlotName, 0)
		? TEXT("present")
		: TEXT("missing");

	return FString::Printf(
		TEXT("StartProfile=%s\nCurrentSystem=%s\nSpawnZone=%s\nSelectedTarget=%s\nClockOwner=%s\nSimulationTimeSeconds=%.2f\nLastStartResult=%d\nLastSessionError=%s\nSaveSlot=%s\nGateTransitionSource=%s\nGateTransitionDestination=%s\nGateTransitionArrival=%s\nGateTransitionFrame=%s\nGateTransitionFailure=%s\nGateTransitionElapsedSeconds=%.2f"),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString(),
		*SelectedTargetId.ToString(),
		*ClockSnapshot.ClockOwner.ToString(),
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		static_cast<int32>(LastStartSessionResult),
		*LastSessionError,
		*SaveSlotStatus,
		*LastGateTransitionResult.SourceGateId.ToString(),
		*LastGateTransitionResult.DestinationSystemId.ToString(),
		*LastGateTransitionResult.DestinationArrivalId.ToString(),
		*LastGateTransitionResult.ArrivalFrame.ToString(),
		*LastGateTransitionResult.FailureReason,
		LastGateTransitionResult.ElapsedTransitionSeconds);
}

bool UStargameSessionSubsystem::BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile)
{
	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = ResolveCatalogSubsystem();
	const bool bResolvedSystem = Catalog && Catalog->ResolveSystemDefinition(StartProfile.SystemId, SystemDefinition);
	if (!bResolvedSystem)
	{
		LastStartSessionResult = EStartSessionResult::MissingSystemDefinition;
		ReportStartupError(FString::Printf(TEXT("Missing system definition '%s'."), *StartProfile.SystemId.ToString()));
		return false;
	}

	FString ValidationError;
	if (!ValidateResolvedSystemDefinitionForRuntime(Catalog, SystemDefinition, ValidationError))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(ValidationError);
		return false;
	}
	FSystemicGameplayState InitialSystemicState;
	if (!ResolveRuntimeSystemicState(SystemDefinition, nullptr, InitialSystemicState, ValidationError))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(ValidationError);
		return false;
	}

	ClearSessionState();

	UWorld* World = ResolveSessionWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(TEXT("Missing star system subsystem."));
		return false;
	}

	if (!StarSystem->BuildSystem(SystemDefinition, 0.0))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(StarSystem->GetLastBuildError());
		ClearSessionState();
		return false;
	}

	FSpawnZoneDefinition SpawnZone;
	if (!StarSystem->FindSpawnZone(StartProfile.SpawnZoneId, SpawnZone))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(FString::Printf(TEXT("Missing spawn zone '%s'."), *StartProfile.SpawnZoneId.ToString()));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	StartProfileId = StartProfile.StartProfileId;
	CurrentSystemId = StartProfile.SystemId;
	CurrentSpawnZoneId = StartProfile.SpawnZoneId;
	SelectedTargetId = NAME_None;
	ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(CurrentSystemId, 0.0);
	ActiveTrafficState.SystemId = CurrentSystemId;
	ActiveTrafficState.Ships = SystemDefinition.LogicalTraffic;
	ActiveTrafficState.Groups = SystemDefinition.ShipGroups;
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(SystemDefinition, ClockSnapshot, ClockSnapshot.AuthoritativeSimulationTimeSeconds, ActiveTrafficState);
	SystemicGameplayState = InitialSystemicState;
	EnsureRuntimeGodotFrontierMarket(SystemicGameplayState);
	ULogicalTrafficQueryService::ApplyPlayerRelevanceRealization(
		SystemDefinition,
		SystemicGameplayState,
		TEXT("actor_budget_m11_low"),
		SpawnZone.Transform.GetLocation(),
		ClockSnapshot,
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		ActiveTrafficState);
	StarSystem->RealizeTrafficActors(ActiveTrafficState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	StarSystem->RealizeSystemicEncounterActors(SystemicGameplayState, ClockSnapshot.AuthoritativeSimulationTimeSeconds);
	bHasPendingGateArrival = false;
	PendingGateArrivalLocation = FShipSaveLocation();

	TSubclassOf<APawn> PawnClass = nullptr;
	FShipArchetypeDefinition Ship;
	if (!Catalog || !Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(FString::Printf(TEXT("Missing ship archetype '%s'."), *StartProfile.ShipArchetypeId.ToString()));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}
	PawnClass = Ship.PawnClass.LoadSynchronous();
	if (!PawnClass)
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(FString::Printf(TEXT("Ship archetype '%s' has no authored pawn class."), *StartProfile.ShipArchetypeId.ToString()));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!StarSystem->SpawnPlayerAtSpawnZone(CurrentSpawnZoneId, PlayerController, PawnClass))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(StarSystem->GetLastBuildError());
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}
	ActivePlayerPawnClass = PawnClass;
	RefreshPlayerShipEquipmentEffects();

	return true;
}

void UStargameSessionSubsystem::ReportStartupError(const FString& Error)
{
	LastSessionError = Error;
	UE_LOG(LogStargameStartup, Error, TEXT("%s"), *LastSessionError);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.0f,
			FColor::Red,
			FString::Printf(TEXT("Stargame startup error: %s"), *LastSessionError));
	}
}

void UStargameSessionSubsystem::ReportSessionFailure(const FString& Error)
{
	LastSessionError = Error;
	UE_LOG(LogStargameStartup, Warning, TEXT("%s"), *LastSessionError);
}

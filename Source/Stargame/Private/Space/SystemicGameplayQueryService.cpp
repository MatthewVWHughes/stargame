#include "Space/SystemicGameplayQueryService.h"
#include "Space/OrbitRouteFrameQueryService.h"

namespace
{
	template <typename T, typename PredicateType>
	T* FindMutable(TArray<T>& Items, PredicateType Predicate)
	{
		return Items.FindByPredicate(Predicate);
	}

	template <typename T, typename PredicateType>
	const T* FindConst(const TArray<T>& Items, PredicateType Predicate)
	{
		return Items.FindByPredicate(Predicate);
	}

	FName MakeId(const TCHAR* Prefix, FName SourceId)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), Prefix, *SourceId.ToString()));
	}

	double GetItemMass(const FSystemicGameplayState& State, FName ItemId)
	{
		if (const FItemDefinition* Item = FindConst(State.Items, [ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		}))
		{
			return Item->MassPerUnitKg;
		}
		return 0.0;
	}

	double GetItemVolume(const FSystemicGameplayState& State, FName ItemId)
	{
		if (const FItemDefinition* Item = FindConst(State.Items, [ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		}))
		{
			return Item->VolumePerUnitM3;
		}
		return 0.0;
	}

	void GetContainerLoad(const FSystemicGameplayState& State, const FContainerState& Container, double& OutMassKg, double& OutVolumeM3)
	{
		OutMassKg = 0.0;
		OutVolumeM3 = 0.0;
		for (const FItemStackState& Stack : Container.Stacks)
		{
			OutMassKg += GetItemMass(State, Stack.ItemId) * Stack.Quantity;
			OutVolumeM3 += GetItemVolume(State, Stack.ItemId) * Stack.Quantity;
		}
	}

	bool HasNamedGameplayTag(const FGameplayTagContainer& Tags, const TCHAR* Token)
	{
		return Tags.ToStringSimple().Contains(Token, ESearchCase::IgnoreCase);
	}

	void AddJournalEntry(FSystemicGameplayState& State, FName TransactionId, int32 Sequence, FName SideEffectType, FName SideEffectRefId, FName IdempotencyKey)
	{
		FGameplayTransactionJournalEntry Entry;
		Entry.JournalEntryId = FName(*FString::Printf(TEXT("%s_journal_%03d"), *TransactionId.ToString(), Sequence));
		Entry.TransactionId = TransactionId;
		Entry.Sequence = Sequence;
		Entry.Phase = TEXT("commit");
		Entry.SideEffectType = SideEffectType;
		Entry.SideEffectRefId = SideEffectRefId;
		Entry.State = TEXT("applied");
		Entry.IdempotencyKey = IdempotencyKey;
		State.TransactionJournal.Add(Entry);
	}
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM9FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.Factions.IsEmpty())
	{
		return SystemDefinition.SystemicGameplay;
	}

	FSystemicGameplayState State;

	FFactionDefinition Authority;
	Authority.FactionId = TEXT("frontier_local_authority");
	Authority.DisplayName = FText::FromString(TEXT("Frontier Local Authority"));
	Authority.FactionType = TEXT("police");
	Authority.DefaultLawProfileId = TEXT("frontier_law_basic");
	Authority.RelationshipGroupId = TEXT("frontier_lawful");
	Authority.bCanOwnStations = true;
	Authority.bCanOwnShips = true;
	Authority.bCanIssueMissions = true;
	State.Factions.Add(Authority);

	FFactionDefinition Traders;
	Traders.FactionId = TEXT("wayfarer_traders");
	Traders.DisplayName = FText::FromString(TEXT("Wayfarer Traders"));
	Traders.FactionType = TEXT("corporation");
	Traders.RelationshipGroupId = TEXT("frontier_civilian");
	Traders.bCanOwnShips = true;
	Traders.bCanIssueMissions = true;
	State.Factions.Add(Traders);

	FFactionDefinition Pirates;
	Pirates.FactionId = TEXT("ember_raiders");
	Pirates.DisplayName = FText::FromString(TEXT("Ember Raiders"));
	Pirates.FactionType = TEXT("pirate");
	Pirates.RelationshipGroupId = TEXT("frontier_hostile");
	Pirates.bCanOwnShips = true;
	State.Factions.Add(Pirates);

	FFactionRelationshipRecord PirateRelationship;
	PirateRelationship.SourceFactionId = Pirates.FactionId;
	PirateRelationship.TargetFactionId = Authority.FactionId;
	PirateRelationship.Standing = -0.8;
	PirateRelationship.HostilityState = TEXT("hostile");
	State.FactionRelationships.Add(PirateRelationship);

	FFactionOperationalState Operations;
	Operations.FactionId = Authority.FactionId;
	Operations.SystemId = SystemDefinition.SystemId;
	Operations.Influence = 0.65;
	Operations.SecurityPressure = 0.55;
	Operations.CriminalPressure = 0.25;
	Operations.PatrolAssetIds = { TEXT("patrol_frontier_local_01") };
	Operations.AvailablePatrolBudget = 1;
	Operations.AlertLevel = TEXT("watch");
	State.FactionOperations.Add(Operations);

	FJurisdictionDefinition Jurisdiction;
	Jurisdiction.JurisdictionId = TEXT("frontier_local_authority");
	Jurisdiction.AuthorityFactionId = Authority.FactionId;
	Jurisdiction.SystemId = SystemDefinition.SystemId;
	Jurisdiction.LawProfileId = TEXT("frontier_law_basic");
	Jurisdiction.Priority = 10;
	State.Jurisdictions.Add(Jurisdiction);

	FItemDefinition OreItem;
	OreItem.ItemId = TEXT("item_ember_ore");
	OreItem.DisplayName = FText::FromString(TEXT("Ember Ore"));
	OreItem.ItemType = TEXT("commodity");
	OreItem.StackLimit = 1000;
	OreItem.MassPerUnitKg = 10.0;
	OreItem.VolumePerUnitM3 = 1.0;
	OreItem.BaseValue = 100;
	State.Items.Add(OreItem);

	FCommodityDefinition OreCommodity;
	OreCommodity.CommodityId = TEXT("commodity_ember_ore");
	OreCommodity.DisplayName = OreItem.DisplayName;
	OreCommodity.BasePrice = 100;
	OreCommodity.MassPerUnitKg = OreItem.MassPerUnitKg;
	OreCommodity.VolumePerUnitM3 = OreItem.VolumePerUnitM3;
	OreCommodity.Category = TEXT("ore");
	State.Commodities.Add(OreCommodity);

	FCommodityItemBridge Bridge;
	Bridge.CommodityItemBridgeId = TEXT("bridge_ember_ore");
	Bridge.CommodityId = OreCommodity.CommodityId;
	Bridge.ItemId = OreItem.ItemId;
	State.CommodityItemBridges.Add(Bridge);

	FContainerState PlayerCargo;
	PlayerCargo.ContainerId = TEXT("player_ship_cargo");
	PlayerCargo.OwnerId = TEXT("player");
	PlayerCargo.LocationType = TEXT("ship");
	PlayerCargo.LocationId = TEXT("player_ship");
	PlayerCargo.CapacityMassKg = 10000.0;
	PlayerCargo.CapacityVolumeM3 = 1000.0;
	State.Containers.Add(PlayerCargo);

	FContainerState MarketCargo;
	MarketCargo.ContainerId = TEXT("brink_watch_market_inventory");
	MarketCargo.OwnerId = TEXT("brink_watch");
	MarketCargo.LocationType = TEXT("market");
	MarketCargo.LocationId = TEXT("brink_watch");
	MarketCargo.CapacityMassKg = 1000000.0;
	MarketCargo.CapacityVolumeM3 = 100000.0;
	FItemStackState MarketStack;
	MarketStack.StackId = TEXT("stack_brink_ore_01");
	MarketStack.ItemId = OreItem.ItemId;
	MarketStack.Quantity = 100;
	MarketCargo.Stacks.Add(MarketStack);
	State.Containers.Add(MarketCargo);

	FCreditAccountRecord PlayerAccount;
	PlayerAccount.AccountId = TEXT("account_player");
	PlayerAccount.OwnerType = TEXT("player");
	PlayerAccount.OwnerId = TEXT("player");
	PlayerAccount.AvailableBalance = 100000;
	State.CreditAccounts.Add(PlayerAccount);

	FCreditAccountRecord MarketAccount;
	MarketAccount.AccountId = TEXT("account_brink_watch_market");
	MarketAccount.OwnerType = TEXT("market");
	MarketAccount.OwnerId = TEXT("brink_watch");
	MarketAccount.AvailableBalance = 1000000;
	State.CreditAccounts.Add(MarketAccount);

	FStationMarketState Market;
	Market.MarketId = TEXT("brink_watch");
	Market.StationId = TEXT("brink_watch");
	Market.MarketProfileId = TEXT("frontier_basic_market");
	Market.StockByCommodity.Add(OreCommodity.CommodityId, 100);
	State.Markets.Add(Market);

	FCommsEndpointDefinition Comms;
	Comms.EndpointId = TEXT("comms_brink_watch");
	Comms.OwnerType = TEXT("station");
	Comms.OwnerId = TEXT("brink_watch");
	Comms.AvailableChannels = { TEXT("cockpit_comms"), TEXT("docked_menu") };
	Comms.AccessPolicyId = TEXT("frontier_open_access");
	State.CommsEndpoints.Add(Comms);

	FStationServiceEndpointDefinition MarketEndpoint;
	MarketEndpoint.ServiceEndpointId = TEXT("service_brink_watch_market");
	MarketEndpoint.StationId = TEXT("brink_watch");
	MarketEndpoint.ServiceType = TEXT("market");
	MarketEndpoint.ProviderFactionId = Authority.FactionId;
	MarketEndpoint.CommsEndpointId = Comms.EndpointId;
	MarketEndpoint.MarketId = Market.MarketId;
	MarketEndpoint.InventoryContainerId = MarketCargo.ContainerId;
	MarketEndpoint.CreditAccountId = MarketAccount.AccountId;
	MarketEndpoint.AccessPolicyId = TEXT("frontier_open_access");
	MarketEndpoint.LegalPolicyId = Jurisdiction.LawProfileId;
	MarketEndpoint.PresentationModes = { TEXT("docked_menu"), TEXT("cockpit_comms"), TEXT("fps_counter") };
	State.StationServiceEndpoints.Add(MarketEndpoint);

	FMessageDefinition Message;
	Message.MessageId = TEXT("msg_market_quote_01");
	Message.MessageType = TEXT("service");
	Message.SpeakerId = MarketEndpoint.ServiceEndpointId;
	Message.TextKey = TEXT("frontier.market.quote");
	State.MessageDefinitions.Add(Message);

	FMissionOfferRecord Offer;
	Offer.OfferId = TEXT("offer_brink_ore_delivery_01");
	Offer.MissionDefinitionId = TEXT("mission_ore_delivery_basic");
	Offer.SourceStationId = TEXT("brink_watch");
	Offer.SourceServiceEndpointId = MarketEndpoint.ServiceEndpointId;
	Offer.IssuerFactionId = Authority.FactionId;
	State.MissionOffers.Add(Offer);

	FMissionInstanceState Mission;
	Mission.MissionInstanceId = TEXT("mission_instance_fixture_01");
	Mission.MissionDefinitionId = Offer.MissionDefinitionId;
	Mission.OfferId = Offer.OfferId;
	Mission.OwnerId = TEXT("player");
	Mission.IssuerFactionId = Authority.FactionId;
	Mission.CurrentState = TEXT("offered");
	Mission.IdempotencyKey = TEXT("m9_fixture_mission_instance");
	Mission.ObjectiveStateIds = { TEXT("objective_deliver_ore_01") };
	State.MissionInstances.Add(Mission);

	FObjectiveState Objective;
	Objective.ObjectiveStateId = TEXT("objective_deliver_ore_01");
	Objective.MissionInstanceId = Mission.MissionInstanceId;
	Objective.ObjectiveType = TEXT("deliver_cargo");
	Objective.TargetType = TEXT("station");
	Objective.TargetId = TEXT("wayfarer_depot");
	Objective.RouteSegmentIds = { TEXT("m7_brink_watch_wayfarer_trade") };
	Objective.JurisdictionId = Jurisdiction.JurisdictionId;
	Objective.State = TEXT("inactive");
	State.ObjectiveStates.Add(Objective);

	return State;
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM10FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.LogicalEncounters.IsEmpty())
	{
		return SystemDefinition.SystemicGameplay;
	}

	FSystemicGameplayState State = MakeM9FixtureState(SystemDefinition);
	const FName RouteId(TEXT("m7_brink_watch_wayfarer_trade"));
	const FName SourceEventId(TEXT("event_m10_pirate_trade_lane_01"));
	const FName DistressEventId(TEXT("event_distress_trade_lane_01"));
	const FName HazardId(TEXT("hazard_interdiction_trade_lane_01"));
	const FName EncounterId(TEXT("encounter_pirate_trade_lane_01"));
	const FName EngagementId(TEXT("engagement_pirate_trade_lane_01"));

	FSimulationEventRecord EncounterEvent;
	EncounterEvent.EventId = SourceEventId;
	EncounterEvent.SystemId = SystemDefinition.SystemId;
	EncounterEvent.EventType = TEXT("logical_pirate_interdiction");
	EncounterEvent.SourceType = TEXT("npc_group");
	EncounterEvent.SourceId = TEXT("pirate_raider_01");
	EncounterEvent.TargetType = TEXT("ship");
	EncounterEvent.TargetId = TEXT("trader_brink_01");
	EncounterEvent.ParticipantShipIds = { TEXT("trader_brink_01"), TEXT("pirate_raider_01"), TEXT("patrol_frontier_local_01") };
	EncounterEvent.ParticipantGroupIds = { TEXT("m8_brink_trade_group"), TEXT("pirate_group_01"), TEXT("patrol_group_01") };
	EncounterEvent.PayloadRef = EncounterId;
	EncounterEvent.IdempotencyKey = TEXT("idem_m10_pirate_trade_lane_01");
	State.Events.Add(EncounterEvent);

	FRouteSecuritySnapshot Security;
	Security.SnapshotId = TEXT("security_trade_lane_window_01");
	Security.RouteSegmentId = RouteId;
	Security.WindowStartTimeSeconds = 0.0;
	Security.WindowEndTimeSeconds = 600.0;
	Security.JurisdictionId = TEXT("frontier_local_authority");
	Security.SecurityRating = 0.45;
	Security.PatrolCoverageScore = 0.4;
	Security.PirateRiskScore = 0.6;
	Security.RouteValue = 0.65;
	Security.SourceEventId = SourceEventId;
	Security.ProcessedWatermark = 1;
	State.RouteSecuritySnapshots.Add(Security);

	FPatrolReservationRecord Reservation;
	Reservation.ReservationId = TEXT("reservation_patrol_trade_lane_01");
	Reservation.PatrolAssetId = TEXT("patrol_frontier_local_01");
	Reservation.FactionId = TEXT("frontier_local_authority");
	Reservation.JurisdictionId = TEXT("frontier_local_authority");
	Reservation.RouteSegmentId = RouteId;
	Reservation.SourceEventId = SourceEventId;
	Reservation.ExpiresAtTimeSeconds = 600.0;
	Reservation.IdempotencyKey = TEXT("idem_m10_patrol_trade_lane_01");
	State.PatrolReservations.Add(Reservation);
	if (FFactionOperationalState* FixtureOperation = FindMutable(State.FactionOperations, [](const FFactionOperationalState& Candidate)
	{
		return Candidate.FactionId == FName(TEXT("frontier_local_authority"));
	}))
	{
		FixtureOperation->ReservedPatrolBudget = FMath::Max(FixtureOperation->ReservedPatrolBudget, 1);
	}

	FContainerState TraderCargo;
	TraderCargo.ContainerId = TEXT("trader_brink_cargo");
	TraderCargo.OwnerId = TEXT("trader_brink_01");
	TraderCargo.LocationType = TEXT("ship");
	TraderCargo.LocationId = TEXT("trader_brink_01");
	TraderCargo.CapacityMassKg = 500.0;
	TraderCargo.CapacityVolumeM3 = 20.0;
	FItemStackState TraderOre;
	TraderOre.StackId = TEXT("trader_brink_ore_stack");
	TraderOre.ItemId = TEXT("item_ember_ore");
	TraderOre.Quantity = 4;
	TraderOre.OwnerFactionId = TEXT("frontier_local_authority");
	TraderCargo.Stacks.Add(TraderOre);
	State.Containers.Add(TraderCargo);

	FContainerState PirateCargo;
	PirateCargo.ContainerId = TEXT("pirate_raider_cargo");
	PirateCargo.OwnerId = TEXT("pirate_raider_01");
	PirateCargo.LocationType = TEXT("ship");
	PirateCargo.LocationId = TEXT("pirate_raider_01");
	PirateCargo.CapacityMassKg = 500.0;
	PirateCargo.CapacityVolumeM3 = 20.0;
	State.Containers.Add(PirateCargo);

	FCreditAccountRecord TraderAccount;
	TraderAccount.AccountId = TEXT("account_trader_brink_01");
	TraderAccount.OwnerId = TEXT("trader_brink_01");
	TraderAccount.OwnerType = TEXT("ship");
	TraderAccount.AvailableBalance = 200;
	State.CreditAccounts.Add(TraderAccount);

	FCreditAccountRecord PirateAccount;
	PirateAccount.AccountId = TEXT("account_pirate_raider_01");
	PirateAccount.OwnerId = TEXT("pirate_raider_01");
	PirateAccount.OwnerType = TEXT("ship");
	PirateAccount.AvailableBalance = 0;
	State.CreditAccounts.Add(PirateAccount);

	FInterdictionHazardRecord Hazard;
	Hazard.HazardId = HazardId;
	Hazard.SourceEventId = SourceEventId;
	Hazard.RouteSegmentId = RouteId;
	Hazard.OwnerGroupId = TEXT("pirate_group_01");
	Hazard.TargetShipId = TEXT("trader_brink_01");
	Hazard.State = TEXT("pending");
	Hazard.ExpiresAtTimeSeconds = 600.0;
	Hazard.SaveLoadEventId = SourceEventId;
	Hazard.IdempotencyKey = TEXT("idem_m10_interdiction_trade_lane_01");
	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteId, 0.55, Clock, 0.0, Hazard.RouteSample);
	State.InterdictionHazards.Add(Hazard);

	FDistressEventRecord Distress;
	Distress.DistressEventId = DistressEventId;
	Distress.SourceEventId = SourceEventId;
	Distress.SourceShipId = TEXT("trader_brink_01");
	Distress.ThreatId = TEXT("pirate_raider_01");
	Distress.LocationTarget.TargetId = RouteId;
	Distress.LocationTarget.TargetType = TEXT("route_sample");
	Distress.LocationTarget.RouteSegmentId = RouteId;
	Distress.RespondingPatrolReservationIds = { Reservation.ReservationId };
	Distress.MessageInstanceIds = { TEXT("message_distress_trade_lane_01") };
	Distress.IdempotencyKey = TEXT("idem_m10_distress_trade_lane_01");
	State.DistressEvents.Add(Distress);

	FMessageDefinition DistressMessage;
	DistressMessage.MessageId = TEXT("msg_distress_trade_lane_01");
	DistressMessage.MessageType = TEXT("distress");
	DistressMessage.SpeakerId = TEXT("service_brink_watch_market");
	DistressMessage.TextKey = TEXT("frontier.distress.trade_lane");
	State.MessageDefinitions.Add(DistressMessage);

	FMessageDefinition PirateDemandMessage;
	PirateDemandMessage.MessageId = TEXT("msg_pirate_demand_01");
	PirateDemandMessage.MessageType = TEXT("pirate_demand");
	PirateDemandMessage.SpeakerId = TEXT("service_brink_watch_market");
	PirateDemandMessage.TextKey = TEXT("frontier.pirate.demand");
	State.MessageDefinitions.Add(PirateDemandMessage);

	FMessageLogEntry DistressLog;
	DistressLog.MessageInstanceId = TEXT("message_distress_trade_lane_01");
	DistressLog.MessageId = DistressMessage.MessageId;
	DistressLog.SourceEndpointId = TEXT("service_brink_watch_market");
	DistressLog.TargetId = TEXT("patrol_frontier_local_01");
	DistressLog.DeliveryChannel = TEXT("cockpit_comms");
	DistressLog.SourceEventId = SourceEventId;
	State.MessageLog.Add(DistressLog);

	FLogicalContactTrack Contact;
	Contact.ContactId = TEXT("contact_pirate_trade_lane_01");
	Contact.SourceShipId = TEXT("trader_brink_01");
	Contact.TargetShipId = TEXT("pirate_raider_01");
	Contact.RouteSegmentId = RouteId;
	Contact.LastKnownTarget.TargetId = RouteId;
	Contact.LastKnownTarget.TargetType = TEXT("route_sample");
	Contact.LastKnownTarget.RouteSegmentId = RouteId;
	Contact.Confidence = 0.9;
	State.LogicalContactTracks.Add(Contact);

	FLogicalEncounterRecord Encounter;
	Encounter.EncounterId = EncounterId;
	Encounter.EncounterType = TEXT("pirate_interdiction");
	Encounter.SourceEventId = SourceEventId;
	Encounter.RouteSegmentId = RouteId;
	Encounter.ParticipantShipIds = EncounterEvent.ParticipantShipIds;
	Encounter.ParticipantGroupIds = EncounterEvent.ParticipantGroupIds;
	Encounter.InterdictionHazardId = HazardId;
	Encounter.DistressEventId = DistressEventId;
	Encounter.EngagementId = EngagementId;
	Encounter.ProcessedWatermark = 1;
	Encounter.IdempotencyKey = EncounterEvent.IdempotencyKey;
	State.LogicalEncounters.Add(Encounter);

	FLogicalActorPromotionRecord Promotion;
	Promotion.PromotionId = TEXT("promotion_attach_pirate_trade_lane_01");
	Promotion.EncounterId = EncounterId;
	Promotion.SourceEventId = SourceEventId;
	Promotion.ShipInstanceId = TEXT("pirate_raider_01");
	Promotion.RealizationToken = TEXT("m10_future_realization_token");
	Promotion.bCanResolveEncounter = false;
	Promotion.IdempotencyKey = TEXT("idem_m10_promotion_attach_pirate_trade_lane_01");
	State.ActorPromotionAttachments.Add(Promotion);

	return State;
}

bool USystemicGameplayQueryService::ValidateSystemicGameplayState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	if (State.Factions.IsEmpty() || State.Jurisdictions.IsEmpty() || State.Items.IsEmpty() || State.Containers.IsEmpty() || State.Markets.IsEmpty())
	{
		return Fail(TEXT("Systemic state is missing required M9 factions, jurisdictions, inventory, or market records."));
	}

	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		if (Faction.FactionId.IsNone() || FactionIds.Contains(Faction.FactionId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate faction ID '%s'."), *Faction.FactionId.ToString()));
		}
		FactionIds.Add(Faction.FactionId);
	}

	TSet<FName> JurisdictionIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		if (Jurisdiction.JurisdictionId.IsNone() || JurisdictionIds.Contains(Jurisdiction.JurisdictionId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate jurisdiction ID '%s'."), *Jurisdiction.JurisdictionId.ToString()));
		}
		if (!Jurisdiction.SystemId.IsNone() && Jurisdiction.SystemId != SystemDefinition.SystemId)
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' belongs to system '%s', expected '%s'."),
				*Jurisdiction.JurisdictionId.ToString(),
				*Jurisdiction.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString()));
		}
		if (!FactionIds.Contains(Jurisdiction.AuthorityFactionId))
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' authority faction does not resolve."), *Jurisdiction.JurisdictionId.ToString()));
		}
		JurisdictionIds.Add(Jurisdiction.JurisdictionId);
	}
	TSet<FName> LawProfileIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		if (Jurisdiction.LawProfileId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' is missing a law profile ID."), *Jurisdiction.JurisdictionId.ToString()));
		}
		LawProfileIds.Add(Jurisdiction.LawProfileId);
	}
	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	TSet<FName> StationIds;
	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		StationIds.Add(Station.StationId);
	}
	for (const FFactionRelationshipRecord& Relationship : State.FactionRelationships)
	{
		if (!FactionIds.Contains(Relationship.SourceFactionId) || !FactionIds.Contains(Relationship.TargetFactionId))
		{
			return Fail(FString::Printf(TEXT("Faction relationship '%s -> %s' does not resolve both factions."),
				*Relationship.SourceFactionId.ToString(),
				*Relationship.TargetFactionId.ToString()));
		}
	}
	for (const FFactionOperationalState& Operation : State.FactionOperations)
	{
		if (!FactionIds.Contains(Operation.FactionId))
		{
			return Fail(FString::Printf(TEXT("Faction operation '%s' does not resolve its faction."), *Operation.FactionId.ToString()));
		}
		if (!Operation.SystemId.IsNone() && Operation.SystemId != SystemDefinition.SystemId)
		{
			return Fail(FString::Printf(TEXT("Faction operation '%s' belongs to system '%s', expected '%s'."),
				*Operation.FactionId.ToString(),
				*Operation.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString()));
		}
	}

	TSet<FName> ItemIds;
	for (const FItemDefinition& Item : State.Items)
	{
		if (Item.ItemId.IsNone() || ItemIds.Contains(Item.ItemId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate item ID '%s'."), *Item.ItemId.ToString()));
		}
		ItemIds.Add(Item.ItemId);
	}

	TSet<FName> ContainerIds;
	for (const FContainerState& Container : State.Containers)
	{
		if (Container.ContainerId.IsNone() || ContainerIds.Contains(Container.ContainerId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate container ID '%s'."), *Container.ContainerId.ToString()));
		}
		if (Container.OwnerId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Container '%s' is missing an owner ID."), *Container.ContainerId.ToString()));
		}
		if (Container.CapacityMassKg <= 0.0 || Container.CapacityVolumeM3 <= 0.0)
		{
			return Fail(FString::Printf(TEXT("Container '%s' has invalid cargo capacity."), *Container.ContainerId.ToString()));
		}
		for (const FItemStackState& Stack : Container.Stacks)
		{
			if (Stack.StackId.IsNone() || !ItemIds.Contains(Stack.ItemId) || Stack.Quantity < 0)
			{
				return Fail(FString::Printf(TEXT("Container '%s' has an invalid cargo stack '%s'."), *Container.ContainerId.ToString(), *Stack.StackId.ToString()));
			}
			if (!Stack.OwnerFactionId.IsNone() && !FactionIds.Contains(Stack.OwnerFactionId))
			{
				return Fail(FString::Printf(TEXT("Cargo stack '%s' owner faction does not resolve."), *Stack.StackId.ToString()));
			}
		}
		ContainerIds.Add(Container.ContainerId);
	}

	TSet<FName> CommodityIds;
	for (const FCommodityDefinition& Commodity : State.Commodities)
	{
		if (Commodity.CommodityId.IsNone() || CommodityIds.Contains(Commodity.CommodityId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate commodity ID '%s'."), *Commodity.CommodityId.ToString()));
		}
		CommodityIds.Add(Commodity.CommodityId);
	}

	TSet<FName> BridgedCommodityIds;
	for (const FCommodityItemBridge& Bridge : State.CommodityItemBridges)
	{
		if (!CommodityIds.Contains(Bridge.CommodityId) || !ItemIds.Contains(Bridge.ItemId) || BridgedCommodityIds.Contains(Bridge.CommodityId))
		{
			return Fail(FString::Printf(TEXT("Commodity bridge '%s' is invalid or duplicated."), *Bridge.CommodityItemBridgeId.ToString()));
		}
		BridgedCommodityIds.Add(Bridge.CommodityId);
	}

	TSet<FName> MarketIds;
	for (const FStationMarketState& Market : State.Markets)
	{
		if (Market.MarketId.IsNone() || MarketIds.Contains(Market.MarketId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate market ID '%s'."), *Market.MarketId.ToString()));
		}
		if (Market.MarketId != Market.StationId)
		{
			return Fail(FString::Printf(TEXT("Market '%s' violates the M9 MarketId = StationId identity rule."), *Market.MarketId.ToString()));
		}
		for (const TPair<FName, int32>& Stock : Market.StockByCommodity)
		{
			if (!CommodityIds.Contains(Stock.Key) || !BridgedCommodityIds.Contains(Stock.Key) || Stock.Value < 0)
			{
				return Fail(FString::Printf(TEXT("Market '%s' has invalid stock for commodity '%s'."), *Market.MarketId.ToString(), *Stock.Key.ToString()));
			}
		}
		MarketIds.Add(Market.MarketId);
	}

	TSet<FName> AccountIds;
	for (const FCreditAccountRecord& Account : State.CreditAccounts)
	{
		if (Account.AccountId.IsNone() || AccountIds.Contains(Account.AccountId) || Account.OwnerId.IsNone() || Account.AvailableBalance < 0 || Account.HeldBalance < 0)
		{
			return Fail(FString::Printf(TEXT("Credit account '%s' is invalid."), *Account.AccountId.ToString()));
		}
		AccountIds.Add(Account.AccountId);
	}

	TSet<FName> CommsEndpointIds;
	for (const FCommsEndpointDefinition& Comms : State.CommsEndpoints)
	{
		if (Comms.EndpointId.IsNone() || CommsEndpointIds.Contains(Comms.EndpointId) || Comms.OwnerId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Comms endpoint '%s' is invalid."), *Comms.EndpointId.ToString()));
		}
		CommsEndpointIds.Add(Comms.EndpointId);
	}

	TSet<FName> ServiceEndpointIds;
	for (const FStationServiceEndpointDefinition& Endpoint : State.StationServiceEndpoints)
	{
		if (Endpoint.ServiceEndpointId.IsNone() || ServiceEndpointIds.Contains(Endpoint.ServiceEndpointId))
		{
			return Fail(FString::Printf(TEXT("Station service endpoint '%s' is empty or duplicated."), *Endpoint.ServiceEndpointId.ToString()));
		}
		if ((!Endpoint.StationId.IsNone() && !StationIds.Contains(Endpoint.StationId)) ||
			!FactionIds.Contains(Endpoint.ProviderFactionId) ||
			(!Endpoint.CommsEndpointId.IsNone() && !CommsEndpointIds.Contains(Endpoint.CommsEndpointId)) ||
			(!Endpoint.MarketId.IsNone() && !MarketIds.Contains(Endpoint.MarketId)) ||
			(!Endpoint.InventoryContainerId.IsNone() && !ContainerIds.Contains(Endpoint.InventoryContainerId)) ||
			(!Endpoint.CreditAccountId.IsNone() && !AccountIds.Contains(Endpoint.CreditAccountId)) ||
			(!Endpoint.LegalPolicyId.IsNone() && !LawProfileIds.Contains(Endpoint.LegalPolicyId)))
		{
			return Fail(FString::Printf(TEXT("Station service endpoint '%s' has unresolved systemic references."), *Endpoint.ServiceEndpointId.ToString()));
		}
		ServiceEndpointIds.Add(Endpoint.ServiceEndpointId);
	}

	TSet<FName> MessageIds;
	for (const FMessageDefinition& Message : State.MessageDefinitions)
	{
		if (Message.MessageId.IsNone() || MessageIds.Contains(Message.MessageId) || !ServiceEndpointIds.Contains(Message.SpeakerId))
		{
			return Fail(FString::Printf(TEXT("Message definition '%s' is invalid or has an unresolved speaker."), *Message.MessageId.ToString()));
		}
		MessageIds.Add(Message.MessageId);
	}
	for (const FMessageLogEntry& MessageLog : State.MessageLog)
	{
		if (MessageLog.MessageInstanceId.IsNone() || !MessageIds.Contains(MessageLog.MessageId) || !ServiceEndpointIds.Contains(MessageLog.SourceEndpointId))
		{
			return Fail(FString::Printf(TEXT("Message log entry '%s' has unresolved message or endpoint references."), *MessageLog.MessageInstanceId.ToString()));
		}
	}

	for (const FCreditLedgerEntry& Ledger : State.CreditLedger)
	{
		if (Ledger.LedgerEntryId.IsNone() || !AccountIds.Contains(Ledger.DebitAccountId) || !AccountIds.Contains(Ledger.CreditAccountId) || Ledger.Amount <= 0 || Ledger.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Credit ledger entry '%s' is invalid."), *Ledger.LedgerEntryId.ToString()));
		}
	}

	TSet<FName> TransactionIds;
	TSet<FName> TransactionIdempotencyKeys;
	for (const FGameplayTransactionRecord& Transaction : State.Transactions)
	{
		if (Transaction.TransactionId.IsNone() || Transaction.IdempotencyKey.IsNone() ||
			TransactionIds.Contains(Transaction.TransactionId) || TransactionIdempotencyKeys.Contains(Transaction.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Gameplay transaction '%s' has invalid transaction or idempotency IDs."), *Transaction.TransactionId.ToString()));
		}
		TransactionIds.Add(Transaction.TransactionId);
		TransactionIdempotencyKeys.Add(Transaction.IdempotencyKey);
	}

	for (const FGameplayTransactionJournalEntry& Entry : State.TransactionJournal)
	{
		if (Entry.JournalEntryId.IsNone() || !TransactionIds.Contains(Entry.TransactionId) || Entry.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Transaction journal entry '%s' is invalid."), *Entry.JournalEntryId.ToString()));
		}
	}

	TSet<FName> CargoTransferIdempotencyKeys;
	for (const FCargoTransferResult& Result : State.CargoTransferResults)
	{
		if (Result.TransferId.IsNone() || Result.RequestId.IsNone() || Result.IdempotencyKey.IsNone() || CargoTransferIdempotencyKeys.Contains(Result.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Cargo transfer result '%s' has invalid durable IDs."), *Result.TransferId.ToString()));
		}
		CargoTransferIdempotencyKeys.Add(Result.IdempotencyKey);
	}

	TSet<FName> MarketTransactionIdempotencyKeys;
	for (const FMarketTransactionResult& Result : State.MarketTransactionResults)
	{
		if (Result.TransactionId.IsNone() || Result.IdempotencyKey.IsNone() || MarketTransactionIdempotencyKeys.Contains(Result.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Market transaction result '%s' has invalid durable IDs."), *Result.TransactionId.ToString()));
		}
		MarketTransactionIdempotencyKeys.Add(Result.IdempotencyKey);
	}

	TSet<FName> MissionOfferIds;
	for (const FMissionOfferRecord& Offer : State.MissionOffers)
	{
		if (Offer.OfferId.IsNone() || MissionOfferIds.Contains(Offer.OfferId) ||
			!StationIds.Contains(Offer.SourceStationId) ||
			!ServiceEndpointIds.Contains(Offer.SourceServiceEndpointId) ||
			!FactionIds.Contains(Offer.IssuerFactionId))
		{
			return Fail(FString::Printf(TEXT("Mission offer '%s' has unresolved station, service, or faction references."), *Offer.OfferId.ToString()));
		}
		MissionOfferIds.Add(Offer.OfferId);
	}

	TSet<FName> MissionInstanceIds;
	TSet<FName> ObjectiveIdsDeclaredByMissions;
	for (const FMissionInstanceState& Mission : State.MissionInstances)
	{
		if (Mission.MissionInstanceId.IsNone() || MissionInstanceIds.Contains(Mission.MissionInstanceId) ||
			!MissionOfferIds.Contains(Mission.OfferId) ||
			!FactionIds.Contains(Mission.IssuerFactionId) ||
			Mission.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Mission instance '%s' has unresolved offer, faction, or durable IDs."), *Mission.MissionInstanceId.ToString()));
		}
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			ObjectiveIdsDeclaredByMissions.Add(ObjectiveStateId);
		}
		MissionInstanceIds.Add(Mission.MissionInstanceId);
	}

	TSet<FName> ObjectiveStateIds;
	for (const FObjectiveState& Objective : State.ObjectiveStates)
	{
		if (Objective.ObjectiveStateId.IsNone() || ObjectiveStateIds.Contains(Objective.ObjectiveStateId) ||
			!MissionInstanceIds.Contains(Objective.MissionInstanceId) ||
			!JurisdictionIds.Contains(Objective.JurisdictionId) ||
			!ObjectiveIdsDeclaredByMissions.Contains(Objective.ObjectiveStateId))
		{
			return Fail(FString::Printf(TEXT("Objective state '%s' has unresolved mission or jurisdiction references."), *Objective.ObjectiveStateId.ToString()));
		}
		for (const FName RouteId : Objective.RouteSegmentIds)
		{
			if (!RouteIds.Contains(RouteId))
			{
				return Fail(FString::Printf(TEXT("Objective state '%s' references missing route '%s'."), *Objective.ObjectiveStateId.ToString(), *RouteId.ToString()));
			}
		}
		ObjectiveStateIds.Add(Objective.ObjectiveStateId);
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ValidateLogicalEncounterState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	FString BaseFailure;
	if (!ValidateSystemicGameplayState(SystemDefinition, State, BaseFailure))
	{
		return Fail(BaseFailure);
	}
	if (State.LogicalContactTracks.IsEmpty() || State.RouteSecuritySnapshots.IsEmpty() || State.PatrolReservations.IsEmpty() ||
		State.InterdictionHazards.IsEmpty() || State.DistressEvents.IsEmpty() || State.LogicalEncounters.IsEmpty() ||
		State.ActorPromotionAttachments.IsEmpty())
	{
		return Fail(TEXT("M10 logical encounter state is missing required contacts, security snapshots, patrol reservations, hazards, distress records, encounters, or promotion attachment metadata."));
	}

	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	TSet<FName> ShipIds;
	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		ShipIds.Add(Ship.ShipInstanceId);
	}
	TSet<FName> GroupIds;
	for (const FShipGroupState& Group : SystemDefinition.ShipGroups)
	{
		GroupIds.Add(Group.GroupId);
	}
	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		FactionIds.Add(Faction.FactionId);
	}
	TSet<FName> JurisdictionIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		JurisdictionIds.Add(Jurisdiction.JurisdictionId);
	}
	TSet<FName> EventIds;
	for (const FSimulationEventRecord& Event : State.Events)
	{
		if (Event.EventId.IsNone() || EventIds.Contains(Event.EventId) || Event.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 event '%s' has an empty, duplicate, or non-idempotent event ID."), *Event.EventId.ToString()));
		}
		EventIds.Add(Event.EventId);
	}
	TSet<FName> MessageInstanceIds;
	for (const FMessageLogEntry& Message : State.MessageLog)
	{
		MessageInstanceIds.Add(Message.MessageInstanceId);
	}
	TSet<FName> CargoTransferIds;
	for (const FCargoTransferResult& Cargo : State.CargoTransferResults)
	{
		CargoTransferIds.Add(Cargo.TransferId);
	}
	TSet<FName> GameplayTransactionIds;
	for (const FGameplayTransactionRecord& Transaction : State.Transactions)
	{
		GameplayTransactionIds.Add(Transaction.TransactionId);
	}
	TSet<FName> CreditLedgerIds;
	for (const FCreditLedgerEntry& Ledger : State.CreditLedger)
	{
		CreditLedgerIds.Add(Ledger.LedgerEntryId);
	}
	TSet<FName> MarketTransactionIds;
	for (const FMarketTransactionResult& Market : State.MarketTransactionResults)
	{
		MarketTransactionIds.Add(Market.TransactionId);
	}
	TSet<FName> OffenseIds;
	for (const FOffenseEvent& Offense : State.Offenses)
	{
		OffenseIds.Add(Offense.OffenseId);
	}
	TSet<FName> EvidenceIds;
	for (const FEvidenceRecord& Evidence : State.EvidenceRecords)
	{
		EvidenceIds.Add(Evidence.EvidenceId);
	}
	TSet<FName> CriminalRecordIds;
	for (const FCriminalRecord& Criminal : State.CriminalRecords)
	{
		CriminalRecordIds.Add(Criminal.CriminalRecordId);
	}

	for (const FLogicalContactTrack& Contact : State.LogicalContactTracks)
	{
		if (Contact.ContactId.IsNone() || !ShipIds.Contains(Contact.SourceShipId) || !ShipIds.Contains(Contact.TargetShipId) ||
			!RouteIds.Contains(Contact.RouteSegmentId) || Contact.LastKnownTarget.RouteSegmentId != Contact.RouteSegmentId ||
			Contact.Confidence < 0.0 || Contact.Confidence > 1.0)
		{
			return Fail(FString::Printf(TEXT("M10 contact '%s' has unresolved ships, route sample, or confidence."), *Contact.ContactId.ToString()));
		}
	}

	for (const FRouteSecuritySnapshot& Security : State.RouteSecuritySnapshots)
	{
		if (Security.SnapshotId.IsNone() || !RouteIds.Contains(Security.RouteSegmentId) || !JurisdictionIds.Contains(Security.JurisdictionId) ||
			!EventIds.Contains(Security.SourceEventId) || Security.WindowEndTimeSeconds <= Security.WindowStartTimeSeconds ||
			Security.ProcessedWatermark <= 0 || Security.SecurityRating < 0.0 || Security.SecurityRating > 1.0 ||
			Security.PatrolCoverageScore < 0.0 || Security.PatrolCoverageScore > 1.0 ||
			Security.PirateRiskScore < 0.0 || Security.PirateRiskScore > 1.0 || Security.RouteValue < 0.0)
		{
			return Fail(FString::Printf(TEXT("M10 route security snapshot '%s' is incomplete or has unresolved references."), *Security.SnapshotId.ToString()));
		}
	}

	TSet<FName> PatrolReservationIds;
	TSet<FName> PatrolIdempotencyKeys;
	for (const FPatrolReservationRecord& Reservation : State.PatrolReservations)
	{
		const FFactionOperationalState* Operation = FindConst(State.FactionOperations, [&Reservation](const FFactionOperationalState& Candidate)
		{
			return Candidate.FactionId == Reservation.FactionId && Candidate.PatrolAssetIds.Contains(Reservation.PatrolAssetId);
		});
		if (Reservation.ReservationId.IsNone() || PatrolReservationIds.Contains(Reservation.ReservationId) ||
			Reservation.IdempotencyKey.IsNone() || PatrolIdempotencyKeys.Contains(Reservation.IdempotencyKey) ||
			!FactionIds.Contains(Reservation.FactionId) || !JurisdictionIds.Contains(Reservation.JurisdictionId) ||
			!RouteIds.Contains(Reservation.RouteSegmentId) || !EventIds.Contains(Reservation.SourceEventId) ||
			!Operation || Reservation.ExpiresAtTimeSeconds <= 0.0 || Reservation.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 patrol reservation '%s' does not resolve to a patrol asset, jurisdiction, route, event, expiry, and idempotency key."), *Reservation.ReservationId.ToString()));
		}
		PatrolReservationIds.Add(Reservation.ReservationId);
		PatrolIdempotencyKeys.Add(Reservation.IdempotencyKey);
	}

	TSet<FName> HazardIds;
	TSet<FName> HazardIdempotencyKeys;
	for (const FInterdictionHazardRecord& Hazard : State.InterdictionHazards)
	{
		if (Hazard.HazardId.IsNone() || HazardIds.Contains(Hazard.HazardId) ||
			Hazard.IdempotencyKey.IsNone() || HazardIdempotencyKeys.Contains(Hazard.IdempotencyKey) ||
			!EventIds.Contains(Hazard.SourceEventId) || !RouteIds.Contains(Hazard.RouteSegmentId) ||
			!GroupIds.Contains(Hazard.OwnerGroupId) || !ShipIds.Contains(Hazard.TargetShipId) ||
			Hazard.RouteSample.RouteSegmentId != Hazard.RouteSegmentId || Hazard.ExpiresAtTimeSeconds <= 0.0 ||
			!EventIds.Contains(Hazard.SaveLoadEventId) || Hazard.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 interdiction hazard '%s' must reference a route sample, owner group, target ship, expiry, save event, and idempotency key."), *Hazard.HazardId.ToString()));
		}
		HazardIds.Add(Hazard.HazardId);
		HazardIdempotencyKeys.Add(Hazard.IdempotencyKey);
	}

	TSet<FName> DistressIds;
	TSet<FName> DistressIdempotencyKeys;
	for (const FDistressEventRecord& Distress : State.DistressEvents)
	{
		if (Distress.DistressEventId.IsNone() || DistressIds.Contains(Distress.DistressEventId) ||
			Distress.IdempotencyKey.IsNone() || DistressIdempotencyKeys.Contains(Distress.IdempotencyKey) ||
			!EventIds.Contains(Distress.SourceEventId) || !ShipIds.Contains(Distress.SourceShipId) ||
			!ShipIds.Contains(Distress.ThreatId) || !RouteIds.Contains(Distress.LocationTarget.RouteSegmentId) || Distress.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 distress record '%s' has unresolved source, threat, route location, state, or idempotency references."), *Distress.DistressEventId.ToString()));
		}
		for (const FName ReservationId : Distress.RespondingPatrolReservationIds)
		{
			if (!PatrolReservationIds.Contains(ReservationId))
			{
				return Fail(FString::Printf(TEXT("M10 distress record '%s' references missing patrol reservation '%s'."), *Distress.DistressEventId.ToString(), *ReservationId.ToString()));
			}
		}
		for (const FName MessageInstanceId : Distress.MessageInstanceIds)
		{
			if (!MessageInstanceIds.Contains(MessageInstanceId))
			{
				return Fail(FString::Printf(TEXT("M10 distress record '%s' references missing message '%s'."), *Distress.DistressEventId.ToString(), *MessageInstanceId.ToString()));
			}
		}
		DistressIds.Add(Distress.DistressEventId);
		DistressIdempotencyKeys.Add(Distress.IdempotencyKey);
	}

	TSet<FName> EncounterIds;
	TSet<FName> EngagementIds;
	TSet<FName> EncounterIdempotencyKeys;
	for (const FAbstractEngagementRecord& Engagement : State.AbstractEngagements)
	{
		EngagementIds.Add(Engagement.EngagementId);
	}
	for (const FLogicalEncounterRecord& Encounter : State.LogicalEncounters)
	{
		if (Encounter.EncounterId.IsNone() || EncounterIds.Contains(Encounter.EncounterId) ||
			Encounter.IdempotencyKey.IsNone() || EncounterIdempotencyKeys.Contains(Encounter.IdempotencyKey) ||
			!EventIds.Contains(Encounter.SourceEventId) || !RouteIds.Contains(Encounter.RouteSegmentId) ||
			!HazardIds.Contains(Encounter.InterdictionHazardId) || !DistressIds.Contains(Encounter.DistressEventId) ||
			Encounter.ProcessedWatermark <= 0 || Encounter.bRequiresActor || Encounter.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 logical encounter '%s' is incomplete, actor-bound, duplicate, or has unresolved event/route/hazard/distress references."), *Encounter.EncounterId.ToString()));
		}
		for (const FName ShipId : Encounter.ParticipantShipIds)
		{
			if (!ShipIds.Contains(ShipId))
			{
				return Fail(FString::Printf(TEXT("M10 logical encounter '%s' references missing ship '%s'."), *Encounter.EncounterId.ToString(), *ShipId.ToString()));
			}
		}
		for (const FName GroupId : Encounter.ParticipantGroupIds)
		{
			if (!GroupIds.Contains(GroupId))
			{
				return Fail(FString::Printf(TEXT("M10 logical encounter '%s' references missing group '%s'."), *Encounter.EncounterId.ToString(), *GroupId.ToString()));
			}
		}
		if (Encounter.State == FName(TEXT("resolved")) && !EngagementIds.Contains(Encounter.EngagementId))
		{
			return Fail(FString::Printf(TEXT("M10 resolved encounter '%s' is missing its abstract engagement output."), *Encounter.EncounterId.ToString()));
		}
		EncounterIds.Add(Encounter.EncounterId);
		EncounterIdempotencyKeys.Add(Encounter.IdempotencyKey);
	}

	TSet<FName> EngagementIdempotencyKeys;
	for (const FAbstractEngagementRecord& Engagement : State.AbstractEngagements)
	{
		if (Engagement.EngagementId.IsNone() || Engagement.IdempotencyKey.IsNone() || EngagementIdempotencyKeys.Contains(Engagement.IdempotencyKey) ||
			!EncounterIds.Contains(Engagement.EncounterId) || !EventIds.Contains(Engagement.SourceEventId) ||
			!ShipIds.Contains(Engagement.AttackerId) || !ShipIds.Contains(Engagement.DefenderId) || Engagement.OutcomeType.IsNone() || Engagement.bRequiresActor)
		{
			return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' is incomplete, actor-bound, duplicate, or has unresolved encounter/event/ship references."), *Engagement.EngagementId.ToString()));
		}
		for (const FName CargoId : Engagement.CargoTransferResultIds)
		{
			if (!CargoTransferIds.Contains(CargoId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing cargo result '%s'."), *Engagement.EngagementId.ToString(), *CargoId.ToString()));
			}
		}
		for (const FName MarketId : Engagement.MarketTransactionResultIds)
		{
			if (!MarketTransactionIds.Contains(MarketId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing market result '%s'."), *Engagement.EngagementId.ToString(), *MarketId.ToString()));
			}
		}
		for (const FName TransactionId : Engagement.GameplayTransactionIds)
		{
			if (!GameplayTransactionIds.Contains(TransactionId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing gameplay transaction '%s'."), *Engagement.EngagementId.ToString(), *TransactionId.ToString()));
			}
		}
		for (const FName LedgerId : Engagement.CreditLedgerEntryIds)
		{
			if (!CreditLedgerIds.Contains(LedgerId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing credit ledger entry '%s'."), *Engagement.EngagementId.ToString(), *LedgerId.ToString()));
			}
		}
		for (const FName OffenseId : Engagement.OffenseIds)
		{
			if (!OffenseIds.Contains(OffenseId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing offense '%s'."), *Engagement.EngagementId.ToString(), *OffenseId.ToString()));
			}
		}
		for (const FName EvidenceId : Engagement.EvidenceIds)
		{
			if (!EvidenceIds.Contains(EvidenceId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing evidence '%s'."), *Engagement.EngagementId.ToString(), *EvidenceId.ToString()));
			}
		}
		for (const FName CriminalId : Engagement.CriminalRecordIds)
		{
			if (!CriminalRecordIds.Contains(CriminalId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing criminal record '%s'."), *Engagement.EngagementId.ToString(), *CriminalId.ToString()));
			}
		}
		for (const FName MessageId : Engagement.MessageInstanceIds)
		{
			if (!MessageInstanceIds.Contains(MessageId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing message '%s'."), *Engagement.EngagementId.ToString(), *MessageId.ToString()));
			}
		}
		EngagementIdempotencyKeys.Add(Engagement.IdempotencyKey);
	}

	TSet<FName> PromotionIds;
	TSet<FName> PromotionIdempotencyKeys;
	for (const FLogicalActorPromotionRecord& Promotion : State.ActorPromotionAttachments)
	{
		if (Promotion.PromotionId.IsNone() || PromotionIds.Contains(Promotion.PromotionId) ||
			Promotion.IdempotencyKey.IsNone() || PromotionIdempotencyKeys.Contains(Promotion.IdempotencyKey) ||
			!EncounterIds.Contains(Promotion.EncounterId) || !EventIds.Contains(Promotion.SourceEventId) ||
			!ShipIds.Contains(Promotion.ShipInstanceId) || Promotion.RealizationToken.IsNone() || Promotion.bCanResolveEncounter)
		{
			return Fail(FString::Printf(TEXT("M10 actor promotion '%s' must attach metadata to an existing logical encounter/event/ship and must not be allowed to resolve the encounter."), *Promotion.PromotionId.ToString()));
		}
		PromotionIds.Add(Promotion.PromotionId);
		PromotionIdempotencyKeys.Add(Promotion.IdempotencyKey);
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ApplySimulationEventOnce(FSystemicGameplayState& InOutState, const FSimulationEventRecord& Event, double AppliedTimeSeconds, FSimulationEventResultRecord& OutResult)
{
	if (Event.EventId.IsNone() || Event.IdempotencyKey.IsNone())
	{
		OutResult = FSimulationEventResultRecord();
		OutResult.EventId = Event.EventId;
		OutResult.OutcomeType = TEXT("rejected");
		OutResult.DebugReason = TEXT("EventId and IdempotencyKey are required.");
		return false;
	}

	if (const FSimulationEventResultRecord* Existing = FindConst(InOutState.EventResults, [&Event](const FSimulationEventResultRecord& Candidate)
	{
		return Candidate.EventId == Event.EventId || Candidate.IdempotencyKey == Event.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		OutResult.OutcomeType = TEXT("duplicate");
		return true;
	}

	if (!FindConst(InOutState.Events, [&Event](const FSimulationEventRecord& Candidate)
	{
		return Candidate.EventId == Event.EventId;
	}))
	{
		InOutState.Events.Add(Event);
	}
	OutResult.EventId = Event.EventId;
	OutResult.ResultId = MakeId(TEXT("result"), Event.EventId);
	OutResult.AppliedTimeSeconds = AppliedTimeSeconds;
	OutResult.OutcomeType = TEXT("accepted");
	OutResult.DebugReason = TEXT("Canonical event accepted once.");
	OutResult.IdempotencyKey = Event.IdempotencyKey;
	InOutState.EventResults.Add(OutResult);
	return true;
}

bool USystemicGameplayQueryService::RecordOffense(FSystemicGameplayState& InOutState, const FOffenseEvent& Offense, FEvidenceRecord& OutEvidence, FCriminalRecord& OutCriminalRecord, FString& OutFailureReason)
{
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [&Offense](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Offense.JurisdictionId;
	});
	if (!Jurisdiction)
	{
		OutFailureReason = TEXT("Offense jurisdiction does not resolve.");
		return false;
	}
	if (!FindConst(InOutState.Factions, [Jurisdiction](const FFactionDefinition& Candidate)
	{
		return Candidate.FactionId == Jurisdiction->AuthorityFactionId;
	}))
	{
		OutFailureReason = TEXT("Offense authority faction does not resolve.");
		return false;
	}

	if (!FindConst(InOutState.Offenses, [&Offense](const FOffenseEvent& Candidate) { return Candidate.OffenseId == Offense.OffenseId; }))
	{
		InOutState.Offenses.Add(Offense);
	}

	OutEvidence.EvidenceId = MakeId(TEXT("evidence"), Offense.OffenseId);
	OutEvidence.OffenseId = Offense.OffenseId;
	OutEvidence.WitnessType = TEXT("systemic_sensor");
	OutEvidence.WitnessId = Offense.JurisdictionId;
	OutEvidence.SourceEventId = Offense.SourceEventId;
	OutEvidence.EvidenceState = TEXT("reported");
	if (!FindConst(InOutState.EvidenceRecords, [&OutEvidence](const FEvidenceRecord& Candidate) { return Candidate.EvidenceId == OutEvidence.EvidenceId; }))
	{
		InOutState.EvidenceRecords.Add(OutEvidence);
	}

	const FName CriminalRecordId = FName(*FString::Printf(TEXT("criminal_%s_%s"), *Offense.JurisdictionId.ToString(), *Offense.OffenderId.ToString()));
	FCriminalRecord* CriminalRecord = FindMutable(InOutState.CriminalRecords, [CriminalRecordId](const FCriminalRecord& Candidate)
	{
		return Candidate.CriminalRecordId == CriminalRecordId;
	});
	if (!CriminalRecord)
	{
		FCriminalRecord NewRecord;
		NewRecord.CriminalRecordId = CriminalRecordId;
		NewRecord.SubjectType = Offense.OffenderType;
		NewRecord.SubjectId = Offense.OffenderId;
		NewRecord.JurisdictionId = Offense.JurisdictionId;
		InOutState.CriminalRecords.Add(NewRecord);
		CriminalRecord = &InOutState.CriminalRecords.Last();
	}
	if (!CriminalRecord->OffenseIds.Contains(Offense.OffenseId))
	{
		CriminalRecord->OffenseIds.Add(Offense.OffenseId);
		CriminalRecord->WantedLevel += 1;
	}
	OutCriminalRecord = *CriminalRecord;
	return true;
}

bool USystemicGameplayQueryService::TransferCargo(FSystemicGameplayState& InOutState, const FCargoTransferRequest& Request, FCargoTransferResult& OutResult)
{
	OutResult = FCargoTransferResult();
	OutResult.TransferId = MakeId(TEXT("transfer"), Request.RequestId);
	OutResult.RequestId = Request.RequestId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;

	if (Request.RequestId.IsNone() || Request.IdempotencyKey.IsNone() || Request.Quantity <= 0)
	{
		OutResult.RejectedReason = TEXT("RequestId, IdempotencyKey, and positive quantity are required.");
		return false;
	}
	if (const FCargoTransferResult* Existing = FindConst(InOutState.CargoTransferResults, [&Request](const FCargoTransferResult& Candidate)
	{
		return Candidate.RequestId == Request.RequestId || Candidate.IdempotencyKey == Request.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		return Existing->Result == ESystemicActionResult::Accepted;
	}
	if (Request.SourceContainerId == Request.DestinationContainerId)
	{
		OutResult.RejectedReason = TEXT("Source and destination containers must be distinct.");
		return false;
	}

	FContainerState* Source = FindMutable(InOutState.Containers, [&Request](const FContainerState& Candidate) { return Candidate.ContainerId == Request.SourceContainerId; });
	FContainerState* Destination = FindMutable(InOutState.Containers, [&Request](const FContainerState& Candidate) { return Candidate.ContainerId == Request.DestinationContainerId; });
	if (!Source || !Destination)
	{
		OutResult.RejectedReason = TEXT("Source or destination container does not resolve.");
		return false;
	}
	if (Source->OwnerId.IsNone() || Destination->OwnerId.IsNone())
	{
		OutResult.RejectedReason = TEXT("Source and destination containers must have durable owner IDs.");
		return false;
	}
	if ((!Request.ExpectedSourceOwnerId.IsNone() && Source->OwnerId != Request.ExpectedSourceOwnerId) ||
		(!Request.ExpectedDestinationOwnerId.IsNone() && Destination->OwnerId != Request.ExpectedDestinationOwnerId))
	{
		OutResult.RejectedReason = TEXT("Cargo transfer owner validation failed.");
		return false;
	}

	FItemStackState* SourceStack = nullptr;
	if (!Request.StackIds.IsEmpty())
	{
		const FName RequestedStackId = Request.StackIds[0];
		SourceStack = Source->Stacks.FindByPredicate([RequestedStackId](const FItemStackState& Candidate) { return Candidate.StackId == RequestedStackId; });
	}
	else if (!Request.ItemId.IsNone())
	{
		SourceStack = Source->Stacks.FindByPredicate([&Request](const FItemStackState& Candidate) { return Candidate.ItemId == Request.ItemId; });
	}
	if (!SourceStack || SourceStack->Quantity < Request.Quantity)
	{
		OutResult.RejectedReason = TEXT("Requested cargo stack or quantity is unavailable.");
		return false;
	}
	const FItemDefinition* SourceItem = FindConst(InOutState.Items, [SourceStack](const FItemDefinition& Candidate)
	{
		return Candidate.ItemId == SourceStack->ItemId;
	});
	if (!SourceItem)
	{
		OutResult.RejectedReason = TEXT("Requested cargo item does not resolve.");
		return false;
	}
	if (!Request.LegalContextId.IsNone() && !FindConst(InOutState.Jurisdictions, [&Request](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.LawProfileId == Request.LegalContextId;
	}))
	{
		OutResult.RejectedReason = TEXT("Cargo transfer legal context does not resolve.");
		return false;
	}
	if (HasNamedGameplayTag(SourceStack->Flags, TEXT("mission_locked")) && Request.Reason != FName(TEXT("mission")))
	{
		OutResult.RejectedReason = TEXT("Mission-locked cargo cannot be transferred outside a mission transaction.");
		return false;
	}
	if ((HasNamedGameplayTag(SourceStack->Flags, TEXT("stolen")) ||
		HasNamedGameplayTag(SourceStack->Flags, TEXT("contraband")) ||
		!SourceItem->LegalityTags.IsEmpty()) &&
		Request.LegalContextId.IsNone())
	{
		OutResult.RejectedReason = TEXT("Restricted cargo requires an explicit legal context.");
		return false;
	}

	const double AddedMass = GetItemMass(InOutState, SourceStack->ItemId) * Request.Quantity;
	const double AddedVolume = GetItemVolume(InOutState, SourceStack->ItemId) * Request.Quantity;
	double CurrentMass = 0.0;
	double CurrentVolume = 0.0;
	GetContainerLoad(InOutState, *Destination, CurrentMass, CurrentVolume);
	if (CurrentMass + AddedMass > Destination->CapacityMassKg || CurrentVolume + AddedVolume > Destination->CapacityVolumeM3)
	{
		OutResult.RejectedReason = TEXT("Destination container capacity would be exceeded.");
		return false;
	}

	const FName SourceStackId = SourceStack->StackId;
	const FName SourceItemId = SourceStack->ItemId;
	const FGameplayTagContainer SourceFlags = SourceStack->Flags;
	const FName SourceOwnerFactionId = SourceStack->OwnerFactionId;
	SourceStack->Quantity -= Request.Quantity;
	FItemStackState NewStack;
	NewStack.StackId = FName(*FString::Printf(TEXT("%s_%s_%d"), *Destination->ContainerId.ToString(), *SourceItemId.ToString(), Destination->Stacks.Num() + 1));
	NewStack.ItemId = SourceItemId;
	NewStack.Quantity = Request.Quantity;
	NewStack.Flags = SourceFlags;
	NewStack.OwnerFactionId = SourceOwnerFactionId;
	Destination->Stacks.Add(NewStack);

	OutResult.Result = ESystemicActionResult::Accepted;
	OutResult.MovedStackIds.Add(SourceStackId);
	OutResult.CreatedStackIds.Add(NewStack.StackId);
	InOutState.CargoTransferResults.Add(OutResult);
	return true;
}

bool USystemicGameplayQueryService::ExecuteMarketTransaction(FSystemicGameplayState& InOutState, const FMarketTransactionRequest& Request, FMarketTransactionResult& OutResult)
{
	OutResult = FMarketTransactionResult();
	OutResult.TransactionId = Request.TransactionId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;
	if (Request.TransactionId.IsNone() || Request.IdempotencyKey.IsNone() || Request.Quantity <= 0 || Request.QuotedUnitPrice <= 0)
	{
		OutResult.RejectionReason = TEXT("TransactionId, IdempotencyKey, positive quantity, and positive quoted unit price are required.");
		return false;
	}
	if (const FMarketTransactionResult* Existing = FindConst(InOutState.MarketTransactionResults, [&Request](const FMarketTransactionResult& Candidate)
	{
		return Candidate.TransactionId == Request.TransactionId || Candidate.IdempotencyKey == Request.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		return Existing->Result == ESystemicActionResult::Accepted;
	}

	FStationMarketState* Market = FindMutable(InOutState.Markets, [&Request](const FStationMarketState& Candidate) { return Candidate.MarketId == Request.MarketId; });
	const FCommodityItemBridge* Bridge = FindConst(InOutState.CommodityItemBridges, [&Request](const FCommodityItemBridge& Candidate)
	{
		return Candidate.CommodityItemBridgeId == Request.CommodityItemBridgeId && Candidate.CommodityId == Request.CommodityId;
	});
	FCreditAccountRecord* Debit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Request.DebitAccountId; });
	FCreditAccountRecord* Credit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Request.CreditAccountId; });
	if (!Market || !Bridge || !Debit || !Credit)
	{
		OutResult.RejectionReason = TEXT("Market, commodity bridge, debit account, or credit account does not resolve.");
		return false;
	}

	const int32* Stock = Market->StockByCommodity.Find(Request.CommodityId);
	if (!Stock || *Stock < Request.Quantity)
	{
		OutResult.RejectionReason = TEXT("Market stock is unavailable.");
		return false;
	}
	const int64 TotalPrice = Request.QuotedUnitPrice * Request.Quantity;
	if (Debit->AvailableBalance < TotalPrice)
	{
		OutResult.RejectionReason = TEXT("Debit account has insufficient credits.");
		return false;
	}

	FCargoTransferRequest CargoRequest;
	CargoRequest.RequestId = MakeId(TEXT("cargo"), Request.TransactionId);
	CargoRequest.SourceContainerId = Request.SourceContainerId;
	CargoRequest.DestinationContainerId = Request.DestinationContainerId;
	CargoRequest.ExpectedSourceOwnerId = Request.SellerId;
	CargoRequest.ExpectedDestinationOwnerId = Request.BuyerId;
	CargoRequest.ItemId = Bridge->ItemId;
	CargoRequest.Quantity = Request.Quantity;
	CargoRequest.Reason = TEXT("trade");
	CargoRequest.SourceEventId = Request.SourceEventId;
	CargoRequest.LegalContextId = Request.LegalContextId;
	CargoRequest.IdempotencyKey = MakeId(TEXT("cargo"), Request.IdempotencyKey);
	FCargoTransferResult CargoResult;
	if (!TransferCargo(InOutState, CargoRequest, CargoResult))
	{
		OutResult.RejectionReason = CargoResult.RejectedReason;
		return false;
	}

	Market->StockByCommodity.FindOrAdd(Request.CommodityId) -= Request.Quantity;
	Debit->AvailableBalance -= TotalPrice;
	Credit->AvailableBalance += TotalPrice;

	FCreditLedgerEntry Ledger;
	Ledger.LedgerEntryId = MakeId(TEXT("ledger"), Request.TransactionId);
	Ledger.DebitAccountId = Debit->AccountId;
	Ledger.CreditAccountId = Credit->AccountId;
	Ledger.Amount = TotalPrice;
	Ledger.Reason = TEXT("market_buy");
	Ledger.SourceEventId = Request.SourceEventId;
	Ledger.SourceTransactionId = Request.TransactionId;
	Ledger.IdempotencyKey = MakeId(TEXT("ledger"), Request.IdempotencyKey);
	Ledger.State = TEXT("applied");
	InOutState.CreditLedger.Add(Ledger);
	Debit->LastLedgerEntryId = Ledger.LedgerEntryId;
	Credit->LastLedgerEntryId = Ledger.LedgerEntryId;

	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = Request.TransactionId;
	Transaction.TransactionType = TEXT("market_trade");
	Transaction.SourceEventId = Request.SourceEventId;
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = Request.BuyerId;
	Transaction.TargetType = TEXT("market");
	Transaction.TargetId = Request.MarketId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = Request.IdempotencyKey;
	Transaction.DebugReason = TEXT("Market stock, cargo, and ledger committed under one transaction.");
	InOutState.Transactions.Add(Transaction);
	AddJournalEntry(InOutState, Request.TransactionId, 1, TEXT("cargo_transfer"), CargoResult.TransferId, Request.IdempotencyKey);
	AddJournalEntry(InOutState, Request.TransactionId, 2, TEXT("ledger"), Ledger.LedgerEntryId, Request.IdempotencyKey);
	AddJournalEntry(InOutState, Request.TransactionId, 3, TEXT("market_stock"), Request.MarketId, Request.IdempotencyKey);

	OutResult.Result = ESystemicActionResult::Accepted;
	OutResult.AppliedQuantity = Request.Quantity;
	OutResult.UnitPrice = Request.QuotedUnitPrice;
	OutResult.StockDelta = -Request.Quantity;
	OutResult.DisplayCreditDelta = -TotalPrice;
	OutResult.LedgerEntryIds.Add(Ledger.LedgerEntryId);
	OutResult.CargoTransferResultId = CargoResult.TransferId;
	InOutState.MarketTransactionResults.Add(OutResult);
	Market->LastTransactionIds.Add(Request.TransactionId);
	return true;
}

bool USystemicGameplayQueryService::ResolveStationServiceEndpoint(const FSystemicGameplayState& State, FName ServiceEndpointId, FStationServiceEndpointDefinition& OutEndpoint, FString& OutDebugReason)
{
	const FStationServiceEndpointDefinition* Endpoint = FindConst(State.StationServiceEndpoints, [ServiceEndpointId](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == ServiceEndpointId;
	});
	if (!Endpoint)
	{
		OutDebugReason = TEXT("Service endpoint does not resolve.");
		return false;
	}
	if (!FindConst(State.Factions, [Endpoint](const FFactionDefinition& Candidate) { return Candidate.FactionId == Endpoint->ProviderFactionId; }) ||
		(!Endpoint->MarketId.IsNone() && !FindConst(State.Markets, [Endpoint](const FStationMarketState& Candidate) { return Candidate.MarketId == Endpoint->MarketId; })) ||
		(!Endpoint->CreditAccountId.IsNone() && !FindConst(State.CreditAccounts, [Endpoint](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Endpoint->CreditAccountId; })) ||
		(!Endpoint->CommsEndpointId.IsNone() && !FindConst(State.CommsEndpoints, [Endpoint](const FCommsEndpointDefinition& Candidate) { return Candidate.EndpointId == Endpoint->CommsEndpointId; })))
	{
		OutDebugReason = TEXT("Endpoint faction, market, credit account, or comms endpoint reference is invalid.");
		return false;
	}

	OutEndpoint = *Endpoint;
	OutDebugReason = FString::Printf(TEXT("Endpoint=%s Station=%s Market=%s Faction=%s LegalPolicy=%s Comms=%s"),
		*Endpoint->ServiceEndpointId.ToString(),
		*Endpoint->StationId.ToString(),
		*Endpoint->MarketId.ToString(),
		*Endpoint->ProviderFactionId.ToString(),
		*Endpoint->LegalPolicyId.ToString(),
		*Endpoint->CommsEndpointId.ToString());
	return true;
}

bool USystemicGameplayQueryService::BuildSystemicDecisionInputSnapshot(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FName RouteSegmentId, FSystemicDecisionInputSnapshot& OutSnapshot)
{
	OutSnapshot = FSystemicDecisionInputSnapshot();
	OutSnapshot.RouteSegmentId = RouteSegmentId;
	const FTrafficRouteSegmentDefinition* Route = SystemDefinition.TrafficRoutes.FindByPredicate([RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route)
	{
		OutSnapshot.DebugReason = TEXT("Route segment does not resolve.");
		return false;
	}
	const FJurisdictionDefinition* Jurisdiction = FindConst(State.Jurisdictions, [Route](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Route->JurisdictionId;
	});
	const FStationMarketState* Market = State.Markets.IsEmpty() ? nullptr : &State.Markets[0];
	const FContainerState* Cargo = FindConst(State.Containers, [](const FContainerState& Candidate) { return Candidate.ContainerId == FName(TEXT("player_ship_cargo")); });
	if (!Jurisdiction || !Market || !Cargo)
	{
		OutSnapshot.DebugReason = TEXT("Decision inputs are missing jurisdiction, market, or cargo state.");
		return false;
	}

	OutSnapshot.JurisdictionId = Jurisdiction->JurisdictionId;
	OutSnapshot.AuthorityFactionId = Jurisdiction->AuthorityFactionId;
	OutSnapshot.MarketId = Market->MarketId;
	OutSnapshot.CargoContainerId = Cargo->ContainerId;
	OutSnapshot.LegalContextId = Jurisdiction->LawProfileId;
	OutSnapshot.bValid = true;
	OutSnapshot.DebugReason = TEXT("Systemic inputs resolved by stable IDs; encounter resolution is deferred to M10.");
	return true;
}

bool USystemicGameplayQueryService::ReservePatrolForRoute(
	const FStarSystemDefinition& SystemDefinition,
	FSystemicGameplayState& InOutState,
	FName RouteSegmentId,
	FName SourceEventId,
	double ExpiresAtTimeSeconds,
	FPatrolReservationRecord& OutReservation,
	FString& OutFailureReason)
{
	OutReservation = FPatrolReservationRecord();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route || !Route->bSupportsPatrolCoverage)
	{
		OutFailureReason = TEXT("Route does not resolve or does not support patrol coverage.");
		return false;
	}
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [Route](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Route->JurisdictionId;
	});
	if (!Jurisdiction)
	{
		OutFailureReason = TEXT("Route jurisdiction does not resolve.");
		return false;
	}

	const FName IdempotencyKey = FName(*FString::Printf(TEXT("idem_m10_patrol_%s_%s"), *RouteSegmentId.ToString(), *SourceEventId.ToString()));
	if (const FPatrolReservationRecord* Existing = FindConst(InOutState.PatrolReservations, [IdempotencyKey](const FPatrolReservationRecord& Candidate)
	{
		return Candidate.IdempotencyKey == IdempotencyKey;
	}))
	{
		OutReservation = *Existing;
		OutFailureReason.Reset();
		return true;
	}

	FFactionOperationalState* Operation = FindMutable(InOutState.FactionOperations, [Jurisdiction](const FFactionOperationalState& Candidate)
	{
		return Candidate.FactionId == Jurisdiction->AuthorityFactionId && Candidate.AvailablePatrolBudget > Candidate.ReservedPatrolBudget && !Candidate.PatrolAssetIds.IsEmpty();
	});
	if (!Operation)
	{
		OutFailureReason = TEXT("No patrol operation has available patrol budget.");
		return false;
	}

	FName SelectedPatrolAssetId;
	for (const FName PatrolAssetId : Operation->PatrolAssetIds)
	{
		const bool bAlreadyActive = InOutState.PatrolReservations.ContainsByPredicate([PatrolAssetId](const FPatrolReservationRecord& Candidate)
		{
			return Candidate.PatrolAssetId == PatrolAssetId && Candidate.State != FName(TEXT("released")) && Candidate.ExpiresAtTimeSeconds > 0.0;
		});
		if (!bAlreadyActive)
		{
			SelectedPatrolAssetId = PatrolAssetId;
			break;
		}
	}
	if (SelectedPatrolAssetId.IsNone())
	{
		OutFailureReason = TEXT("All patrol assets are already reserved.");
		return false;
	}

	OutReservation.ReservationId = FName(*FString::Printf(TEXT("reservation_%s_%s"), *SelectedPatrolAssetId.ToString(), *SourceEventId.ToString()));
	OutReservation.PatrolAssetId = SelectedPatrolAssetId;
	OutReservation.FactionId = Operation->FactionId;
	OutReservation.JurisdictionId = Jurisdiction->JurisdictionId;
	OutReservation.RouteSegmentId = RouteSegmentId;
	OutReservation.SourceEventId = SourceEventId;
	OutReservation.State = TEXT("reserved");
	OutReservation.ExpiresAtTimeSeconds = ExpiresAtTimeSeconds;
	OutReservation.IdempotencyKey = IdempotencyKey;
	InOutState.PatrolReservations.Add(OutReservation);
	Operation->ReservedPatrolBudget += 1;
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::SelectPirateInterdictionHazard(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName RouteSegmentId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double SimulationTimeSeconds,
	FInterdictionHazardRecord& OutHazard,
	FString& OutFailureReason)
{
	OutHazard = FInterdictionHazardRecord();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route || !Route->bSupportsPirateAmbush)
	{
		OutFailureReason = TEXT("Route does not resolve or does not support pirate ambush.");
		return false;
	}

	const FShipGroupState* PirateGroup = FindConst(SystemDefinition.ShipGroups, [](const FShipGroupState& Candidate)
	{
		return Candidate.GroupRole == FName(TEXT("pirate"));
	});
	const FShipTrafficInstance* TargetShip = FindConst(SystemDefinition.LogicalTraffic, [](const FShipTrafficInstance& Candidate)
	{
		return Candidate.ShipInstanceId == FName(TEXT("trader_brink_01"));
	});
	const FRouteSecuritySnapshot* Security = FindConst(State.RouteSecuritySnapshots, [RouteSegmentId](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	FSystemicDecisionInputSnapshot Snapshot;
	const bool bInputsResolve = BuildSystemicDecisionInputSnapshot(SystemDefinition, State, RouteSegmentId, Snapshot) && Snapshot.bValid;
	const bool bHostilePirateFaction = State.FactionRelationships.ContainsByPredicate([](const FFactionRelationshipRecord& Candidate)
	{
		return Candidate.SourceFactionId == FName(TEXT("ember_raiders")) &&
			Candidate.TargetFactionId == FName(TEXT("frontier_local_authority")) &&
			Candidate.HostilityState == FName(TEXT("hostile"));
	});
	const bool bCargoTargetHasValue = State.Containers.ContainsByPredicate([](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == FName(TEXT("trader_brink_cargo")) && !Candidate.Stacks.IsEmpty();
	});
	const bool bMarketHasRouteCommodity = State.Markets.ContainsByPredicate([](const FStationMarketState& Candidate)
	{
		const int32* Stock = Candidate.StockByCommodity.Find(FName(TEXT("commodity_ember_ore")));
		return Stock && *Stock > 0;
	});
	if (!PirateGroup || !TargetShip || !Security || !bInputsResolve || !bHostilePirateFaction || !bCargoTargetHasValue || !bMarketHasRouteCommodity ||
		Security->JurisdictionId != Snapshot.JurisdictionId || Security->PirateRiskScore <= Security->PatrolCoverageScore || Route->RouteValue <= 0.0)
	{
		OutFailureReason = TEXT("Pirate ambush policy requires M9 decision inputs, hostile faction/legal context, valued cargo/market data, and risk above patrol coverage.");
		return false;
	}

	OutHazard.HazardId = FName(*FString::Printf(TEXT("hazard_%s_%s"), *PirateGroup->GroupId.ToString(), *RouteSegmentId.ToString()));
	OutHazard.SourceEventId = Security->SourceEventId;
	OutHazard.RouteSegmentId = RouteSegmentId;
	OutHazard.OwnerGroupId = PirateGroup->GroupId;
	OutHazard.TargetShipId = TargetShip->ShipInstanceId;
	OutHazard.State = TEXT("pending");
	OutHazard.ExpiresAtTimeSeconds = SimulationTimeSeconds + 600.0;
	OutHazard.SaveLoadEventId = Security->SourceEventId;
	OutHazard.IdempotencyKey = FName(*FString::Printf(TEXT("idem_m10_hazard_%s_%s"), *PirateGroup->GroupId.ToString(), *RouteSegmentId.ToString()));
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteSegmentId, 0.55, ClockSnapshot, SimulationTimeSeconds, OutHazard.RouteSample))
	{
		OutFailureReason = TEXT("Pirate ambush policy could not sample the moving route.");
		return false;
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ChooseFleeDestination(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName ThreatRouteSegmentId,
	FMovingFrameTarget& OutDestination,
	FString& OutFailureReason)
{
	OutDestination = FMovingFrameTarget();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [ThreatRouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == ThreatRouteSegmentId;
	});
	if (!Route)
	{
		OutFailureReason = TEXT("Threat route does not resolve.");
		return false;
	}
	const bool bRouteHasSecurity = State.RouteSecuritySnapshots.ContainsByPredicate([ThreatRouteSegmentId](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == ThreatRouteSegmentId && Candidate.SecurityRating > 0.0;
	});
	if (!bRouteHasSecurity)
	{
		OutFailureReason = TEXT("Flee policy needs a route security snapshot.");
		return false;
	}

	OutDestination.TargetId = Route->DestinationAnchorId;
	OutDestination.TargetType = TEXT("station");
	OutDestination.AnchorId = Route->DestinationAnchorId;
	OutDestination.CoordinateFrame.FrameType = TEXT("station_relative");
	OutDestination.CoordinateFrame.AnchorId = Route->DestinationAnchorId;
	OutDestination.RouteSegmentId = ThreatRouteSegmentId;
	OutDestination.RouteProgress01 = 1.0;
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::SelectLogicalFightPolicy(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName EncounterId,
	FName& OutPolicyId,
	FString& OutFailureReason)
{
	OutPolicyId = NAME_None;
	const FLogicalEncounterRecord* Encounter = FindConst(State.LogicalEncounters, [EncounterId](const FLogicalEncounterRecord& Candidate)
	{
		return Candidate.EncounterId == EncounterId;
	});
	if (!Encounter || Encounter->bRequiresActor)
	{
		OutFailureReason = TEXT("Fight policy requires an actor-free logical encounter.");
		return false;
	}
	FSystemicDecisionInputSnapshot Snapshot;
	const bool bInputsResolve = BuildSystemicDecisionInputSnapshot(SystemDefinition, State, Encounter->RouteSegmentId, Snapshot) && Snapshot.bValid;
	const bool bHazardResolves = State.InterdictionHazards.ContainsByPredicate([Encounter](const FInterdictionHazardRecord& Candidate)
	{
		return Candidate.HazardId == Encounter->InterdictionHazardId && Candidate.State == FName(TEXT("pending"));
	});
	const bool bDistressResolves = State.DistressEvents.ContainsByPredicate([Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId && !Candidate.MessageInstanceIds.IsEmpty();
	});
	if (!bInputsResolve || !bHazardResolves || !bDistressResolves)
	{
		OutFailureReason = TEXT("Fight policy requires M9 route/legal inputs plus pending hazard and distress records.");
		return false;
	}

	OutPolicyId = TEXT("m10_abstract_fight_distress_patrol_response");
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ResolveLogicalEncounterOnce(
	FSystemicGameplayState& InOutState,
	FName EncounterId,
	double AppliedTimeSeconds,
	FLogicalEncounterResolutionResult& OutResult,
	FString& OutFailureReason)
{
	OutResult = FLogicalEncounterResolutionResult();
	OutResult.EncounterId = EncounterId;

	FLogicalEncounterRecord* Encounter = FindMutable(InOutState.LogicalEncounters, [EncounterId](const FLogicalEncounterRecord& Candidate)
	{
		return Candidate.EncounterId == EncounterId;
	});
	if (!Encounter)
	{
		OutFailureReason = TEXT("Logical encounter does not resolve.");
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}
	const FAbstractEngagementRecord* ExistingEngagement = FindConst(InOutState.AbstractEngagements, [Encounter](const FAbstractEngagementRecord& Candidate)
	{
		return Candidate.EncounterId == Encounter->EncounterId || Candidate.EngagementId == Encounter->EngagementId;
	});
	if (Encounter->State == FName(TEXT("resolved")) || ExistingEngagement)
	{
		OutResult.ResultEventId = Encounter->SourceEventId;
		OutResult.OutcomeType = TEXT("duplicate");
		OutResult.bDuplicate = true;
		OutResult.DebugReason = TEXT("Logical encounter was already resolved; no side effects were replayed.");
		OutFailureReason.Reset();
		return true;
	}

	const FSimulationEventRecord* Event = FindConst(InOutState.Events, [Encounter](const FSimulationEventRecord& Candidate)
	{
		return Candidate.EventId == Encounter->SourceEventId;
	});
	if (!Event)
	{
		OutFailureReason = TEXT("Logical encounter source event does not resolve.");
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	FSimulationEventResultRecord EventResult;
	if (!ApplySimulationEventOnce(InOutState, *Event, AppliedTimeSeconds, EventResult))
	{
		OutFailureReason = EventResult.DebugReason;
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	const FRouteSecuritySnapshot* Security = FindConst(InOutState.RouteSecuritySnapshots, [Encounter](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == Encounter->RouteSegmentId;
	});
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [Security](const FJurisdictionDefinition& Candidate)
	{
		return Security && Candidate.JurisdictionId == Security->JurisdictionId;
	});
	FOffenseEvent Offense;
	Offense.OffenseId = MakeId(TEXT("offense"), Encounter->EncounterId);
	Offense.OffenderType = TEXT("ship");
	Offense.OffenderId = TEXT("pirate_raider_01");
	Offense.VictimType = TEXT("ship");
	Offense.VictimId = TEXT("trader_brink_01");
	Offense.JurisdictionId = Security ? Security->JurisdictionId : FName(TEXT("frontier_local_authority"));
	Offense.OffenseType = TEXT("pirate_attack");
	Offense.SourceEventId = Encounter->SourceEventId;
	Offense.OccurredTimeSeconds = AppliedTimeSeconds;

	FEvidenceRecord Evidence;
	FCriminalRecord CriminalRecord;
	if (!RecordOffense(InOutState, Offense, Evidence, CriminalRecord, OutFailureReason))
	{
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	FCargoTransferRequest CargoRequest;
	CargoRequest.RequestId = MakeId(TEXT("cargo_loot"), Encounter->EncounterId);
	CargoRequest.SourceContainerId = TEXT("trader_brink_cargo");
	CargoRequest.DestinationContainerId = TEXT("pirate_raider_cargo");
	CargoRequest.ExpectedSourceOwnerId = TEXT("trader_brink_01");
	CargoRequest.ExpectedDestinationOwnerId = TEXT("pirate_raider_01");
	CargoRequest.ItemId = TEXT("item_ember_ore");
	CargoRequest.Quantity = 1;
	CargoRequest.Reason = TEXT("pirate_loot");
	CargoRequest.SourceEventId = Encounter->SourceEventId;
	CargoRequest.LegalContextId = Jurisdiction ? Jurisdiction->LawProfileId : FName();
	CargoRequest.IdempotencyKey = MakeId(TEXT("idem_m10_cargo_loot"), Encounter->EncounterId);
	FCargoTransferResult CargoResult;
	if (!TransferCargo(InOutState, CargoRequest, CargoResult))
	{
		OutFailureReason = CargoResult.RejectedReason;
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	FCreditAccountRecord* TraderAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == FName(TEXT("account_trader_brink_01"));
	});
	FCreditAccountRecord* PirateAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == FName(TEXT("account_pirate_raider_01"));
	});
	if (!TraderAccount || !PirateAccount || TraderAccount->AvailableBalance < 25)
	{
		OutFailureReason = TEXT("Pirate encounter economy accounts do not resolve or cannot pay the abstract ransom.");
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	const FName EconomyTransactionId = MakeId(TEXT("tx_pirate_economy"), Encounter->EncounterId);
	const FName LedgerId = MakeId(TEXT("ledger_pirate_economy"), Encounter->EncounterId);
	const FName EconomyIdempotencyKey = MakeId(TEXT("idem_m10_economy"), Encounter->EncounterId);
	TraderAccount->AvailableBalance -= 25;
	PirateAccount->AvailableBalance += 25;
	FCreditLedgerEntry Ledger;
	Ledger.LedgerEntryId = LedgerId;
	Ledger.DebitAccountId = TraderAccount->AccountId;
	Ledger.CreditAccountId = PirateAccount->AccountId;
	Ledger.Amount = 25;
	Ledger.Reason = TEXT("pirate_ransom_abstract");
	Ledger.SourceEventId = Encounter->SourceEventId;
	Ledger.SourceTransactionId = EconomyTransactionId;
	Ledger.IdempotencyKey = MakeId(TEXT("ledger"), EconomyIdempotencyKey);
	Ledger.State = TEXT("applied");
	InOutState.CreditLedger.Add(Ledger);
	TraderAccount->LastLedgerEntryId = Ledger.LedgerEntryId;
	PirateAccount->LastLedgerEntryId = Ledger.LedgerEntryId;

	FGameplayTransactionRecord EconomyTransaction;
	EconomyTransaction.TransactionId = EconomyTransactionId;
	EconomyTransaction.TransactionType = TEXT("pirate_abstract_economy");
	EconomyTransaction.SourceEventId = Encounter->SourceEventId;
	EconomyTransaction.InitiatorType = TEXT("ship");
	EconomyTransaction.InitiatorId = TEXT("pirate_raider_01");
	EconomyTransaction.TargetType = TEXT("ship");
	EconomyTransaction.TargetId = TEXT("trader_brink_01");
	EconomyTransaction.State = TEXT("committed");
	EconomyTransaction.IdempotencyKey = EconomyIdempotencyKey;
	EconomyTransaction.DebugReason = TEXT("Pirate encounter cargo and ransom economy side effects committed without actors.");
	InOutState.Transactions.Add(EconomyTransaction);
	AddJournalEntry(InOutState, EconomyTransactionId, 1, TEXT("cargo_transfer"), CargoResult.TransferId, EconomyIdempotencyKey);
	AddJournalEntry(InOutState, EconomyTransactionId, 2, TEXT("ledger"), Ledger.LedgerEntryId, EconomyIdempotencyKey);

	FAbstractEngagementRecord Engagement;
	Engagement.EngagementId = Encounter->EngagementId.IsNone() ? MakeId(TEXT("engagement"), Encounter->EncounterId) : Encounter->EngagementId;
	Engagement.SourceEventId = Encounter->SourceEventId;
	Engagement.EncounterId = Encounter->EncounterId;
	Engagement.AttackerId = Offense.OffenderId;
	Engagement.DefenderId = Offense.VictimId;
	Engagement.OutcomeType = TEXT("distress_patrol_response");
	Engagement.CargoTransferResultIds = { CargoResult.TransferId };
	Engagement.GameplayTransactionIds = { EconomyTransactionId };
	Engagement.CreditLedgerEntryIds = { Ledger.LedgerEntryId };
	Engagement.OffenseIds = { Offense.OffenseId };
	Engagement.EvidenceIds = { Evidence.EvidenceId };
	Engagement.CriminalRecordIds = { CriminalRecord.CriminalRecordId };
	if (const FDistressEventRecord* Distress = FindConst(InOutState.DistressEvents, [Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId;
	}))
	{
		Engagement.MessageInstanceIds = Distress->MessageInstanceIds;
	}
	Engagement.IdempotencyKey = MakeId(TEXT("idem_m10_engagement"), Encounter->EncounterId);
	InOutState.AbstractEngagements.Add(Engagement);

	Encounter->State = TEXT("resolved");
	Encounter->EngagementId = Engagement.EngagementId;
	Encounter->ProcessedWatermark += 1;
	if (FInterdictionHazardRecord* Hazard = FindMutable(InOutState.InterdictionHazards, [Encounter](const FInterdictionHazardRecord& Candidate)
	{
		return Candidate.HazardId == Encounter->InterdictionHazardId;
	}))
	{
		Hazard->State = TEXT("resolved");
	}
	if (FDistressEventRecord* Distress = FindMutable(InOutState.DistressEvents, [Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId;
	}))
	{
		Distress->State = TEXT("responded");
		for (const FName ReservationId : Distress->RespondingPatrolReservationIds)
		{
			if (FPatrolReservationRecord* Reservation = FindMutable(InOutState.PatrolReservations, [ReservationId](const FPatrolReservationRecord& Candidate)
			{
				return Candidate.ReservationId == ReservationId;
			}))
			{
				Reservation->State = TEXT("consumed");
			}
		}
	}

	OutResult.ResultEventId = Encounter->SourceEventId;
	OutResult.OutcomeType = EventResult.OutcomeType == FName(TEXT("duplicate")) ? FName(TEXT("duplicate")) : FName(TEXT("resolved"));
	OutResult.bDuplicate = EventResult.OutcomeType == FName(TEXT("duplicate"));
	OutResult.DebugReason = TEXT("Logical pirate encounter resolved through M9 event, legal, evidence, message, and abstract engagement contracts without actors.");
	OutFailureReason.Reset();
	return true;
}

FString USystemicGameplayQueryService::BuildSystemicDebugSummary(const FSystemicGameplayState& State)
{
	return FString::Printf(
		TEXT("Events=%d Results=%d Transactions=%d Journal=%d Factions=%d Jurisdictions=%d Offenses=%d Evidence=%d CriminalRecords=%d Containers=%d Markets=%d Ledger=%d Services=%d Messages=%d Missions=%d"),
		State.Events.Num(),
		State.EventResults.Num(),
		State.Transactions.Num(),
		State.TransactionJournal.Num(),
		State.Factions.Num(),
		State.Jurisdictions.Num(),
		State.Offenses.Num(),
		State.EvidenceRecords.Num(),
		State.CriminalRecords.Num(),
		State.Containers.Num(),
		State.Markets.Num(),
		State.CreditLedger.Num(),
		State.StationServiceEndpoints.Num(),
		State.MessageLog.Num(),
		State.MissionInstances.Num());
}

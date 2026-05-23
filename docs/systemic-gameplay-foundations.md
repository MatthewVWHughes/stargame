# Systemic Gameplay Foundations

This document defines the minimum systemic gameplay contracts needed before pirates, police, markets, trading, missions, and station services can behave consistently.

The goal is to avoid fake systems. Space AI, legal response, economy, inventory, comms, and missions all reference each other. They need small data-first foundations before content-heavy versions are built.

The first playable path should stay thin and player-facing: buy or sell a small
commodity set, accept and complete a simple station-contact mission, mutate
credits/cargo/basic reputation, show the result through docked UI or station
interaction, and survive save/load. The broader transaction, escrow,
arbitration, and generated-follow-up machinery below is the hardening path for
cross-system consequences; it must not become an excuse to delay that thin loop.

## Runtime Position

These foundations are required before full logical encounters and the realized pirate/police AI slice can produce durable consequences:

- legal/faction skeleton
- canonical simulation event envelope
- inventory and cargo containers
- commodity and market transaction contracts
- comms/message contracts

Without these, encounters and realized AI either fake outcomes or bake temporary assumptions into AI code.

## Canonical Simulation Events

Use one durable event envelope for economy, AI, legal, faction, comms, and news.

`FSimulationEventRecord`:

- `EventId`
- `SystemId`, optional
- `EventType`
- `SourceType`: ship, group, station, faction, market, mission, debug
- `SourceId`, optional
- `TargetType`, optional
- `TargetId`, optional
- `LocationTarget`, optional
- `ParticipantShipIds`, optional
- `ParticipantGroupIds`, optional
- `PayloadRef`, optional
- `CreatedTimeSeconds`
- `ScheduledTimeSeconds`, optional
- `ExpiryTimeSeconds`
- `RngStreamId`
- `RngCounter`
- `IdempotencyKey`
- `ProcessedEventWatermark`

`FSimulationEventResultRecord`:

- `EventId`
- `ResultId`
- `AppliedTimeSeconds`
- `OutcomeType`
- `AffectedStateRefs`
- `DebugReason`
- `IdempotencyKey`

Rules:

- every durable event resolves exactly once
- every side effect references the source `EventId`
- retry/load uses existing result records before recomputing
- AI encounter events, market arrivals, crimes, distress calls, and messages use this envelope

Specialized records such as logical encounters may wrap or reference the canonical event, but they do not create a separate idempotency system.

## Gameplay Transaction Commit Journal

Any player-facing action or simulation event that mutates more than one gameplay system must use one canonical transaction and commit journal. This is required for market buys, mission accept/turn-in, service repair/refuel/rearm, legal confiscation/fines, cargo delivery, reputation changes, and generated follow-up creation.

For M0, the transaction may be intentionally small: one validated request, one
result record, ordered credit/cargo/mission/reputation mutations, and a save/load
test proving the mutation is idempotent. Reservations, escrow, recovery records,
and generated follow-ups are required only when the selected gameplay actually
needs pending state, delayed commit, or retry recovery.

`FGameplayTransactionRecord`:

- `TransactionId`
- `TransactionType`: market_trade, station_service, mission_accept, mission_turn_in, legal_enforcement, cargo_delivery, npc_arrival, reputation_delta, faction_operation, debug
- `SourceEventId`, optional
- `SourceServiceRequestId`, optional
- `SourceMissionInstanceId`, optional
- `InitiatorType`: player, ship, station, faction, market, mission, service, system
- `InitiatorId`
- `TargetType`, optional
- `TargetId`, optional
- `PrepareResultId`, optional
- `CommitResultId`, optional
- `RecoveryResultId`, optional
- `State`: preparing, prepared, committing, committed, recovering, recovered, rejected, failed
- `IdempotencyKey`
- `CreatedTimeSeconds`
- `LastChangedTimeSeconds`
- `DebugReason`

`FGameplayTransactionPrepareRecord`:

- `PrepareResultId`
- `TransactionId`
- `ValidatedInputs`: accounts, containers, stacks, market_stock, mission_objectives, service_endpoint, legal_context, faction_state, ship_resources
- `ReservedRefs`
- `QuotedLedgerEntryIds`, optional
- `PredictedSideEffectIds`
- `Result`: prepared, rejected
- `RejectedReason`, optional
- `PreparedTimeSeconds`

`FGameplayTransactionCommitRecord`:

- `CommitResultId`
- `TransactionId`
- `AppliedSideEffectIds`
- `JournalSequenceStart`
- `JournalSequenceEnd`
- `Result`: committed, partial_recovered, failed
- `CommittedTimeSeconds`
- `DebugReason`

`FGameplayTransactionJournalEntry`:

- `JournalEntryId`
- `TransactionId`
- `Sequence`
- `Phase`: prepare, commit, recovery
- `SideEffectType`: ledger, escrow, cargo_transfer, market_stock, service_resource, mission_state, reputation_delta, faction_operational_delta, legal_action, message_arbitration, generated_followup
- `SideEffectRefId`
- `ApplyOrder`
- `State`: pending, applied, skipped, compensated, failed
- `IdempotencyKey`
- `AppliedTimeSeconds`, optional
- `DebugReason`

`FGameplayReservationRecord`:

- `ReservationId`
- `ReservationType`: market_stock, cargo_space, cargo_item, credit_escrow, service_slot, docking_port, patrol_budget, mission_offer, comms_window
- `OwnerType`
- `OwnerId`
- `ResourceId`, optional
- `TargetRef`, optional typed target reference containing target type and target ID
- `Quantity`
- `SourceTransactionId`
- `State`: prepared, active, consumed, released, expired, cancelled, failed
- `ExpiresAtSimulationTime`, optional
- `RecoveryPolicyId`
- `CreatedEventId`, optional
- `LastChangedSimulationTime`

At least one of `ResourceId` or `TargetRef` must be present. `ReservedRefs` must point to durable reservation records, not informal strings. Validation must reject an active reservation without a live prepared/committing transaction, a valid expiry/cancel record, or a completed consume/release result. Recovery must be deterministic and idempotent: reloading during prepare, commit, release, or expiry cannot duplicate stock, cargo, credits, service slots, patrol budget, or port occupancy.

`FGameplayTransactionRecoveryRecord`:

- `RecoveryResultId`
- `TransactionId`
- `ObservedJournalEntries`
- `RecoveredSideEffectIds`
- `CompensatedSideEffectIds`
- `Result`: no_op, completed_commit, compensated, failed_validation
- `RecoveredTimeSeconds`
- `DebugReason`

Commit ordering:

1. validate access, legality, mission state, resources, accounts, cargo, and endpoint IDs
2. write prepare record and reservations without mutating final state
3. apply ledger and escrow entries
4. apply cargo, market stock, ship resource, and equipment/resource mutations
5. apply mission lifecycle, legal, reputation, and faction operational deltas
6. apply message arbitration and generated follow-up opportunity records
7. write commit record, result records, and processed event watermark

Recovery rules:

- load first rebuilds the transaction index from journal records before catch-up
- a transaction in `prepared` with no applied side effects may be rejected or resumed from prepare deterministically
- a transaction with any applied side effects must complete from the next missing ordered side effect or write an explicit recovery record
- no side effect may be applied outside its journal entry when the action is transaction-scoped
- repeated player input, repeated event processing, and active mission turn-in retries use `IdempotencyKey` and existing commit/result records instead of issuing duplicate rewards
- debug output must show transaction ID, phase, ordered side effects, result records, and recovery decision

## Durable Credits And Ledger

Credits are persistent account state, not loose integer fields on UI, market, or mission records.

`FCreditAccountRecord`:

- `AccountId`
- `OwnerType`: player, ship, station, faction, market, mission, escrow, service
- `OwnerId`
- `CurrencyId`, default `credits`
- `AvailableBalance`
- `HeldBalance`
- `AccountState`: active, suspended, closed
- `LastLedgerEntryId`
- `LastChangedEventId`

`FCreditLedgerEntry`:

- `LedgerEntryId`
- `DebitAccountId`
- `CreditAccountId`
- `Amount`
- `CurrencyId`
- `Reason`: market_buy, market_sell, service_fee, fine, bounty, mission_reward, escrow_hold, escrow_release, confiscation, debug
- `SourceEventId`, optional
- `SourceTransactionId`, optional
- `IdempotencyKey`
- `AppliedTimeSeconds`
- `State`: pending, applied, reversed, rejected
- `ReversalOfLedgerEntryId`, optional

`FEscrowHoldRecord`:

- `EscrowId`
- `HoldingAccountId`
- `BeneficiaryAccountId`
- `Amount`
- `CurrencyId`
- `Reason`: market_order, mission_reward, fine_appeal, delivery_contract, service_reservation
- `SourceEventId`
- `SourceTransactionId`, optional
- `CreatedTimeSeconds`
- `ExpiryTimeSeconds`, optional
- `State`: held, released, forfeited, cancelled, expired
- `ReleaseLedgerEntryId`, optional
- `IdempotencyKey`

Rules:

- every credit mutation is a ledger entry
- market transactions, station service fees, mission rewards, fines, bounties, and confiscation values reference ledger entries
- a retry/load checks `IdempotencyKey` and existing `LedgerEntryId` before changing balances
- escrow moves funds from `AvailableBalance` to `HeldBalance`; release or cancellation is a separate ledger/result record
- no service or market code writes account balances directly
- debug output must show the source event/transaction that caused each balance change
- ledger validation must prove double-entry conservation for every transfer where both accounts are known
- balances may not become negative unless the account definition explicitly allows debt and defines a debt limit
- escrow hold, release, cancel, forfeit, and reversal transitions must update available and held balances exactly once
- ledger reversal and escrow recovery must be idempotent across save/load

## Factions And Reputation

Factions are stable gameplay entities.

`FFactionDefinition`:

- `FactionId`
- `DisplayName`
- `FactionType`: civilian, corporation, government, pirate, police, independent, story
- `DefaultLawProfileId`
- `DefaultMarketModifierProfileId`
- `RelationshipGroupId`
- `bCanOwnStations`
- `bCanOwnShips`
- `bCanIssueMissions`

`FFactionRelationshipRecord`:

- `SourceFactionId`
- `TargetFactionId`
- `Standing`
- `HostilityState`
- `LastChangedEventId`

`FReputationRecord`:

- `OwnerId`: player, NPC, ship, group, or faction
- `FactionId`
- `ReputationValue`
- `RankId`, optional
- `PermitIds`
- `LastChangedEventId`

`FFactionOperationalState`:

- `FactionId`
- `SystemId`, optional
- `ZoneId`, optional
- `Influence`
- `SecurityPressure`
- `CriminalPressure`
- `PatrolAssetIds`
- `AvailablePatrolBudget`
- `ReservedPatrolBudget`
- `TradePolicyState`
- `MissionPolicyState`
- `AlertLevel`: normal, watch, restricted, lockdown, war
- `LastOperationalEventId`
- `LastUpdateTimeSeconds`

`FReputationDeltaRecord`:

- `ReputationDeltaId`
- `ReputationRecordId`
- `OwnerId`
- `FactionId`
- `DeltaValue`
- `Reason`: mission_reward, crime, bounty, trade_support, patrol_support, piracy, story, debug
- `SourceEventId`, optional
- `SourceTransactionId`, optional
- `AppliedTimeSeconds`
- `ResultingReputationValue`
- `ResultingRankId`, optional
- `IdempotencyKey`

`FFactionRelationshipDeltaRecord`:

- `RelationshipDeltaId`
- `SourceFactionId`
- `TargetFactionId`
- `StandingDelta`
- `HostilityStateBefore`
- `HostilityStateAfter`
- `Reason`
- `SourceEventId`, optional
- `SourceTransactionId`, optional
- `AppliedTimeSeconds`
- `IdempotencyKey`

`FFactionOperationalDeltaRecord`:

- `OperationalDeltaId`
- `FactionId`
- `SystemId`, optional
- `ZoneId`, optional
- `InfluenceDelta`
- `SecurityPressureDelta`
- `CriminalPressureDelta`
- `PatrolBudgetDelta`
- `ReservedPatrolBudgetDelta`
- `AlertLevelBefore`
- `AlertLevelAfter`
- `TradePolicyStateBefore`
- `TradePolicyStateAfter`
- `ServicePolicyStateBefore`
- `ServicePolicyStateAfter`
- `MissionPolicyStateBefore`
- `MissionPolicyStateAfter`
- `Reason`: trade_delivery, piracy, patrol_loss, enforcement_success, mission_result, route_disruption, generated_event, debug
- `SourceEventId`, optional
- `SourceTransactionId`, optional
- `AppliedTimeSeconds`
- `IdempotencyKey`

Faction state feeds:

- police authority
- pirate hostility
- market modifiers
- docking/service access
- mission availability
- route security
- patrol and interdiction asset availability
- generated market, mission, and service weighting

Faction operational state is the mutable simulation state for a faction in a system or zone. Faction definitions remain authored identity and policy. Operational state may change from patrol losses, crime pressure, trade disruption, player missions, station events, and route security outcomes.

Reputation, faction relationship, and faction operational mutations must be written as delta records. The current state records are snapshots derived from applying committed deltas in deterministic order. Mission rewards, crimes, enforcement outcomes, route security changes, and generated follow-ups reference the delta records they caused.

## Legal Skeleton

Legal data is required before police/pirate systemic AI.

Minimum contracts:

- `FJurisdictionDefinition`
- `FLawProfile`
- `FEnforcementProfile`
- `FOffenseEvent`
- `FEvidenceRecord`
- `FCriminalRecord`

Required rules:

- offenses can be player-owned or NPC-owned
- NPC-NPC offenses use the same evidence pipeline as player crimes
- enforcement decisions reference jurisdiction, law, evidence, faction, and patrol assets
- police response does not spawn infinite assets from density
- enforcement actions are durable lifecycle records, not one-off AI branches

The first legal implementation may be simple, but it must be data-shaped correctly.

## Inventory And Cargo

Inventory is the foundation for trading, cargo loss, mission cargo, piracy, mining, salvage, and equipment.

`FItemDefinition`:

- `ItemId`
- `DisplayName`
- `ItemType`: commodity, equipment, consumable, mission_item
- `StackLimit`
- `MassPerUnitKg`
- `VolumePerUnitM3`
- `BaseValue`
- `LegalityTags`

`FContainerState`:

- `ContainerId`
- `OwnerId`
- `LocationType`: player, ship, station, market, mission, wreck
- `LocationId`
- `CapacityMassKg`
- `CapacityVolumeM3`
- `Stacks`

`FItemStackState`:

- `StackId`
- `ItemId`
- `Quantity`
- `Flags`: mission_locked, stolen, contraband, damaged
- `OwnerFactionId`, optional
- `SourceEventId`, optional

`FCargoTransferRequest`:

- `SourceContainerId`
- `DestinationContainerId`
- `StackSelection`: explicit_stack_ids, manifest_line_ids, item_id_policy
- `StackIds`, optional
- `ManifestLineIds`, optional
- `ItemId`, optional only when using an explicit selection policy
- `SelectionPolicy`: oldest_first, mission_first, stolen_first, contraband_first, player_selected
- `Quantity`
- `Reason`: trade, jettison, piracy, mining, mission, salvage
- `SourceEventId`, optional

`FCargoTransferResult`:

- `TransferId`
- `RequestId`
- `Result`: accepted, rejected, partial
- `MovedStackIds`
- `CreatedStackIds`
- `RejectedReason`
- `SourceEventId`, optional

Transfers must validate capacity, ownership, legality, mission locks, and available quantity before mutating state.

## Ship Instance, Durability, And Loadout Resources

Ship archetypes define base capabilities. Ship instances are saved mutable gameplay state.

`FShipInstanceState`:

- `ShipInstanceId`
- `ShipArchetypeId`
- `OwnerId`
- `DisplayName`, optional
- `LocationStateRef`
- `CargoContainerId`
- `DurabilityStateId`
- `LoadoutStateId`
- `LoadoutResourceStateId`
- `InsuranceStateId`, optional
- `LastServiceTransactionId`, optional
- `LastChangedEventId`, optional
- `State`: active, stored, docked, transferring, destroyed, impounded

`FShipDurabilityState`:

- `DurabilityStateId`
- `ShipInstanceId`
- `HullCurrent`
- `HullMax`
- `ShieldCurrent`
- `ArmorCurrent`
- `ModuleDurabilityBySlot`
- `LastDamageEventId`, optional
- `LastRepairResultId`, optional
- `LastChangedTransactionId`, optional

`FShipLoadoutResourceState`:

- `LoadoutResourceStateId`
- `ShipInstanceId`
- `FuelByResourceId`
- `AmmoByHardpointOrModule`
- `ChargeCountsByModule`
- `ConsumableCountsByModule`
- `LastRefuelResultId`, optional
- `LastRearmResultId`, optional
- `LastChangedTransactionId`, optional

Rules:

- save/load persists ship instance, durability, and loadout resource state with stable IDs
- repair, refuel, and rearm services mutate these records only through service result records and transaction journal entries
- actor health, fuel, and ammo caches are runtime views and are not the persistent source of truth
- respawn, insurance, storage, and ship swapping reference `ShipInstanceId`; they do not infer identity from pawn class or actor name

## Commodities And Markets

Markets are persistent logical state. UI is only a presentation layer.

`FCommodityDefinition`:

- `CommodityId`
- `DisplayName`
- `BasePrice`
- `MassPerUnitKg`
- `VolumePerUnitM3`
- `Category`
- `LegalityTags`
- `ProductionTags`

Commodity identity rule:

- `CommodityId` is the economy/market identity.
- `ItemId` is the inventory identity.
- commodities that can be carried must have exactly one canonical bridge record.
- market, route, production, and price data use `CommodityId`.
- containers, stacks, equipment, cargo transfer, and physical loot use `ItemId`.
- transaction records that move carried goods must store both IDs through the bridge, not infer one with string matching.

`FCommodityItemBridge`:

- `CommodityItemBridgeId`
- `CommodityId`
- `ItemId`
- `BridgePolicy`: carried_stack, virtual_market_only, mission_locked_variant
- `UnitConversionNumerator`, default `1`
- `UnitConversionDenominator`, default `1`
- `bMarketTradable`
- `bCargoHoldStorable`
- `bConfiscatable`
- `bMissionDeliverable`
- `LastValidatedContentVersion`

Validation must reject duplicate bridges for the same tradable commodity, commodity stacks with no bridge, and market transactions that try to deposit cargo without resolving a bridge.

`FMarketProfileDefinition`:

- `MarketProfileId`
- `ProducedCommodities`
- `ConsumedCommodities`
- `BaselineStocks`
- `PricePolicyId`
- `AccessPolicyId`

`FStationMarketState`:

- `MarketId`
- `StationId`
- `MarketProfileId`
- `StockByCommodity`
- `ReservedStockByCommodity`
- `PriceInputs`
- `LastProductionTickSeconds`
- `LastTransactionIds`

Market identity rule:

- default `MarketId = StationId`
- use explicit `MarketId` only when a station has multiple distinct markets

`FMarketTransactionRequest`:

- `TransactionId`
- `MarketId`
- `BuyerId`
- `SellerId`
- `CommodityId`
- `CommodityItemBridgeId`, required when cargo changes containers
- `Quantity`
- `QuotedUnitPrice`
- `SourceContainerId`
- `DestinationContainerId`
- `DebitAccountId`
- `CreditAccountId`
- `EscrowId`, optional
- `LegalContextId`
- `SourceEventId`, optional
- `IdempotencyKey`

`FMarketTransactionResult`:

- `TransactionId`
- `Result`: accepted, rejected, partial
- `AppliedQuantity`
- `UnitPrice`
- `StockDelta`
- `DisplayCreditDelta`, derived from ledger entries and not used as the account mutation source
- `LedgerEntryIds`
- `CargoTransferResultId`, optional
- `EscrowResult`, optional
- `RejectionReason`
- `SourceEventId`

Transactions must be idempotent and reference the event or player action that caused them.

Market acceptance order:

1. validate access, jurisdiction, and legal restrictions
2. validate commodity/item bridge if cargo containers are involved
3. validate stock/reservation and quoted price freshness
4. validate container capacity and stack rules
5. validate account balance or escrow hold
6. apply stock, cargo, and ledger entries under one idempotent transaction result

Partial success is allowed only if the result records the applied quantity, cargo transfer, and ledger entries that actually happened.

## Trade Jobs

`FTradeJobInstance`:

- `TradeJobId`
- `OwningShipId`
- `SourceMarketId`
- `DestinationMarketId`
- `CargoManifest`
- `ReservedSourceStock`
- `DepartureTimeSeconds`
- `ArrivalTimeSeconds`
- `RouteSegmentIds`
- `RiskProfileId`
- `CurrentEventId`
- `State`: planned, loaded, in_transit, interrupted, completed, failed

Trade jobs reserve stock when loaded and release or apply it exactly once on completion/failure.

## Station Service Endpoints

Station services are gameplay endpoints shared by docked menus, cockpit comms, and future FPS interiors. UI mode changes presentation only; it must not create separate service logic.

`FStationServiceEndpointDefinition`:

- `ServiceEndpointId`
- `StationId`
- `ServiceType`: market, repair, refuel, rearm, shipyard, outfitting, storage, mission_board, legal_office, black_market, passenger_lounge, vendor, bar_contact
- `ProviderFactionId`
- `CommsEndpointId`, optional
- `MarketId`, optional
- `InventoryContainerId`, optional
- `CreditAccountId`
- `AccessPolicyId`
- `LegalPolicyId`
- `ServiceProfileId`
- `PresentationModes`: docked_menu, cockpit_comms, fps_counter, fps_dialogue, remote

`FStationServiceRequest`:

- `ServiceRequestId`
- `ServiceEndpointId`
- `RequesterId`
- `RequesterMode`: docked_menu, cockpit, fps
- `ActionType`: quote, buy, sell, repair, refuel, rearm, pay_fine, post_bond, accept_mission, turn_in_mission, store_item, retrieve_item
- `PayloadRef`
- `SourceContainerId`, optional
- `DestinationContainerId`, optional
- `DebitAccountId`, optional
- `CreditAccountId`, optional
- `SourceConversationId`, optional
- `SourceEventId`, optional
- `IdempotencyKey`

`FStationServiceResult`:

- `ServiceRequestId`
- `Result`: accepted, rejected, partial, pending
- `SourceTransactionId`, optional
- `GeneratedTransactionIds`
- `LedgerEntryIds`
- `CargoTransferResultIds`
- `ShipServiceResultIds`
- `MissionInstanceIds`
- `MessageInstanceIds`
- `LegalActionIds`
- `RejectionReason`, optional
- `DebugReason`

`FShipRepairResultRecord`:

- `RepairResultId`
- `ServiceRequestId`
- `ShipInstanceId`
- `SourceTransactionId`
- `HullRepaired`
- `ModuleRepairResults`
- `DurabilityStateAfterRef`
- `LedgerEntryIds`
- `Result`: accepted, rejected, partial
- `RejectionReason`, optional

`FShipRefuelResultRecord`:

- `RefuelResultId`
- `ServiceRequestId`
- `ShipInstanceId`
- `SourceTransactionId`
- `FuelResourceId`
- `QuantityAdded`
- `ResourceStateAfterRef`
- `LedgerEntryIds`
- `Result`: accepted, rejected, partial
- `RejectionReason`, optional

`FShipRearmResultRecord`:

- `RearmResultId`
- `ServiceRequestId`
- `ShipInstanceId`
- `SourceTransactionId`
- `AmmoResourceIds`
- `QuantityAddedByResource`
- `ResourceStateAfterRef`
- `LedgerEntryIds`
- `CargoTransferResultIds`
- `Result`: accepted, rejected, partial
- `RejectionReason`, optional

Rules:

- menu trading, cockpit service comms, and FPS vendors call the same endpoint contracts
- service endpoints resolve market, inventory, faction, legal, comms, and account IDs before executing
- all money movement goes through the credit ledger
- all cargo movement goes through cargo transfer contracts
- all repair, refuel, and rearm mutations write ship service result records and ship resource state refs
- all mission accept/turn-in flows go through mission lifecycle records
- station service debug output must show endpoint, access decision, legal decision, and generated transaction IDs

## Comms And Messages

Comms are gameplay events with presentation, not direct UI calls.

`FCommsEndpointDefinition`:

- `EndpointId`
- `OwnerType`: station, ship, faction, mission, system
- `OwnerId`
- `AvailableChannels`
- `RangePolicyId`
- `AccessPolicyId`

`FMessageDefinition`:

- `MessageId`
- `MessageType`: docking, police_scan, pirate_demand, distress, mission, news, service, story
- `SpeakerId`
- `TextKey`
- `ResponseOptions`
- `GameplayEffectRefs`

`FMessageResponseDefinition`:

- `ResponseId`
- `MessageId`
- `TextKey`
- `AllowedEffectIds`
- `RequiresEventId`, optional
- `CreatesEventType`, optional

`FMessageResponseResult`:

- `MessageInstanceId`
- `ResponseId`
- `SourceEventId`
- `GeneratedEventIds`
- `AppliedEffectIds`
- `Result`: accepted, rejected
- `RejectedReason`, optional

`FMessageLogEntry`:

- `MessageInstanceId`
- `MessageId`
- `SourceEndpointId`
- `TargetId`
- `DeliveryChannel`
- `CreatedTimeSeconds`
- `ExpiryTimeSeconds`
- `SourceEventId`
- `ChosenResponseId`, optional

Messages can be authored or generated, but delivery and effects must be driven by stable IDs and events.

`FConversationInstanceState`:

- `ConversationId`
- `ConversationDefinitionId`, optional
- `OwnerType`: station, ship, faction, mission, service, story
- `OwnerId`
- `ParticipantIds`
- `CurrentNodeId`
- `State`: offered, active, waiting_for_response, resolved, expired, cancelled
- `SourceEventId`, optional
- `RelatedMissionInstanceId`, optional
- `RelatedServiceRequestId`, optional
- `LastMessageInstanceId`
- `LastChangedTimeSeconds`

`FMessageDeliveryArbitrationState`:

- `ArbitrationId`
- `TargetId`
- `CandidateMessageInstanceIds`
- `SelectedMessageInstanceId`, optional
- `SuppressedMessageInstanceIds`
- `DeliveryChannel`
- `PriorityInputs`: legal, mission, distress, docking, service, combat, story
- `InterruptPolicy`: queue, replace_lower_priority, block_until_response, expire_lower_priority
- `DecisionTimeSeconds`
- `DebugReason`

`FMessageArbitrationPolicy`:

- `PolicyId`
- `DeliveryChannel`
- `PriorityOrder`: police, combat, distress, docking, mission, story, service, news
- `TieBreakOrder`: created_time_then_message_instance_id, source_event_then_message_instance_id
- `ReplacementRules`
- `SuppressionRules`
- `ExpiryRules`
- `bPersistSuppressedMessages`

`FMessageArbitrationResultRecord`:

- `ArbitrationResultId`
- `ArbitrationId`
- `PolicyId`
- `TargetId`
- `DeliveryChannel`
- `InputMessageInstanceIds`
- `SelectedMessageInstanceId`, optional
- `QueuedMessageInstanceIds`
- `SuppressedMessageInstanceIds`
- `ExpiredMessageInstanceIds`
- `Decision`: deliver, queue, replace, suppress, expire, no_op
- `SourceTransactionId`, optional
- `SourceEventId`, optional
- `DecisionTimeSeconds`
- `IdempotencyKey`
- `DebugReason`

Delivery rules:

- one arbitration pass chooses what the player or NPC sees on a channel when multiple systems speak at once
- high-priority police, distress, docking, and combat messages can suppress or queue lower-priority service chatter
- arbitration uses `FMessageArbitrationPolicy`; ties are deterministic and must not depend on array iteration order
- each arbitration pass writes `FMessageArbitrationResultRecord`, and save/load restores pending delivery from result records before presenting UI
- response effects apply only once through `FMessageResponseResult`
- delivery, suppression, expiry, and chosen responses must survive save/load while a conversation is active
- mission and service conversations reference their owning lifecycle/service request records

## Mission Lifecycle

Mission content may stay thin while the foundation is being proven, but lifecycle records must exist for generated and authored missions, including cargo, legal exceptions, comms, rewards, failure, and turn-in arbitration.

`FMissionDefinition`:

- `MissionDefinitionId`
- `MissionType`: courier, cargo_delivery, bounty, patrol, mining, salvage, passenger, story, faction
- `IssuerFactionId`
- `SourceServiceEndpointId`, optional
- `ObjectiveDefinitions`
- `RewardDefinitionIds`
- `FailurePolicyId`
- `LegalExceptionPolicyId`, optional
- `GenerationTags`

`FMissionOfferRecord`:

- `OfferId`
- `MissionDefinitionId`
- `SourceStationId`
- `SourceServiceEndpointId`
- `IssuerFactionId`
- `TargetIds`
- `GeneratedSeed`
- `OfferCreatedTimeSeconds`
- `OfferExpiryTimeSeconds`
- `RewardPreview`
- `RequiredCargoSpace`
- `RequiredReputation`
- `LegalRiskSummary`
- `State`: available, reserved, accepted, expired, withdrawn
- `SourceEventId`, optional

`FMissionInstanceState`:

- `MissionInstanceId`
- `MissionDefinitionId`
- `OfferId`, optional
- `OwnerId`
- `IssuerFactionId`
- `CurrentState`: offered, accepted, active, turn_in_available, completed, failed, abandoned, expired
- `ObjectiveStateIds`
- `MissionCargoManifestIds`
- `LegalExceptionIds`
- `ConversationIds`
- `ServiceRequestIds`
- `RewardEscrowIds`
- `RewardLedgerEntryIds`
- `SourceEventId`, optional
- `AcceptedTimeSeconds`, optional
- `CompletedTimeSeconds`, optional
- `FailureReason`, optional
- `IdempotencyKey`

`FObjectiveState`:

- `ObjectiveStateId`
- `MissionInstanceId`
- `ObjectiveType`: travel_to, traverse_route, pass_gate, dock_at_station, deliver_cargo, pickup_cargo, scan, destroy, defend, escort, talk_to, pay, recover_item, wait_for_event, security_scan, clear_security_threat
- `TargetType`: system, station, gate, route, route_segment, cargo_container, cargo_stack, ship, group, zone, service_endpoint, character, event
- `TargetId`
- `RouteId`, optional
- `RouteSegmentIds`, optional
- `RequiredGateIds`, optional
- `SourceStationId`, optional
- `DestinationStationId`, optional
- `RequiredCargoManifestId`, optional
- `SecurityContextId`, optional
- `JurisdictionId`, optional
- `RequiredQuantity`, optional
- `ProgressQuantity`, optional
- `State`: inactive, active, satisfied, failed, skipped
- `LastProgressEventId`, optional

`FRewardDefinition`:

- `RewardDefinitionId`
- `CreditAmount`
- `ItemRewards`
- `ReputationDeltas`
- `PermitIds`
- `FollowupMissionTags`
- `EscrowPolicy`: none, hold_on_accept, reserve_on_turn_in

`FMissionCargoFlag`:

- `MissionInstanceId`
- `StackId`
- `CommodityId`, optional
- `ItemId`
- `Quantity`
- `CargoState`: reserved, loaded, delivered, lost, confiscated, consumed
- `LegalExceptionId`, optional

`FGeneratedFollowupOpportunityRecord`:

- `FollowupOpportunityId`
- `SourceEventId`
- `SourceTransactionId`, optional
- `SourceMissionInstanceId`, optional
- `OpportunityType`: mission_offer, bounty, distress_response, market_tip, patrol_request, smuggling_offer, story_hook
- `TargetIds`
- `RequiredValidationRefs`: route, cargo, security, faction_state, legal_state, service_endpoint, reward_policy
- `GeneratedSeed`
- `State`: proposed, validated, rejected, offered, expired
- `ValidationResultId`, optional
- `CreatedTimeSeconds`
- `ExpiryTimeSeconds`, optional
- `IdempotencyKey`

`FFollowupOpportunityValidationResult`:

- `ValidationResultId`
- `FollowupOpportunityId`
- `Result`: accepted, rejected
- `ResolvedRouteId`, optional
- `ResolvedCargoManifestId`, optional
- `ResolvedSecurityContextId`, optional
- `ResolvedServiceEndpointId`, optional
- `RewardEscrowId`, optional
- `RejectionReasons`
- `ValidatedTimeSeconds`

Mission rules:

- accepting a mission creates or updates `FMissionInstanceState` exactly once
- generated mission IDs are durable and must survive save/load
- mission cargo and legal exceptions attach to inventory, legal, and comms systems through stable IDs
- rewards use escrow or ledger entries; mission code does not write credits directly
- turn-in through docked menu and FPS dialogue uses the same station service endpoint and mission result records
- mission completion/failure emits canonical simulation events for faction, economy, legal, news, and message systems
- route, cargo, and security jobs must use typed objective fields instead of unstructured target IDs
- generated follow-up opportunities from mission, legal, market, combat, or route events must validate routes, cargo manifests, security context, service endpoints, faction state, legal state, and reward/escrow policy before becoming offers

## Roadmap Gate

Before logical encounters are treated as complete, the game needs a thin version
of:

- canonical event records
- durable credit accounts and ledger entries; escrow holds only when a selected
  slice needs pending payment or delayed release
- minimal faction definitions
- faction operational state for route/security/service decisions
- minimal jurisdiction/law/evidence/offense records
- inventory containers and cargo stacks
- one canonical commodity-to-item bridge
- commodity definitions and market states
- market transaction request/result
- station service endpoint request/result contracts shared by menu and FPS
- comms endpoints and message log entries
- conversation and delivery arbitration state
- gameplay transaction records and commit journal scaled to the selected slice
- mission offer, instance, objective, cargo, reward, and turn-in lifecycle records
- ship service result records for repair, refuel, and rearm
- reputation, faction relationship, and faction operational delta records
- generated follow-up opportunity validation records when generated follow-ups
  are in scope

The first version can be tiny. It still needs the right ownership and IDs.

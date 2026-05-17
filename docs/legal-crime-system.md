# Legal And Crime System

The legal/crime system should be a careful simulation layer, not a simple global wanted level.

It connects factions, policing, piracy, smuggling, mining rights, station access, comms, missions, reputation, and economy. It should be designed early enough that other systems can leave hooks for it, but not implemented before the core flight/system architecture is stable.

This is a future-system architecture document. The offense, evidence, and response sections describe required data shapes and integration seams, not behavior that the current foundation should build.

## Core Principle

Crime is jurisdictional.

The player should not become "criminal everywhere" from one local incident unless the offense, faction relationship, or story state justifies it.

## Required Concepts

### Jurisdiction

Defines who has legal authority in a place.

Examples:

- system jurisdiction
- sector jurisdiction
- station jurisdiction
- patrol zone jurisdiction
- faction-controlled space
- contested/uncontrolled space

Required data:

- jurisdiction ID
- controlling faction ID
- legal profile ID
- enforcement profile ID
- station/system/zone references
- priority
- explicit uncontrolled-jurisdiction behavior

When jurisdictions overlap, resolution must be deterministic. Station-local jurisdiction should normally beat broad system jurisdiction; explicit patrol or restricted zones may override both if their priority says so. Debug output must show why a jurisdiction won.

### Law Profile

Defines what is illegal under a jurisdiction.

Possible laws:

- assault
- murder
- piracy
- theft
- smuggling
- trespass
- illegal mining
- illegal salvage
- carrying restricted goods
- attacking police/faction ships
- docking violation
- unpaid fines
- mission-specific violations

Law profiles should be data-driven. Different factions can care about different offenses.

### Offense Event

A gameplay event that may become a crime.

Required data:

- offense event ID
- offense type
- source actor/entity ID
- victim/target ID, optional
- location/system/zone
- jurisdiction ID
- severity
- timestamp
- related item/commodity/ship/mission ID, optional
- evidence state
- source simulation event ID
- enforcement action IDs

Examples:

- firing on a neutral ship
- destroying a trader
- NPC pirate attacking an NPC trader
- NPC patrol destroying or disabling a wanted ship
- NPC smuggler refusing a scan
- mining in a restricted zone
- carrying contraband during a scan
- docking without clearance
- refusing a police scan

### Evidence And Witnessing

Crimes should not always be magically known.

Evidence sources:

- station sensors
- police scan
- eyewitness NPC ship
- distress call
- black box/log recovery
- faction surveillance zone
- mission script

Evidence states:

- unknown
- suspected
- witnessed
- verified
- reported
- cleared

This allows stealth, smuggling, piracy, and remote crime to have room for gameplay.

### Criminal Record

A persistent record of offenses, fines, and bounties.

Records should usually be per jurisdiction or faction.

Required data:

- record owner: player or NPC
- jurisdiction ID
- faction ID
- offenses
- unpaid fines
- active bounty
- wanted level/status
- last update time
- decay/expiry rules

### Enforcement Action Lifecycle

Enforcement is a durable action chain. A police scan, challenge, fine, confiscation, surrender, interdiction, or attack must be inspectable and idempotent across save/load.

`FEnforcementActionRecord`:

- `EnforcementActionId`
- `ActionType`: scan, challenge, warning, fine, confiscation, demand_surrender, accept_surrender, deny_surrender, interdict, escort, attack, call_reinforcements, close
- `State`: proposed, queued, communicated, accepted, refused, executing, resolved, failed, expired, cancelled
- `JurisdictionId`
- `LawProfileId`
- `EnforcementProfileId`
- `AuthorityFactionId`
- `SubjectId`
- `SubjectType`: player, ship, person, group, faction
- `TargetId`, optional
- `OffenseEventIds`
- `EvidenceRecordIds`
- `AssignedPatrolAssetIds`
- `ConversationId`, optional
- `MessageInstanceIds`
- `FineId`, optional
- `ConfiscationId`, optional
- `InterdictionHazardId`, optional
- `SurrenderRecordId`, optional
- `SourceEventId`
- `ResultEventId`, optional
- `CreatedTimeSeconds`
- `ExpiryTimeSeconds`, optional
- `IdempotencyKey`
- `DebugReason`

`FScanChallengeRecord`:

- `ScanChallengeId`
- `EnforcementActionId`
- `ScannerId`
- `SubjectId`
- `ScanProfileId`
- `RequiredCompliance`: hold_position, open_cargo, transmit_manifest, surrender, leave_zone
- `ResponseDeadlineSeconds`
- `DetectedContrabandStackIds`
- `DetectedCommodityIds`
- `EvidenceRecordIds`
- `State`: issued, complied, refused, evaded, completed, expired

`FFineRecord`:

- `FineId`
- `EnforcementActionId`
- `CriminalRecordId`
- `AccountId`
- `Amount`
- `CurrencyId`
- `DueTimeSeconds`, optional
- `PaidLedgerEntryId`, optional
- `State`: issued, paid, overdue, converted_to_bounty, forgiven, voided

`FConfiscationRecord`:

- `ConfiscationId`
- `EnforcementActionId`
- `SourceContainerId`
- `AuthorityContainerId`
- `StackIds`
- `CommodityIds`
- `CargoTransferResultIds`
- `ForfeitureLedgerEntryIds`
- `State`: ordered, completed, partial, refused, failed, voided

`FSurrenderRecord`:

- `SurrenderRecordId`
- `EnforcementActionId`
- `SubjectId`
- `AuthorityId`
- `Terms`: pay_fine, drop_cargo, submit_scan, dock_at_station, power_down, transfer_bounty
- `AcceptedTimeSeconds`, optional
- `CompletionEventIds`
- `State`: offered, accepted, refused, breached, completed, cancelled

Lifecycle rules:

1. classify offense and evidence under the current jurisdiction
2. propose an enforcement action from law, evidence, profile, faction state, and available patrol assets
3. reserve patrol/security assets if the action requires them
4. communicate challenge or demand through comms/conversation records
5. wait for compliance, refusal, timeout, or escalation
6. apply fine, confiscation, surrender, interdiction, combat, or closure through event/result records
7. release or update patrol assets and criminal records

Required invariants:

- every enforcement side effect references `FEnforcementActionRecord`
- fines use credit ledger entries; legal code does not write balances directly
- confiscation uses cargo transfer results; legal code does not mutate stacks directly
- fines, confiscation, surrender, illegal cargo rejection, interdiction fees, and bounty payouts that touch multiple systems use the gameplay transaction prepare/commit/recovery journal
- interdiction references logical encounter/hazard state rather than creating a separate encounter identity
- surrender terms are explicit records so FPS dialogue, cockpit comms, and station menus can all resolve the same action
- failed, refused, expired, and cancelled actions remain in debug history

### Fines And Bounties

Separate minor legal penalties from serious pursuit.

Possible outcomes:

- warning
- fine
- cargo confiscation
- station service denial
- docking denial
- hostile patrol response
- bounty
- faction-wide hostility
- system/sector permit revoked

Fines should be payable. Bounties may require surrender, combat, bribes, faction contacts, or story resolution.

### Enforcement Response

Determines how police/security reacts.

Response levels:

- ignore
- warn
- scan
- fine
- demand cargo
- escort out
- deny docking
- interdict
- attack
- call reinforcements

Enforcement should use the same NPC flight, comms, faction, and combat architecture as other ships.

Response selection must also consume `FFactionOperationalState` or an equivalent patrol/security availability record. Patrol density may bias the chance or urgency of response, but it is not a spawning license.

## Integration Points

### Factions

Legal status is related to faction status, but not identical.

A faction may dislike the player without the player being legally wanted. A player may be wanted in one jurisdiction while still neutral elsewhere.

### Reputation

Reputation should track trust/standing.

Legal status should track crimes, fines, and enforcement.

Keep them separate so the game can support:

- respected smuggler
- disliked but legal trader
- forgiven faction member
- wanted criminal with black-market friends

### Stations

Stations need legal hooks:

- docking clearance
- service access
- market restrictions
- police/security response
- mission board access
- contraband scans

Station comms should be able to warn, deny, fine, or escalate.

### Economy And Trading

Legal system affects:

- contraband
- restricted goods
- black markets
- confiscation
- fines
- smuggling missions
- faction price modifiers

Illegal cargo rejection must happen during transaction prepare. If a jurisdiction, scan, service access policy, or mission legal exception rejects the cargo, market stock, cargo stacks, ledger balances, mission objectives, and service state must remain unchanged. The rejection result must reference the legal context, jurisdiction, service/market request, and transaction ID for debug.

### Mining And Salvage

Mining/salvage needs legal ownership:

- legal mining zone
- restricted mining zone
- claimed wreck
- salvage rights
- faction permits
- illegal extraction offense

### Piracy

Piracy needs the legal system to avoid being just "enemy spawn behavior."

Pirate actions should create offenses where there is jurisdiction or evidence. Pirate factions may have their own reputation and protection rules.

This applies to NPC-NPC piracy, not only player piracy. If an NPC pirate attacks an NPC trader and the event is witnessed, reported, or discovered through a distress call or patrol scan, it should emit the same offense/evidence/criminal-record pipeline used for player crimes.

### Police Patrols

Police need:

- jurisdiction
- scan rules
- escalation rules
- pursuit authority
- interdiction behavior
- communication messages
- fine/bounty actions

Police response must consume or reserve real patrol assets in the simulation. Patrol density can influence assignment chance, but it must not create infinite police ships without asset state, cooldown, jurisdiction, and response availability.

Police scans, challenges, fines, cargo confiscation, surrender offers, and interdictions must all write enforcement action lifecycle records. A player who saves after receiving a scan challenge and reloads should resume the same challenge, deadline, patrol assignment, and conversation state.

### NPC-Owned Records

The legal system must support criminal records for NPC ships, NPC persons, pirate groups, and factions.

Required behavior:

- NPC-NPC offenses create offense events when jurisdiction/evidence rules allow it
- NPC pirate records can influence patrol targeting and bounty generation
- NPC police actions can clear, escalate, or close offense events
- NPC legal outcomes can feed faction influence, route security, economy disruption, and news
- destroying a wanted NPC is legally different from murdering a neutral NPC

### Missions And Story

Missions may:

- grant temporary legal authority
- hide or forgive offenses
- create warrants
- require illegal acts
- alter faction law profiles
- resolve bounties

Story can override normal law, but should do so through explicit data/state.

Mission legal exceptions must be scoped to typed objectives and cargo manifests. A route, gate, cargo, or security objective can grant a narrow exception, but turn-in still validates the current jurisdiction, cargo state, source service endpoint, reward policy, and any reputation/faction operational deltas before committing.

## Debug Requirements

The legal/crime system must have debug surfaces from day one.

Required debug views:

- current jurisdiction
- jurisdiction resolution trace
- active law profile
- recent offense events
- evidence state
- criminal records by faction/jurisdiction
- fines and bounties
- enforcement response level
- station access/legal denial reason
- police scan result
- contraband list
- legal status changes
- decision trace: jurisdiction -> law profile -> offense classification -> evidence source -> propagation/reporting -> enforcement response -> denial reason

Required validation:

- every jurisdiction has a controlling faction or explicit uncontrolled state
- every law profile references valid offense types
- every station/system/zone with legal behavior references a valid jurisdiction
- every contraband commodity references valid law behavior
- every enforcement response references valid comms/faction/NPC behavior
- overlapping jurisdictions have explicit priority or a deterministic default
- laws that require evidence have at least one possible evidence source
- enforcement responses that communicate with the player reference valid message/comms content
- contraband definitions connect to market handling or confiscation behavior
- station denial paths include player-facing reason strings
- every enforcement action references valid jurisdiction, law, evidence, comms, and patrol/faction operational state
- every fine references a valid account and ledger path
- every confiscation references valid commodity/item bridge and cargo transfer behavior
- every surrender/interdiction action references valid conversation and logical encounter records where applicable
- every enforcement transaction has a valid prepare/commit/recovery journal when it mutates ledger, cargo, mission, reputation, or faction operational state
- every illegal cargo rejection can explain which jurisdiction/law/service/mission exception decision blocked the transaction before mutation
- every legal reputation, relationship, or operational consequence writes a delta record and references the source enforcement action or transaction

## Risks

### Binary Wanted State

Avoid a single global wanted flag. It will make the world feel shallow and punitive.

### Magic Knowledge

Do not make every offense instantly known everywhere. Evidence/witnessing matters.

### Reputation Collapse

Do not collapse legal status and reputation into one number.

### Over-Enforcement

If minor mistakes always create combat, players will stop experimenting. Warnings, fines, scans, and docking denial are useful intermediate states.

### No Debug Visibility

Legal systems become frustrating if players and developers cannot understand why a response happened. Debug visibility is mandatory.

## Foundation Relevance

The current foundation does not implement crime.

The foundation should only avoid blocking it by preserving:

- stable faction IDs later
- station IDs
- zone IDs
- jurisdiction IDs
- law profile IDs
- enforcement profile IDs
- offense event IDs
- criminal record IDs
- permit/access requirement IDs
- comms endpoint IDs
- navigation target IDs
- save versioning path

The legal system becomes relevant once stations, NPC patrols, mining, trading, combat, and faction status start coming online.

# Phase 1 Foundation Review

This note records the current state of the Unreal foundation without using staged build labels as the organizing structure.

## Summary

The current Unreal implementation is a non-Sol, data-owned foundation for a Freelancer-like space game with more believable scale and stronger simulation contracts. It is engineering-playable and validation-backed, but it is not yet a complete player-facing vertical slice.

The foundation includes:

- non-Sol startup through `frontier_test_start`
- authored source system `frontier_test_01`
- authored destination arrival system `frontier_arrival_test_01`
- catalog-backed route edge for `frontier_gate_a`
- deterministic reference-frame and orbit queries
- active system registry build/teardown/rebuild
- normal flight, supercruise, docking, and gate arrival
- logical traffic and promotion/demotion hooks
- systemic gameplay records for faction, legal, inventory, market, service, comms, mission, transaction, and ledger flows
- logical encounter, combat/threat, and realized AI contracts
- service-level progression tests and debug traces

## Non-Sol Policy

Do not build Sol as the active Phase 1 or next-phase target.

Current active validation is `frontier_test_01` through `frontier_test_start`. Sol remains legacy authored/reference content that may be preserved later, but it must not be the startup default, substitution path, or architecture validation system.

## Compared To The Godot Version

The Unreal implementation has stronger data contracts, validation gates, save/load discipline, stable IDs, transaction journals, systemic records, and automation coverage.

The Godot version had more immediate playable breadth: menus, save/continue, Sol-sector flight, supercruise, interdiction, visible combat, docking/launch, station interaction spaces, trading, repair, inventory/equipment display, and mission flow.

The Unreal version is structurally stronger. The Godot version was more visibly playable.

## Current Readiness

The foundation is ready to build player-facing Phase 1 work on top of it when these gates are green:

- editor module builds
- full `Stargame` automation namespace passes
- build validation commandlet passes with warnings treated as failures
- no runtime path silently substitutes missing systems, spawn zones, start profiles, systemic state, or route data
- no automation test treats missing runtime context as a pass

The next work should make the existing validated systems visible, usable, and repeatable as a player-facing loop.

## Next Work

The next phase should be player-facing loop integration, not another backend-only stage.

Recommended target: Non-Sol Playable Loop Integration.

Goal:

Make the validated systems usable by a player in `frontier_test_01` without requiring direct test harness calls.

Scope:

- gate transition and arrival as a normal gameplay interaction
- thin mission board or dev panel backed by mission records
- station service UI for repair, refuel, rearm, and market transaction calls
- basic cargo buy/sell through the existing market, cargo, ledger, and legality services
- visible comms/message arbitration for service, encounter, police, and distress messages
- one pirate/distress/patrol scenario that surfaces through normal UI/debug flow
- save/reload through the player-facing loop
- keep all of this on `frontier_test_01`, not Sol

Done means:

- a player can start at `frontier_test_01`
- fly to or select meaningful targets
- dock or use a station/service flow
- run a simple mission/trade/service transaction
- encounter at least one systemic consequence
- save/reload and continue without duplicated rewards, cargo, ledger entries, messages, or encounter outcomes
- pass build validation and the player-facing loop automation

## Risk

The main risk is not missing backend structure. The risk is that the game still feels like systems behind glass. The foundation should now be used to build a thin but real playable loop.

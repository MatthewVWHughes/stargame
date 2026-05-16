# Stargame Docs

Architecture and build notes for Stargame.

## M0 Execution Path

The current build target is M0: boot a non-Sol test system, spawn the ship, expose stable-ID targets, and prove save/reload.

Read and implement in this order:

1. [Build Roadmap](build-roadmap.md) for scope and non-negotiable constraints.
2. [M0 Implementation Plan](implementation-plan-m0.md) for the exact startup chain, structs, APIs, tests, and current code debt.
3. [Runtime Architecture](runtime-architecture.md) for subsystem ownership, active system lifecycle, and registry rules.
4. [C++ And Blueprint Ownership](cpp-blueprint-ownership.md) for what must remain authoritative in C++.
5. [Frontier Test Fixture](frontier-test-fixture.md) for the literal `frontier_test_01` fixture values.
6. [System Data Contracts](system-data-contracts.md) for the Unreal-native data shape.
7. [Content Validation And Tooling](content-validation-and-tooling.md) for the build gate that keeps authored content, fixtures, and MCP-created assets usable by the game.
8. [Save, Load, And Versioning](save-load-and-versioning.md) for the durable session boundary.

Expected first code touch points:

- `Source/Stargame/Public/Runtime/StargameSessionSubsystem.h`
- `Source/Stargame/Private/Runtime/StargameSessionSubsystem.cpp`
- `Source/Stargame/Public/Space/StarSystemSubsystem.h`
- `Source/Stargame/Private/Space/StarSystemSubsystem.cpp`
- `Source/Stargame/Private/Core/StargameGameModeBase.cpp`
- `Source/Stargame/Private/Tests/M0StartupTests.cpp`

The immediate tests should prove `frontier_test_start -> frontier_test_01`, no fallback to `sol`, fixture target registration, and M0 save/reload.

M0 validator scope is intentionally limited at the roadmap/index level: start-profile resolution, non-Sol boot, the M0 fixture slice, stable target IDs, and M0 save/reload. The full content validation program remains in [Content Validation And Tooling](content-validation-and-tooling.md).

## Milestone Spine

Milestone sequencing is owned by [Build Roadmap](build-roadmap.md). Current named gates include M1 System Data Schemas And Registries, M5.5 Gate Transition And Arrival, M10.5 Combat, Damage, And Threat Contract, M11 Realized Space AI Slice, and M12 Playable Systemic Progression. Validation profiles are mapped in [Content Validation And Tooling](content-validation-and-tooling.md), including M2-M8, M5.5, and M10.5.

Milestone implementation should use this read order:

| Milestone | Primary docs | Supporting docs |
| --- | --- | --- |
| M0 | Build Roadmap, M0 Implementation Plan, Frontier Test Fixture, System Data Contracts | Runtime Architecture, C++ And Blueprint Ownership, Save Load And Versioning |
| M1 | Build Roadmap, System Data Contracts, Content Validation And Tooling | Runtime Architecture, Frontier Test Fixture |
| M2-M5.5 | Build Roadmap, System Data Contracts, Flight Supercruise And Docking, Frontier Test Fixture | Save Load And Versioning, Content Validation And Tooling |
| M6-M8 | Build Roadmap, Simulation Tiering And Economy, Space AI And Dynamic Orbits | System Data Contracts, Content Validation And Tooling |
| M9-M10.5 | Systemic Gameplay Foundations, Legal And Crime System, Simulation Tiering And Economy, Space AI And Dynamic Orbits | Save Load And Versioning, Future Systems Support |
| M11 | Space AI And Dynamic Orbits, Build Roadmap, Content Validation And Tooling | Systemic Gameplay Foundations, Flight Supercruise And Docking |
| M12 | Build Roadmap, Systemic Gameplay Foundations, Save Load And Versioning, Content Validation And Tooling | Future Systems Support, Legal And Crime System, Simulation Tiering And Economy |

## Canonical Docs

- [Build Roadmap](build-roadmap.md)
- [M0 Implementation Plan](implementation-plan-m0.md)
- [Frontier Test Fixture](frontier-test-fixture.md)
- [System Data Contracts](system-data-contracts.md)
- [Content Validation And Tooling](content-validation-and-tooling.md)
- [Save, Load, And Versioning](save-load-and-versioning.md)
- [Flight, Supercruise, And Docking](flight-supercruise-docking.md)
- [Systemic Gameplay Foundations](systemic-gameplay-foundations.md)
- [Runtime Architecture](runtime-architecture.md)
- [C++ And Blueprint Ownership](cpp-blueprint-ownership.md)

## Supporting Architecture

- [Simulation Tiering And Economy](simulation-tiering-and-economy.md)
- [Space AI And Dynamic Orbits](space-ai-and-dynamic-orbits.md)
- [Future Systems Support](future-systems-support.md)
- [Legal And Crime System](legal-crime-system.md)
- [Planet Rendering Direction](planet-rendering-direction.md)

## Tooling

- [Unreal MCP Setup](unreal-mcp.md)

## Archive

- [Legacy Gas Giant Scale](archive/legacy-gas-giant-scale.md)
- [Unreal Platform Research Snapshot](archive/unreal-platform-research-2026-05-16.md)
- [Legacy Godot Source Review](archive/legacy-godot-source-review.md)

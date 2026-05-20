# Stargame Docs

Architecture and game contracts for Stargame.

The current foundation is an authored frontier slice: a non-Sol start profile, a minimal boot new/continue flow, one active source system, a destination arrival system reached by gate transition, deterministic frame/scale queries, normal flight, supercruise, docking, generated physical-system support, logical traffic, systemic gameplay records, logical encounters, combat/threat state, realized AI hooks, and a first service-level systemic progression loop. It is engineering-playable and validation-backed, but the player-facing mission, service, market, comms, ledger, and reputation surfaces are still thin.

The docs are authoritative for gameplay and data contracts. Code should be brought up to the docs when a contract is already documented. When the docs still describe staging history, prefer migrating them toward what the game is instead of adding more chronology.

## Current Contract

- Startup resolves `frontier_test_start` into `frontier_test_01`; no runtime path may silently substitute `sol`.
- Active systems are built from stable gameplay IDs, data assets, and native validation, not actor names or level-script ownership.
- `frontier_gate_a` transitions to `frontier_arrival_test_01` through catalog data, route-edge data, a destination gate, and a gate-relative arrival marker.
- Save/load stores logical session state, player ship location, clock state, traffic/systemic state, and pending arrival state through C++ owned structs.
- Spaceflight is intentionally between Freelancer and KSP: approachable controls and combat, but believable scale, deterministic frames, and data-owned systems.
- Sol is legacy reference content only. It is not a default, substitution path, required fixture, or foundation target.

## Read Order

Use these docs as the active architecture set:

1. [Runtime Architecture](runtime-architecture.md) for subsystem ownership, active system lifecycle, and registry rules.
2. [System Data Contracts](system-data-contracts.md) for the Unreal-native data shape.
3. [Frontier Test Fixture](frontier-test-fixture.md) for literal fixture IDs and authored frontier-system values.
4. [Content Validation And Tooling](content-validation-and-tooling.md) for the build gate that keeps authored content, fixtures, generated content, and MCP-created assets usable by the game.
5. [Save, Load, And Versioning](save-load-and-versioning.md) for durable session boundaries.
6. [Flight, Supercruise, And Docking](flight-supercruise-docking.md) for player flight, travel, docking, and gate-arrival behavior.
7. [Space AI And Dynamic Orbits](space-ai-and-dynamic-orbits.md) for route prediction, traffic, groups, promotion/demotion, and realized AI.
8. [Systemic Gameplay Foundations](systemic-gameplay-foundations.md) for legal, economy, mission, services, comms, faction, transaction, and encounter records.

The historical build sequence remains in [Build Roadmap](build-roadmap.md), but it should not be treated as the primary game design index. As contracts stabilize, move durable game rules into the domain docs above and leave the roadmap as implementation history.

## Canonical Docs

- [Runtime Architecture](runtime-architecture.md)
- [System Data Contracts](system-data-contracts.md)
- [Frontier Test Fixture](frontier-test-fixture.md)
- [Content Validation And Tooling](content-validation-and-tooling.md)
- [Save, Load, And Versioning](save-load-and-versioning.md)
- [Flight, Supercruise, And Docking](flight-supercruise-docking.md)
- [Space AI And Dynamic Orbits](space-ai-and-dynamic-orbits.md)
- [Systemic Gameplay Foundations](systemic-gameplay-foundations.md)
- [C++ And Blueprint Ownership](cpp-blueprint-ownership.md)
- [Playable Parity Roadmap](playable-parity-roadmap.md)

## Supporting Architecture

- [Simulation Tiering And Economy](simulation-tiering-and-economy.md)
- [Future Systems Support](future-systems-support.md)
- [Legal And Crime System](legal-crime-system.md)
- [Planet Rendering Direction](planet-rendering-direction.md)

## Tooling

- [Unreal MCP Setup](unreal-mcp.md)

## Linux Audio Startup

On this workstation, Unreal 5.7's Linux audio path uses SDL3 through `AudioMixerSDL`. The installed editor build originally requested six output channels from SDL/Pulse on Linux, which could wedge PipeWire/WirePlumber and stall other audio/video clients. The local Unreal install is patched so `Engine/Binaries/Linux/libUnrealEditor-AudioMixerSDL.so` requests stereo output instead. Its backup is `libUnrealEditor-AudioMixerSDL.so.bak-codex-audio`.

The project also pins the Unreal Editor audio output in `Config/DefaultEditor.ini`:

- `bUseSystemDevice=False`
- `AudioOutputDeviceId=JBL Quantum350 Wireless Analog Stereo`

The wrappers do not mutate system audio profiles. They only choose the SDL3 Pulse backend, keep the legacy SDL2 variable for Unreal's stale environment check, and set a conservative Pulse latency:

- `tools/run-unreal-editor.sh` launches the editor with `SDL_AUDIO_DRIVER=pulseaudio`, `SDL_AUDIODRIVER=pulseaudio`, and `PULSE_LATENCY_MSEC=60`.
- `tools/run-unreal-automation.sh <test-filter>` runs headless automation with the same audio environment and `-NullRHI`.
- `tools/recover-linux-audio.sh` restarts `wireplumber`, `pipewire`, and `pipewire-pulse` if the desktop audio graph gets stuck.

Change `AudioOutputDeviceId` in `Config/DefaultEditor.ini` when intentionally routing Unreal to another output device.

## Historical Planning

- [Build Roadmap](build-roadmap.md)
- [Startup Implementation Plan](implementation-plan-m0.md)

## Archive

- [Legacy Gas Giant Scale](archive/legacy-gas-giant-scale.md)
- [Unreal Platform Research Snapshot](archive/unreal-platform-research-2026-05-16.md)
- [Legacy Godot Source Review](archive/legacy-godot-source-review.md)

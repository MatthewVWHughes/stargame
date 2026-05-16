# M0 Implementation Plan

M0 proves the non-Sol playable boot path.

The goal is not a full system architecture. The goal is a crude loop that forces the architecture to support a real player: start the project, spawn in `frontier_test_01`, fly, target objects by stable ID, and save/reload the current system/spawn state.

## Deliverable

PIE starts in `frontier_test_01` using `frontier_test_start`.

The player sees:

- one visible body marker/proxy
- one station marker/proxy
- one gate marker/proxy
- a flyable ship
- a debug/target list sourced from stable IDs

## M0 Done Means

M0 is complete when an automated test can start a new session, resolve `frontier_test_start`, build the M0 fixture slice, spawn the player at `spawn_deep_space`, expose exactly one body, one station, and one gate target from the registry, save `SystemId` plus `SpawnZoneId`, reload, and return to `frontier_test_01` without any Sol/Earth/Mars/Luna startup dependency.

The ship only needs to prove the existing pawn accepts input and changes transform in PIE. M0 does not require approach mechanics, target-distance HUD, flight-mode architecture, or polished flight tuning.

M0 flight acceptance: possess the existing ship pawn, inject or manually provide forward/right input, and assert the pawn transform changes during PIE.

## Startup Call Chain

M0 startup should follow this exact ownership chain:

1. `AStargameGameModeBase::BeginPlay`
2. `UStargameSessionSubsystem::StartNewSession(DefaultStartProfileId)`
3. `FFrontierTestFixtureProvider::ResolveStartProfile(StartProfileId)`
4. `UStarSystemSubsystem::BuildSystem(SystemId)`
5. `UStarSystemSubsystem::SpawnPlayerAtSpawnZone(SpawnZoneId)`
6. `UStarSystemSubsystem::GetNavigationTargets()` feeds HUD/debug target views

The default `StartProfileId` is `frontier_test_start`. Resolving it yields `SystemId = frontier_test_01`.

## M0 Struct Contract

These are the required M0 fields. Anything else is post-M0 unless the implementation needs it for Unreal plumbing.

`EStartSessionResult`:

- `Success`
- `MissingStartProfile`
- `MissingSystemDefinition`
- `MissingSpawnZone`
- `ValidationFailed`

`FStartProfileDefinition`:

- `FName StartProfileId`
- `FText DisplayName`
- `FName SystemId`
- `FName SpawnZoneId`
- `FName DockedStationId`, optional and unset for M0
- `FName DockingPortId`, optional and unset for M0

`FStarSystemDefinition`:

- `FName SystemId`
- `FText DisplayName`
- `TArray<FBodyDefinition> Bodies`
- `TArray<FStationDefinition> Stations`
- `TArray<FGateDefinition> Gates`
- `TArray<FSpawnZoneDefinition> SpawnZones`

`FBodyDefinition`:

- `FName BodyId`
- `FText DisplayName`
- `FName BodyType`
- `FName FrameType`
- `FName AnchorId`, optional
- `FTransform Transform`
- `double VisualRadiusCm`
- `FNavigationTargetDefinition NavigationTarget`

`FStationDefinition`:

- `FName StationId`
- `FText DisplayName`
- `FName FrameType`
- `FName AnchorId`, optional
- `FTransform Transform`
- `double VisualRadiusCm`
- `FNavigationTargetDefinition NavigationTarget`

`FGateDefinition`:

- `FName GateId`
- `FText DisplayName`
- `FName FrameType`
- `FName AnchorId`, optional
- `FTransform Transform`
- `double VisualRadiusCm`
- `double ActivationRangeCm`
- `FName DestinationSystemId`, optional and unset for M0
- `FName DestinationGateId`, optional and unset for M0
- `FName DestinationArrivalId`, optional and unset for M0
- `FNavigationTargetDefinition NavigationTarget`

`FSpawnZoneDefinition`:

- `FName SpawnZoneId`
- `FText DisplayName`
- `FName FrameType`
- `FName AnchorId`, optional
- `FTransform Transform`
- `double RadiusCm`
- `TArray<FName> AllowedContexts`

`FNavigationTargetDefinition`:

- `FName TargetId`
- `FText DisplayName`
- `FName TargetType`
- `FName FrameType`
- `FName AnchorId`, optional
- `bool bShowInHud`
- `bool bShowInSystemMap`
- `bool bCanTarget`

For M0, `TargetId == source object ID`. The target IDs are exactly `ember`, `brink_watch`, and `frontier_gate_a`.

## Implementation Steps

### 1. Remove Sol Startup Default

Change session startup so new sessions resolve through a start profile.

Required:

- add `StartProfileId`
- add `CurrentSystemId`
- default start profile is `frontier_test_start`
- no fallback to `sol`
- missing start profile returns a failure/error enum, logs `LogStargameStartup` at Error, shows an on-screen debug error in PIE, and does not build a system
- remove or restrict generic Blueprint-callable setters for save-critical state such as `CurrentSystemId`

### 2. Add Minimal Data Structs

Create the smallest C++ structs needed for M0:

- start profile
- system fixture
- body marker
- station marker
- gate marker
- spawn zone
- navigation target

Use one temporary in-code fixture provider for M0. It must expose one coherent `FStarSystemDefinition`-shaped fixture. Actors must not hard-code IDs or transforms locally. Primary DataAssets start in M1.

### 3. Add Active System Spawn Path

Create or extend the active system subsystem so it can:

- load/resolve `frontier_test_01`
- spawn placeholder body/station/gate actors
- register stable IDs
- expose navigation target list
- report build complete

Visuals can be crude placeholder meshes.

Minimal M0 registry contract:

- register entities and navigation targets by stable ID
- duplicate entity or navigation target IDs are errors
- expose read-only target list after build complete
- distinguish `RegisteredEntities` from `TargetableNavigationTargets`
- assert exactly three targets where `bCanTarget && bShowInHud`
- teardown/rebuild validation can wait until M1

### 4. Spawn Player From Start Profile

Use `spawn_deep_space`.

Required:

- spawn transform comes from fixture data
- ship does not assume Sol/Earth/Mars/Luna
- debug output prints selected start profile and system ID

### 5. Add Minimal Target List

The player/HUD/debug path must list targets from registry data:

- body target
- station target
- gate target

No hard-coded labels.

### 6. Minimal Save/Reload

Save:

- current system ID
- spawn zone ID
- selected target ID, optional

Reload:

- rebuilds `frontier_test_01`
- resolves saved `SpawnZoneId`
- restores player to a valid location

M0 reload ignores the physical ship transform and respawns at the saved `SpawnZoneId`. Full coordinate-frame ship location save starts in M2.

Minimal save object:

`FStargameM0SaveState`:

- `int32 SaveFormatVersion`
- `int32 GameContentVersion`
- `FString BuildCompatibilityId`
- `FName SystemId`
- `FName SpawnZoneId`
- `FName SelectedTargetId`, optional

The M0 payload is intentionally small, but it still uses the real save header so version/invalidation behavior is tested from the first slice.

Required development APIs:

- `bool SaveDevelopmentSlot(const FStargameM0SaveState& State)`
- `bool LoadDevelopmentSlot(FStargameM0SaveState& OutState)`
- slot name: `m0_development`
- load failure returns `false`, logs `LogStargameStartup` at Error, and does not fallback to `sol`

## M0 Debug Minimum

The debug surface can be a console command, Canvas HUD line, log dump, or MCP-readable state. It must show:

- selected start profile ID
- current system ID
- active system build state and last error
- spawn zone ID
- registered entity IDs and types
- targetable navigation target IDs and display names
- selected target ID
- save/load slot status

## Known Current Debt

These current source areas conflict with the intended M0 architecture and are first-class M0 cleanup targets:

- `UStargameSessionSubsystem::StartNewSession` currently resets to `sol`
- `UStargameSessionSubsystem::SetCurrentSystemId` exposes a generic Blueprint setter for save-critical state
- `AStargameGameModeBase` still performs prototype scene composition directly
- `SpaceFlightPawn` and gas-giant prototype actors still use `TActorIterator` discovery paths
- `PrototypeFlightHud` still contains hard-coded target/gameplay labels

## M0 Acceptance Tests

Create tests in `Source/Stargame/Private/Tests/M0StartupTests.cpp` using Unreal automation tests such as `IMPLEMENT_SIMPLE_AUTOMATION_TEST`. Tests that need a world can run in editor context; pure fixture validation should run headless.

- `StartNewSession_UsesDefaultStartProfile`: asserts `StartProfileId == frontier_test_start` and `CurrentSystemId == frontier_test_01`
- `BuildM0Fixture_RegistersTargets`: asserts navigation target IDs include exactly the M0 body, station, and gate targets
- `SaveReload_PreservesSystemAndSpawnZone`: saves and reloads `SystemId`, `SpawnZoneId`, and optional selected target
- `InvalidStartProfile_ReturnsFailureAndLogsError`: asserts no fallback to `sol` and no active system build
- `StartupSource_DoesNotReferenceSolBodies`: automated check or validation rule rejects startup dependency on `Earth`, `Mars`, `Luna`, or `Sol`
- `BlueprintCannotSetCurrentSystemId`: no public Blueprint-callable generic setter can mutate `CurrentSystemId`
- `HudTargetsUseNavigationData`: target labels come from registered navigation target data, not literals
- `NoActorIteratorBodyDiscovery`: M0 body/target queries use explicit registry/component references, not `TActorIterator`

## Explicit Non-Goals

M0 does not need:

- procedural generation
- full DataAsset authoring workflow
- full fixture population
- orbit simulation
- teardown/rebuild validation
- coordinate-frame save locations
- supercruise
- docking
- docking ports
- moving-frame docking
- gravity lockout behavior
- system map
- economy
- missions
- polished planet rendering
- atmosphere work

M0 is a pressure test. Keep it small and make it run.

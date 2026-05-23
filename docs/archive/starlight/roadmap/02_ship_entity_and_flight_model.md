# (02) — Ship Entity & Flight Model

**Layer 1: MOVEMENT — how things move through the world.**

**Depends on:** (01) (gravity wells, SOI, reference frames, floating origin).
**Blocks:** (03) (NPCs use the same flight model), (04) (docking needs ship state).

## Overview

Every ship in the game — player and NPC — uses the same entity model and flight physics. The player gets additional state (credits, equipment, discovery). The flight model has three speed tiers with the Phase Compression Drive enabling supercruise for inter-planetary travel.

## Ship Entity

`ShipEntity` is the unified data structure for all ships — player and NPC. Full definition and nested types (`PlayerState`, `NpcState`, `CriminalStatus`, etc.) live in [data_contracts.md](../reference/data_contracts.md). Summary:

- **Identity:** `Guid Id`, `string Name`, `string Faction` (faction id), `string Archetype` (archetype id — resolves to `ShipArchetype` stats)
- **Survival:** `Hull / MaxHull / Shield / MaxShield` (floats)
- **Movement:** `LinearVelocity`, `AngularVelocity`, `FlightMode` enum, `EngineKill` bool
- **Extensions:** `PlayerState Player` (null for NPCs) and `NpcState Npc` (null for player) — only one is non-null at a time

Archetype stats (max speed, turn rate, cargo capacity, slot counts, signature) come from the `ShipArchetype` record resolved by id — never stored inline on the entity. Hardpoint marker positions come from the ship model / wrapper scene, not from archetype JSON.

## Flight Model — Three Tiers

### Normal Flight (0–NormalMaxSpeed)

NormalMaxSpeed comes from the equipped engine (17: Tier 1 = 10, Tier 2 = 12, Tier 3 = 15 u/s). Force-based with quadratic drag. Thrust applies force in facing direction; drag provides a natural speed limit.

```
drag_coefficient = thrust_force / (normal_max_speed²)
drag_force = -velocity.normalized * drag_coefficient * speed²
velocity += (thrust + drag + strafe) * dt
position += velocity * dt
```

- Mouse cursor position (relative to screen center) applies angular acceleration (yaw/pitch)
- Angular drag bleeds off spin gradually — the ship has momentum
- Q/E for roll
- Terminal turn rate: ~2 rad/s at full mouse deflection

### Cruise (CruiseSpeed)

CruiseSpeed comes from the equipped engine (17: Tier 1 = 140, Tier 2 = 160, Tier 3 = 200 u/s). Direct velocity control — bypasses the force model to avoid numerical instability at high thrust/drag ratios.

- Shift toggles Normal ↔ Cruise
- Spool-up time from the equipped engine (17: Tier 1 = 4s, Tier 2 = 3s, Tier 3 = 2s)
- Speed lerps from NormalMaxSpeed to CruiseSpeed during spool
- Reduced steering (25% of normal turn rate)
- X key (brake) drops back to Normal

### Supercruise (CruiseSpeed–20,000 u/s) — Phase Compression Drive

The PCD creates an Alcubierre-type compression field. Speed is governed by two layers:

**Layer 1 — Star Distance Ceiling:**
```
ceiling = lerp(NearStarMax, DeepSpaceMax, sqrt(starDistance / scaleDistance))
```
Near the star: ~2,000 u/s. Past Saturn: ~18,000 u/s. The drive gets more efficient further from the star's gravitational gradient.

**Layer 2 — Gravity Well Slowdown:**
```
limit = CruiseSpeed + (ceiling - CruiseSpeed) * sqrt(surfaceDist / (bodyRadius * 10))
```
Approaching any planet/moon reduces speed toward CruiseSpeed. The final speed is the minimum of all gravity well limits.

**Supercruise Lockout Zone (3x body radius):**
Supercruise cannot engage or maintain inside any body's lockout SOI. Entering the zone forces dropout to Cruise. This prevents supercruise near stations/planets — you must cruise the last stretch.

- J toggles Cruise ↔ Supercruise (must be in Cruise first)
- 5-second spool-up
- Speed ramps toward the computed limit with acceleration (800 u/s²) and deceleration (3,000 u/s² — faster for safety)
- Steering scales inversely with speed (barely turning at 20,000 u/s)

### Afterburner

Temporary speed burst above normal max. Drains from the weapon power pool (same resource as energy weapons). Regenerates when not in use.

- Tab (hold) to afterburn
- Speed: ~2x normal max while active
- Duration: 3-5 seconds at full drain, depends on power allocation to engines
- Cannot afterburn during cruise or supercruise — normal flight only
- Combat use: close distance, break pursuit, dodge missiles
- Cancels on release or power depleted

### Engine Kill

Z toggles engine kill. All thrust stops, ship drifts on momentum with only drag slowing it. Drops any cruise/supercruise mode. Full rotation still available — useful for combat (drift while aiming).

## Autopilot & Reference Frames

### Station Keeping Autopilot

The reference frame system is the autopilot. In lore, the ship's computer automatically matches the orbital velocity of any body it's near. The player doesn't manually orbit-match — autopilot handles it.

**Inside any SOI** (planet, moon, or station) → Autopilot engages automatically. The ship inherits the body's orbital motion. HUD shows:
```
AUTOPILOT: Station Keeping
SOI: Earth
```

Stations have their own small SOI (~500 units). When near a station, the ship locks to the station's frame (which itself is inside the planet's frame). HUD shows:
```
AUTOPILOT: Station Keeping
SOI: Earth Hub
```

**How it works:**
1. Each frame, find the nearest gravity well the ship is inside (checks all bodies including stations)
2. Track that body's GlobalPosition delta from previous frame
3. Apply the delta to the ship's position
4. Skip deltas > 100 units (origin shift detection)
5. Chase camera compensates for the delta to prevent jitter

### Goto Autopilot

The player selects a target (station, planet, or any navigation object) and engages Goto. The ship flies itself to the target, managing speed tiers automatically.

**Engagement:** Player sets a navigation target via the system map (`M` key — click a station/planet and confirm) or via the comms menu. Once a nav target is set, `Shift+W` engages Goto. `Shift+W` again cancels it. Mouse movement, WASD thrust, and other inputs do **not** cancel Goto — only `Shift+W`. This prevents accidental dropouts on a long cruise when the player bumps the mouse or taps a key.

`T` remains reserved for combat targeting (06).

**Flight sequence:**
1. **Turn** — ship rotates to face the target
2. **Assess distance** — determines which speed tier to use
3. **Cruise spool** — if target is beyond cruise range, engage cruise
4. **Supercruise spool** — if target is beyond the supercruise lockout zone and far enough to benefit, engage PCD
5. **Cruise** — PCD speed scales naturally with star distance and gravity wells. Ship just flies straight.
6. **Approach** — as the ship enters the target's supercruise lockout zone, PCD drops out automatically. Ship continues at cruise.
7. **Decelerate** — as the ship approaches the target's SOI, decelerates to normal flight speed
8. **Arrive** — enters target's SOI, switches to Station Keeping. HUD shows `AUTOPILOT: Station Keeping | SOI: [target name]`

**Cancellation (Shift+W while Goto is active):** autopilot disengages immediately. Flight mode stays at whatever tier the ship was in — no forced mode change, no ejection. The player takes over at current velocity.

| Cancel during... | Behavior |
|------------------|----------|
| Cruise spool | Spool stops. Ship returns to Normal flight at the velocity it reached. |
| Cruise (steady) | Ship stays at cruise speed. Player can `Shift` to drop to Normal, `J` to engage Supercruise, or just fly. |
| Supercruise spool | Spool stops. Ship drops to Cruise (safe dropout — no transition state). |
| Supercruise (steady) | Ship stays in Supercruise. Same manual controls as if `J` had engaged it. |
| Approach / decelerate | Decel stops. Ship carries current velocity, player steers. |
| After arrival (Station Keeping) | Goto already ended; `Shift+W` has no effect. |

Emergency override is always one keypress away. Nothing in the spool sequences can "strand" the ship — cancel is a clean state transition at every step.

**HUD during Goto:**
```
AUTOPILOT: Goto — Mars Colony
DISTANCE: 156.2k u
ETA: 42s
MODE: Supercruise (4,200 u/s)
```

The ETA updates in real-time based on current speed and the PCD speed profile along the remaining route. Distance counts down.

**NPC equivalent:** NPCs in Tier 1 (rendered near the player) essentially run the same Goto logic — face the destination, fly along the route, manage speed tiers. The player's Goto is the same system with a HUD overlay.

### Disengaging Autopilot

The player can toggle autopilot off. When disengaged:
- The ship stops inheriting the body's orbital motion
- The planet/station drifts away as it continues orbiting
- HUD shows `AUTOPILOT: OFF`
- The ship is now subject to gravity drift (see below)

Re-entering an SOI with autopilot off does NOT auto-engage — the player must manually re-enable it. This prevents unwanted frame switching during combat near a planet.

### Gravity Drift

When autopilot is off AND engines are off (engine kill), the ship slowly accelerates toward the nearest major body (planet, moon, or star). This is NOT the full reference frame — just a gentle gravitational pull.

```
drift_direction = (nearest_body.GlobalPosition - ship.GlobalPosition).Normalized()
drift_acceleration = body_mass_class * 0.5  // small/medium/large → 0.1/0.3/0.5 u/s²
velocity += drift_direction * drift_acceleration * dt
```

- **Engines on** (any thrust, cruise, supercruise) → no gravity drift. The engines overpower it.
- **Engine kill** → drift activates. You slowly fall toward the nearest body.
- **Autopilot on** → no drift. You're station-keeping.

The drift is slow enough that the player has time to react (re-engage engines) but fast enough to be noticeable after 10-20 seconds of engine kill. It creates stakes:
- Engine kill near a planet = you're falling toward it
- Engine kill in deep space = you drift toward the sun (very slowly)
- Engines destroyed in combat near a planet = genuine danger

### HUD Display

| State | AUTOPILOT | SOI | GRAVITY |
|-------|-----------|-----|---------|
| Near station, autopilot on | Station Keeping | Earth Hub | — |
| Near planet, autopilot on | Station Keeping | Earth | — |
| Flying to Mars Colony | Goto — Mars Colony | — | — |
| Goto arriving at destination | Goto — Mars Colony | Mars Colony | — |
| Near planet, autopilot off, engines on | OFF | Earth (nearby) | — |
| Near planet, autopilot off, engine kill | OFF | Earth (nearby) | Earth (drift 0.3 u/s) |
| Deep space, engine kill | OFF | — | Sol (drift 0.1 u/s) |
| Deep space, engines on | OFF | — | — |

## Steering — Freelancer-Style Cursor

Mouse position relative to screen center creates a steering input:
- Center = no turn
- Edge = max turn rate
- Clamped to unit circle (consistent diagonal speed)
- Dead zone (5%) prevents micro-twitching
- Space bar toggles steering on/off (for UI interaction)

Angular velocity (not direct rotation) gives the ship mass and momentum. Angular drag (6.5/s) bleeds off spin. The result is a ship that feels heavy but responsive.

## Chase Camera

Spring-damped follow camera ported from the Freelancer rebuild:

- **SmoothDamp** (critically-damped spring) for position
- **Turn lag** — stiffness drops during turns, camera swings wider
- **Speed-based stiffness** — springy at normal speed, snaps rigid at cruise/supercruise
- **Look-ahead** — camera looks from its own position + forward * distance, naturally putting the ship in the lower screen
- **Smoothed up vector** — 85% ship-up, 15% world-up to prevent jarring flips
- **Speed FOV** — 50° base, widening to 55° at cruise
- **Reference frame compensation** — camera position shifts with the dominant body delta
- **Camera fill light** — OmniLight on the camera ensures the ship is visible from any angle

## Collision System

Ships are physical objects — they can hit things and take damage. The collision model is simple and speed-based.

### Collision Layers

| Layer | Contains | Collides With |
|-------|----------|---------------|
| Player Ship | Player only | NPC ships, stations, asteroids, celestial bodies |
| NPC Ships (Tier 1) | Spawned NPCs | Player, other NPCs, stations, asteroids |
| Stations | All stations | Player, NPCs |
| Asteroids | Collidable rocks in fields | Player, NPCs |
| Celestial Bodies | Planets, moons, star | Player, NPCs |
| Pickups | Floating cargo, loot | Player only (Area3D, no physics) |

Tier 2/3 NPCs have no collision — they're pure data.

### Collision Damage

Damage is based on closing speed at the moment of impact:

```
impact_speed = relative_velocity.Length()
if impact_speed < damage_threshold:
    // Bump only — no damage. Ship bounces off.
    return
damage = (impact_speed - damage_threshold) * mass_factor
```

| Impact Type | Damage Threshold | Mass Factor | Notes |
|-------------|-----------------|-------------|-------|
| Ship ↔ Ship | 3 u/s | 1.0 | Both ships take damage proportional to the other's mass class |
| Ship ↔ Asteroid | 2 u/s | 1.5 | Asteroids don't move — they're static collision shapes |
| Ship ↔ Station | 2 u/s | 0.5 | Stations are forgiving — docking guidance reduces this |
| Ship ↔ Planet/Moon | 0 u/s | 999 | Instant death. You hit a planet, you're dead. |

### Speed Tier Protections

- **Normal flight**: collisions are possible and expected. Low speeds mean low damage from bumps.
- **Cruise (160 u/s)**: asteroid field collisions are lethal at full cruise. The player must drop to normal flight or navigate carefully. This IS the navigation hazard from (09).
- **Supercruise**: the lockout zone (3x body radius) prevents supercruise near planets and stations. Asteroid fields DO NOT have lockout zones — but asteroids are small enough that the collision check at supercruise speeds is effectively "did you fly directly into one." At 5,000+ u/s the player passes through an asteroid field in seconds; the risk is low but nonzero.

### Bounce Response

Below the damage threshold, ships bounce off surfaces with a reflected velocity (coefficient of restitution ~0.3 — mostly absorbed, not pinball). Above the threshold, the ship still bounces but also takes damage.

### Implementation

Player and Tier 1 NPC ships use `CharacterBody3D` (position controlled by the flight model, not Godot physics). Collision detection uses `MoveAndCollide` for contact detection. On contact, the flight model applies the bounce impulse and the damage system applies hull damage.

Stations and celestial bodies are `StaticBody3D`. Asteroids in fields are `StaticBody3D` instances (collidable subset of the MultiMesh visual field — not every visual rock has collision, just the large ones).

## What This Plan Produces

- ShipController component (all flight modes, steering, autopilot/reference frames, gravity drift)
- GravityWell struct (consumed from (01), extended to include stations)
- Autopilot system (station keeping toggle, SOI detection, gravity drift)
- ChaseCamera component (spring-damped, speed-adaptive)
- Collision system (speed-based damage, bounce response, layer separation)
- Ship visual (box-mesh prototype, replaced with real models later)
- FreeCamera (debug, F1 toggle)

## Prototype Status

**Status: `✓ PROTOTYPE`** — behavior works in `game/`, but must be rewritten against foundation contracts ([data_contracts.md](../reference/data_contracts.md) `ShipEntity` + `ShipArchetype` registry). Do not port code blindly; treat the prototype as a reference for flight feel, not a source tree.

Three-tier flight, momentum steering, spring camera, reference frame inheritance, supercruise lockout zones, PCD speed curve — all implemented and tested. 

**Not yet prototyped:** Autopilot toggle (currently always on inside SOI), gravity drift when autopilot is off, station SOIs (stations aren't in the gravity wells array yet in prototype), collision system (speed-based damage, bounce response, collision layers). The real version also adds: proper RigidBody3D/kinematic collision, equipment-based stats, damage/destruction.

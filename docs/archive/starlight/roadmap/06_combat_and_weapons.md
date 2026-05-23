# (06) — Combat & Weapons

**Layer 4: CONFLICT — the universe pushes back.**

**Depends on:** (02) (ship entity, flight model, engine kill), (03) (NPC ships as combat targets), (04) (events, reputation basics).
**Blocks:** (07) (faction reputation drives economy access), (09) (mining protection/raiding).

## Overview

Combat is the consequence layer. It emerges from piracy, faction hostility, mission objectives, and player choices. The system must handle everything from a 1v1 pirate ambush to a fleet battle near a station, using the same rules for player and NPC.

## Two-Phase Implementation

### Phase A: Abstract DPS (build first)

Ships deal damage per second based on weapon loadout. No projectiles, no aiming — just range checks and DPS numbers. This enables:
- NPC combat AI (engage/flee decisions based on DPS comparison)
- Economy consequences (destroyed traders = lost cargo = price spikes)
- Reputation effects (who shot whom)
- Faction fleet combat (07)

### Phase B: Projectile Weapons (build later)

Real projectiles with travel time, aiming, leading targets. Visual weapon effects. Player skill matters. Phase B replaces Phase A for rendered ships (Tier 1) while Tier 2/3 ships still use abstract DPS.

## Weapon Types

| Type | Range | Speed | Behavior |
|------|-------|-------|----------|
| Kinetic (Autocannon/Railgun) | Short-Medium (~600 u) | Very fast projectile | Physical rounds. Strong against hull. Weak against shields. Ammo-limited. |
| Laser | Short (~500 u) | Instant | Hitscan, constant DPS while in range. Energy-based (no ammo, draws from weapon power). |
| Plasma | Medium (~800 u) | Fast projectile | Splash damage, good against shields. Energy-based. |
| Missile | Long (~2,000 u) | Tracking | Lock-on, countermeasure-vulnerable, burst damage. Ammo-limited. |
| Cruise Disruption Missile | Very long (~5,000 u) | Fast | Knocks target out of cruise. Specialist ordnance. Ammo-limited, rare, expensive. |
| Graviton Net Emitter | Deployable | Stationary | Area denial. Collapses PCD field on trigger. (03) |

### Resource Model — Primaries vs Secondaries

**Primary weapons** (autocannon, railgun, laser, plasma — gun-mount slot) have **no ammo**. They're rate-limited by heat (see Weapon Overheat below) and by the weapons-power allocation. All primary weapons share the same resource model — their gameplay differentiation is damage type + range + fire rate, not ammo vs energy.

**Secondary / consumable equipment** — the only ammo-limited items:

| Slot | Item | Ammo | Restock at |
|------|------|------|-----------|
| Missile rail | Missile Rack | yes | Hangar / equipment dealer |
| Missile rail | CDM Launcher | yes | Hangar / equipment dealer |
| Utility | Decoy Flare Launcher | yes (stacks of 10) | Hangar / equipment dealer |

**Why no primary ammo:** running out of ammo mid-fight is theory-good, practice-annoying — it pulls the player out of the fun loop to scavenge or retreat. Heat already provides burst-then-cool pacing. Fuel systems are out for the same reason (see [data_contracts.md](../reference/data_contracts.md) "Shared enums — What's NOT").

### Damage Type Effectiveness

| Weapon Type | vs Shields | vs Hull |
|-------------|-----------|---------|
| Kinetic | Weak (25% damage) | Strong (150% damage) |
| Laser | Normal (100%) | Normal (100%) |
| Plasma | Strong (150%) | Weak (75%) |
| Missile | Normal (100%) | Normal (100%) |

This encourages weapon variety: plasma to strip shields, then kinetic to shred hull. Or all-laser for consistency. Or missiles for alpha strike.

### Weapon Overheat (All Primaries)

Every primary weapon (autocannon, railgun, laser, plasma) generates heat when fired. Heat is tracked per weapon mount independently. Different weapon types have different heat profiles — a railgun spikes heat per shot then cools; an autocannon generates heat steadily under sustained fire.

```
heat += heat_per_shot * dt   (while firing)
heat -= cool_rate * dt       (always, even while firing)
```

| Threshold | Effect |
|-----------|--------|
| 0-80% | Normal operation. Heat bar fills on HUD. |
| 80-100% | Warning zone. HUD bar turns yellow. Slight damage falloff (90% DPS at 100%). |
| 100% | Overheated. Weapon stops firing. Forced cooldown until heat drops below 50%. |

**Cooldown period**: ~3 seconds from 100% to 50%. During this time the weapon is offline — the player must manage burst length or alternate weapons.

**Heat and power allocation**: weapons-power allocation affects heat dissipation. At 100% weapons power, cool rate is 1.5x. At 0% weapons power, primaries are disabled entirely (they can't fire — no heat concern).

### Equipment Slots

Every ship has a fixed set of slots defined by its archetype. Equipment is installed in slots — one item per slot, swapped at equipment dealers.

| Slot Type | Count (varies by hull) | Accepts |
|-----------|----------------------|---------|
| Gun mount | 2-6 | Autocannon, Railgun, Laser, Plasma, Point Defense Turret |
| Missile rail | 0-2 | Missile Rack, CDM Launcher |
| Shield | 1 | Shield Generator (tiered) |
| Engine | 1 | Engine (tiered) |
| Scanner | 1 | Scanner (tiered) |
| Utility | 1-2 | Tractor Beam, ECM Jammer, Decoy Flare Launcher |

**Gun mounts** fire together on left click. All equipped guns fire simultaneously — no weapon group assignment. This keeps combat simple and avoids UI complexity. If a player wants selective fire, they unequip guns they don't want firing.

**Missile rails** fire on right click. If multiple rails are equipped, they fire in sequence (alternating left/right rail). Each rail has its own ammo pool.

**Utility slots** are passive or hotkey-activated:
- Tractor beam: automatic (activates when cargo in range)
- ECM Jammer: passive (always on)
- Decoy Flare Launcher: hotkey (G)

### Hardpoints

Ships have weapon hardpoint counts defined in archetype data, but the actual hardpoint marker positions come from the ship model / wrapper scene:
- Light fighters: 2-4 gun mounts, 1 missile rail, 1 utility
- Heavy fighters: 4-6 guns, 2 missile rails, 2 utilities
- Freighters: 1-2 gun mounts (turrets), 0 missile rails, 2 utilities
- Gunboats: 4-6 guns, 2 missile rails, 1 utility

## Damage Model

### Shields First, Then Hull

```
if (shield > 0):
    shield -= damage
    if (shield < 0):
        hull += shield  // overflow hits hull
        shield = 0
        shield_offline_timer = rebuild_time
else:
    hull -= damage
```

Shields regenerate after a delay when not taking damage. If depleted, they go offline for a rebuild period (~5 seconds) before regenerating.

### Shield Regeneration

Shields regenerate automatically after a delay without taking damage:

| State | Behavior |
|-------|----------|
| Taking damage | No regen. Shield absorbs hits. |
| Damage stopped | Regen delay timer starts (~3 seconds). |
| Delay elapsed | Shield regenerates at regen rate (affected by power allocation). |
| Shield depleted (0) | Shield goes **offline**. Rebuild timer (~5 seconds). After rebuild, shield starts regenerating from 0. |

**Offline state is the punishment for letting shields drop to zero.** The 5-second rebuild window means the player takes full hull damage with no shield protection. Managing shields (disengaging, boosting shield power) before they drop is a core combat skill.

### Range Attenuation

Weapon damage falls off with distance. Linear falloff from full damage at point-blank to ~25% at max range. This encourages closing distance.

```
distance_ratio = clamp(distance / max_range, 0, 1)
attenuation = lerp(1.0, 0.25, distance_ratio)
damage = base_damage * attenuation * type_effectiveness
```

## Targeting System

Targeting is how the player selects and tracks a specific ship or object. A target is required for missile lock-on, cargo scanning, and Goto autopilot.

### Target Selection

| Input | Action |
|-------|--------|
| T | Target object under cursor. If nothing under cursor, cycle to nearest untargeted ship. |
| Shift+T | Cycle to previous target (reverse order). |
| Y | Target nearest hostile. Skips neutrals and friendlies. |
| Ctrl+T | Clear target. |
| Click on system map | Set navigation target (station/planet — for Goto, not combat). |

**Under cursor targeting**: the cursor (which also controls steering) can point at a ship bracket on screen. Pressing T when the cursor overlaps a bracket targets that ship. The detection bracket must be visible (ship must be at least at "Contact" detection level — (06) Detection States).

**Cycle targeting**: if nothing is under the cursor, T cycles through detected ships by distance (nearest first). Each press of T advances to the next ship. Shift+T goes backward.

### Target Display (HUD)

When a target is selected:

```
┌────────────────────────┐
│ [Target Name]          │
│ Faction: Sol Navy      │
│ Ship: Patrol Fighter   │
│ ████████░░ Hull  80%   │
│ ██████████ Shield 100% │
│ Distance: 450u         │
│ Closing: -12 u/s       │
└────────────────────────┘
```

- **On screen**: yellow diamond bracket around target. Name + distance below bracket.
- **Off screen**: yellow arrow at screen edge pointing toward target. Distance shown.
- **Behind camera**: arrow flips correctly. V key (rear view) shows the target behind you.
- **Target info panel**: top-right corner of HUD. Shows name, faction, ship type, hull/shield bars, distance, and closing speed (positive = separating, negative = approaching).
- **Detection-limited**: info detail depends on detection state. Contact = distance only. Identified = name + faction + ship type. Scanned = hull/shield bars + cargo.

### Lead Indicator

When a target is selected and within weapon range, a **lead reticle** appears — a small circle showing where to aim to hit the target with projectile weapons given its current velocity.

```
lead_position = target_position + target_velocity * (distance / projectile_speed)
```

- Only shows for gun-mount weapons (kinetic, laser, plasma). Not for missiles.
- Accounts for target's current velocity but not acceleration — the player must predict turns.
- Disappears when target is beyond max weapon range.
- Color: faint yellow circle. Turns red when the cursor (aim direction) is within the lead reticle — indicating shots will likely hit.

## Weapon Firing

### Primary Weapons (Left Mouse — Hold to Fire)

Each equipped gun-mount weapon fires on its **own independent clock** while left mouse is held — no sync across mounts. Each weapon has a `fire_cooldown = 1 / fire_rate` timer; when the timer elapses, it fires. This means an autocannon at 8Hz fires 8 shots for every 1 shot a railgun fires at 1Hz. Each weapon's full DPS is realized regardless of what else is equipped.

- **Lasers**: continuous beam from each gun mount to the point the cursor is aimed at. Hitscan — instant hit. Beam visually connects gun hardpoint to impact point. DPS applied continuously (no per-shot cooldown)
- **Kinetic (Autocannon)**: rapid tracer rounds fired toward aim direction. Projectiles have travel time. Rate of fire: ~5-8 rounds/second per gun. Each round does discrete damage on hit
- **Kinetic (Railgun)**: single high-damage shot, slow fire rate (~1 shot/second). Brief bright line flash. Projectile is very fast but not instant
- **Plasma**: slow glowing orb projectiles. Fire rate ~2/second. Travel time visible. Splash damage on impact (small radius, can hit multiple targets if close together)

**Aiming**: the player aims with the mouse cursor (which also steers the ship). Projectiles fire toward the cursor's world-space direction from each gun hardpoint. Convergence point is at the target's distance when a target is selected, or at 500u when no target.

**Mixed loadouts**: each weapon runs on its own clock, so mixed loadouts don't interfere — lasers deal continuous DPS, autocannons chatter, railguns punctuate. Different projectile speeds mean the player must choose range — lasers hit instantly, autocannons need lead. The lead indicator uses the slowest equipped projectile weapon's speed (ignoring hitscan lasers) so it's useful for the weapons that actually need lead.

**Implementation:** each `WeaponSlot` instance owns its own `_cooldownTimer`. On `_Process`, if left mouse is held and `_cooldownTimer <= 0`, fire the weapon and reset the timer. Independent state across weapon slots. No global "weapon group" fire clock.

### Secondary Weapons (Right Mouse — Missiles)

Right mouse fires one missile from the next available rail. Requires:
1. A target is selected
2. Missile lock is achieved (see below)
3. Ammo is available

If no target or no lock, right click does nothing. No dumbfire mode — missiles always require lock.

### Missile Lock-On

Locking onto a target is automatic once conditions are met:

1. **Target selected** (T, Y, or cursor targeting)
2. **Target within missile range** (weapon max range, typically 1,500-2,000u)
3. **Target within tracking cone** (60° cone from ship nose — the target must be roughly ahead of you)

**Lock sequence**:
- Lock timer begins: default ~2 seconds (doubled to ~4 seconds if target has ECM)
- HUD: shrinking diamond animation around target bracket, tightening as lock progresses
- Audio: beeping tone that increases in frequency as lock builds
- Lock progress resets if target leaves the tracking cone or exceeds range
- **Lock achieved**: solid tone. Diamond bracket locks solid. "MISSILE LOCK" on HUD.
- Right click fires. Missile tracks the target.

**Lock persistence**: once locked, the lock holds as long as the target stays within missile range (even if it leaves the tracking cone). Lock breaks if target exceeds range, is destroyed, or player clears target.

**Being locked onto**: when an enemy achieves missile lock on the player, the HUD shows "MISSILE WARNING" with a directional indicator showing which direction the lock is coming from. This gives the player time to deploy countermeasures (G key — flares) or break line of sight.

### Ammo Management

Only missiles, CDMs, and decoy flares consume ammo. Primary weapons (autocannon, railgun, laser, plasma) have no ammo — they're heat-limited. Ammo is restocked at the Hangar or equipment dealer.

- **HUD display**: ammo count shown per equipped ammo-limited item. Low ammo warning at 20% remaining.
- **Empty**: when missile rack is empty, right-click does nothing. When flare stack is empty, G key does nothing. Primaries never run out — they overheat instead.
- **Missile count**: shown near the secondary weapon indicator. "MSL: 12" or "CDM: 2". "FLR: 6" below.

### Power Management

Three systems share a fixed power budget: **Shields (key 1)**, **Weapons (key 2)**, **Engines (key 3)**. Each system has an allocation from 0% to 100%, and all three always sum to 100%. The HUD displays them in key order: S/W/E left to right, so pressing a number key visually moves the corresponding bar.

**Controls:**

| Key | Action |
|-----|--------|
| 1 | Increase Shields by 10%, decrease the other two proportionally |
| 2 | Increase Weapons by 10%, decrease the other two proportionally |
| 3 | Increase Engines by 10%, decrease the other two proportionally |
| Shift+1 | Max Shields (100/0/0) |
| Shift+2 | Max Weapons (0/100/0) |
| Shift+3 | Max Engines (0/0/100) |
| 4 | Reset to balanced (33/33/33) |

**Proportional redistribution:** When tapping 1/2/3, the 10% increase is taken from the other two pools in proportion to their current allocation. If Weapons is at 50% and Engines is at 20%, pressing 1 takes ~7% from Weapons and ~3% from Engines. No pool drops below 0%.

**Stat multipliers scale linearly with allocation:**

| System | 0% | 33% (balanced) | 50% | 100% |
|--------|-----|----------------|------|------|
| Engines | 0.7x max speed, 2x cruise spool, no afterburner | 1.0x (baseline) | 1.15x speed, 0.8x spool | 1.3x speed, 0.5x spool, 2x afterburner duration |
| Shields | No regen, capacity unchanged | 1.0x regen | 1.25x regen | 1.5x regen, shields rebuild faster after collapse |
| Weapons | Energy weapons disabled, kinetic unaffected | 1.0x fire rate + damage | 1.25x energy DPS | 1.5x energy DPS, faster lock-on |

**Design notes:**
- 0% in a system is punishing but not fatal — engines still work at 0.7x, shields hold but don't regen, only energy weapons go offline (kinetic/missiles still fire)
- The multipliers are gentle at balanced (1.0x) and meaningful at extremes — encourages active management without punishing players who ignore it
- Tapping is for adjustments mid-combat, Shift+key is for hard commits ("all power to engines, we're running")

**NPC power profiles** are fixed per goal — no dynamic reallocation:

| NPC Goal | E / S / W |
|----------|-----------|
| Trading / Fleeing | 50 / 30 / 20 |
| Patrolling | 30 / 40 / 30 |
| Attacking | 20 / 30 / 50 |
| Mining | 40 / 40 / 20 |

## Combat AI (NPC Behavior)

### Engagement Rules

| NPC Type | Engages When | Flees When |
|----------|-------------|------------|
| Trader | Never (defensive only) | Hull < 50% |
| Patrol | Hostile detected, distress call | Hull < 25% |
| Pirate | Target is vulnerable (post-interdiction) | Hull < 30% or reinforcements arrive |
| Convoy Escort | Convoy under attack | Never (fights to death or convoy escapes) |
| Hired Escort | Player under attack | Hull < 20% (self-preservation — (03)) |

### Combat Loop (Tier 1 — rendered ships)

1. Detect hostile in range
2. Turn to face target
3. Close to optimal weapon range
4. Fire weapons (Phase A: DPS tick, Phase B: projectile aim)
5. Manage shields (face shield toward enemy)
6. Break off if flee condition met

## Phase A Combat Resolution

The damage math for abstract combat. Used by rendered (Tier 1) combat until Phase B ships, and used by Tier 2 data combat forever. Tier 3 uses the simpler instant-resolution variant at the end of this section.

### Tick cadence — 2 Hz

Combat ticks every 0.5 seconds, not every frame.

- Fast enough that hull/shield feedback feels reactive
- Slow enough that 20+ simultaneous combatants stay cheap
- Round intervals make numbers easier to balance
- The player-facing HUD smooths hull/shield bars between ticks (lerps over 0.5s) so visible combat looks continuous even though damage is applied in steps

### Per-tick damage — per attacker → target pair

For each tick where an attacker has `target.position` within firing cone and at least one primary weapon in range:

```
total_tick_damage = 0

for each equipped primary weapon on the attacker:
    distance = attacker.pos.distance_to(target.pos)
    if distance > weapon.Range: continue         // this weapon out of range

    // Range attenuation: full damage near, 25% at max range
    range_ratio = clamp(distance / weapon.Range, 0, 1)
    attenuation = lerp(1.0, 0.25, range_ratio)

    // Base damage this tick from this weapon
    weapon_tick_dps = weapon.Dps * 0.5          // 0.5 = tick length in seconds

    // Damage type effectiveness (per weapon, against current target layer)
    type_mult = type_effectiveness(weapon.Type, target.shield > 0)

    // Weapons-power multiplier (0% = 0.0, 33% = 1.0, 100% = 1.5)
    power_mult = attacker.weapons_power_multiplier

    // Exception: Kinetic primaries still fire at 0% weapons power (they
    // don't draw from the power pool). Laser + Plasma are disabled at 0%.
    if weapon.Type in [Laser, Plasma] and attacker.weapons_power == 0:
        continue

    total_tick_damage += weapon_tick_dps * attenuation * type_mult * power_mult
```

Type-effectiveness values come from the table in Damage Type Effectiveness above (kinetic: 25% shields / 150% hull; laser: 100%/100%; plasma: 150%/75%; etc.). Each weapon contributes with its *own* effectiveness — a mixed kinetic+plasma loadout stacks shield-stripping and hull-breaking per tick, no "average weapon type" compromise.

### Damage application — same tick, then death pass

After `total_tick_damage` is computed for every attacker → target pair across the battle:

```
// Apply damage pass — shields first, overflow to hull
for each target in battle:
    damage_to_absorb = sum(tick_damage from all attackers this tick)

    if target.shield > 0:
        absorbed = min(damage_to_absorb, target.shield)
        target.shield -= absorbed
        target.hull   -= (damage_to_absorb - absorbed)
        if target.shield <= 0:
            target.shield_offline_timer = equipped_shield.RebuildSeconds
    else:
        target.hull -= damage_to_absorb

    fire DamageDealt(each_attacker, target, per_attacker_damage, dominant_weapon_type)

// Death pass — after all damage is applied, not during
for each target in battle:
    if target.hull <= 0:
        target.hull = 0
        fire ShipDestroyed(target, last_attacker, target.position)
        remove target from battle
```

Running damage and death in separate passes prevents a dead ship from "landing" its last shot retroactively, and lets two ships kill each other on the same tick cleanly (both fire `ShipDestroyed`, both drop loot, both trigger reputation).

### Shield regen — same tick, target side

After the damage pass, any target that took no damage this tick and whose `shield_offline_timer` has elapsed regens:

```
if target.shield_offline_timer <= 0 and not_damaged_this_tick:
    if regen_delay_elapsed_since_last_damage:
        regen_amount = equipped_shield.RegenRate * 0.5 * shields_power_multiplier
        target.shield = min(target.shield + regen_amount, target.MaxShield)
```

### HUD smoothing

The HUD reads the per-tick hull/shield snapshots and lerps the visible bar over 0.5s to the new value. Players see bars drop smoothly, not in steps. The same lerp applies to NPC target-info panels the player has selected.

### Tier 2/3 — instant resolution

Tier 2/3 ships don't tick. On encounter:

```
attacker_effective_dps = sum(weapon.Dps) * avg_attenuation * weapons_power_mult
defender_effective_hp  = hull + shield

attacker_ttk = defender_effective_hp / attacker_effective_dps
defender_ttk = attacker_effective_hp / defender_effective_dps
```

Loser dies after `min(attacker_ttk, defender_ttk)` seconds of game time. Winner takes proportional damage: `winner.hull -= winner_effective_dps * loser_ttk`. Single-frame bookkeeping, no simulation loop. Fires `ShipDestroyed` on the death timer.

### Player ↔ NPC during Phase A

Until Phase B projectile weapons exist, the player's primaries also resolve via this tick. Left-click held flags primaries as "firing this tick"; the 2 Hz tick applies attenuated DPS to whatever's under the crosshair within range and firing cone. When Phase B ships, the player's primaries become per-projectile (per-shot visible bullets); Tier 2 NPC ↔ NPC stays on Phase A indefinitely.

## Reputation System

### Per-Faction Standing

Each faction tracks the player's reputation: -1.0 (hostile) to +1.0 (allied).

| Range | Status | Effect |
|-------|--------|--------|
| -1.0 to -0.5 | Hostile | Attack on sight. No station access. |
| -0.5 to -0.2 | Unfriendly | Denied docking at some stations. Higher prices. |
| -0.2 to +0.2 | Neutral | Normal access and pricing. |
| +0.2 to +0.5 | Friendly | Better prices. More mission options. |
| +0.5 to +1.0 | Allied | Best prices. Exclusive missions. Faction assistance. |

### Reputation Effects

- **Killing a faction's ships** decreases reputation with that faction
- **Killing enemies of a faction** increases reputation with them
- **Cascade**: factions have relationships. Helping faction A might anger faction B. Cascade math in (07).
- **Decay**: extreme reputation drifts toward neutral at 0.01 per in-game hour, evaluated every 5 game-minutes. Going from -1.0 hostile to 0.0 takes ~100 hours of gameplay — meaningful commitment, not permanent exile.

**Owner**: `ReputationManager` (16) holds cascade math, decay, and API. The `Reputation` dictionary lives on `PlayerState` (see [data_contracts.md](../reference/data_contracts.md)).

### Criminal Status

Attacking lawful ships flags the player as criminal. Criminal status has two levels:

| Level | Trigger | Duration | Effect |
|-------|---------|----------|--------|
| Suspect | Attacking a lawful ship (first offense, minor damage) | 10 minutes of game time | Patrols will scan you on sight. Scan reveals criminal flag → hostile. |
| Wanted | Destroying a lawful ship, carrying contraband when scanned | 30 minutes of game time | Patrols attack on sight in the offended faction's space. |

**Decay**: criminal status timer ticks down against `GameTime.Elapsed` continuously — including while docked. Docking doesn't reset the timer (you don't escape by docking) but you *can* wait at a station until the heat dies. Tradeoff: dock fees eat credits and you can't earn during. Not a free hideout, but a legitimate strategy.

**Clearing early**: certain criminal/frontier stations offer bribe services in the bar. Pay credits to clear criminal status with a specific faction immediately.

**Per-faction**: criminal status is tracked per faction. Being Wanted by Sol Navy doesn't make you Wanted by Miners' Guild (unless they're allied and cascade rules apply).

**Owner**: `CriminalStatusManager` (see (16)) holds the tick logic and `Flag / Get / Clear` API; the data lives on `PlayerState.Criminal` (see [data_contracts.md](../reference/data_contracts.md)).

### Cargo Scanning

Patrol ships and station authorities scan ships for contraband. This is the enforcement mechanic that creates risk for smuggling.

**Patrol scan flow:**

1. Patrol NPC detects the player (normal detection rules)
2. If player is within scan range (based on scanner tier — typically ~1,000u), patrol initiates a cargo scan
3. HUD warning: `CARGO SCAN IN PROGRESS` with a progress bar (~5 seconds)
4. Player options during scan:
   - **Do nothing**: scan completes. If carrying contraband → reputation hit + Wanted status with that faction. Patrol demands you drop the contraband or turns hostile.
   - **Jettison contraband**: dump illegal goods before scan completes. Scan finds nothing. No penalty. But cargo is lost (pirates or patrol may scoop it).
   - **Run**: engage cruise/supercruise and flee. Patrol gives chase. If you escape scan range, scan fails. But fleeing a scan raises Suspect status.
   - **Fight**: attack the patrol. Immediate Wanted status. Other patrols in range respond.

**Station scan**: when docking at a lawful station, an automatic scan occurs during the docking approach. Same rules — if contraband is found, docking is denied and player gets a reputation hit. The player must jettison contraband before entering the station's SOI to avoid this.

**Scanner tier interaction**: the player's own scanner tier affects scan resistance. A Tier 3 scanner provides early warning of incoming scans (HUD shows "SCAN DETECTED — 8s to contact"), giving more time to jettison or flee. This is passive — no player action needed.

## Interdiction Flow (Complete Picture)

Combining (03)'s graviton net with this plan's combat:

1. **Graviton net triggers** — ship drops from supercruise to normal, engine stutter
2. **Pirates close in** during stutter window (2-3 seconds)
3. **Comms demand** — "Drop your cargo or die" (player choice)
4. **Combat or compliance** — fight back, drop cargo, or try to re-engage cruise and run
5. **Cruise disruption missile** — if the player tries to cruise away, pirates fire CDMs to keep them in normal flight
6. **Escalation** — nearby patrols detect the fight and respond (if in patrol range)

## Countermeasures

Defensive systems that counter missiles, cruise disruption, and graviton nets. These create real loadout decisions — offensive capability vs. survivability.

### Countermeasure Types

| Type | Slot | Ammo | Effect |
|------|------|------|--------|
| Decoy Flare | Utility slot | Consumable (10 per stack) | 70% chance to redirect each incoming missile away from the ship |
| ECM Jammer | Utility slot | Passive (always on) | Doubles the time enemies need to achieve missile lock. No effect on already-fired missiles. |
| Point Defense Turret | Weapon hardpoint | Energy (weapon power pool) | Auto-targets incoming missiles within 300u, fires rapid pulses to destroy them. Uses a weapon slot — direct offensive trade-off. |

### Decoy Flares

- Launched rearward on keypress (default: G)
- Creates a heat/signal source that attracts tracking missiles
- Each flare rolls independently against each incoming missile (70% base chance, reduced to 40% against CDMs — they have hardened targeting)
- Flare persists for 3 seconds, then burns out
- Cooldown: 1 second between launches
- Must be purchased at stations and restocked like missile ammo
- HUD shows remaining flare count

### ECM Jammer

- Passive equipment — no activation needed
- Enemy missile lock time: normally ~2 seconds → with ECM: ~4 seconds
- Gives the player more time to break line of sight, turn away, or get out of range before a lock is achieved
- Does NOT affect missiles already in flight
- Stacks with evasive maneuvering: turning away from the locking ship resets the lock timer
- NPC pirates with ECM are harder for the player to lock onto with missiles — the system is symmetrical

### Point Defense Turret

- Occupies one weapon hardpoint (gun mount, not missile rail)
- Auto-fires at incoming missiles within 300u range
- Kill chance: ~80% per missile before it reaches the ship (depends on closing speed — faster missiles are harder to intercept)
- Draws from weapon power pool — firing point defense reduces energy available for offensive weapons
- Cannot be manually aimed — fully automatic, defensive only
- Effective against standard missiles, less effective against CDMs (CDMs are faster and have a lower intercept window)

### Graviton Net Countermeasures

Graviton nets are countered by detection, not by defensive equipment:
- **Tier 2 scanner**: 5,000u warning when a net is in your path
- **Tier 3 scanner**: 10,000u warning + marker on HUD, time to alter course
- **Shooting the net**: nets can be targeted and destroyed before triggering (small, hard to see, but scannable)
- **No passive counter**: if you hit a net, you drop out. Period. The counter is awareness, not equipment.

### NPC Countermeasure Usage

NPCs use countermeasures based on their archetype:
- **Traders**: often carry flares (defensive loadout)
- **Patrol ships**: ECM jammer standard, some carry point defense
- **Pirates**: rarely carry countermeasures (offensive focus)
- **Escorts**: point defense turrets common (protecting the convoy)

## Detection & Signatures

How ships detect each other. This drives combat engagement, ambush tactics, and smuggling gameplay.

### Ship Signature

Every ship emits a signature — a combination of heat, electromagnetic, and gravitational output. Signature is a single float value.

**Base signature** is defined per ship archetype:

| Ship Class | Base Signature | Rationale |
|-----------|---------------|-----------|
| Light Fighter | 0.5 | Small, low power output |
| Heavy Fighter | 0.8 | More systems, more output |
| Freighter | 1.5 | Big engines, big reactor |
| Gunboat | 2.0 | Military power plant |
| Mining Ship | 1.2 | Industrial equipment |

### Engine Mode Multiplier

Flight mode dramatically affects how visible you are:

| Flight Mode | Signature Multiplier | Notes |
|-------------|---------------------|-------|
| Engine Kill | 0.2x | Minimal emission — drifting dark |
| Normal (idle) | 0.5x | Low output, engines warm but not thrusting |
| Normal (thrusting) | 1.0x | Base signature |
| Cruise | 2.0x | High engine output visible from far |
| Supercruise | 5.0x | PCD compression field is a massive beacon |
| Afterburner | 3.0x | Bright flare, even at normal speed |

### Detection Range

```
detection_range = scanner_base_range * (target_signature * mode_multiplier) / 1.0
```

Example with a Tier 2 scanner (50,000u base range):
- Light fighter in engine kill: 50,000 * 0.5 * 0.2 = **5,000u** (nearly invisible)
- Freighter in supercruise: 50,000 * 1.5 * 5.0 = **375,000u** (visible across the system)
- Light fighter in normal flight: 50,000 * 0.5 * 1.0 = **25,000u** (standard)

Clamped: minimum detection range 1,000u (you can always see something right next to you), maximum = scanner base range * 5 (can't see further than your hardware allows regardless of target size).

### Zone Modifiers

| Zone | Detection Multiplier | Rationale |
|------|---------------------|-----------|
| Open space | 1.0x | Normal |
| Nebula / Gas Cloud | 0.3x | Sensor interference, signal scatter |
| Asteroid Field | 0.7x | Radar clutter from rocks |
| Station Vicinity | 1.0x | Clean signal environment |

Applied to the final detection range. A nebula makes everything harder to see.

### Detection States (HUD Brackets)

| Detection Quality | Range | HUD Display |
|------------------|-------|-------------|
| Undetected | Beyond detection range | Nothing — you don't know it's there |
| Contact | Detection range to 60% of range | Grey dot. "CONTACT" — heading only, no ID |
| Identified | 60% to 30% of range | Colored bracket. Ship type, faction visible. |
| Scanned | Below 30% of range | Full info — cargo, loadout, reputation status |

### Tactical Implications

- **Pirates** can lurk in engine kill near trade routes — nearly invisible until ships pass close. Ambush tactics.
- **Smugglers** in engine kill reduce scan chance — patrols must be closer to detect and scan them.
- **Supercruise** announces your arrival to an entire system. No sneaking in at PCD speeds.
- **Nebulae** are natural hiding spots — reduced detection means closer encounters, more surprise.
- **The player** can use engine kill + drift to approach a target stealthily. Engine kill near a planet lets you drift in undetected — but you're also falling (gravity drift, (02)).

## What This Plan Produces

- Equipment slot system (gun mounts, missile rails, shield, engine, scanner, utility)
- Damage model (shields + hull, shield regen + offline state, range attenuation)
- Weapon types (laser, plasma, kinetic, missile, CDM, graviton net)
- Weapon overheat system (energy weapons, forced cooldown)
- Targeting system (cursor targeting, cycle, nearest hostile, lead indicator)
- Weapon firing (left click = guns, right click = missiles, hold to fire)
- Missile lock-on (auto-lock in cone, audio/visual feedback, ECM interaction)
- Ammo tracking (kinetic + missiles, restock at stations)
- Power management (engines/shields/weapons split, 1/2/3/4 keys)
- Countermeasures (decoy flares, ECM jammer, point defense turret)
- Detection & signature system (ship signature, mode multipliers, zone modifiers, detection states)
- Phase A combat (abstract DPS for NPC resolution)
- Combat AI (engage/flee logic, NPC type behaviors)
- Reputation system (per-faction standing, cascade effects, criminal status)
- Cargo scanning (patrol scan flow, contraband detection, player counterplay)
- Phase B combat (projectile weapons — built later as visual layer)

## Not Yet Proven

None of the combat system is prototyped. The prototype has ships but no weapons, no damage, no reputation. This is the first major untested system.

### Build Order Within This Plan

1. Equipment slot data model (slot types, per-archetype definitions)
2. Damage model (shields + hull + regen + offline)
3. Targeting system (select, cycle, nearest hostile, HUD bracket)
4. Phase A abstract DPS (NPC combat resolution)
5. Lead indicator (requires targeting + weapon data)
6. Weapon firing (left click guns, hold to fire, projectile spawning)
7. Missile lock-on (tracking cone, lock timer, audio feedback)
8. Weapon overheat (energy weapons, heat bar, forced cooldown)
9. Ammo tracking (kinetic + missiles, HUD display)
10. Power management (E/S/W split, key controls)
11. Reputation system (per-faction standing, cascade)
12. Detection & signature system (base signatures, mode multipliers)
13. Combat AI (engage/flee/patrol)
14. Countermeasures (flares, ECM, point defense)
15. Criminal status + cargo scanning
16. Phase B projectile weapons (visual layer — last)

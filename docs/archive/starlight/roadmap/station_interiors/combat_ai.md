# Combat AI (Hostile NPCs)

Station interior FPS combat. Enemies use cover intelligently, peek around corners, coordinate loosely as squads, and feel dangerous without being omniscient. The goal is the F.E.A.R. illusion: individual self-interest + good level design = emergent tactical behavior.

Ambient (friendly) AI lives in [npc_ai.md](npc_ai.md). Both share movement primitives, the room marker infrastructure, and the perception system.

---

## Architecture

```
CombatArena
├── SquadCoordinator     — squad-level decisions, attack tokens, confidence (1Hz tick)
├── CoverManager         — cover point registry, reservation, tactical scoring
└── HostileNPC × N
    ├── Perception       — sight cone + hearing (shared PerceptionSystem)
    ├── StateMachine     — Idle / Patrol / Hunting / Engage / Attack / Retreat / Dead
    └── NavigationAgent3D
```

---

## State Machine

```
IDLE ──has patrol points──> PATROL ──sees player──> ENGAGE
  ↑                              │                     │
  │                       hears noise          finds cover
  │                              │                     ↓
  │                          HUNTING ──sees player──> ATTACK
  │                              │                     │
  │                        timeout 5s          flanked: reposition
  │                              │                     ↓
  │                              ↓              confidence < threshold
  └────────────────── IDLE    RETREAT ────safe────> ENGAGE
                                                        │
                                                   HP ≤ 0
                                                        ↓
                                                      DEAD
```

| State | Behavior | Movement | Fires? |
|-------|----------|----------|--------|
| `Idle` | Standing, scanning | None | No |
| `Patrol` | Walking between waypoints | Walk | No |
| `Hunting` | Moving to last known player position, scanning | Run | No |
| `Engage` | Seeking a cover point with LOS to player | Run | No |
| `Attack` | At cover, peeking and shooting | Stationary | Yes |
| `Retreat` | Moving to cover further from player | Run | While moving (inaccurate) |
| `Dead` | Ragdoll / death anim | None | No |

### Key transitions

- **Idle / Patrol → Engage** — sees player OR receives squad alert (via room sound propagation — see [npc_ai.md](npc_ai.md))
- **Any state → Engage** — taking damage forces re-evaluation
- **Attack → Engage** — current cover is flanked (player moved, cover no longer blocks LOS); find new cover
- **Attack → Retreat** — squad confidence drops below threshold
- **Hunting → Idle** — 5s scan at last known position, no contact

---

## Perception

Shared `PerceptionSystem` utility, used by both ambient and combat AI.

### Sight

- Forward cone: 80° wide, 25m range (default — see archetype table for variants)
- Raycast from the NPC head to the player's chest; must be unobstructed
- Detection range shortens in dark/flickering rooms (gameplay value — derelict atmosphere)

### Hearing

- Player weapon fire: heard within 30m, through walls, through closed doors (muffled)
- Player sprint: 10m, same room only
- NPC death cry: same room + adjacent rooms via open doorways (propagation walks the `StationWaypointGraph` — see [npc_ai.md](npc_ai.md) "Use 2 — Combat alert propagation")

### No omniscience

NPCs only know the player's position if they have current LOS or heard a recent sound. They navigate to the *last known* position, not the player's actual position. Breaking LOS and repositioning is a valid player strategy.

---

## Cover System

### CoverPoint

Custom `[Tool] [GlobalClass]` Marker3D placed in levels by hand (or auto-generated from wall geometry).

```
CoverPoint : Marker3D
├── Position           (inherited)
├── CoverNormal        (Vector3)  — direction cover faces AWAY from (toward danger)
├── CoverHeight        (float)    — height of the geometry
├── Type               (enum)     — Low (<1.2m), High (>1.6m)
├── CanPeekLeft        (bool)
├── CanPeekRight       (bool)
├── CanPeekOver        (bool)     — low cover, can fire over top
├── OccupiedBy         (HostileNPC) — reservation
└── NavMeshValid       (bool)     — on walkable nav mesh
```

**Peek detection** (computed at level load or in editor):

- Cast ray from `position + cover_right × 0.8m` toward open space → `CanPeekRight`
- Cast ray from `position - cover_right × 0.8m` toward open space → `CanPeekLeft`
- Low cover: cast ray from `position + (0, cover_height + 0.3, 0)` toward open → `CanPeekOver`

### CoverManager

Owns all cover points in the arena. Provides scoring and reservation.

**Scoring formula** (when an enemy needs cover):

```
// GATES: disqualify cover that fails these
if raycast(cover_pos + up*1.5, threat_pos) is NOT blocked:
    return -1000                           // doesn't block LOS
if no peek direction has LOS to threat:
    return -1000                           // can hide but can't fight

// SCORING: all inputs are attractiveness scores
score = 0
score += (1.0 - clamp(dist_to_self / 25.0)) × 0.3    // closer to me
if dist_to_threat < 5:  score -= 0.2                  // too close, dangerous
if 8 <= dist_to_threat <= 15:  score += 0.15          // sweet spot
if closest_ally_cover < 3m:  score -= 0.4             // don't clump
if closest_ally_cover > 6m:  score += 0.1             // spread out
score += clamp(angle_diff / 90°, 0, 1) × 0.15         // flanking angle bonus
if cover.height > my_pos.Y + 1.0:  score += 0.1       // height advantage
```

### Cover behaviors

**High cover** (wall, pillar):
1. Stand behind cover, back to wall
2. Peek left or right (whichever has LOS)
3. Fire 1-3 shots during peek window (1-2s)
4. Return to cover for hide duration (2-4s)
5. Repeat

**Low cover** (crate, console):
1. Crouch behind cover
2. Rise to peek over top OR lean to peek around side
3. Fire 1-3 shots
4. Duck back down
5. Repeat

**Peek side selection:**

```
threat_dir = (threat_pos - cover_pos).normalized()
cover_right = cover_normal.cross(Vector3.UP).normalized()
dot = threat_dir.dot(cover_right)
if dot > 0 and CanPeekRight:      peek right
elif dot < 0 and CanPeekLeft:     peek left
elif CanPeekOver (low cover):     peek over
else:                              blind fire or reposition
```

Peek timing randomized per NPC (1.5-3s between peeks) so multiple enemies don't sync.

---

## Squad Coordination

### Attack tokens

The player has an **engagement capacity of 3**. Each actively shooting enemy consumes 1 token. When all tokens are used, remaining enemies must wait in reserve — repositioning, patrolling, or holding at cover without firing.

Token assignment priority:
1. Enemy currently in `Attack` with LOS to player
2. Enemy closest to player with valid cover
3. Longest-waiting enemy in reserve

Tokens release when:
- Enemy enters `Retreat`
- Enemy dies
- Enemy loses LOS for >3s

### Confidence

Squad-level value, recalculated every 1s:

```
squad_strength = alive_members × avg_hp_fraction
threat_level = 1.0                              // player is always 1.0
confidence = squad_strength / threat_level

High    (>0.6) → Push: assign flankers, shorter hide times, advance to closer cover
Medium  (0.3-0.6) → Hold: standard cover behavior, maintain positions
Low    (<0.3) → Retreat: fall back in waves, longer hide, no flanking
```

### Suppress-and-flank

Triggered when confidence is High and ≥3 enemies alive:

1. Coordinator assigns 1 enemy as **suppressor** (best LOS to player's cover)
2. Suppressor fires at the player's COVER POSITION continuously (doesn't need to hit)
3. After 2s of suppression, coordinator assigns 1 enemy as **flanker**
4. Flanker seeks cover with significantly different angle on player (>45° from suppressor)
5. Flanker moves to new cover, transitions to Attack
6. Suppressor continues until flanker is in position, then returns to normal Attack cycle

Player signal: sustained fire on the same cover = flank incoming.

### Grenade flush (future)

Player stays in same cover >8s AND an enemy has a grenade → enemy throws grenade at player's cover position. Forces relocation. Other enemies reposition during the chaos.

---

## Enemy Archetypes

Same state machine, parameter presets:

| Parameter | Grunt | Rusher | Heavy | Sniper |
|-----------|-------|--------|-------|--------|
| MaxHP | 50 | 30 | 120 | 40 |
| Damage | 8 | 12 | 15 | 25 |
| Accuracy | 0.1 | 0.2 | 0.08 | 0.03 |
| FireInterval | 1.5s | 0.8s | 0.5s | 3.0s |
| PeekDuration | 1.5s | 0.5s | 3.0s | 2.0s |
| HideDuration | 3.0s | 1.0s | 2.0s | 4.0s |
| RunSpeed | 4.5 | 7.0 | 3.0 | 4.0 |
| SightRange | 25 | 15 | 25 | 40 |
| FleeThreshold | 0.3 | 0.0 | 0.15 | 0.5 |
| PreferredDist | 10m | 3m | 12m | 20m |
| Behavior | Standard | Rushes to melee, no cover | Sustained fire, hard to kill | Stays far, high damage |

**Rusher specials:** ignores cover, runs directly at player, fires while moving, self-preservation 0. Squad coordinator sends rushers when player is reloading or hurt.

---

## Level Design Requirements

The illusion of intelligent AI depends on cover placement more than on the AI algorithm.

### Cover placement

- **Spacing:** cover points every 4-8m (1-2 grid cells on the 4m grid)
- **Mix:** alternate low cover (crates, consoles, half-walls) and high cover (pillars, wall corners, doorframes)
- **Flanking routes:** every cover position approachable from ≥2 directions
- **Sightline breaks:** no straight corridor longer than 12m without cover
- **Asymmetric:** wall corners (one-sided) create better AI behavior than pillars (symmetric)
- **Height variation:** elevated positions with ramp access (sniper nests, catwalks)

### Required geometry properties (for auto-detection)

- Wall corners need clear space on one side (the peek side)
- Low cover: 0.8-1.2m height
- High cover: ≥1.6m height
- Cover must be on or adjacent to the nav mesh
- Cover normal can be derived from the wall face direction or set manually

### Station kit integration

The 4m grid naturally produces good cover layouts:
- **Corridors** — wall corners at doorways = high cover with side peek
- **Rooms** — desks, consoles, crates = low cover with over-peek
- **Pillars** — symmetric high cover (less ideal, but works)
- **Doorframes** — natural chokepoints with corner cover on both sides

Cover points authored per-piece in the station layout editor, stored in the JSON layout alongside props and spawn markers.

---

## Performance Budget

| Metric | Budget |
|--------|--------|
| Active enemies | 10-15 max |
| Cover evaluation per enemy | ~14 cover points × 3 rays ≈ 42 raycasts |
| Cover re-evaluation frequency | Every 1-3s per enemy, staggered (not every frame) |
| LOS checks | 1 raycast per enemy per physics frame (15/frame) |
| Squad coordinator tick | 1Hz, negligible cost |
| Total raycast budget | ~60-100/physics frame (under the ~450/frame comfort zone) |

NavigationAgent3D handles its own performance. Staggered path recalculation (0.2-0.5s interval, offset initial delays) prevents all agents recalculating on the same frame.

---

## Weapons

Hitscan for baseline. Raycast from NPC weapon position toward player with spread scaled by archetype accuracy. No projectile travel.

| Weapon | Profile |
|--------|---------|
| Pistol | Grunt / Scavenger default — accurate, slow |
| SMG | Rusher default — full-auto, sprays |
| Rifle | Heavy default — sustained fire, high damage |
| Sniper rifle | Sniper default — single-shot, devastating, long cooldown |
| Shotgun | Heavy alt — close range, wide spread |

Ammo is not tracked per enemy (gameplay simplicity — they have "enough"). Player ammo *is* tracked.

Phase B (future) could upgrade to projectile weapons for visual parity with space combat.

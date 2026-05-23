# (03) — NPC Simulation & Trade Routes

**Layer 2: THE LIVING WORLD — the universe moves and breathes.**

**Depends on:** (01) (stations, system data, gravity wells), (02) (ship entity, flight model — NPCs use the same physics).
**Blocks:** (04) (economy needs NPC trade flow), (06) (combat needs NPC targets).

## Overview

Thousands of NPC ships fly trade routes between stations. Most are invisible data — only the ones near the player are rendered. This tiered simulation is the backbone of the living world: NPCs carry cargo, generate trade flow, create targets for pirates, make space feel populated.

Every NPC has one **goal** driving its behavior. Goals are a flat state machine — one goal active at a time. Events can interrupt the current goal and push a new one, with the old saved for resumption. The same goal system runs at all three simulation tiers; only *execution* changes (Tier 1 steers a ship, Tier 3 ticks a timer).

---

## AI Architecture

### Goals

| Goal | Description | Used by |
|------|-------------|---------|
| `Trade` | Fly route, dock, sell cargo, buy cargo, depart | Traders |
| `Patrol` | Fly between patrol waypoints, scan contacts, respond to threats | Navy, faction military |
| `Mine` | Fly to resource zone, extract, return to station, deposit | Miners |
| `Pirate` | Loiter near trade route in engine kill, wait for target, interdict, demand/attack | Pirates |
| `Escort` | Match speed/position with escorted ship, engage threats to escort target | Convoy escorts, hired escorts |
| `Flee` | Cruise spool, fly toward nearest friendly station, dock | Any NPC under threat |
| `Fight` | Close to weapon range, engage target, manage shields | Any NPC in combat |
| `Dock` | Approach station, enter bay queue, dock | Any NPC arriving at station |
| `Undock` | Depart station, clear SOI, resume previous goal | Any NPC leaving station |
| `Dead` | Ship destroyed, remove from simulation | Any NPC |

### State transitions (interrupts)

When an event occurs (attacked, interdicted, distress call detected), the NPC evaluates whether to interrupt:

```
Interrupt priority (highest first):
1. Ship destroyed        → Dead (immediate, no save)
2. Hull < flee threshold → Flee (saves current goal)
3. Attacked / hostile    → Fight (saves current goal)
4. Interdicted (net)     → forced cruise dropout, then Fight or Flee
5. Distress call nearby  → Patrol NPCs respond, others ignore
6. Nothing               → continue current goal
```

Saved goal structure:

```csharp
struct SavedGoal {
    GoalType Type;
    int RouteIndex;
    float RouteProgress;
    int DestinationStation;
    // enough to resume where we left off
}
```

After `Fight` or `Flee` resolves (enemy dead, reached safety, escaped detection range), the NPC restores `SavedGoal` and resumes. If the saved goal is no longer valid (station destroyed, route disrupted), the NPC picks a new goal based on its type.

### Goal selection (after docking)

When an NPC completes a dock cycle, it picks its next goal:

| NPC type | Goal selection logic |
|----------|---------------------|
| Trader | Evaluate all routes from current station. Pick the one with best `profit / travel_time`. Load cargo. Depart on `Trade`. |
| Patrol | Pick next patrol waypoint from assigned list. Depart on `Patrol`. |
| Miner | Check assigned resource zone. If station needs raw materials, depart on `Mine`. If stock is full, idle at dock for 30-60s then re-check. |
| Pirate | Pick a trade route near the current system. Fly to ambush position. Enter `Pirate` (engine kill loiter). |
| Escort | Wait for assigned convoy/player to undock. Match their departure. Enter `Escort`. |

---

## Tiered Simulation

### Tier 1: Active (near player, ~50,000 units)

- Full Node3D in the scene tree with mesh, light, collision
- Moves along its trade route at flight-appropriate speed (runs the same flight model as the player — (02))
- Can be scanned, hailed, attacked, interdicted
- Spawned from pool when NPC's interpolated position enters range
- Despawned when 80,000+ units away (hysteresis prevents flicker)
- **Max spawned: 200** (GPU descriptor heap limit — see [godot-knowledge/gpu_descriptor_limits.md](../reference/godot/gpu_descriptor_limits.md))

### Tier 2: Adjacent (same system, far from player)

- No Node3D — just a struct in an array
- Position interpolated along route: `smoothstep(elapsed / travelTime)` between station positions
- Distance checked to player each frame for spawn promotion
- **Cost per NPC:** 2 float ops + 1 distance check per frame

### Tier 3: Distant (other sectors)

- Timer only: departure time + travel time → arrival time
- No position, no interpolation, no distance checks
- On arrival: dock (trigger economy), pick new route, depart
- Ships dock for 10-60 seconds (random) simulating loading/unloading
- **Cost per NPC:** 1 float comparison per frame

### Performance (proven in prototype)

| Tier | Count | Memory | CPU per frame |
|------|-------|--------|---------------|
| Tier 1 | 200 max | ~200 Node3D | GPU-bound (descriptor heap) |
| Tier 2 | 10,000 | ~240KB | Trivial |
| Tier 3 | 1,000,000 | ~24MB | ~2% of one core |

10,000 local + 1,000,000 distant ships at 60fps.

### Goal execution per tier

The goal state machine is the same object at all tiers. Only execution changes.

**Trade goal:**

| Tier | Execution |
|------|-----------|
| 1 | Steering behaviors fly the ship. Cruise/supercruise animations play. Docks visually. |
| 2 | Position interpolated via smoothstep along route. On "arrival" (progress = 1.0), trigger dock. |
| 3 | Timer. On expiry, dock event fires, stock changes applied. New route selected. |

**Fight goal:**

| Tier | Execution |
|------|-----------|
| 1 | Steering closes to weapon range. Weapons fire (Phase A: DPS tick, Phase B: projectiles). Shield management. Flee check each frame. |
| 2 | Abstract DPS comparison. `time_to_kill` computed for both ships. Loser flees or dies after the duration. Winner takes proportional damage. |
| 3 | Simpler — instant resolution on encounter. Winner continues, loser removed. |

### Tier promotion/demotion

When a Tier 2 NPC enters spawn range (~50,000u from player):

1. Allocate a Node3D from the spawn pool
2. Set position to interpolated route position
3. Set velocity to match route direction and speed
4. Transfer goal state — NPC continues current goal with steering instead of interpolation
5. Promote to Tier 1

When a Tier 1 NPC exceeds despawn range (~80,000u):

1. Record current position and goal state
2. Free the Node3D back to the pool
3. Convert position to route progress (nearest point on route)
4. Demote to Tier 2

Hysteresis (50k spawn, 80k despawn) prevents flicker at the boundary.

---

## Steering Behaviors (Tier 1)

Rendered ships fly using composable behaviors that output a desired direction + throttle.

### Core behaviors

| Behavior | Output | Used for |
|----------|--------|----------|
| `Seek(target)` | Steer toward target position | Flying to waypoint, station, route endpoint |
| `Arrive(target, slowRadius)` | Seek but decelerate near target | Final approach to station, parking near waypoint |
| `Flee(threat)` | Steer directly away from threat position | Running from combat |
| `Pursue(target)` | Seek predicted future position of moving target | Chasing a fleeing ship, closing to weapon range |
| `Evade(threat)` | Flee from predicted future position of threat | Dodging incoming fire (light fighters) |
| `OffsetFollow(leader, offset)` | Maintain fixed offset from leader's position | Escort formation, convoy formation |
| `ObstacleAvoid(ahead)` | Raycast ahead, steer away from obstacles | Asteroid field navigation |

### Behavior composition per goal

```
Trade goal:
  if far from destination → Seek(destination) + full throttle
  if near destination → Arrive(destination, 500u)
  if in asteroid field → blend ObstacleAvoid(0.7) + Seek(0.3)

Fight goal:
  if beyond weapon range → Pursue(target)
  if within weapon range → maintain distance (orbit strafe)
  if hull < flee threshold → Flee(target) + cruise spool

Escort goal:
  if leader exists → OffsetFollow(leader, 300u right)
  if threat detected → Pursue(threat) until dead or 2000u from leader
  if leader too far → Seek(leader) to rejoin formation

Patrol goal:
  Seek(next_waypoint)
  on arrival → idle 5-10s → Seek(next_waypoint)
```

### Speed tier management

Tier 1 NPCs manage cruise/supercruise the same way the player does (02):

- Beyond supercruise lockout zone + far from destination → supercruise
- Inside lockout zone OR approaching destination → cruise
- Near destination OR in combat → normal flight
- Flee goal → immediate cruise spool, supercruise if possible

NPC flight transitions match (02)'s PCD rules so NPC ships look correct flying alongside the player.

### Turn rate per ship class

Affects how quickly an NPC can face a target — directly affects combat effectiveness.

| Class | Turn rate |
|-------|-----------|
| Light fighter | ~3 rad/s (dodgy, hard to hit) |
| Heavy fighter | ~2 rad/s |
| Freighter | ~0.8 rad/s (ponderous, vulnerable) |
| Gunboat | ~0.5 rad/s (slow, compensated by firepower) |

---

## Trade Route Graph

### Structure

Stations are nodes. Trade routes are edges — direct A→B paths.

```csharp
struct StationData { string Name; OrbitalBody Body; }
struct TradeRoute { int Start; int End; }
```

### Route generation

Prototype: hardcoded. Production: generated from system data based on:

- Station type compatibility (mining station → refinery → trade hub)
- Distance (shorter routes more common)
- Economic demand (routes form where there's profit signal — (04))

### Star avoidance

Routes crossing near the star need a waypoint to go around. Check: does the segment from A to B pass within `star.radius + margin`? If yes, insert a waypoint perpendicular to the line at the star's distance.

### Route refresh

Stations orbit their parent bodies — route distances are technically dynamic. But orbits are slow (hours to days); routes refresh every 5-10 minutes. Background job recalculates distances and star-avoidance flags.

---

## NPC Movement Along Routes

### Smoothstep progress

NPCs don't fly at constant speed. Smoothstep `(3t² - 2t³)` naturally creates:

- Slow departure from station (cruise spool + clearing gravity well)
- Fast mid-route travel (supercruise in open space)
- Slow approach to destination (gravity well deceleration)

This matches the Phase Compression Drive behavior without computing it per-NPC.

### Travel time estimation

```
averageSpeed = route near inner system ? 3,000 u/s : 8,000 u/s
travelTime = max(distance / averageSpeed, 10 seconds)
```

Production: integrate the actual PCD speed profile along the route for accurate ETAs.

---

## NPC Types

| Type | Route behavior | When encountered |
|------|---------------|-----------------|
| Trader | Station ↔ Station, follows trade routes | Most common, carries cargo |
| Navy Patrol | Patrol waypoints near stations, responds to distress | In station vicinities, along routes |
| Miner | Mining zone ↔ Station, resource extraction cycle | Near asteroid fields, gas clouds |
| Pirate | Lurks near trade routes, doesn't follow them | Near ambush points |
| Convoy | Multiple traders + escorts on same route | On high-value routes |
| Hired Escort | Follows player ship | After hire (see Hired Escorts below) |

### Spawn pool

Pre-allocate a fixed pool of Node3D ship instances. Reuse on spawn/despawn — no runtime creation/destruction.

**Three pools, not one:**

| Pool | Size | Owner | Used for |
|------|------|-------|----------|
| Trade traffic | 200 | `NpcManager` (this plan) | Tier 1 traders, patrols, pirates, convoy members, miners — the ambient living world |
| Hired escorts | 2 | `NpcManager` | Player-allied escorts (see Hired Escorts below). Marked distinctly; don't count against trade traffic |
| Mission NPCs | 20 | `MissionManager` (13) | Temporary, mission-owned NPCs with mission-specific AI profiles |

Total worst case: **222 active Tier 1 ships** — still under the GPU descriptor budget (~250 headroom on D3D12). Mission NPCs are usually far fewer than 20 at a time; the reservation is worst-case.

**Why three pools, not one unified:** mission NPCs have different lifecycle (spawn when player enters mission area, despawn when mission ends) and different AI profiles (defend a zone, attack the player on sight) that would complicate the trade-traffic simulation if mixed. Hired escorts need player-affinity tagging that shouldn't bleed into neutral traders. Separation keeps each pool's logic simple.

Each pooled ship has:

- Simple box-mesh visual (prototype) or loaded ship model (production)
- Small OmniLight for visibility
- Collision via CharacterBody3D (production)

---

## Pirate Tactical AI

The most complex NPC type — multi-phase behavior.

### Ambush setup

1. Pick a trade route with traffic
2. Fly to a position near the route (within detection range of passing ships)
3. Enter engine kill — signature drops to 0.2× (nearly invisible)
4. Optionally deploy a graviton net emitter on the route (if equipped)
5. Wait

### Target selection

When a ship enters detection range:

- **Alone?** Pirates prefer solo targets over convoys
- **Trader/miner?** Pirates want cargo, not combat
- **Can I take it?** Compare own combat strength vs target. Pirates avoid patrols and gunboats
- **Worth it?** Freighters with cargo = high value; empty ships ignored

If the target passes all checks, the pirate engages.

### Engagement flow

1. **Power up** — engines on (signature spikes, target may detect)
2. **Close in** — `Pursue(target)` at full thrust
3. **Graviton net triggers** — target drops from cruise, stunned 2-3s. Pirate closes during stun
4. **No net** — fire CDM to knock target out of cruise (if equipped). Otherwise chase
5. **Comms demand** — "Drop your cargo or die" with 10s timer
6. **Target complies** — scoop jettisoned cargo, disengage, return to ambush
7. **Target refuses/ignores** — engage in combat
8. **Too strong** — flee before hull gets critical

### Multi-pirate behavior

Pirates don't coordinate tactically (no squad AI). But multiple pirates in the same area respond independently to the same target. This creates emergent pile-on — one attacks, target is distracted, another closes. Emergent, not scripted.

---

## Patrol Behavior

### Patrol waypoints

Each faction assigns patrol waypoints near their stations and trade routes. Patrol NPCs cycle through them:

```
Station A vicinity → midpoint along route A-B → Station B vicinity → ...
```

### Contact response

1. **Check faction** — allied / neutral / hostile?
2. **Allied/neutral** → ignore (or hail with flavor text if Tier 1)
3. **Hostile** → engage (`Fight` goal)
4. **Player with criminal status:**
   - Suspect → close to scan range, initiate cargo scan
   - Wanted → engage immediately
5. **Distress call** → break patrol, `Seek(distress_location)`, engage hostiles on arrival

### Scan behavior (Tier 1 only)

1. Close to scan range (~1,000u)
2. Initiate scan (5s timer, visible to player)
3. During scan: match target velocity (fly alongside)
4. Scan completes → check for contraband
5. Contraband found → demand jettison or engage

Full scan mechanics in (06).

---

## Convoy & Formation AI

### Structure

A convoy is 2-4 traders + 1-2 escorts on the same trade route. Traders use standard `Trade` goal. Escorts use `Escort` goal with the lead trader as leader.

### Formation

Escorts maintain offset positions relative to the lead trader:

- Escort 1: 300u right, 100u behind
- Escort 2: 300u left, 100u behind

`OffsetFollow(leader, offset)` handles this. Offset is in leader's local space, so the formation rotates with the convoy's heading.

### Threat response

When the convoy is attacked:

1. Traders continue on route (or spool cruise to escape faster)
2. Escorts break formation → `Fight` goal targeting attacker
3. Escort wins → resume `Escort`, catch up to convoy
4. Escort destroyed → traders are unprotected (pirate opportunity)
5. Trader destroyed → remaining traders flee, escorts protect survivors

---

## Flee Behavior

When any NPC enters `Flee`:

1. **Pick destination** — nearest friendly station (faction-compatible, not destroyed)
2. **Turn away** from the threat
3. **Spool cruise** — 3s spool (vulnerable during)
4. **Hit during spool** — interrupted by CDMs or graviton nets. Restart spool
5. **Cruise engaged** — fly toward destination at cruise speed
6. **Pursuer falls behind** (beyond detection for 10s) → threat cleared, consider resuming previous goal
7. **Pursuer keeps up** — spool supercruise if possible, otherwise keep fleeing at cruise
8. **Arrive at station** → dock, safe. Resume previous goal or pick new route

Freighters flee at 50% hull (can't win fights). Fighters flee at 25-30%. Convoy escorts never flee while their convoy exists.

---

## NPC Encounter Flows

### Patrol encounter

The most common lawful encounter. Patrol ships actively police their faction's space.

1. Player enters patrol's detection range
2. Patrol identifies player (detection reaches "Identified")
3. **Player reputation Hostile** → patrol immediately engages. Comms: "Hostile vessel detected. Engaging."
4. **Neutral or better** → patrol hails with faction chatter. "This is Sol Navy patrol alpha. Fly safe." No gameplay effect
5. **Suspect criminal** → close to scan range, initiate cargo scan. "Hold your course. Initiating cargo scan."
6. **Wanted** → attack on sight. No scan, no warning

Patrols denser near stations. Deep space patrols are rare — further from a station = less law enforcement.

### Trader / convoy encounter

Traders are background traffic. They don't interact unless provoked.

- **Passing traders** — fly past on trade routes. Ship brackets with faction colors at Tier 1 range. No interaction unless hailed or attacked
- **Convoys** — 2-4 traders + 1-2 escorts. Escorts fly in formation. Higher-value targets for pirates
- **Hailing a trader** — generic chatter. Possible rumor about prices or danger. Based on real economy data
- **Attacking a trader** — trader flees (cruise spool). Escorts engage. Reputation hit with trader's faction. Destroyed trader drops floating cargo

### Distress call

NPCs under attack broadcast distress calls. Ambient events the player can engage with.

1. NPC trader/miner is attacked (NPC-on-NPC combat, resolved by abstract DPS for Tier 2/3)
2. Combat in player's detection range → comms notification: `DISTRESS: "Under attack near [zone]! Any ships, please assist!"`
3. HUD shows distress marker (blue exclamation) at combat location with distance
4. **Player ignores** — combat resolves by abstract DPS. Trader dies or escapes. No reputation effect
5. **Player responds** — fly to location. Within Tier 1 range, pirates and trader spawn. Engage
6. **Reward for helping** — reputation boost with trader's faction. Possible credits: "Thank you, pilot. Transferring 200 credits."
7. **Expiration** — if no response within ~60s, combat resolves off-screen, marker disappears

### Pirate lurking

Pirates don't fly trade routes — they wait near them.

- **Ambush positions** — engine kill (low signature) near trade routes, asteroid fields, or ambush point zones
- **Detection** — hard to see until close (~5,000u with engine kill signature). Tier 3 scanner helps
- **Trigger** — ship passes within engagement range → pirates power up engines (signature spikes), close in
- **Player encounter** — interdiction flow (graviton net or direct pursuit + CDMs)
- **NPC encounter** — attack happens, player can intervene or avoid

### Miner at work

Near resource zones, the player encounters NPC miners.

- **Visual** — mining ship stationary near an asteroid, mining laser beam visible
- **Behavior** — ignore the player unless attacked. Absorbed in extraction
- **Hailing** — brief dialogue: "Busy here. Good ore in this belt though." Possible hint about rich zones
- **Attacking** — miner flees toward nearest station. Reputation hit with owning faction. Drops partially filled cargo on destruction
- **Protecting miners** — escort and patrol missions involve keeping miners safe. Reinforces mission purpose visually

### NPC-to-NPC combat (visible)

When Tier 1 NPCs from hostile factions are near each other, combat breaks out:

- Pirates attack traders on sight within engagement range
- Patrols engage pirates on sight
- Faction military ships fight opposing faction ships in contested zones

Player watches weapon fire, maneuvering, distant combat audio. Can intervene on either side (reputation consequences) or avoid entirely. Universe feels active — fights happen whether or not the player is involved.

Resolution: Tier 1 NPC combat uses the same damage model as player combat (Phase A abstract DPS; Phase B projectiles once the visual layer exists). Ships at flee threshold disengage.

---

## Graviton Net (Pirate Interdiction)

Pirates deploy graviton net emitters along known trade routes:

1. Small, hard-to-detect device placed in space
2. Ship in supercruise/cruise passes within 5,000-10,000u → detonates
3. Massive gravity pulse collapses the PCD field — ship drops to normal flight
4. Brief engine stutter (2-3s)
5. Pirates waiting nearby close in during the stutter window
6. Emitter is single-use (consumed on detonation)

Escorts can detect and destroy emitters before the convoy triggers them. This creates the escort gameplay loop.

---

## Distant Sector Simulation

Each non-active sector is a `DistantSector` — pure data, no 3D:

```csharp
class DistantSector {
    string Name;
    int ShipCount;
    DistantShip[] ships;    // route index, elapsed, docked flag, dock timer
    DistantRoute[] routes;  // travel time only
    void Update(float dt);  // tick all ships
}
```

Ships depart, travel (timer), arrive, dock (timer), pick new route, repeat. On arrival, cargo delivery triggers economy effects (04).

Stats exposed: `ShipsInTransit`, `ShipsDocked`, `TotalArrivals` — for HUD/debug display and economy calculations.

---

## Hired Escorts

The player can hire NPC escorts at station bars or via comms with friendly patrol ships.

### Hiring

- Available at station bars — roster of 1-3 escorts depending on station size and faction military strength
- Each has: ship archetype, equipment tier, asking price, personality blurb
- Player pays upfront for a per-sector contract
- Multi-sector contracts cost more and escort follows through wormhole jumps

### Escort types

| Type | Cost | Ship | Strength | Notes |
|------|------|------|----------|-------|
| Wingman | Low | Light fighter | Weak — 1 pirate at a time | Cheap, expendable |
| Mercenary | Medium | Heavy fighter | Moderate — 2-3 pirates | Reliable, flees at low hull |
| Gunship | High | Gunboat | Strong — area denial, heavy weapons | Expensive, slow, devastating |

### Behavior

- **Following** — match player's speed tier (cruise/supercruise), maintain formation (~200-500u offset)
- **Combat** — engage any hostile attacking the player or entering aggression range. (06) AI with "escort" profile — fight until player leaves combat range or escort hits 20% hull
- **Docking** — player docks → escort holds outside station SOI. Persists across docks within the same sector
- **Dismissal** — via comms menu at any time. No refund
- **Death** — contract ends. Comms: "[Name] has been destroyed." No refund, no insurance
- **Sector jump** — single-sector escorts dismissed on jump. Multi-sector follow through wormhole (Tier 3 during transit, re-promoted on arrival)

### Limits

- Max 2 active escorts
- Escorts NOT saved (14) — on load, escorts are gone. Player re-hires after loading. Consistent with dock-only save
- Escort availability refreshes each time the player docks (new roster)

### NPC pool integration

Hired escorts use the Tier 1 spawn pool but are marked player-allied. Don't count toward trade route NPC budget — reserved pool slots (2 max).

---

## What This Plan Produces

- Goal state machine (10 goals, interrupt priorities, save/restore)
- Goal selection logic per NPC type
- Tiered simulation (Tier 1/2/3 with goal execution per tier)
- Tier promotion/demotion (spawn pool lifecycle)
- NpcManager component
- DistantSector class
- Steering behaviors (seek/arrive/flee/pursue/evade/offset follow/obstacle avoid)
- Speed tier management for NPCs (cruise/supercruise rules)
- Turn rate per ship class
- StationData / TradeRoute structs
- Trade route graph (hardcoded → data-driven)
- Smoothstep movement
- NPC type classification (trader, patrol, miner, pirate, convoy, escort)
- Pirate tactical AI (ambush, target selection, engagement flow, multi-pirate emergent)
- Patrol behavior (waypoints, contact response, cargo scanning)
- Convoy/formation AI
- Flee behavior
- NPC encounter flows (patrol, trader, distress, pirate ambush, miner, NPC-NPC combat)
- Graviton net emitter
- Hired escort system

---

## Prototype Status

10,000 Tier 2 ships + 1,000,000 Tier 3 ships at 60fps. Spawn pool of 200. Smoothstep movement. Radar dots on system map. Station-to-station routing with 14 stations and 13 routes.

**Not yet prototyped:** NPC cargo/faction, combat AI, star avoidance, dynamic route generation, goal interrupt system, steering behaviors, pirate tactical AI, convoy formation, hired escorts.

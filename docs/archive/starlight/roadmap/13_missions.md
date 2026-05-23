# (13) — Missions

**Layer 6: PLAYER EXPERIENCE — directed gameplay within the sandbox.**

**Depends on:** (05) (bar — missions are offered at stations), (06) (combat — most missions involve fighting), (07) (economy/factions — missions are faction-specific, affect reputation), (08) (sector transitions — missions can span systems).
**Blocks:** Nothing directly. Missions are the player-facing wrapper around all other systems.

## Overview

Missions give the sandbox direction without forcing it. Three categories: random procedural (infinite, always available), faction quests (authored chains with story), and the main campaign (hand-crafted narrative). The player can ignore all missions and just trade — or chase them for credits, reputation, and story.

## Mission Categories

### Random Missions (Procedural)

Generated on the fly at station bars and job boards. Always available, always different. The backbone of the early game.

| Type | Description | Gameplay |
|------|-------------|----------|
| Delivery | Carry cargo from A to B within a time limit | Trading with a deadline. Cargo provided (doesn't use player stock). |
| Patrol | Visit 2-3 waypoints in a sector, report contacts | Flying + scanning. May encounter hostiles. |
| Bounty Hunt | Destroy a specific NPC target | Combat. Target is in a known area. |
| Escort | Protect a convoy/NPC on a route | Fly alongside, defend from ambushes. |
| Assassination | Destroy a high-value target with escorts | Harder combat. Target flees if you're slow. |
| Base Assault | Attack a criminal station/installation | Multi-wave combat at a fixed location. |

### Faction Quests (Authored Chains)

Multi-mission arcs with story beats. Offered by specific NPCs at faction stations. Build reputation and unlock faction-exclusive rewards.

Example chain: "The Mining Crisis"
1. Investigate missing miners near an asteroid field
2. Discover pirate base in a debris field
3. Escort a strike force to assault the base
4. Defend the reopened mining route from retaliation

Each step is a mission with objectives, dialogue, and consequences. Completing the chain significantly boosts faction reputation + credits + possibly unique equipment.

### Story Campaign (Hand-Crafted)

A main narrative arc that spans the wormhole network. Not required — the player can engage whenever they're ready. The campaign introduces lore, reveals the universe's history, and drives the player through multiple sectors.

Structure: breadcrumbs. Each campaign mission ends with a lead to the next location/NPC. The player follows at their own pace, doing side content between story beats.

**Campaign is designed last.** The systems must exist first.

## Mission Structure

### Multi-Beat Design

Each mission has 2-4 beats (phases). Beats execute sequentially — each beat must complete before the next begins.

```
Beat 1: Travel to the area
Beat 2: Perform the objective (scan, destroy, deliver, defend)
Beat 3: Complication (optional, 30% chance — reinforcements, ambush, target flees)
Beat 4: Return to station for reward
```

### Beat Types

| Beat Type | Completion Condition | HUD Display |
|-----------|---------------------|-------------|
| goto | Player enters target zone (within radius) | Waypoint marker + distance. "Go to [zone name]" |
| destroy | All target NPCs destroyed | Target bracket (red) + count. "Destroy targets: 2/3" |
| defend | Survive N waves or protect target for N seconds | Wave counter or timer. "Defend [target]: Wave 2/3" |
| deliver | Dock at target station with mission cargo in hold | Station marker + distance. "Deliver to [station]" |
| scan | Get within scan range of target object | Target bracket (blue) + distance. "Scan [target]" |
| survive | Stay alive for N seconds in a hostile area | Timer countdown. "Survive: 0:45" |
| escort | Escort NPC reaches destination alive | NPC health bar + distance to destination. "Escort [NPC] to [station]" |
| return | Dock at the mission-giving station | Station marker + distance. "Return to [station]" |

### Mission State Machine

```
OFFERED → ACCEPTED → ACTIVE (beat 1) → ACTIVE (beat 2) → ... → COMPLETE
                         ↓                   ↓
                       FAILED              FAILED
```

| State | Trigger | Effect |
|-------|---------|--------|
| Offered | Generated at station (bar NPC or job board) | Visible to player in bar/board UI. Can be accepted or ignored. |
| Accepted | Player accepts via dialogue/board | Mission added to active slot. First beat activates. HUD waypoint appears. |
| Active | Player is working on a beat | Beat-specific HUD elements shown. Mission NPCs may spawn when player enters mission area. |
| Beat Advance | Current beat's completion condition met | Dialogue notification (comms): "Targets eliminated. Return to base." Next beat activates. |
| Complication | 30% chance when advancing between beats 2→3 | Complication event triggers (see below). Additional beat inserted. |
| Complete | Final beat (return) completed — player docks at reward station | Credits + reputation + XP awarded. Comms: "[Faction] thanks you, pilot." Mission removed from active slot. |
| Failed | Failure condition met (see below) | Reputation hit. Mission removed. May re-appear at the same station later (random missions) or be lost (faction quests). |

### Mission NPCs

Missions spawn their own NPCs (targets, escorts, waves). These are owned by the mission — they appear when the player enters the mission area (~20,000 units from the objective) and despawn when the mission ends. They don't come from the general NPC pool.

**Spawn behavior:**
- Mission NPCs are pre-defined in the mission data (ship archetype, faction, count)
- They spawn from the NPC pool's reserved slots (separate from trade traffic pool)
- Maximum ~20 mission NPCs active at once (across all active missions)
- Mission NPCs have "mission" AI profiles — they follow mission-specific behavior (defend a zone, attack the player, flee when conditions are met) rather than standard trade/patrol AI

### Mission Log (L Key)

Shows active missions and their current state:

```
┌──────────────────────────────────────────────┐
│ ACTIVE MISSIONS                              │
│──────────────────────────────────────────────│
│ [Random] Bounty: Red Claw Captain            │
│   Status: Destroy targets (1/3)              │
│   Location: Mars Patrol Route                │
│   Reward: 5,000 credits                      │
│──────────────────────────────────────────────│
│ [Faction] The Mining Crisis — Part 2         │
│   Status: Escort strike force                │
│   Location: Ceres Belt                       │
│   Reward: 8,000 credits + Faction Rep        │
│──────────────────────────────────────────────│
│ [Campaign] — (none active)                   │
└──────────────────────────────────────────────┘
```

- Clicking a mission sets its current objective as the navigation target (waypoint on HUD)
- Mission descriptions are 1-2 sentences — enough to remind the player what to do
- Completed missions briefly show "COMPLETE" before being removed

### Mission Data

```json
{
  "type": "bounty_hunt",
  "title": "Wanted: Red Claw Captain",
  "description": "A pirate captain has been raiding convoys near Mars. Destroy their ship.",
  "faction": "sol_navy",
  "level_requirement": 0,
  "reward_credits": 5000,
  "reward_reputation": 0.1,
  "reward_xp": 250,
  "time_limit": null,
  "beats": [
    { "type": "goto", "target_zone": "mars_patrol_route", "radius": 15000 },
    { "type": "destroy", "targets": [
        { "archetype": "pirate_fighter", "count": 2 },
        { "archetype": "pirate_captain", "count": 1, "name": "Red Claw Captain" }
      ]
    },
    { "type": "return", "station": "earth_military" }
  ],
  "complication": {
    "chance": 0.3,
    "type": "reinforcements",
    "spawns": [{ "archetype": "pirate_fighter", "count": 3 }]
  },
  "failure_conditions": [
    { "type": "player_destroyed" },
    { "type": "leave_area", "warning_time": 30, "radius": 30000 }
  ],
  "dialogue": {
    "offer": "A pirate captain's been raiding convoys on the Mars route. We need someone to take them out. 5,000 credits.",
    "accept": "Good hunting, pilot.",
    "complete": "Confirmed kill. Transferring payment. Sol Navy thanks you.",
    "fail": "Mission failed. We'll find someone else."
  }
}
```

### Active Mission Limits

- 1 active random mission
- 1 active faction quest
- 1 active campaign mission

Three simultaneous missions max. Simple, no mission log overload. If the player wants a new random mission, they must complete or abandon the current one. Abandoning costs a small reputation hit.

## Failure & Consequences

### Failure Conditions

| Condition | Effect |
|-----------|--------|
| Player destroyed | Mission failed. Respawn at last docked station (14). |
| Time limit expired (delivery) | Mission failed. Cargo returned to origin. |
| Escort target destroyed | Mission failed. Reputation hit with faction. |
| Player leaves the mission area | Warning → timer → mission failed if not returned. |

### Consequences of Failure

- No reward (obviously)
- Small reputation hit with the offering faction
- The mission may reappear later (random missions) or be lost (faction quests may have limited retry)
- No cascading punishment — failing a mission shouldn't spiral into a death loop

### Consequences of Success

- Credits
- Reputation with the offering faction (and possibly negative rep with opposing factions)
- Possible equipment reward (faction quests)
- Campaign progression (story missions)
- Discovery (missions may lead to new wormholes, stations, or system data)

## Complications (30% Chance)

Random events that make missions harder mid-flow:

| Complication | Effect |
|-------------|--------|
| Reinforcements | Extra enemies arrive during combat |
| Ambush | Enemies waiting at the objective |
| Target Flees | Assassination target tries to cruise away — pursue or lose them |
| Cargo Scan | Patrol scans the player during a delivery — contraband check |
| System Failure | Brief engine/weapon malfunction (timer-based) |
| Distress Call | Unrelated NPC needs help nearby — moral choice, no gameplay penalty for ignoring |

Complications are announced: "Warning: additional contacts detected" or "Target is attempting to flee." The player has time to react.

## Integration With Other Systems

### Economy (07)

- Delivery missions move synthetic cargo (doesn't affect the real economy — prevents players from crashing markets via missions)
- Mission rewards inject credits (money supply)
- Escort/patrol missions protect real NPC trade routes (indirect economic benefit)

### Reputation (06)

- Completing faction missions increases standing
- Some missions are only available at certain reputation levels
- Criminal missions (smuggling, assassination) decrease standing with lawful factions

### NPC Simulation (03)

- Mission NPCs are separate from the traffic simulation
- But missions can reference real stations, real routes, real trade goods for narrative context

### Wormhole Network (08)

- Multi-sector missions route the player through specific wormhole connections
- "Deliver this cargo to Barnard's Star" requires the player to plan a route and jump

## What This Plan Produces

- Mission data format (JSON with beats, dialogue, complications, failure conditions)
- Mission state machine (offered → accepted → active beats → complete/failed)
- Beat types (goto, destroy, defend, deliver, scan, survive, escort, return)
- Mission HUD integration (waypoints, objective counters, timers per beat type)
- Mission log (L key — active missions, objectives, rewards)
- Random mission generator (per-station, per-faction, scaled to player level)
- Faction quest framework (authored chains with multi-mission progression + dialogue)
- Mission NPC spawning (temporary, mission-owned, reserved pool slots)
- Complication system (30% chance, mid-mission events)
- Reward system (credits, reputation, XP, discovery)
- Mission abandonment (reputation cost)
- Bar + dialogue UI integration (05, (10))

## Not Yet Proven

No missions in the prototype. Entirely designed, not built. Build order:

1. Mission data format
2. Mission state machine (accept → track → complete/fail)
3. Random delivery missions (simplest — go to A, return to B)
4. Random combat missions (bounty hunt)
5. Complication system
6. Faction quest framework
7. Campaign structure (last — needs everything else working)

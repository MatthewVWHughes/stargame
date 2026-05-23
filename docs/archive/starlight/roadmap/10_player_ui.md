# (10) — Player UI

**Layer 6: PLAYER EXPERIENCE — how the player sees and interacts with everything.**

**Depends on:** (02) (flight state for HUD), (04) (docking flow), (05) (station rooms for base menus), (06) (combat for target brackets, reputation display), (07) (economy for trade UI), (08) (sector map for navigation).
**Blocks:** Nothing — UI is a pure consumer of every other system.

## Overview

The UI has two modes: flight (in-space HUD) and docked (station menus). The flight HUD is minimal — speed, mode, target, threats. The station menus are where the player trades, equips, takes missions, and reads news. Everything is functional first, pretty later.

## Flight HUD

### Core Readouts (always visible)

| Element | Position | Content |
|---------|----------|---------|
| Speed | Bottom center-left | Current speed in u/s, max speed indicator |
| Flight Mode | Bottom center | NORMAL / CRUISE / SUPERCRUISE + spool % |
| Autopilot Status | Below flight mode | Station Keeping / Goto [target] / OFF |
| SOI | Below autopilot | Current dominant body name |
| Gravity Warning | Below SOI (when active) | Body name + drift rate |
| FPS | Top left (debug) | Frames per second |
| Hull/Shield | Bottom right | Horizontal bars. Shield bar above hull bar. Numeric percentage on each. |
| Power Distribution | Left of hull/shield | Three vertical bars labeled S/W/E (matches key order 1/2/3). Height shows allocation %. Active adjustments flash briefly. |
| Credits | Top right | Current balance, level badge |
| Cargo Summary | Top right, below credits | "Cargo: 15/20" (used/capacity). Only visible when carrying cargo. |

### Target Info Panel

When a target is selected (06 — Targeting System), a panel appears below credits/cargo summary in the top-right area:

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

Detail shown depends on detection state:
- **Contact**: distance + heading only. No name, no bars.
- **Identified**: name, faction, ship type, distance.
- **Scanned**: full info — hull/shield bars, cargo contents, closing speed.

### Target Bracket

When a target is set (from targeting, system map, comms, or mission):

**On screen:** Yellow diamond bracket around the target. Name + distance below it.
**Off screen:** Yellow arrow at screen edge pointing toward target. Distance shown.
**Behind camera:** Arrow flips to correct direction. V key (rear view) helps.

Non-targeted detected ships show simpler brackets:
- **Grey**: unidentified contact
- **White**: neutral identified
- **Green**: friendly / allied faction
- **Red**: hostile
- **Blue**: mission objective

### Combat HUD Elements

These elements appear during combat situations (hostile in detection range or under fire):

| Element | Position | Content |
|---------|----------|---------|
| Weapon Status | Bottom left | Per-weapon-type indicators: ammo count (kinetic/missiles) or heat bar (energy). Shows "OVERHEAT" when cooling. |
| Lead Indicator | In 3D view | Small reticle showing where to aim to hit the current target. Turns red when crosshair is aligned. |
| Missile Lock Progress | Around target bracket | Shrinking diamond animation. Solid when locked. "MISSILE LOCK" text. |
| Missile Warning | Screen edge | Red pulsing arrow showing direction of incoming missile. "MISSILE INCOMING" text. |
| Flare Count | Below weapon status | "FLARE: 8" — remaining decoy flares |
| Kill Notification | Center screen, brief | "[Ship Name] destroyed" — fades after 2 seconds |
| Scan Warning | Top center | "CARGO SCAN IN PROGRESS" + progress bar when being scanned |

### Weapon Status Detail

The weapon HUD shows what the player has equipped and its state:

```
Bottom left area:
┌──────────────────────┐
│ GUN: Autocannon x2   │  ████████░░  AMO: 240
│ GUN: Laser x1        │  ██████░░░░  HEAT: 60%
│ MSL: Missile Rack    │  LOCKED      AMO: 8
│ FLR: Decoy Flares    │              AMO: 6
└──────────────────────┘
```

- Each equipped weapon type shown on its own line
- Kinetic weapons: ammo count, turns red at 20%
- Energy weapons: heat bar, turns yellow at 80%, shows "OVERHEAT" during cooldown
- Missiles: lock status (SEEKING / LOCKED / NO TGT) + ammo count
- Flares: count only, shown when equipped

### Autopilot Goto Display

When Goto is active:
```
AUTOPILOT: Goto — Mars Colony
DISTANCE: 156.2k u
ETA: 42s
MODE: Supercruise (4,200 u/s)
```

### Crosshair

Thin white cross that follows the mouse cursor position. Shows where you're steering toward.

### Radar / Proximity

Not a traditional radar screen — instead, nearby ships appear as colored brackets in the 3D view:
- **White**: neutral
- **Green**: friendly / allied faction
- **Red**: hostile
- **Yellow**: target
- **Blue**: mission objective

Detection range tiers:
- ~50,000 u: grey dot (detected, unidentified)
- ~30,000 u: colored by faction (identified)
- ~10,000 u: ship type visible (fighter, freighter, capital)

### Warnings

| Warning | Trigger | Display |
|---------|---------|---------|
| MISSILE INCOMING | Missile locked on player | Red flashing + direction indicator |
| GRAVITON NET | Net detected in path | Yellow warning + location marker |
| CRUISE DISRUPTED | Forced out of cruise/supercruise | Red flash + "CRUISE DISRUPTED" |
| GRAVITY WELL | Supercruise lockout zone entered | "SC UNAVAILABLE — GRAVITY WELL" |
| HOSTILE DETECTED | Enemy ship in radar range | "HOSTILE — [faction name]" |
| LOW HULL | Hull below 25% | Red hull bar flash |

## Comms Menu

Press C to open comms. A radial or list menu appears at screen center. Options are context-sensitive — only relevant actions for the current situation are shown. Mouse steering pauses while the comms menu is open. The menu closes on selection or pressing C again / Esc.

### Near a Station (within SOI, ~500u)

| Option | Action |
|--------|--------|
| Request Docking | Triggers docking flow (04). Autopilot takes over. |
| Jump to [Sector Name] | Only at wormhole stations. Lists available destinations. Selecting one triggers jump sequence (08). |
| Hail Station | Station responds with brief info: faction, available services, and one news headline. |

### Near an NPC Ship (targeted, within comms range ~1,000u)

| Option | Action |
|--------|--------|
| Hail | Opens dialogue panel. NPC responds based on type: trader gives rumor, patrol gives warning, miner gives zone tip. |
| Demand Cargo | Piracy. NPC receives threat → may comply (jettisons cargo) or refuse (flees/fights). Reputation hit with NPC's faction. Not available on faction-allied ships. |
| Hire Escort | Only on patrol/mercenary NPCs in non-combat state. Offers hire dialogue. |

### No Specific Target (deep space)

| Option | Action |
|--------|--------|
| Distress Call | Broadcasts distress on open channel. Patrol ships within 50,000u respond and fly to player's position. Cooldown: 60 seconds. |
| Surrender | Only while under attack. Jettisons all cargo. Attackers may disengage if they wanted cargo. If they wanted you dead, they keep fighting. |

### Comms Log

Recent comms messages (hails, demands, distress calls, station responses) are stored in a scrollable log accessible from the comms menu. Last 20 messages. Helps the player recall rumor details or mission instructions they dismissed too quickly.

## Inventory Screen (I Key)

View of the player's ship, cargo, and equipment during flight. Pauses mouse steering while open. `I` toggles; `Esc` closes (second `Esc` opens pause menu).

### Layout — sectioned, left-nav + details pane

```
┌─────────────────────────────────────────────────────────┐
│  SHIP: Corsair Mk2       HULL: 85%    CARGO: 15/40     │
├──────────────┬──────────────────────────────────────────┤
│ Cargo Hold   │ ┌─ Cargo Hold ──────────────────────┐    │
│ Equipment    │ │ Titanium        x10    280 cr ea  │    │
│ Ship Info    │ │ Food            x3      80 cr ea  │    │
│              │ │ Narcotics       x2  ⚠  500 cr ea  │    │
│              │ └───────────────────────────────────┘    │
│              │                                           │
│              │ ┌─ Selected: Narcotics ─────────────┐    │
│              │ │ Quantity held: 2                   │    │
│              │ │ Mass/unit: 0.5t                    │    │
│              │ │ Illegal: Sol Navy, Miners' Guild   │    │
│              │ │                                    │    │
│              │ │ Jettison qty:  [− 1 +]  / 2        │    │
│              │ │                                    │    │
│              │ │ [ Jettison ]  [ Jettison All ]     │    │
│              │ └───────────────────────────────────┘    │
└──────────────┴──────────────────────────────────────────┘
```

### Flow

1. Press `I` in flight → inventory opens, mouse steering pauses
2. Select **Cargo Hold** in the left nav (default section on open)
3. Click an item in the list → details pane appears on the right
4. Use `[− 1 +]` stepper (or click to type) to set quantity to jettison (1 to quantity held)
5. Click **Jettison** → that quantity drops behind the ship as floating cargo (04). **Jettison All** drops the full stack in one click
6. Cargo list updates; details pane stays on the selected item until it's empty (then clears)

### Sections

- **Cargo Hold** — per-item list with quantity, unit value, contraband flags. Full details + jettison in the side pane when selected
- **Equipment** — read-only view of all equipment slots (guns, missile rails, shield, engine, scanner, utilities, ammo counts, weapon heat bars). "Change at Equipment Dealer (05)" hint text. No jettison — equipment is sold, not dumped
- **Ship Info** — archetype, max hull/shield, cargo capacity, flight stats. Also read-only

### Contraband warnings

Items illegal in one or more factions show a ⚠ icon in the list and a red **Illegal:** line in the details pane listing which factions will flag them on scan. Serves as the "dump this before the patrol arrives" cue.

### No drag-and-drop, no reorder

Inventory is a quick reference + safety valve (jettison contraband / free cargo space), not a management surface. All real changes (buying/selling, equipment swaps) happen at stations.

## System Map (M key)

Top-down 2D view of the current system. Already built in prototype:
- All planets, moons, stations as dots/squares
- Trade routes as faint lines
- NPC radar bubble showing nearby ships
- Zoom, pan, click to set target
- Target info with distance

## Sector Map (N key)

3D view of the wormhole network. Already built in prototype:
- ~49 star systems at real galactic positions
- Wormhole connections
- BFS route from current system to selected destination
- Jump count and intermediate systems shown

## Docked UI (Base Menus)

When docked at a station, the flight HUD is replaced with the station interior. Room definitions, available services, and which stations have what are specified in (05) (Station Services).

### V1 Implementation (Menu)

Simple menu tscn. Station name + type as header. Clickable list of available rooms (filtered by station type). Clicking a room opens its interaction panel. "Launch" button in Hangar to undock.

### Navigation When Docked

| Key / Action | Context | Effect |
|-------------|---------|--------|
| Click room name | Room list | Opens that room's interaction panel |
| Esc (or Back button) | Inside a room panel | Closes the panel, returns to room list |
| Esc | At the room list (no panel open) | Opens pause menu |
| Click "Launch" | Hangar room | Undock (04 undocking flow) |

This two-layer Esc behavior means one press always goes "back" and two presses always reaches the pause menu. All overlay screens during flight (comms, inventory, mission log, system map, sector map) use the same pattern: Esc closes the overlay first, second Esc opens pause.

### News Feed

News from the simulation (economy events, faction combat, player actions) displayed as a scrolling feed or ticker in the station UI — not a separate room. Visible from the main station menu so the player sees it passively.

News items are generated from real simulation data (15) and expire after ~30 minutes of game time.

## NPC Dialogue System

All NPC communication flows through a single dialogue panel — a text window with optional portrait and response buttons. No branching dialogue trees — exchanges are linear with choice points.

### Dialogue Panel

```
┌─────────────────────────────────────────────┐
│  [Portrait]  NPC Name — Faction             │
│                                             │
│  "I've got a job that needs doing. Some     │
│   miners went dark near the Ceres Belt.     │
│   500 credits if you check it out."         │
│                                             │
│  [Accept]  [Decline]  [Ask for Details]     │
└─────────────────────────────────────────────┘
```

- **Portrait**: displayed for bar contacts and station NPCs. No portrait for ship-to-ship comms (radio only — shows a faction icon instead).
- **Text**: 1-3 sentences per exchange. Short. Players don't read walls of text.
- **Responses**: 2-4 buttons. Context-dependent. Always includes a way to exit.

### Dialogue Contexts

| Context | Trigger | Portrait | Content |
|---------|---------|----------|---------|
| Bar contact | Click NPC in bar menu | Yes | Mission offers, rumors, info for sale |
| Station comms | Dock request, jump request | No (station icon) | Docking clearance, jump clearance, station warnings |
| Ship hail | Comms menu → Hail | No (faction icon) | Trade tips, warnings, faction chatter |
| Pirate demand | Pirate initiates | No (skull icon) | Cargo demands with a timed response. Timer bar counts down — ignoring = combat. |
| Mission NPC | Mission beat triggers | Depends | Scripted mission dialogue (faction quests), template dialogue (random missions) |
| Distress call | NPC under attack | No | "Help! Under attack at [location]" — player can respond or ignore |

### Rumor System

Bar contacts and hailed ships can share rumors — template sentences filled with real simulation data:

```
Templates:
- "I heard {commodity} prices are through the roof at {station}."
- "Watch yourself near {zone} — pirates have been hitting convoys hard."
- "There's a wreck site in {zone} that nobody's picked clean yet."
- "Word is {faction} is losing ground near {planet}. Could be opportunity."
```

Rumors are generated from the current economy/faction state. They're TRUE — following them leads to real profit or real danger. The player learns to value bar visits.

### Pirate Demand Flow

1. Pirate hails player: "Drop your cargo or we open fire."
2. Timer bar appears (10 seconds)
3. Player options:
   - **Comply**: jettison all cargo. Pirates scoop it and leave. No reputation hit.
   - **Refuse**: pirates attack immediately. Combat.
   - **Ignore** (timer expires): pirates attack. Same as refuse.
   - **Flee** (start cruise spool during timer): pirates fire CDMs to prevent escape, then attack.

### Technical Implementation

Dialogue data is a simple struct: speaker name, faction, portrait ID (or null), text string, list of response options (label + callback). No dialogue tree engine — each exchange is self-contained. Faction quests chain exchanges via the mission state machine (13).

## Route Planning UI

### System Map Enhancements (M key)

The existing system map gains a route planning overlay:

- **Click station → Set Target**: existing behavior. Shows distance + name.
- **Commodity overlay toggle**: color-codes stations by a selected commodity's price. Green = cheap (buy here), Red = expensive (sell here). The "buy green, sell red" visual.
- **Trade route visualization**: when the player has cargo, faint lines show the most profitable sell destination for each commodity in the hold. No spreadsheet needed — just look at the map.

### Sector Map Enhancements (N key)

- **Multi-hop route**: click multiple systems to build a jump chain. Shows: total jumps, intermediate stations, estimated travel time.
- **Commodity search**: select a commodity → highlights systems where it's cheap to buy and expensive to sell (across known/discovered sectors only).
- **Route saved as navigation target**: the first hop becomes the active Goto target. On arrival in a new sector, the next hop auto-selects.

### Limitations

- Only shows data for discovered sectors and stations. Unknown sectors show "?" — no trade info.
- Prices shown are from last visit or last NPC rumor — they may have changed. Stale data is marked with a timestamp.
- No guaranteed profit — the route planner shows estimates, not guarantees. Prices shift between planning and arrival.

## Settings & Pause Menu

### Pause (ESC Key)

Time stops — the accumulated time counter freezes, orbits halt, NPC simulation pauses, economy ticks stop. Everything holds.

```
┌─────────────────────────┐
│       STARLIGHT         │
│                         │
│     [Resume]            │
│     [Settings]          │
│     [Save Info]         │
│     [Quit to Menu]      │
│     [Quit to Desktop]   │
└─────────────────────────┘
```

**Save Info** shows: last autosave station, timestamp, play time. Reminder that death reverts to this point. No manual save — just information.

### Settings Menus

All settings persist in `user://settings.cfg` (Godot's `ConfigFile` — INI-like, fast, built-in API). Separate from gameplay save files; never invalidated by save-format changes. Not schema-versioned — missing keys silently default, unknown keys ignored. On startup, a `SettingsManager` loads the file and applies values to: `AudioServer` bus volumes, `DisplayServer` window/vsync/resolution, `InputMap` overrides (see Input System section above), FlightHud (HUD scale + opacity), colorblind palette. Setting changes write immediately; no "Apply" button needed.

#### Display

| Setting | Options | Default |
|---------|---------|---------|
| Resolution | System-detected list | Native |
| Window Mode | Fullscreen / Borderless / Windowed | Borderless |
| VSync | On / Off | On |
| Quality Preset | Low / Medium / High / Ultra | Medium |
| Draw Distance | Slider (affects Far plane) | Medium |
| HUD Scale | 0.75x — 1.5x | 1.0x |

#### Audio

Maps directly to the audio bus layout (12):

| Setting | Control | Default |
|---------|---------|---------|
| Master Volume | Slider 0-100 | 80 |
| Music | Slider 0-100 | 70 |
| SFX | Slider 0-100 | 100 |
| Voice (Ship Computer) | Slider 0-100 | 100 |
| Ambient | Slider 0-100 | 60 |

#### Controls

| Setting | Control | Default |
|---------|---------|---------|
| Mouse Sensitivity | Slider 0.1 — 3.0 | 1.0 |
| Mouse Dead Zone | Slider 0% — 15% | 5% |
| Invert Y Axis | Toggle | Off |
| Key Bindings | Rebind table | See keybindings.md |

Key rebinding: shows a two-column table (action → key). Click a key cell → "Press new key..." → captures input → updates binding. Conflict detection: if the new key is already bound, warn and offer to swap.

### Input System & Rebind Persistence

The Godot `InputMap` is loaded from code at startup, not configured in project settings. Two sources layered:

1. **Default bindings** — `data/input/default_bindings.json` (shipped with the game, version-controlled). Maps every action to its default key/mouse binding.
2. **Player overrides** — `user://settings.cfg` (per-user, see Settings Persistence below). Any action the player rebound.

Startup: load defaults → overlay overrides → register in `InputMap`. Rebind UI writes to `user://settings.cfg` and re-registers the affected action immediately.

**Action naming convention.** `snake_case`, prefixed by system:

| Prefix | Examples |
|--------|---------|
| `flight_` | `flight_thrust_forward`, `flight_strafe_left`, `flight_roll_right`, `flight_brake`, `flight_engine_kill`, `flight_cruise_toggle`, `flight_supercruise_toggle`, `flight_goto_toggle`, `flight_autopilot_toggle`, `flight_afterburner` |
| `combat_` | `combat_fire_primary`, `combat_fire_missile`, `combat_target_cursor`, `combat_target_nearest_hostile`, `combat_target_cycle_prev`, `combat_target_clear`, `combat_flare` |
| `power_` | `power_shields`, `power_weapons`, `power_engines`, `power_balance`, `power_max_shields`, `power_max_weapons`, `power_max_engines` |
| `ui_` | `ui_comms`, `ui_inventory`, `ui_system_map`, `ui_sector_map`, `ui_mission_log`, `ui_pause`, `ui_interact` |
| `camera_` | `camera_toggle_free`, `camera_rear_view` |
| `debug_` | `debug_ship_inspector`, `debug_dock_inspector`, etc. |

Canonical key mappings: [keybindings.md](../reference/keybindings.md). Default bindings file mirrors that table 1:1.

**No multi-context input maps.** Mouse steering and flight inputs auto-disable when a UI overlay is open (system map, comms menu, inventory, etc.) — the FlightHud gates which actions fire, not the InputMap. One flat action table, context-gated at read time.

#### Accessibility

| Setting | Options | Default |
|---------|---------|---------|
| Colorblind Mode | Off / Deuteranopia / Protanopia / Tritanopia | Off |
| Screen Shake | Off / Reduced / Full | Full |
| HUD Opacity | Slider 50% — 100% | 100% |

Colorblind mode remaps faction bracket colors and trade UI colors to colorblind-safe palettes.

## What This Plan Produces

- Flight HUD (speed, mode, autopilot, SOI, target info panel, warnings, radar brackets)
- Combat HUD (weapon status, ammo/heat bars, lead indicator, missile lock, missile warning, kill notifications)
- Crosshair (mouse-following)
- Target bracket (on-screen diamond, off-screen arrow, detection-state detail)
- Power distribution display (E/S/W vertical bars)
- Comms menu (context-sensitive radial/list, C key, comms log)
- Inventory screen (cargo view, jettison, equipment read-only, I key)
- System map (2D top-down, M key) — already built, enhanced with commodity overlay + trade route visualization
- Sector map (3D network, N key) — already built, enhanced with multi-hop route planning + commodity search
- Docked UI (V1 docked menu, room navigation, per-room interaction panels — (05))
- News feed (simulation-generated, passive display when docked)
- Warning system (missiles, graviton nets, hull, hostiles, cargo scan)
- NPC dialogue system (panel, portraits, responses, timed demands, rumor templates)
- Route planning UI (commodity overlay, multi-hop, profit hints)
- Pause menu (resume, settings, save info, quit)
- Settings (display, audio, controls with rebinding, accessibility)

## Partially Proven

Flight HUD (speed, mode, FPS, crosshair, target marker), system map (zoom, pan, click, radar), and sector map (3D stars, pathfinding) are built. Docked UI, comms, radar brackets, warnings, NPC dialogue, route planning UI, settings menus, and pause menu are designed but not prototyped.

# Doors & Airlocks

Bulkheads become functional doors: open on proximity, close behind you, lock/unlock, interlock as airlock pairs. Doors are the primary control point for station flow and the foundation for faction/reputation gating.

Depends on [modular_kit.md](modular_kit.md) — doors are `Wall_Door` variants instanced at boundaries. Depends on phase-1 bulkhead positioning fix.

---

## State Machine

```
    Closed ──proximity──> Opening ──animation done──> Open
      ↑                      │                          │
      │                 body in doorway                all bodies out + 2s timer
      │                      ↓                          │
  animation                Blocked ──clear──> Opening   ↓
      │                                                 │
      └─────────────── Closing <──────────────────────┘
            ↑body                  │
            in doorway             ↓
                              (reverses to Opening via Blocked)

  Locked ──unlock action──> Opening
  Any  ──power failure──> Malfunction
  Any  ──hp ≤ 0──> Destroyed (future; not in scope)
```

### States

| State | Collision | Proximity responsive? | Animation |
|-------|-----------|----------------------|-----------|
| `Closed` | on | yes | panels together |
| `Opening` | disabled at 30% of animation | no | panels sliding apart |
| `Open` | off | no | panels at max travel |
| `Closing` | off until animation end | yes (reverses to Opening) | panels sliding together |
| `Blocked` | off | yes | stops, reverses to Opening |
| `Locked` | on | no (shows "Locked" prompt on F) | sealed, red indicator |
| `Malfunction` | state-dependent | no | frozen in current position |

### Critical transitions

- **Closing → Blocked → Opening** — if any body is detected in the proximity zone during Closing, immediately reverse. Doors never crush players.
- **Open → Closing** — only when *all* bodies have exited the proximity zone AND a 2s timer has elapsed. Rapid re-entry resets the timer; never stack timers.
- **Locked → Opening** — only via explicit unlock (keycard, hack, quest trigger). Proximity does nothing on a locked door.
- **Collision disabled partway through Opening** (~30% of animation) so the player can start walking through before the door is fully open. This feels responsive.

---

## Components

```
StationDoor (Node3D, script)
├── PanelLeft  (MeshInstance3D + StaticBody3D)
├── PanelRight (MeshInstance3D + StaticBody3D)
├── Frame      (MeshInstance3D) — static, never moves
├── ProximityZone (Area3D) — 2m radius in front of door, collision mask = player + NPCs
├── StatusLight  (MeshInstance3D with emissive material — green/red/yellow)
└── AudioPlayer (AudioStreamPlayer3D)
```

### Animation: Tween, not AnimationPlayer

Use Tween for panel slide — simpler, ~30% cheaper than AnimationPlayer in dynamic scenes, easier to parameterize per door type. Single `TweenProperty` on panel position with `EASE_IN_OUT` over 0.4s.

Always `Kill()` the previous tween before starting a new one. Bind to the door node for automatic cleanup.

Migrate to AnimationPlayer only when doors need multi-track synchronization (panels + lights + particles + sound on one timeline) — not needed for baseline doors.

---

## Proximity Triggers

- `Area3D` with collision layer/mask matching player (layer 4) and NPCs (layer 2).
- `body_entered` → if state is `Closed` or `Locked` (with unlock condition met), transition to `Opening`.
- `body_entered` during `Closing` → transition to `Blocked` → `Opening`.
- `body_exited` → start 2s close timer. Another `body_entered` cancels the timer.

---

## Locked Doors

```json
"walls": {
  "north": "door_locked",
  "east_0": { "type": "door", "security": 2 }
}
```

Security tiers gate unlock methods:

| Tier | Unlock methods |
|------|----------------|
| 1 | Basic keycard (cargo bay, general access) |
| 2 | Security keycard + low-tier hack |
| 3 | Admin keycard + high-tier hack OR mission trigger |

Status light: green = unlocked, red = locked, yellow = cycling (airlock).

Interacting (F) with a locked door shows "[F] Locked" prompt. If the player has a valid keycard, the prompt becomes "[F] Unlock with {keycard_name}" and pressing F transitions `Locked → Opening`.

---

## Airlocks

Two doors on the same airlock piece: `DoorInner` (faces station interior) and `DoorOuter` (faces exterior). The pair is controlled by a single `AirlockController` node.

**Interlock rule:** both doors must never be `Open` simultaneously. The controller enforces this.

### Cycle sequence

1. Player interacts with airlock control panel
2. Controller reads current pressure state (pressurized/vacuum)
3. Close whichever door is open (if any)
4. Wait 1s — pressure equalize phase:
   - Status lights on both doors switch to yellow
   - Warning klaxon audio loop
   - Particle effect: fog wisps (pressurizing) or directional suction (venting)
5. Open the opposite door
6. Status lights return to green/red as appropriate

### Emergency override (stretch)

Manual button opens both doors simultaneously — catastrophic if station is pressurized, intentional gameplay mechanic (explosive decompression, boarding action).

---

## NPC Integration — NavigationLink3D

The nav mesh is baked with doors in the *open* position so walkable area exists through every doorway. Closed doors don't cut the mesh — instead, a `NavigationLink3D` at each doorway connects the regions on both sides.

When an NPC's path crosses a link:
1. `NavigationAgent3D.link_reached` signal fires when the NPC reaches the link entry
2. On the signal (via `CallDeferred` — direct calls cause infinite recursion with `get_next_path_position()`), the NPC triggers the door to open
3. NPC waits for `Opening` animation to pass the collision-disable threshold (~30%)
4. NPC proceeds through

### NPC queuing

Each doorway maintains a reservation dictionary: `{door_id → {npc_id, direction}}`.

- When an NPC's path will cross a door, check reservation.
- If reserved by another NPC in the opposite direction: wait at an approach marker (~1m back from the doorway) until clear.
- Reservation released on exit or 3s timeout (failsafe).

If an NPC is stuck in a doorway for >2s with no movement progress: teleport to the nearest waypoint on the other side. Player probably isn't watching.

---

## Audio

Single `AudioStreamPlayer3D` per door. Each sound variant is a single file layering multiple sources — cheaper than stacking multiple players.

| Event | Sound layers |
|-------|-------------|
| Open | Pneumatic hiss + mechanical slide |
| Close | Mechanical slide + magnetic seal thunk |
| Locked (denied) | Electronic chirp, 2-note "nope" |
| Unlock | Positive chirp + mechanism click |
| Airlock cycle | Warning klaxon loop + equalization hiss |

---

## Visual Feedback

**Emissive status strip** on the door frame. Single `ShaderMaterial` with an `emission_color` uniform set by the state machine. No particles needed:

| State | Color |
|-------|-------|
| Closed / Open / Opening / Closing | Green (0, 1, 0.3) |
| Locked | Red (1, 0.2, 0.1) |
| Cycling (airlock) | Yellow (1, 0.9, 0) |
| Malfunction | Flickering red or off |

**Airlock particles**: GPUParticles3D emitting only during the 1s cycle phase. Fog wisps (pressurizing, upward directional) or suction (venting, toward outer door). Performance cost is negligible because they're only active briefly.

---

## Builder Integration

`PlaceBulkheadAtBoundary` (from [modular_kit.md](modular_kit.md)) instances `StationDoor.tscn` instead of a static mesh when the wall variant is `door` or `door_locked`. Lock state, security tier, and airlock pairing are passed through the JSON wall override.

For airlock pieces, the piece itself owns the `AirlockController` and references both `DoorInner` + `DoorOuter` as children.

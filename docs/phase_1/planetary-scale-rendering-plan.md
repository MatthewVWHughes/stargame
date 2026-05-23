# Planetary Scale And Rendering Plan

This note captures the current direction for planetary scale, system scale, and
planet rendering so the next implementation discussion has a stable reference.
It is planning only. The canonical architecture remains in `../runtime-architecture.md`,
`../system-data-contracts.md`, `../flight-supercruise-docking.md`, and
`../planet-rendering-direction.md`.

## Direction

Use a compressed astronomical scale inspired by KSP, roughly "KSP but about
half size" as an initial feel target. The goal is not literal KSP values. The
goal is a system that feels large, readable, and worth traveling through without
making routine trips tedious.

Tune for travel feel before locking exact numbers:

- planets and moons should feel large during close approach
- orbital distances should be compressed, but not so much that bodies feel
  clustered together
- outer-system travel should remain viable through supercruise scaling
- local Unreal space should stay small enough for precision and performance
- surface detail should support readable orbital and upper-atmosphere flybys,
  not full landing gameplay

The game should use an authored game astronomical scale, not true solar-system
scale and not raw Unreal physics as the solar-system source of truth.

## Scale Separation

Do not collapse these into one number:

- simulation scale: logical body, orbit, route, and travel positions
- render scale: how bodies, atmospheres, rings, and proxies are presented
- travel scale: supercruise speed, gravity lockout, dropout, and approach feel
- map scale: readable system and sector UI
- local Unreal scale: ship, camera, combat, docking, nearby VFX, and local
  atmosphere

Every body needs distinct authored or derived values for:

- physical reference radius
- visual/render radius
- collision radius
- atmosphere entry radius
- cloud-top and deep-layer radii
- map icon/display scale

Associated profiles need their own values:

- gravity wells: slowdown radius, lockout radius, forced-dropout radius, and
  heat/radiation/destruction bands
- supercruise tuning: efficiency curve, speed ceiling curve, braking behavior,
  and target-approach behavior
- atmosphere profiles: haze, cloud top, dense layer, deep danger layer,
  local VFX, damage, pressure, heat, and radiation
- representation profiles: enter/exit distances, hysteresis, blend duration,
  owning components, and debug state

These values may intentionally differ, but each difference needs an owner and a
reason. The canonical split remains: body data describes the body, gravity-well
data describes travel constraints, atmosphere data describes atmosphere/hazard
bands, and map/render profiles describe player-facing presentation.

## Planet Rendering

For the first pass, large high-segment spheres are acceptable. They must have
enough vertices that close orbital views do not show obvious straight edges.
Those spheres should still sit inside a future-ready representation model rather
than becoming the only planet rendering approach.

Planned representation layers:

- far orbital sphere: clean silhouette and broad color
- near orbital sphere: higher-detail material, normals, height/detail texture,
  and readable surface variation
- atmosphere shell: haze, rim, clouds, and color shift separate from the body
- upper-atmosphere flyby layer: surface readability and atmospheric depth cues
  without terrain-following, collision, landing, or surface traversal gameplay
- local VFX layer: fog, cloud wisps, turbulence, storm, heat, radiation, and
  speed cues around the player
- deep/danger layer: pressure, heat, radiation, damage, lockout, or destruction

The desired behavior is like KSP, KSP2, KSA, and other space games that swap or
blend representations as the player moves from map distance to orbital distance
to upper-atmosphere approach. No single mesh, material, or volumetric cloud
setup should be expected to solve every distance.

The invariant is horizon continuity. The player should not see the body limb,
surface, cloud deck, or atmosphere shell pop when representation changes.

## Gameplay Scope

This remains a space game. There are no random planetary landings in this scope.

Allowed:

- low-orbit and upper-atmosphere flybys over rocky worlds
- readable rocky surface detail from ship/orbital distance
- upper-atmosphere or cloud-layer stations on gas giants
- gas mining and gas-giant station approach
- destructive deep-atmosphere, pressure, heat, or radiation rules

Not allowed in this scope:

- arbitrary landing anywhere on a planet
- first-person surface exploration
- full ground environment rendering for inhabited Earth-like worlds
- terrain-following flight, walkable terrain, or collision terrain for whole
  planets
- terrain gameplay that turns the planet pass into a landable-world project

For inhabited Earth-like or garden worlds, entering the upper atmosphere should
be illegal and dangerous. Planetary defense systems can warn, escalate, and shoot
the player down. For phase one, this is deliberately lightweight: show an
on-screen warning during illegal atmosphere approach, then apply 100% player
damage at the defined hard altitude/deep-atmosphere boundary. Customs or
airport-like orbital stations provide the legal interface to the planet. This is
both lore and a production boundary: it avoids requiring full inhabited-surface
rendering.

Future idea, not phase-one scope: authored surface-access stations, mining
outposts, or hidden pirate bases on rocky worlds. These should be reached
through explicit station/docking/service contracts if they are added later, not
through arbitrary free landing.

For hostile worlds and gas giants, deeper layers should become increasingly
dangerous and eventually destroy the ship.

Rings are visual-only for now. They may become hazards, mining regions, or
navigation content later, but not in the first planetary scale pass.

## Supercruise And Gravity

Preserve the Godot-derived principle: supercruise is governed by local gravity
wells.

Rules:

- stars, planets, and moons may have anchored gravity-well definitions
- supercruise cannot engage inside a deep lockout layer
- body gravity wells slow, lock out, or force dropout from supercruise
- the primary star has a major well that affects inner-system travel
- near the star, heat and radiation eventually destroy the ship even if the
  player continues sublight
- farther from the primary star, supercruise becomes more efficient and permits
  higher top speeds

This solves the outer-system problem without making the system feel tiny. Inner
system travel can be slower and denser; travel toward gas giants and outer
planets can ramp to much higher speeds as the ship climbs out of the star's
deep gravity influence.

The implementation should be expressed as a curve or profile:

```text
distance from primary star -> supercruise efficiency / max speed
```

The exact numbers should be derived from desired travel times, not guessed in
isolation.

Supercruise travel-time calculation should be a shared system-level service.
Player flight, AI travel, route estimates, mission timing, and debug/headless
tests should query the same curve/profile so they agree about how long a trip
takes. All ships use the same supercruise speed behavior because the cruise
drive bends spacetime at a consistent rate. Sublight flight is different:
acceleration and top speed can vary by ship mass, thrust, and tuning because it
is conventional game flight rather than cruise-drive spacetime travel.

Travel time should be rule-based, not manually authored per route. Bodies use
their Kepler orbit positions, so distances and travel times naturally change as
the system moves. The Sol Mercury-to-Neptune trip is the calibration case:
because it is a very large span, orbital min/max variation is small enough for a
stable top-end pacing target. Tune the supercruise efficiency curve so that this
trip takes under five minutes, then apply the same math across all systems.
Normal route times should fall out of current body positions, local gravity-well
states, and the shared supercruise curve.

## Frontier Scale Validation

Before applying this to all content, scale and tune the existing playable
frontier fixture. This is a validation surface, not a second content target. Use
`frontier_test_01` so the work proves the same player-facing travel, docking,
map, save/load, and service flows used by the frontier loop.

Recommended contents:

- one primary star
- one rocky planet
- one gas giant
- one moon
- one orbital customs/service station
- one gas-layer station
- optional visual-only ring
- supercruise paths between bodies
- gravity slowdown, lockout, and dropout
- low-orbit or upper-atmosphere rocky flyby
- upper gas-layer approach
- atmosphere/deep-layer destruction volumes
- debug readouts for distance, speed, gravity well, atmosphere layer,
  representation state, and local bubble state

This lab should prove scale, travel, and representation transitions before the
values are promoted into broader authored systems. Acceptance should be
player-facing: select a target, enter supercruise, see slowdown/lockout/dropout
reasons, approach a body or station, dock or be denied, encounter legal/hazard
feedback, and save/reload without losing logical location.

## Next Decisions

Set these through discussion before implementation:

- baseline planet radius equivalent, using the KSP-half-size target as the first
  reference
- baseline moon, inner-planet, gas-giant, and outer-planet orbital distances
- travel-time rules, not fixed per-route travel times. First tuning target:
  planet-to-moon should feel like about a one-minute trip overall, including
  escaping the planet gravity well, launching supercruise, spending roughly
  10-15 seconds in supercruise, hitting the moon gravity well, spooling down,
  and finishing the approach in sublight cruise. Sol-scale inner-to-outer-system
  travel should use Mercury-to-Neptune as the upper reference and take under
  five minutes overall; that is long enough to communicate that the trip is
  huge, but short enough to remain playable.
- Sol and a small number of special systems may preserve near-Sol-scale layout
  because players know their scale and contents. Most authored systems can be
  smaller or more compressed for game pacing and creative freedom.
- gravity-well bands for slowdown, lockout, forced dropout, heat/radiation, and
  destruction
- first visual proof target: rocky upper-atmosphere flyby or gas giant
  upper-atmosphere station approach. Fidelity can be rough; the first proof only
  needs to show that the representation/transition technology works and that the
  player can visually understand what they are looking at.
- debug surfaces required before tuning begins

## Open Questions

No major direction questions remain for the first pass. Open implementation
details should now be derived from the accepted rules:

- use Kepler body positions and the shared supercruise curve to calculate
  current travel time instead of authoring fixed travel times per route
- use Mercury-to-Neptune under five minutes as the Sol-scale top-end calibration
  case
- keep visual fidelity rough until the player can clearly understand the body,
  atmosphere, transition state, hazard boundary, and approach target

Recommended next discussion order:

1. Define the first frontier body/orbit scale values.
2. Calibrate the shared supercruise curve against planet-to-moon feel and the
   Mercury-to-Neptune under-five-minute top-end case.
3. Verify route-time output from current Kepler body positions.
4. Define body and atmosphere representation states.
5. Choose the first frontier-scale visual target.

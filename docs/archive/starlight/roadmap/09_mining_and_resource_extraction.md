# (09) — Mining & Resource Extraction

**Layer 4: CONFLICT — resources are the lifeblood, controlling them is power.**

**Depends on:** (01) (zones — asteroid fields, gas clouds define mining areas), (03) (NPC miners follow routes between mining zones and stations), (06) (combat — mining protection/raiding gameplay), (07) (economy consumes raw materials, mining is the only source), (08) (wormholes — brown dwarf mining sectors accessed via wormholes).
**Blocks:** Nothing directly — mining feeds the economy which feeds everything else.

## Overview

Raw materials enter the economy exclusively through mining. NPC mining ships cycle between resource zones and stations, extracting materials and delivering them. Disrupting mining (destroying miners, blockading routes) starves downstream production and triggers price cascades. The player can participate as a miner, protect miners, or raid them.

## Mining Cycle

### NPC Miners

Mining stations own dedicated mining ships (3-5 per station). Each miner follows a fixed cycle:

```
1. DOCKED     — at mining station, loading equipment
2. OUTBOUND   — flying to assigned resource zone
3. EXTRACTING — at the resource zone, filling cargo hold (timer-based)
4. RETURN     — flying back to mining station, full of raw material
5. DOCKED     — unloading cargo → station stock increases
   → cycle repeats
```

### Extraction Time

Time at the resource zone depends on:
- Zone richness (metadata in system data)
- Ship cargo capacity
- Equipment quality (better mining lasers = faster)

Typical: 60-180 seconds per extraction cycle. The miner is stationary and vulnerable during this time.

### Route

Miners don't use the general trade route graph. They have a dedicated route: mining station ↔ assigned resource zone. Simple A→B→A loop.

## Resource Zones

### Types

| Zone Type | Resources | Visual | Gameplay |
|-----------|-----------|--------|----------|
| Asteroid Field | Ore, Rare Metals, Ice | MultiMesh rock instances | Navigate through, mine specific rocks |
| Gas Cloud | Hydrogen, Helium-3, Deuterium | Volumetric fog, reduced visibility | Fuel scooping, sensor interference |
| Ice Field | Water Ice, Volatiles | Translucent crystalline bodies | Cold environment, fragile resources |
| Debris Field | Salvage, Scrap Metal, Components | Wreckage meshes, floating containers | Salvage operations, derelict encounters |

### Zone Data

```json
{
  "name": "Ceres Belt",
  "type": "asteroid_field",
  "center_body": "star",
  "orbit": 400000,
  "radius": 20000,
  "richness": 0.8,
  "resources": ["raw_ore", "rare_metals"],
  "danger_level": "low"
}
```

### Brown Dwarf Sectors (Special Case)

Brown dwarf systems are essentially one giant resource zone. They're the opposite of Sol — no clean emptiness, no neat orbital planes; you're flying through a dense, murky, resource-rich soup.

#### Visual Design

- **Volumetric gas clouds** — thick orange/red/brown haze everywhere. Reduced visibility. Sensor interference.
- **Dense asteroid debris** — not neat belts, chaotic clouds of tumbling rocks. Navigation hazard.
- **Gas streamers** — material bleeding from the brown dwarf atmosphere, especially in binary systems (Luhman 16) where the two bodies exchange material.
- **The brown dwarf itself** — a massive banded sphere glowing faint infrared. Jupiter on steroids. Fills the sky when close.
- **No "clear space"** — always in gas, dust, or debris. Claustrophobic compared to Sol's void.

#### Scale

Much smaller than a star system. The interesting zone (debris, gas clouds, mining areas) extends only as far as Earth's orbit from Sol (~300,000 m) or smaller. The brown dwarf's gravity well dominates a compact region. Typically 100,000-200,000 m across for the whole playable area — cross the entire sector in a couple of minutes. Dense, not wide.

#### Gameplay

- **Mining paradise** — asteroids for metals, gas scooping for Hydrogen / Deuterium / Helium-3. The sector IS the resource.
- **Fuel economy** — cheapest Fusion Fuel precursors in the network. Factions fight over scooping rights.
- **Limited visibility** — volumetric clouds limit sensor and visual range. Ambush territory. Pirates love it.
- **Navigation hazard** — dense asteroids force slower flight. Can't supercruise in a straight line — need to pick paths through debris.
- **No permanent colonies** — nothing solid to build on. Orbital refineries, fuel depots, temporary mining camps. Transient population.
- **Hidden operations** — dim, off official charts, sensor-blind. Perfect for illegal refining, smuggling waypoints, dead drops.

#### Key Systems

**Luhman 16** (6.5 ly from Sol) — Binary brown dwarf. Two massive bodies orbiting each other with gas bridges, competing gravity wells, turbulent debris between them. Strategically critical: closest fuel source to Sol. Whoever controls it controls the inner colony fuel supply.

**WISE 0855** (7.4 ly from Sol) — Coldest known brown dwarf (~250K). Water ice clouds. So faint it wasn't discovered until 2014. In-game: an uncharted system with an underground fuel operation running for years before anyone noticed.

**Epsilon Indi Ba/Bb** (11.9 ly from Sol, part of the Epsilon Indi system) — binary brown dwarf pair orbiting a K-type star at extreme distance. The brown dwarfs are in the outer system, separate from the inner habitable zone. A system with two faces: civilized inner system, wild brown dwarf outer zone.

#### Technical Notes

- Volumetric fog/clouds via Godot's `FogVolume` nodes with noise density textures
- Asteroids via MultiMesh for hundreds/thousands of rock instances
- Gas scooping: gameplay zone (Area3D) near the brown dwarf with resource collection mechanics
- Sensor interference: reduce radar/detection range when inside cloud volumes
- Smaller sector scale means higher detail density — more rocks per unit volume, thicker clouds

Brown dwarf sector access is gated by wormhole network reach (08) — Luhman 16 is the closest, and faction control of its wormhole station is a strategic lever.

## Economic Impact

### Mining as the Economy's Foundation

The economy (07) has production chains: base materials → manufactured goods → end consumers. Mining is the ONLY source of base metals, ice, and gases. Everything downstream depends on it.

```
Iron Mining Station extracts: Iron (4 units per 10s tick — see (17))
  → Shipyard consumes: Iron + Titanium → produces: Ship Components
    → Military Base consumes: Ship Components + Weapons + Fuel (end consumer)
      → Military Base consumes: Ship Components
```

### Disruption Cascades

If mining output drops:
1. **Immediate**: mining station stock of raw materials decreases
2. **Short-term**: refinery runs low on inputs, production slows
3. **Medium-term**: refined metal prices spike at downstream stations
4. **Long-term**: shipyard output drops, military readiness suffers
5. **Cascade**: faction fleet replacement slows, patrol coverage decreases, piracy increases

This is entirely emergent — no scripted events. The economy simulation naturally propagates shortages.

### Player Disruption Opportunities

- **Destroy miners** → immediate supply disruption (reputation hit with owning faction)
- **Blockade a mining route** → miners can't deliver, same effect
- **Destroy a mining station** → permanent disruption until rebuilt
- **Raid cargo** → steal raw materials from returning miners

### Player Protection Opportunities

- **Escort missions** → protect miners on their route
- **Patrol missions** → clear pirates from a resource zone
- **Station defense** → protect a mining station from raid

## Wreck Sites & Salvage

Debris fields (zone type from (01)) contain wreck sites — named locations with lootable containers and derelict ships.

### Structure

- Fixed positions within debris field zones (defined in system data JSON)
- Each wreck site has a loot table: commodity drops, equipment drops, data (nav coordinates, mission intel)
- Loot respawns on a timer (30-60 minutes of game time)
- Discoverable for XP — first visit to a wreck site grants discovery XP (saved in (14))

### Gameplay

- Fly into debris field → scanner detects wreck sites (if scanner tier is high enough)
- Navigate to the wreck → floating containers with glow markers
- Scoop cargo (same mechanic as floating cargo from destroyed ships)
- Some wrecks have data logs → reveal hidden wormholes, criminal base locations, mission hints

### Salvage as a Career

The player can specialize in salvage:
- Equip a tractor beam (pulls floating cargo from further away)
- Better scanners detect more wreck sites and show loot type before arrival
- Risky: debris fields are navigation hazards and pirate territory

## Player Mining

The player can mine directly. Mining requires equipping a mining laser (occupies a gun mount — direct combat trade-off) and flying to a resource zone.

### Mining Equipment

| Slot | Item | Effect |
|------|------|--------|
| Gun mount | Mining Laser Mk1 | 1 unit per 5 seconds extraction rate. Range: 200u. |
| Gun mount | Mining Laser Mk2 | 1 unit per 3 seconds. Range: 300u. |
| Gun mount | Mining Laser Mk3 | 1 unit per 1.5 seconds. Range: 400u. |
| Utility | Gas Scoop | Required for gas cloud mining. Passive — collects gas resources while flying through gas zones at below cruise speed. 1 unit per 4 seconds. |

Mining lasers replace a gun mount. The player trades direct firepower for the ability to mine. A fully combat-fitted ship cannot mine. A fully mining-fitted ship is nearly defenseless.

### Mining Flow (Asteroids)

1. **Fly to an asteroid field** zone (marked on system map)
2. **Scan for mineable rocks**: within the asteroid field, specific asteroids are targetable (T key). Targetable asteroids have a HUD bracket with a resource icon. Non-targetable asteroids are visual-only background rocks.
3. **Approach a target asteroid**: fly within mining laser range (200-400u depending on tier)
4. **Fire mining laser** (left mouse, same as weapon fire): beam connects ship to asteroid. The ship must remain stationary or slow-moving — mining beam breaks if the ship exceeds 2 u/s.
5. **Extraction**: cargo appears in hold over time. HUD shows extraction progress: `Mining: Iron +1 (15/20 hold)` (actual commodity depends on which asteroid — see (17) commodity table)
6. **Asteroid depletion**: each asteroid has a finite yield (5-20 units depending on zone richness). When depleted, the bracket disappears. The asteroid mesh remains but is no longer targetable. Respawns after 10-15 minutes.
7. **Hold full**: mining stops automatically. HUD: `CARGO HOLD FULL`. Player must return to station to sell.

### Mining Flow (Gas Clouds)

Gas cloud mining uses the Gas Scoop utility item instead of a mining laser:

1. **Fly into a gas cloud** zone
2. **Reduce speed** below cruise (gas scoop only works at normal flight speed)
3. **Scoop activates automatically**: cargo fills while the player flies through the cloud. No targeting required — the gas is ambient.
4. **Visual**: particle effects show gas being funneled into the ship's intake
5. **Hazard**: gas clouds reduce visibility and sensor range (06 zone modifiers). Pirates use this.

### Mining Risk/Reward

- **Risk**: mining equipment replaces weapon slots. A miner is vulnerable to pirate attack. Mining zones are often in contested or lawless space.
- **Reward**: raw materials sell well at nearby refineries. Short mining loops (mine → sell → mine) are steady income.
- **Escort gameplay**: miners can hire escorts (03) or respond to distress calls from NPC miners being attacked. Protecting mining operations is a viable career via missions.

Player mining is less efficient than NPC mining (NPCs have dedicated ships and equipment) but more flexible — the player can mine anywhere, not just assigned zones.

## NPC Miner Behavior

### Tier 1 (Near Player)

Full 3D ship visible at the resource zone:
- Flies to a specific asteroid/gas pocket
- Activates mining visual (laser beam, scoop particle effect)
- Stationary for extraction duration
- Vulnerable to attack — low combat capability
- On attack: attempts to flee toward nearest station

### Tier 2/3 (Far from Player)

Timer-based cycle:
```
outbound_time = distance_to_zone / average_speed
extraction_time = cargo_capacity / extraction_rate
return_time = distance_to_zone / average_speed
dock_time = 10-30 seconds (unloading)
```

On arrival at station: stock increases by cargo capacity worth of raw materials. Automatic, deterministic.

## What This Plan Produces

- Mining cycle (NPC miners: dock → outbound → extract → return → dock)
- Resource zone data (type, richness, resources, location)
- Extraction mechanics (timer-based for NPCs, interactive for player)
- Mining equipment (player weapon slot trade-off)
- Brown dwarf sector template (dense resource zones, volumetric gas, no permanent stations)
- Economic integration (mining output → production chain input)
- Disruption mechanics (destroy miners/stations → shortage cascade)

## Not Yet Proven

No mining is prototyped. The prototype has stations and NPC traffic but no resource zones, no extraction, no cargo. Build order:

1. Resource zone data in system JSON
2. NPC miner cycle (timer-based, Tier 2/3)
3. Mining station stock integration with economy (07)
4. Tier 1 miner visual (mining laser, stationary extraction)
5. Player mining equipment + interactive extraction
6. Brown dwarf sector template
7. Disruption cascades (test by destroying miners, observing price changes)

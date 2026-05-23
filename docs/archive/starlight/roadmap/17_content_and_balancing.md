# (17) — Content & Balancing

**Layer 8: PRODUCTION — filling the universe with actual game.**

**Depends on:** Everything. This is the last plan. All systems must be functional before content tuning makes sense.
**Blocks:** Nothing. This IS the finish line.

## Overview

Content is the data that makes the systems feel like a game. Ship archetypes, commodity tables, faction identities, station inventories, mission templates, and all the tuning values that determine whether the game is fun. Systems without content are tech demos. Content without systems is a spreadsheet.

## Ship Archetypes

### Player Ships (Progression)

| Tier | Name | Hull | Cargo | Guns | Missiles | Utility | Speed | Role |
|------|------|------|-------|------|----------|---------|-------|------|
| 1 | Starling | 500 | 20 | 2 | 1 | 1 | Fast | Starter — nimble, fragile, tiny hold |
| 2 | Hauler | 800 | 80 | 2 | 0 | 2 | Slow | Trading — big hold, weak weapons, extra utility |
| 2 | Viper | 600 | 15 | 4 | 1 | 1 | Fast | Combat — lots of guns, small hold |
| 3 | Corsair | 1000 | 40 | 4 | 1 | 2 | Medium | All-rounder — balanced |
| 3 | Barge | 1200 | 120 | 2 | 0 | 2 | Slow | Heavy trading — massive hold, no missiles |
| 4 | Falcon | 1500 | 50 | 6 | 2 | 2 | Fast | Late-game combat — tanky + armed |
| 4 | Titan | 2000 | 200 | 4 | 1 | 2 | Slow | Late-game trading — fortress |

All ships have exactly 1 shield slot, 1 engine slot, and 1 scanner slot (not shown — always 1 each).

Each tier represents a power/cost bracket. The player works up through tiers by earning credits. Higher tiers are available at higher player levels and reputation-gated at specific faction stations.

### Ship Pricing

| Tier | Price Range | Trade-in (60%) |
|------|-----------|----------------|
| 1 | 2,000 (starting ship) | 1,200 |
| 2 | 8,000 - 12,000 | 4,800 - 7,200 |
| 3 | 25,000 - 35,000 | 15,000 - 21,000 |
| 4 | 80,000 - 120,000 | 48,000 - 72,000 |

### NPC Ship Archetypes

| Type | Used By | Stats |
|------|---------|-------|
| Trader | NPC traders, convoys | Medium hull, big cargo, weak weapons |
| Patrol | Faction military | Good hull, good weapons, no cargo |
| Pirate Fighter | Criminal factions | Low hull, fast, moderate weapons |
| Pirate Raider | Criminal factions | Medium hull, some cargo (for stolen goods) |
| Mining Ship | Mining stations | Low hull, mining equipment, moderate cargo |
| Escort | Convoys, faction military | Good hull, good weapons, formation-capable |
| Gunboat | Faction capitals | High hull, heavy weapons, slow |

## Equipment

### Weapons (06)

Tiered within each type:

| Weapon | Tier 1 | Tier 2 | Tier 3 |
|--------|--------|--------|--------|
| Autocannon | 15 DPS, 400u range | 25 DPS, 500u range | 40 DPS, 600u range |
| Railgun | 30 DPS, 600u range | 50 DPS, 700u range | 80 DPS, 800u range |
| Laser | 12 DPS, 400u range | 20 DPS, 500u range | 35 DPS, 600u range |
| Plasma | 20 DPS, 600u range | 35 DPS, 750u range | 55 DPS, 800u range |
| Missile Rack | 100 burst, 1500u | 180 burst, 1800u | 300 burst, 2000u |
| CDM Launcher | — | 1 shot, 4000u | 2 shot, 5000u |

### Shields

| Tier | Capacity | Regen Rate | Offline Time |
|------|----------|-----------|-------------|
| 1 | 200 | 5/s | 5s |
| 2 | 400 | 10/s | 4s |
| 3 | 700 | 18/s | 3s |

### Engines

| Tier | Normal Max | Cruise Speed | PCD Efficiency | Spool Time |
|------|-----------|-------------|----------------|-----------|
| 1 | 10 u/s | 140 u/s | 0.8x | 4s |
| 2 | 12 u/s | 160 u/s | 1.0x | 3s |
| 3 | 15 u/s | 200 u/s | 1.2x | 2s |

PCD Efficiency multiplier affects the supercruise speed ceiling — a better engine lets the PCD compress space more effectively.

### Scanners

| Tier | Radar Range | Graviton Net Detection | Cargo Scan Range |
|------|------------|----------------------|-----------------|
| 1 | 30,000u | None | 200u |
| 2 | 50,000u | 5,000u warning | 500u |
| 3 | 80,000u | 10,000u warning + marker | 1,000u |

### Countermeasures (06)

| Type | Tier | Cost | Slot | Stats |
|------|------|------|------|-------|
| Decoy Flare Stack | 1 | 50/stack (10 uses) | Utility | 70% missile redirect, 40% vs CDM |
| ECM Jammer | 1 | 2,000 | Utility | 2x enemy lock time |
| ECM Jammer | 2 | 6,000 | Utility | 3x enemy lock time |
| Point Defense Turret | 1 | 4,000 | Weapon hardpoint | 80% intercept, 300u range |
| Point Defense Turret | 2 | 10,000 | Weapon hardpoint | 90% intercept, 400u range |

### Tractor Beam (04)

| Tier | Cost | Slot | Pickup Range | Pull Speed | Cone |
|------|------|------|-------------|-----------|------|
| 1 | 1,500 | Utility | 150u | 20 u/s | 90° |
| 2 | 5,000 | Utility | 300u | 40 u/s | 120° |
| 3 | 12,000 | Utility | 500u | 60 u/s | 150° |

### Mining Equipment (09)

| Item | Cost | Slot | Extraction Rate | Range | Notes |
|------|------|------|----------------|-------|-------|
| Mining Laser Mk1 | 1,000 | Gun mount | 1 unit / 5s | 200u | Basic. Replaces a weapon. |
| Mining Laser Mk2 | 4,000 | Gun mount | 1 unit / 3s | 300u | Faster extraction. |
| Mining Laser Mk3 | 10,000 | Gun mount | 1 unit / 1.5s | 400u | Professional grade. |
| Gas Scoop | 3,000 | Utility | 1 unit / 4s | Passive | Required for gas cloud mining. Auto-collects while flying through gas. |

## Commodity Table

~42 commodities across six categories. Metals are **single entries** (no ore/refined split — mining yields usable material directly). Processing chains are kept only where the output is a genuinely different substance from the input (ice → water, water → oxygen, hydrogen → fuel, organics → polymers).

### Base Materials (20) — mined, scooped, grown, or basic processed
| Id | Commodity | Base Price | Source |
|----|-----------|-----------|--------|
| `iron` | Iron | 80 | Asteroid — very common |
| `copper` | Copper | 120 | Asteroid — common |
| `silicon` | Silicon | 140 | Asteroid — common (semiconductor base) |
| `titanium` | Titanium | 280 | Asteroid — uncommon, high-grade structural |
| `lithium` | Lithium | 340 | Asteroid — uncommon (batteries) |
| `silver` | Silver | 420 | Asteroid — rare, precious |
| `platinum` | Platinum | 720 | Asteroid — rare, industrial catalyst |
| `gold` | Gold | 880 | Asteroid — rare, precious |
| `uranium` | Uranium | 1,100 | Asteroid — rare, restricted (fission fuel + warheads) |
| `rare_metals` | Rare Metals | 780 | Asteroid — rare, specialty electronics |
| `ice` | Ice | 30 | Ice fields |
| `water` | Water | 60 | Ice Processor (from Ice) |
| `oxygen` | Oxygen | 80 | Oxygen Plant (from Water or gas) |
| `hydrogen` | Hydrogen | 60 | Gas clouds — common |
| `deuterium` | Deuterium | 130 | Gas clouds — fusion precursor |
| `helium_3` | Helium-3 | 180 | Brown dwarfs / deep gas clouds — fusion-grade |
| `fuel` | Fuel | 150 | Fuel Refinery (from Hydrogen ×2) |
| `fusion_fuel` | Fusion Fuel | 420 | Fusion Fuel Plant (from Helium-3 or Deuterium) |
| `organics` | Organics | 40 | Agricultural stations |
| `polymers` | Polymers | 120 | Chemical Plant (from Organics) |

### Consumer Goods (3) — population demand
| Id | Commodity | Base Price | Source |
|----|-----------|-----------|--------|
| `food` | Food | 80 | Food Production (Water + Organics) |
| `medicine` | Medicine | 220 | Pharmaceutical (Organics + Water) |
| `luxury_items` | Luxury Items | 500 | Luxury Manufacturer (Gold + Electronics) |

### Industrial (9) — manufactured goods
| Id | Commodity | Base Price | Source |
|----|-----------|-----------|--------|
| `construction_materials` | Construction Materials | 180 | Construction Yard (Iron) |
| `electronics` | Electronics | 280 | Electronics Factory (Silver + Polymers + Copper) |
| `semiconductors` | Semiconductors | 650 | Semiconductor Fab (Silicon + Rare Metals) |
| `batteries` | Batteries | 520 | Battery Plant (Lithium + Copper) |
| `advanced_alloys` | Advanced Alloys | 680 | Alloy Plant (Titanium + Platinum) |
| `ship_components` | Ship Components | 350 | Shipyard (Iron + Titanium) |
| `robotics` | Robotics | 920 | Robotics Factory (Electronics + Advanced Alloys) |
| `medical_equipment` | Medical Equipment | 780 | Medical Factory (Electronics + Advanced Alloys) |
| `scientific_instruments` | Scientific Instruments | 1,100 | Instrument Lab (Semiconductors + Rare Metals) |

### Military (5)
| Id | Commodity | Base Price | Source |
|----|-----------|-----------|--------|
| `ammunition` | Ammunition | 100 | Munitions Factory (Iron) |
| `explosives` | Explosives | 260 | Explosives Plant (Polymers + Iron) |
| `armor_plating` | Armor Plating | 540 | Armor Works (Advanced Alloys) |
| `weapons` | Weapons | 400 | Arms Manufacturer (Titanium + Electronics) |
| `warheads` | Warheads | 1,600 | Warhead Plant (Uranium + Explosives) — restricted |

### Contraband (5)
| Id | Commodity | Base Price | Legal Status |
|----|-----------|-----------|-------------|
| `narcotics` | Narcotics | 500 | Illegal in all lawful factions |
| `stolen_goods` | Stolen Goods | varies | Illegal — from piracy / salvage |
| `unlicensed_weapons` | Unlicensed Weapons | 650 | Illegal in government space |
| `prohibited_tech` | Prohibited Tech | 1,400 | Black-market cybernetics, unlicensed AI |
| `alien_artifacts` | Alien Artifacts | 2,000+ | Restricted — research-grade, some factions confiscate |

### Reading the table

- **Metals are single-entry.** Mining an iron-bearing asteroid yields `iron` — no ore/refined split. Processing is implied inline.
- **Processing chains kept only where the substance genuinely changes.** Ice → Water (phase change), Water → Oxygen (electrolysis), Hydrogen → Fuel (refining), Organics → Polymers (chemistry). These are distinct commodities with distinct stations.
- **Precious metals form distinct chains.** Gold → Luxury Items. Silver → Electronics → Robotics. Platinum → Advanced Alloys → Armor/Robotics/Medical. Don't lump.
- **Specialty materials bottleneck endgame.** Rare Metals are required for semiconductors and scientific instruments. Uranium is the only feeder to Warheads. Titanium + Platinum gate Advanced Alloys, which gate Armor and Robotics.
- **Two fuel paths:** `fuel` (hydrogen-based, cheap, everywhere) and `fusion_fuel` (helium-3 or deuterium, premium — drives brown dwarf sector value).
- **Life support is always in demand.** Food + Water + Oxygen + Medicine are consumed by every populated station indefinitely. Blockading a populated station causes real suffering.

## Production Chains

Canonical production table. Every station has a `production_profile` list in its sector JSON; a station can carry multiple profiles (e.g., a trade hub is both a `populated_large` consumer and might run `fuel_refinery` as a side business). The EconomyManager (04 / (07)) ticks each active profile per its period, consuming inputs from station stock and producing outputs into it.

### Extraction (raw materials)

Single-step metal/resource mining — output is the finished metal, no ore/refined duplication.

| Production Profile | Tick Period | Consumes | Produces |
|---|---|---|---|
| `iron_mining` | 10s | — | Iron ×4 |
| `copper_mining` | 12s | — | Copper ×3 |
| `silicon_mining` | 12s | — | Silicon ×3 |
| `titanium_mining` | 15s | — | Titanium ×2 |
| `lithium_mining` | 18s | — | Lithium ×1 |
| `silver_mining` | 20s | — | Silver ×1 |
| `platinum_mining` | 25s | — | Platinum ×1 |
| `gold_mining` | 25s | — | Gold ×1 |
| `uranium_mining` | 30s | — | Uranium ×1 (restricted — lawful stations only) |
| `rare_metals_mining` | 25s | — | Rare Metals ×1 |
| `ice_harvest` | 10s | — | Ice ×4 |
| `hydrogen_scoop` | 10s | — | Hydrogen ×4 |
| `deuterium_scoop` | 15s | — | Deuterium ×2 |
| `helium3_scoop` | 15s | — | Helium-3 ×2 |
| `agricultural` | 20s | Water ×1 | Organics ×3 |

### Processing (substance-change steps)

| Production Profile | Tick Period | Consumes | Produces |
|---|---|---|---|
| `ice_processor` | 10s | Ice ×2 | Water ×1 |
| `oxygen_plant` | 15s | Water ×1 | Oxygen ×1 |
| `chemical_plant` | 15s | Organics ×3 | Polymers ×1 |
| `fuel_refinery` | 15s | Hydrogen ×2 | Fuel ×1 |
| `fusion_fuel_plant_d` | 20s | Deuterium ×2 | Fusion Fuel ×1 |
| `fusion_fuel_plant_he3` | 20s | Helium-3 ×1 | Fusion Fuel ×2 |

### Manufacturing (industrial + consumer + military)

| Production Profile | Tick Period | Consumes | Produces |
|---|---|---|---|
| `food_production` | 20s | Water ×1, Organics ×2 | Food ×2 |
| `pharmaceutical` | 30s | Organics ×2, Water ×1 | Medicine ×1 |
| `luxury_manufacturer` | 60s | Gold ×1, Electronics ×1 | Luxury Items ×1 |
| `construction_yard` | 30s | Iron ×2 | Construction Materials ×2 |
| `electronics_factory` | 60s | Silver ×1, Copper ×1, Polymers ×1 | Electronics ×1 |
| `semiconductor_fab` | 90s | Silicon ×2, Rare Metals ×1 | Semiconductors ×1 |
| `battery_plant` | 45s | Lithium ×1, Copper ×1 | Batteries ×1 |
| `alloy_plant` | 60s | Titanium ×1, Platinum ×1 | Advanced Alloys ×1 |
| `shipyard` | 60s | Iron ×3, Titanium ×1 | Ship Components ×1 |
| `robotics_factory` | 90s | Electronics ×1, Advanced Alloys ×1 | Robotics ×1 |
| `medical_factory` | 90s | Electronics ×1, Advanced Alloys ×1 | Medical Equipment ×1 |
| `instrument_lab` | 120s | Semiconductors ×1, Rare Metals ×1 | Scientific Instruments ×1 |
| `munitions_factory` | 30s | Iron ×1 | Ammunition ×5 |
| `explosives_plant` | 30s | Polymers ×1, Iron ×1 | Explosives ×1 |
| `armor_works` | 60s | Advanced Alloys ×1 | Armor Plating ×1 |
| `arms_manufacturer` | 60s | Titanium ×1, Electronics ×1 | Weapons ×1 |
| `warhead_plant` | 120s | Uranium ×1, Explosives ×2 | Warheads ×1 (restricted — military only) |

### Consumption (end consumers / population)

Stations running these profiles drain inputs without producing output. They represent end-user demand that pulls cargo through the whole chain.

| Production Profile | Tick Period | Consumes | Produces |
|---|---|---|---|
| `military_base` | 30s | Weapons ×1, Fuel ×1, Ammunition ×2, Armor Plating ×1 | — |
| `populated_small` | 60s | Food ×2, Water ×2, Oxygen ×1, Medicine ×1 | — |
| `populated_large` | 60s | Food ×3, Water ×3, Oxygen ×2, Medicine ×1, Luxury Items ×1 | — |
| `research_outpost` | 60s | Scientific Instruments ×1, Medical Equipment ×1, Oxygen ×1 | — |
| `criminal_hub` | 120s | Narcotics ×1, Unlicensed Weapons ×1, Prohibited Tech ×1 | — |

### Stock capacity

A separate `capacity_profile` (not tabled here) defines per-station per-commodity caps. An `iron_mining` station has high Iron capacity but minimal Gold capacity — stock caps constrain what a station can hoard. Tune in playtesting.

### Resilience rules

From (07), applied on top of the production math:
- **5% floor** when inputs are partial — some output still trickles (emergency reserves). Zero inputs = zero output (real starvation).
- **Baseline drift** — stock drifts toward 50% capacity at ~1% of max per tick (prevents runaway surplus / permanent shortage).
- **Price clamp** — prices cannot exceed [0.25×, 3×] of base.

### Multi-profile stations

A station can carry multiple profiles. A trade hub typically has `populated_large` (consumption) plus one or two production profiles (e.g. `fuel_refinery`). A criminal base has `criminal_hub` plus `explosives_plant` or similar. The EconomyManager ticks each profile independently — partial input satisfaction on one profile doesn't block the other.

## Faction Definitions

### Sol Navy (Government)

- Controls: Sol inner system, major stations
- Personality: bureaucratic, well-resourced, slow to act
- Stations: Earth Hub, Earth Military, Mars Colony
- Enemies: Red Claw Pirates
- Trade: all legal commodities, no contraband

### Miners' Guild (Corporation)

- Controls: asteroid belts, mining stations
- Personality: practical, profit-driven, independent
- Stations: Sol Mining, Saturn Ring Mining
- Enemies: anyone who raids their operations
- Trade: base materials (metals, ice/water, fuel precursors)

### Red Claw Pirates (Criminal)

- Controls: hidden bases in debris fields, asteroid hideouts
- Personality: aggressive, opportunistic, territorial
- Stations: hidden (must discover)
- Enemies: Sol Navy, Miners' Guild
- Trade: contraband, stolen goods, black market equipment

### Frontier Alliance (Independent)

- Controls: outer system stations, frontier outposts
- Personality: self-reliant, neutral, trade-friendly
- Stations: Titan Outpost, Neptune Frontier, Uranus Research
- Enemies: none (neutral with everyone, but will defend themselves)
- Trade: everything including contraband (look the other way)

*Additional factions per sector — each star system may have unique local factions.*

## Balancing Philosophy

### The Player Should Always Be Able To

- Make money (there's always a profitable trade route somewhere)
- Find a mission (job boards always have something)
- Progress (XP from trading, combat, exploration — all activities)
- Recover from mistakes (death loses progress, not the save)

### The Player Should Never

- Need a spreadsheet to trade profitably (buy green, sell red)
- Be softlocked by reputation (there's always a neutral faction)
- Face impossible combat (enemies scale with zone, not player level)
- Run out of things to do (procedural missions are infinite)

### Difficulty Curve

Not level-gated zones. Instead: proximity to core vs frontier.

| Zone | Danger | Reward |
|------|--------|--------|
| Inner Sol | Low — heavy patrols, safe routes | Low — prices stable, modest margins |
| Outer Sol | Medium — fewer patrols, pirate presence | Medium — better margins, some contraband |
| Frontier systems | High — sparse patrols, active piracy | High — big margins, rare commodities |
| Criminal systems | Very high — everyone's hostile if you're lawful | Very high — contraband, black market, unique equipment |
| Brown dwarf sectors | Environmental — gas, asteroids, low visibility | Resource bonanza — fuel, rare materials |

The player naturally gravitates toward higher-risk space as they get better ships and equipment. No artificial gates.

## What This Plan Produces

- Ship archetype data (JSON per hull type)
- Equipment data (weapons, shields, engines, scanners — JSON per tier)
- Commodity table (JSON with base prices, categories, contraband flags)
- Faction definitions (JSON per faction with stations, relationships, personality)
- Production chain definitions (JSON linking commodities to stations)
- Balancing framework (design rules for difficulty, progression, economy)
- Starting conditions (player starts in Starling at Earth Hub with 2,000 credits)

## Build Order

1. Commodity table + production chains (needed by economy)
2. Ship archetypes (needed by player + NPCs)
3. Equipment tiers (needed by combat balance)
4. Faction definitions (needed by reputation + stations)
5. Starting conditions (the "new game" state)
6. Playtesting + tuning (iterate on numbers until it feels right)

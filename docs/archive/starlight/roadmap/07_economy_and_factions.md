# (07) — Economy & Factions

**Layer 4: CONFLICT — money makes the world go round, factions fight over it.**

**Depends on:** (03) (NPC trade flow — NPCs carry cargo between stations), (04) (cargo system, pricing primitives, docking), (06) (reputation — faction standing gates access).
**Blocks:** (08) (wormhole access may be faction-controlled), (09) (mining feeds raw materials into economy).

## Overview

The economy is a single persistent simulation running across all sectors simultaneously. Prices emerge from supply and demand — no hand-tuned price tables. NPC traders drive the flow by physically moving cargo between stations. Factions own stations and compete for economic dominance. The player participates by trading, disrupting, or protecting trade.

## Economy Core

### Per-Station Stock

Every station tracks stock levels for every commodity it handles:

```
stock: float        // current units
max_capacity: float // storage limit
produces: float     // units per tick (>0 if this station makes it)
consumes: float     // units per tick (>0 if this station uses it)
```

### Pricing (from (04), expanded)

```
fill_ratio = stock / max_capacity
price = base_price * (2.0 - fill_ratio * 1.5)
```

- Fill ratio near 1.0 (surplus): price drops to ~0.5x base
- Fill ratio near 0.0 (shortage): price rises to ~2.0x base
- Natural trade signals: buy at surplus stations, sell at shortage stations

### Production Chains

Base materials → manufactured goods → end consumers. Stations specialize by production profile (see (17) for the canonical table). Multi-step chains fall out of the profile graph — no explicit "tiers" in the data, just inputs and outputs.

Example chain (ship construction):
```
Iron Mining Station      produces: Iron
Titanium Mining Station  produces: Titanium
Shipyard                 consumes: Iron + Titanium → produces: Ship Components
Military Base            consumes: Weapons + Ship Components + Fuel   (end consumer)
```

If mining is disrupted (pirate raid, NPC miners destroyed), metal supply drops. The shipyard runs low, ship component prices spike, arms manufacturers slow, military readiness drops. **Cascade effects are emergent, not scripted.**

Full production graph: [(17) — Production Chains](17_content_and_balancing.md).

### Production Tick

Every few seconds (configurable), each station:
1. Consumes inputs (reduces stock)
2. Produces outputs (increases stock, scaled by input availability)
3. Partial-input floor: stations with some inputs can still produce at a minimum 5% rate. Zero inputs = zero output.

### Economy Resilience (Critical Design Rule)

**The economy must feel responsive but never collapse.** The player should notice the effects of their actions without needing a spreadsheet to play. Design rules:

1. **Partial-input production floor (5%)** — a constrained station with some inputs can still trickle out goods. A truly starved station with zero required inputs produces zero output.

2. **Stock drift toward baseline** — if stock deviates too far from 50% capacity (either direction), a gentle correction pushes it back. This prevents runaway surpluses or permanent shortages. Rate: ~1% of max capacity per tick toward baseline.

3. **Price clamping** — prices can never exceed 3x base or drop below 0.25x base. The player sees dynamic prices but can't exploit infinite arbitrage.

4. **NPC route rebalancing** — NPC traders naturally correct shortages by routing toward profit signals. If a station is low on ore, the price rises, NPCs bring ore. Self-correcting within minutes, not hours.

5. **No negative stock** — stock floors at 0. No debt, no negative inventory. Consumption simply stops when stock is empty; production only continues if the profile still has at least some required inputs.

6. **Disruption is temporary** — destroying miners or blocking routes creates a shortage that lasts minutes to tens of minutes, not hours. NPC spawning and route reassignment fills gaps quickly. The player feels the impact without permanently breaking the game.

7. **No required trading knowledge** — the player can trade profitably by following the simple rule: buy green (cheap/surplus), sell red (expensive/shortage). The HUD shows this. No need to understand production chains to make money.

The economy is a **mood system**, not a spreadsheet. It creates the FEELING of a living universe — prices change, stations have different stock, routes matter. But it's always recovering, always stable at the macro level. The player's actions create ripples, not tsunamis.

### NPC Trade Drive

When an NPC trader docks:
1. Sells carried cargo → station stock increases
2. Evaluates available routes for profit opportunity
3. Buys cargo with the best margin → station stock decreases
4. Departs on the most profitable route

This creates a feedback loop: NPCs naturally gravitate toward profitable routes, balancing supply and demand over time. Disrupting trade (destroying traders, blocking routes) creates shortages that ripple through the chain.

## Commodity Categories

| Category | Examples | Notes |
|----------|---------|-------|
| Raw Materials | Ore, Ice, Gas, Organics | From mining (09), consumed by refineries |
| Base Materials | Iron, Copper, Titanium, Silicon, Lithium, Silver, Platinum, Gold, Uranium, Rare Metals, Ice/Water/Oxygen, Hydrogen/Deuterium/Helium-3, Fuel, Fusion Fuel, Organics, Polymers | Mined, scooped, grown, or single-step processed |
| Consumer Goods | Food, Medicine, Luxury Items | Consumed by populated stations |
| Industrial | Ship Components, Electronics, Construction Materials | Consumed by military/industrial stations |
| Military | Weapons, Ammunition, Armor Plating | Consumed by military bases |
| Contraband | Narcotics, Stolen Goods, Unlicensed Weapons | Illegal in some factions, profitable |

### Contraband

Some commodities are illegal in specific factions:
- Carrying contraband through faction space risks scan + reputation hit
- Selling contraband at the right (criminal) station is very profitable
- Creates a risk/reward smuggling loop

## Factions

### Structure

Each faction has:
- **Name and identity** (government, corporation, criminal syndicate)
- **Stations they own** (determines where the player can trade, dock, get missions)
- **Reputation with the player** (06)
- **Relationships with other factions** (ally/neutral/hostile — affects NPC behavior)
- **Fleet pool** (military assets — see below)

### Faction Types

| Type | Behavior | Economy Role |
|------|----------|-------------|
| Government | Patrols space, enforces law, taxes trade | Largest station networks, stable pricing |
| Corporation | Runs specific industries, competes with others | Specialized production chains |
| Criminal | Raids trade routes, runs contraband | Creates demand for escorts, drives smuggling |
| Independent | Small stations, neutral territory | Trade hubs, black markets, player-friendly |

### Faction Relationships

Factions have predefined relationships with each other:

| Relationship | Effect |
|-------------|--------|
| Allied (+1.0) | NPCs cooperate. Helping one helps the other. Harming one harms the other. |
| Friendly (+0.5) | NPCs don't fight. Mild reputation cascade. |
| Neutral (0.0) | No relationship. Actions against one don't affect the other. |
| Unfriendly (-0.5) | NPCs avoid each other. Helping one mildly hurts the other. |
| Hostile (-1.0) | NPCs attack on sight. Helping one significantly boosts rep with the opposing faction. |

### Reputation Cascade Math

When the player performs an action that changes reputation with faction A, related factions are also affected:

```
primary_change = action_value  (e.g., -0.15 for destroying a ship)
cascade_change = primary_change * relationship * cascade_factor

cascade_factor = 0.5  (cascades are always weaker than the primary effect)
```

**Example**: Player destroys a Sol Navy patrol ship.
- Sol Navy: -0.15 (direct)
- Miners' Guild (Allied with Sol Navy, relationship +1.0): -0.15 * 1.0 * 0.5 = **-0.075**
- Red Claw Pirates (Hostile with Sol Navy, relationship -1.0): -0.15 * -1.0 * 0.5 = **+0.075** (pirates like you more)
- Frontier Alliance (Neutral, relationship 0.0): no change

### Reputation Action Values

| Action | Rep Change | Notes |
|--------|-----------|-------|
| Destroy faction ship | -0.15 | Per ship. Major hit. |
| Damage faction ship (no kill) | -0.05 | Aggression without destruction. |
| Destroy faction enemy | +0.05 | Per enemy killed. Smaller than the negative — easier to lose rep than gain it. |
| Complete faction mission | +0.10 to +0.20 | Scales with mission difficulty. |
| Trade at faction station | +0.01 | Per trade. Tiny but cumulative — regular traders gain standing slowly. |
| Contraband caught in scan | -0.10 | Per scan event. |
| Abandon faction mission | -0.03 | Small deterrent. |

### Reputation Price Modifier

Faction standing affects prices at their stations:

```
price_modifier = lerp(1.3, 0.85, (reputation + 1.0) / 2.0)
```

| Standing | Price Modifier | Effect |
|----------|---------------|--------|
| Hostile (-1.0) | 1.30x | 30% markup (if they let you dock at all) |
| Unfriendly (-0.5) | 1.19x | Noticeable markup |
| Neutral (0.0) | 1.075x | Slight markup — you're an outsider |
| Friendly (+0.5) | 0.96x | Slight discount |
| Allied (+1.0) | 0.85x | 15% discount — best prices |

This stacks with the supply/demand pricing (04). Allied status at a surplus station = the cheapest prices in the game. Hostile status at a shortage station = the most expensive.

### Faction Fleets

Each faction maintains a fleet pool — ships deployed across their stations and patrol routes.

**Fleet States (State Machine):**

| State | Behavior | Transitions To |
|-------|----------|---------------|
| **Stable** | Normal patrols, trade route protection, standard deployment | → Mobilizing (if influence drops in any zone) |
| **Mobilizing** | Pulling ships from patrols, consolidating toward threatened zones | → Defensive (if influence drops below 30% in key zone) or → Stable (if threat resolved) |
| **Defensive** | All ships to defense positions around owned stations, no patrols | → Stable (if influence recovers above 50%) or stays Defensive |
| **Expansive** | Deploying extra patrols to contested zones, pushing influence | → Stable (if overextended) or → Mobilizing (if counter-attacked) |

State transitions are evaluated every 60 seconds based on faction influence levels across zones.

**Fleet Replacement:**

Destroyed ships are rebuilt over time. Each faction has a build rate (ships per hour) scaled by:
- Number of shipyard stations they control
- Availability of Ship Components commodity at those stations
- Current fleet state (Defensive = faster rebuild priority, Expansive = slower — ships are deployed not built)

A faction that loses its shipyards cannot rebuild. A faction whose supply chain is disrupted (no Iron / Titanium → no Ship Components) rebuilds slower. This connects fleet strength directly to the economy.

Fleet strength affects:
- Station security (more patrols = safer trade routes)
- Economic output (military stations consume fleet supplies)
- Player experience (hostile faction's fleet determines how dangerous their space is)

### Faction Influence Per Zone

Each zone (from (01)) tracks faction influence:
- **Dominant (>70%)** — faction controls the zone. Their patrols are everywhere.
- **Contested (30-70%)** — multiple factions compete. More combat encounters.
- **Lost (<30%)** — faction has abandoned the zone. Lawless.

Influence changes based on: fleet presence, station proximity, NPC combat outcomes, player actions.

## Cross-Sector Economy

### Active Sector (Full Simulation)

The sector the player is in runs full economy: per-station stock updates, NPC trading, production ticks, price recalculation every tick.

### Distant Sectors (Simplified)

Distant sectors (03's DistantSector) run simplified economy:
- Production/consumption ticks still run (just stock arithmetic)
- NPC arrivals trigger cargo delivery (stock changes)
- No spatial awareness — just timers and numbers
- When the player jumps to a sector, it promotes to full simulation

### Cross-Sector Trade

Some NPC routes cross sector boundaries (via wormhole stations, (08)):
- NPC departs Station A in Sector 1
- Timer ticks during wormhole transit
- NPC arrives at Station B in Sector 2
- Cargo delivery updates Sector 2's stock

This connects the economies: a mining shortage in Sol affects prices in Alpha Centauri after a delay (travel time through the wormhole network).

## What This Plan Produces

- Full production/consumption economy with stock tracking
- Dynamic pricing from supply/demand + reputation modifier
- Production chains (raw → refined → end product)
- NPC profit-driven route selection
- Commodity data (categories, contraband flags per faction)
- Faction system (ownership, relationships, fleet states)
- Faction relationships (allied/friendly/neutral/unfriendly/hostile between factions)
- Reputation cascade math (actions ripple through faction relationships)
- Reputation action values (per-action rep change table)
- Reputation price modifier (standing affects buy/sell prices)
- Faction influence per zone
- Faction fleet state machine (stable/mobilizing/defensive/expansive)
- Fleet replacement tied to economy (shipyard output depends on supply chain)
- Cross-sector economic flow via NPC trade
- Cascade effects (disruption → shortage → price spike → rerouting)

## Not Yet Proven

The prototype has stations and NPC traffic but no cargo, no economy, no factions. This is entirely theoretical. Build order within this plan:

1. Commodity data (JSON)
2. Per-station stock + production/consumption tick
3. NPC cargo loading/unloading on dock
4. Dynamic pricing
5. Faction data + reputation integration (from (06))
6. Cross-sector stock flow (integrate with DistantSector)
7. Faction influence tracking

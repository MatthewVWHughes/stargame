# Stargame Direction Understanding

This is my working interpretation of `docs/stargame-direction-questions.md`.
It is written so you can check whether I understood the direction before I
make more gameplay or presentation changes.

## 1. Game Promise

Stargame should feel primarily like a modernized Freelancer 2003: accessible
mouse-driven arcade spaceflight, simple readable combat, trading, missions,
factions, and a player who can choose how to make money and build a life in
space.

The game should not become a hard-science simulator, but it should preserve
believable scale where it matters. Kepler orbits, moving bodies, large travel
distances, gas giants, upper atmospheres, and KSP-inspired planetary scale are
important. Full planet landing is explicitly out of scope.

My intent:

- Treat Freelancer as the primary design reference.
- Use KSP as a reference for scale, planetary representation, and orbital
  believability, not for hardcore orbital gameplay.
- Avoid scope creep disguised as "be whoever you want." The game should support
  trader, fighter, miner, thief, and faction paths over time, but the current
  work should prove one primary playable frontier sector first.
- Build toward a game where the player has a ship, makes choices, takes jobs,
  travels, gets interrupted by events, docks, talks to people, earns money, and
  upgrades.

What I will do:

- Prefer player-facing loops over abstract framework work.
- Keep story/lore hooks possible without blocking the current playable slice.
- Avoid adding planet landing or ship boarding systems unless you explicitly
  change that scope.

## 2. Current Playable Slice

The next meaningful slice is:

`Start game -> spawn near a station in the primary frontier system -> fly around a sector with planets/star/stations -> dock -> enter a simple station interior -> interact with NPCs/shop/mission giver -> accept a mission or trade -> launch -> travel/fight/observe NPC traffic -> return or complete objective -> save/reload/persist.`

The minimum acceptable version is functional, not pretty. Cubes for NPC ships,
plain interiors, rough planets, and simple UI are acceptable if the loop works
from normal play.

My intent:

- Build one primary frontier sector as the proving ground.
- Keep the minimal gate destination/arrival fixture in scope when it proves
  travel, save/load, and arrival behavior; do not treat it as a second broad
  content target.
- Make the loop playable from New Game without relying on console commands.
- Prioritize actual interactions: target, comm, dock, walk up, talk, accept,
  buy/sell, launch, fight, save/load.

What I will do:

- Treat "can the player naturally reach and use this?" as the main acceptance
  test.
- Keep console/debug commands as development aids only.
- Prefer small complete loops over deeper isolated systems.

## 3. Player Role

The player starts as nobody important: a freelancer with the cheapest ship,
basic equipment, about 2000 credits, and no fixed identity.

Their first-hour motivation is money and opportunity:

- trade
- fight
- mine
- steal
- take missions
- gain reputation
- buy better gear or a better ship

My intent:

- Avoid forcing a class, job, or story role at startup.
- Support a "small ship, open options" fantasy.
- Let reputation and money gradually open better options.

What I will do:

- Keep early systems simple but expandable.
- Avoid locking the player into one profession.
- Make early rewards and upgrades understandable.

## 4. World Feel

The world should feel alive and dynamic, but balanced. It should have lawful
traffic, pirates, police, navy/security, trade routes, stations, and economic
movement. It should not be uniformly dangerous, empty, or militarized.

Space feel depends on location:

- Deep space can be quiet and vast.
- Major routes and hubs should feel busier and commercial.
- Pirate regions and anomaly zones should feel more dangerous.
- Visual style should lean Freelancer: colorful and readable, but not alien.
- No alien/weird fantasy direction unless that changes later.

My intent:

- Use location and route context to decide traffic density and danger.
- Avoid random universal combat frequency.
- Use Godot's world-life work as source material where useful.

What I will do:

- Build traffic around routes, stations, economy, law, and piracy.
- Let combat emerge from route danger and pirate activity rather than fixed
  timers everywhere.
- Keep early world life modest but visible.

## 5. Flight Feel

Flight should feel closest to Freelancer. Normal flight should be arcade-first,
with possible light Newtonian flavor if it improves feel. Supercruise should
follow the Godot version: steerable, but not twitchy or fully free.

Travel should not create long stretches where nothing happens, but occasional
quiet cruising is acceptable and desirable.

My intent:

- Preserve fast, readable, approachable control.
- Avoid making the player fight the physics.
- Keep travel times playable.

What I will do:

- Tune flight around feel, responsiveness, and readable objectives.
- Avoid hard-realism changes unless they clearly improve the game.
- Use system scenery, traffic, and occasional events to make travel feel alive.

## 6. Combat Feel

Combat should be occasional and sector-dependent. It should mostly resemble
Freelancer: accessible arcade dogfighting, with some tactical additions such as
power management later.

The minimum combat loop is the Godot baseline:

- player shoots
- NPCs shoot
- shields absorb damage first
- hull takes damage after shields
- zero hull means death/destruction

The player should be able to fight, flee, negotiate, call for help, or make
their own choice depending on the situation.

My intent:

- Keep combat simple and readable first.
- Avoid building a deep combat sim before the basic loop feels usable.
- Add tactical systems only after shooting, damage, enemy behavior, and feedback
  work.

What I will do:

- Use Godot combat as the minimum contract.
- Focus on visible feedback: shots, hits, shields, hull, death, hostile response.
- Keep power management as a later enhancement, not a blocker.

## 7. Stations And Interiors

Walkable station interiors are important, but they can be very simple for now:
flat planes, four walls, static NPCs, simple interaction points, and blockout
spaces are acceptable.

Stations should be primarily physical interiors, not endless menus. Menus are
allowed when needed, but the player should interact with the world.

First station interior should include enough to support the loop:

- mission giver/contact
- shop or trade interaction
- ship repair/service interaction
- exit/launch path
- possibly one hostile/combat room later

My intent:

- Treat physical station interaction as part of the core identity.
- Keep the first implementation ugly but usable.
- Avoid making station gameplay a spreadsheet/menu stack.

What I will do:

- Prefer walk-up interaction actors over menu-only station screens.
- Use simple Blueprint-authored spaces and static NPCs where possible.
- Keep multiple rooms, decoration, advanced NPC behavior, and visual polish for
  later.

## 8. Economy And Jobs

Early job types should all become functional eventually, except ship boarding.
Station interior combat is acceptable; boarding other ships is scope creep.

Important early jobs:

- delivery
- courier/passenger
- trade
- repair/refuel service
- bounty
- patrol
- mining
- salvage
- smuggling
- exploration/scanning
- station interior combat

This section is high-level direction. The supporting roadmap and system docs
should decide the exact implementation order, depth, and acceptance tests.

The first goal is functionality across the broader loop, not making one job
highly polished while everything else is absent.

Markets should eventually be simulated and influenced by NPC activity. For
example, a destroyed water convoy should affect the receiving station. The
simulation must avoid uncontrolled death spirals.

My intent:

- Start with a small number of commodities and simple flows.
- Build the economy so NPC activity can matter later.
- Avoid fleshing out the whole economy before the playable loop exists.

What I will do:

- Implement only a few commodities at first.
- Keep transaction paths deterministic and saveable.
- Add economic consequences carefully, with guardrails.

## 9. Factions, Law, And Reputation

Factions are foundational. Crime and police should be present early, but they do
not need to dominate the first playable slice. The player should eventually be
able to become a criminal or pirate and face consequences.

Reputation should affect access to opportunities, missions, ships, gear, and
faction treatment over time.

My intent:

- Keep factions/law/reputation as groundwork from the start.
- Do not make the first slice depend on a huge faction simulation.
- Preserve "do what you want, deal with consequences."

What I will do:

- Add simple reputation changes tied to player actions.
- Make police/crime present but lightweight first.
- Avoid deep law systems until the basic loop is playable.

## 10. UI And Presentation

UI should be modern minimal. It should support gameplay without making the game
feel like a spreadsheet. Menus should only exist where they are actually needed.
No menu should exist just to open another menu if the interaction can happen in
world.

Prototype UI is acceptable if the gameplay loop works.

My intent:

- Keep UI functional and minimal.
- Avoid nested menu stacks.
- Favor in-world interaction for stations and gameplay actions.

What I will do:

- Build UI only to support the current loop.
- Keep station interactions physical where practical.
- Avoid polishing UI before the loop is proven.

## 11. Visual Quality Bar

Visuals are not the current priority. The star was a stress test of whether I
could handle subjective/artistic work, not the current production focus.

For now, an asset is good enough if its intent is obvious:

- trader ship reads as trader
- gas giant reads as gas giant
- station reads as station
- weapon effect reads as weapon fire
- NPC reads as interactable NPC

References:

- Godot version for existing prepared direction
- KSP for planetary LOD and scale representation
- Freelancer for general space design

My intent:

- Avoid visual shortcuts that pretend to be final.
- Use placeholders honestly when they are placeholders.
- Ask for clarification before deep subjective/artistic work.

What I will do:

- Focus on clear silhouettes and obvious function.
- Avoid spending long loops on art unless the task is specifically an art task.
- If I am uncertain about a subjective visual target, I will stop and ask before
  pushing ahead.

## 12. Blueprint Versus C++

Blueprint/editor control should be used as much as reasonably possible because
C++ changes require editor restarts and slow iteration. This preference applies
to presentation, authored layout, tuning, interaction surfaces, and visual
variation. C++ remains the authority for deterministic state, validation,
persistence, transactions, orbit/frame queries, system loading, docking, travel,
economy mutations, and anything save-critical.

The balance should be:

- C++: save-critical rules, simulation, IDs, validation, deterministic systems,
  complex data transforms, performance-sensitive logic.
- Blueprint/editor: presentation, level composition, VFX, materials, tuning,
  interaction actors, station layouts, authored variation, designer-facing
  setup.

My intent:

- Push tweakable and presentation-facing behavior into Blueprint/editor.
- Avoid hiding simple designer tuning in C++.
- Still use C++ where correctness, persistence, validation, or scale requires it.

What I will do:

- Before adding C++, consider whether Blueprint/data assets can own the desired
  iteration surface.
- Expose C++ systems through clear Blueprint-callable APIs and editable
  properties.
- Avoid making Blueprint the source of truth for save-critical state.

## 13. Save/Load And Persistence

For now, save/load should be simple. One autosave is enough. Saving should happen
while docked for the first real slice.

It should preserve the basics needed for the current loop:

- player location/state
- credits
- ship/equipment basics
- cargo
- active missions
- basic reputation/faction changes
- station/economy mutations needed by the loop

My intent:

- Keep persistence simple until more of the game is locked in.
- Do not build complex save slots or broad persistence UI yet.

What I will do:

- Preserve player-visible mutations in the current slice.
- Prefer docked autosave for now.
- Add save coverage when a gameplay action changes durable state.

## 14. Near-Term Priorities

The proposed priority list is acceptable:

1. Docked station UI/interactions
2. Mission board/contact flow
3. Buy/sell commodity UI
4. Pirate encounter behavior
5. Ship combat feel
6. Station interior interaction
7. System map usability
8. Better planet visuals
9. Audio feedback
10. Save/load polish

My intent:

- Treat this as the working priority order unless you override it.
- Keep everything pointed at the primary frontier-sector playable loop.

What I will do:

- Select work that advances the playable slice.
- Avoid starting unrelated large systems before these are usable.

## 15. Non-Goals

Current non-goals:

- other sectors
- lore expansion
- full economy depth
- ship boarding
- planet landing
- complex versions of every system
- large content breadth
- too many commodities

My intent:

- Build ground-up.
- Introduce new systems only when their foundation is ready.
- Keep each system simple until it achieves its intended goal.

What I will do:

- Stay focused on one primary frontier sector.
- Use a small commodity/item/job set.
- Avoid expanding breadth before the first loop works.

## 16. Done Definition

A task is done when it contributes to a playable skeleton of the game:

`New Game -> fly -> fight -> get missions -> earn money -> buy/sell commodities -> change faction reputation -> land/dock on stations -> save/load.`

Required standards should include:

- usable from menu or normal play
- no console command requirement
- automated coverage for gameplay/systemic features that mutate state or define
  player-reachable behavior
- save/load coverage for durable player-visible changes
- visual/editor verification for player-facing features
- screenshot or short recording where visuals/presentation matter
- Blueprint/editor tweakability where reasonable

For gameplay/systemic work, normal-play reachability, automation, and save/load
coverage are mandatory when state changes. For visual or editor-facing work,
editor/player verification and screenshot or recording evidence are mandatory
where the result is subjective or presentational. Tiny doc/config changes only
need the relevant review or validation.

My intent:

- Do not call backend-only work "done" if the player cannot use it.
- Do not skip verification.
- Make sure features survive normal play and persistence.

What I will do:

- Tie implementation to player reachability.
- Run relevant automation/validation.
- Verify editor/player-facing behavior when the task has a visual or interaction
  component.

## 17. Dealbreakers

The biggest risks are:

- rushing large systems with shallow two-minute implementations
- getting stuck looping on one system repeatedly
- being too confident when I do not actually understand the target
- treating subjective/artistic guesses as solved
- failing to ask for clarification
- saying something is good when it is not

My intent:

- Slow down for expansive systems.
- Confirm understanding before subjective or ambiguous work.
- Stop loops earlier when an approach is not working.

What I will do:

- When unclear, restate the request as: "I think you want X; I plan to do Y."
- For subjective/art tasks, stop and ask before pushing guesses too far.
- Update docs when a decision changes the project direction.
- Resume only after the target is clear enough to implement.

## Working Rule

Until this document is corrected or replaced, my default direction is:

Build one functional Freelancer-inspired frontier sector. Keep systems simple,
playable, and reachable through normal play. Keep the minimal gate
destination/arrival fixture in scope as validation support, but do not expand
into broad additional sector content. Prefer Blueprint/editor control for
presentation and iteration, while keeping C++ authoritative for deterministic,
persistent, transactional, and save-critical foundations. Do not expand scope
into planet landing, ship boarding, deep lore, or full economy complexity. If
the task is subjective, visual, or unclear, stop, ask, record the decision, then
continue.

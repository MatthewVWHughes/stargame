# Scale Metrics

This file is legacy prototype context for the removed gas-giant flight slice. It is not the active scale contract.

Active fixture values live in [`../frontier-test-fixture.md`](../frontier-test-fixture.md). Future scale-band decisions belong in the active domain contract docs and the system data contracts.

## Unit Convention

- Unreal distance uses default engine units: `1 uu = 1 cm`.
- `100 uu = 1 m`.
- `100000 uu = 1 km`.
- Flight values in code should be documented in metres or kilometres as well as uu when the number affects feel or scene scale.

## Legacy Gas-Giant Prototype Scale

This was the removed gas-giant flight slice scale. It is intentionally not full astrophysical scale; keep it only as historical context for horizon feel, cloud approach, visibility, and ship control experiments.

| Item | Unreal value | Real-world label | Notes |
| --- | ---: | ---: | --- |
| Normal flight max speed | `24000 uu/s` | `240 m/s` | Four times the first Unreal port speed so the gas giant approach read faster during prototype testing. |
| Current placeholder ship mesh | `240 uu` long | `2.4 m` | Visual brick only. Do not treat this as ship canon. |
| Small craft reference target | `1400-2500 uu` long | `14-25 m` | Based on the old interceptor/fighter/shuttle blockout range. |
| Prototype gas giant visual radius | `5000000 uu` | `50 km` | Visual reference sphere only. This is not a real-world gas giant scale. |
| Prototype gas giant visual diameter | `10000000 uu` | `100 km` | About 4000 times a 25 m shuttle reference. |
| Atmosphere entry altitude | `4500000 uu` | `45 km` above visual radius | First haze/local fog influence begins. |
| Dense layer top altitude | `1500000 uu` | `15 km` above visual radius | Fog and dust should become clearly readable below this altitude. |
| Deep layer top altitude | `500000 uu` | `5 km` above visual radius | First lower/deep layer tuning band. |
| Crush depth start altitude | `-400000 uu` | `4 km` below visual radius | Damage hook only; ship damage is not implemented yet. |
| Crush depth fatal altitude | `-1500000 uu` | `15 km` below visual radius | Future pressure/destruction reference. |
| Gas giant centre spawn offset | `8000000 uu` | `80 km` ahead | Places the player roughly `30 km` above the visual radius at start, already inside the broad atmosphere envelope. |

## Godot Reference Notes

- The old `ShipController` used `NormalMaxSpeed = 60`, with comments describing normal flight as combat/docking flight. The removed Unreal gas-giant slice was deliberately faster for readability while testing planet approach.
- Old ship blockouts were authored in gameplay metres: interceptor `14 m`, fighter `18 m`, shuttle `25 m`, larger freighters and warships from `50 m` to `1000 m`.
- The Godot planet radii were useful for blockout context, but they were not final for the Unreal gas giant slice.

## Open Scale Decisions

- Decide the first real player ship length before replacing the brick mesh.
- Decide when to introduce planet-local coordinates, origin rebasing, or streamed space sectors.
- Decide whether gas giant gameplay happens near cloud-top scale, deep atmosphere scale, or both.
- Revisit the 50 km prototype radius only if gas-giant rendering returns as a deliberate later slice.
- Keep visual radius, atmosphere entry, deep layer, and crush depth separate. The old single "planet radius" value was too ambiguous for gameplay tuning.

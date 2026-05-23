# HD 219134 Frontier System

This note records the scientific seed for the first body-only frontier
flyaround system.

## Source Star

Chosen star: HD 219134, also known as HR 8832.

Reasons:

- nearby at about 21 light years / 6.5 parsecs
- K3V orange dwarf, less overused than Alpha Centauri or Tau Ceti
- bright, well-studied, and known to host a compact multi-planet system
- real published planets give us a grounded inner-system layout

Reference facts:

- NASA describes HD 219134 b as the closest transiting rocky exoplanet known at
  the time of the 2015 Spitzer confirmation, around 21 light years away.
- The 2015 six-planet paper reports a nearby K3V star with planet periods of
  about 3.1, 6.8, 22.8, 46.7, 94.2, and 2247 days.
- A 2025 asteroseismic study reports HD 219134 as a K3V planet host with mass
  about 0.763 solar masses, radius about 0.748 solar radii, and age about
  10.15 Gyr.

References:

- NASA Spitzer announcement: https://www.nasa.gov/news-release/nasas-spitzer-confirms-closest-rocky-exoplanet/
- NASA Exoplanet Catalog, HD 219134 b: https://science.nasa.gov/exoplanet-catalog/hd-219134-b/
- NASA Exoplanet Catalog, HD 219134 c: https://science.nasa.gov/exoplanet-catalog/hd-219134-c/
- NASA Exoplanet Catalog, HD 219134 f: https://science.nasa.gov/exoplanet-catalog/hd-219134-f/
- NASA Exoplanet Catalog, HD 219134 d: https://science.nasa.gov/exoplanet-catalog/hd-219134-d/
- NASA Exoplanet Catalog, HD 219134 g: https://science.nasa.gov/exoplanet-catalog/hd-219134-g/
- NASA Exoplanet Catalog, HD 219134 h: https://science.nasa.gov/exoplanet-catalog/hd-219134-h/
- A Six-Planet System Orbiting HD 219134: https://arxiv.org/abs/1509.07912
- K-dwarf Radius Inflation and a 10-Gyr Spin-down Clock: https://arxiv.org/abs/2502.00971

## Observed Bodies Used

Use the known planet names and published orbital ordering. These observed
planets override earlier gameplay wishlist counts.

| Body | In-game type | Scientific basis |
| --- | --- | --- |
| HD 219134 b | venus-like rocky planet | known super-Earth, very close orbit, 3.1-day period |
| HD 219134 c | rocky planet | known super-Earth, 6.8-day period |
| HD 219134 f | rocky planet | known super-Earth, 22.7-day period |
| HD 219134 d | dense rocky planet | known high-mass super-Earth, 46.9-day period |
| HD 219134 g | ice giant / Neptune-like | known Neptune-like planet, 94.2-day period |
| HD 219134 h | gas giant | known outer gas giant, 6.2-year period |

## Non-Planet Extrapolations

HD 219134 does not currently give us every requested gameplay body as an
observed object. The planet set remains limited to the known planets above.
The current flyaround fixture adds only non-planet support bodies:

- no moons around the compact inner super-Earths, because close-in small worlds
  are a poor first assumption for stable, readable moon systems
- two small moons around the Neptune-like HD 219134 g
- several varied moons around the known gas giant HD 219134 h

These moons are not presented as real discoveries. They are game bodies
constrained by known system style: old K-dwarf, compact inner rocky planets, at
least one known Neptune-like world, and one known outer gas giant.

## Phase-One Scope

This pass intentionally excludes stations, gates, NPC traffic, markets, missions,
and combat. The goal is to fly around the system, target bodies, exercise nested
orbits, and prove that the system scale can be understood before reintroducing
gameplay actors.

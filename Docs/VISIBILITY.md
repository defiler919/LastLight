# DARKWELL visibility and fog contract

## Player knowledge states

Every world-space fog cell has exactly one player-facing state:

1. `Unexplored`: pure black. Geometry, actors, prompts, and HUD threat data are unavailable.
2. `Explored`: a dim gray last-known presentation. This is memory, not a live sensor.
3. `Visible`: the current world state is authoritative and rendered live.

A visible cell becomes explored when sight leaves it. Explored cells never return to unexplored during a continuation. Save v6 persists both the authoritative 100 cm knowledge cells and the 10 cm presentation cells, including their world-space scale; current visibility is always recomputed after loading. Save v5's 25 cm presentation field and v4's gameplay-only field are migrated into the current presentation resolution when loaded.

The saved 100 cm knowledge grid is gameplay data, not the final display resolution. Current sight is rendered from a continuous final-visibility margin plus a 1024-angle player line-of-sight field, so movement and turning never wait for the 10 Hz gameplay refresh. Full long-range ray density is retained throughout the active sight cone; outside it, where only the 120 cm awareness disk can contribute, 128 short wall samples are interpolated into the same field. Its screen mask updates at 30 Hz at one-half viewport resolution, capped at 1024x576, and samples each pixel's angular footprint for a stable wall silhouette. Pixel evaluation and all separable contour passes execute through Unreal worker tasks. Every rendered 30 Hz visibility state is accumulated into a sparse 10 cm world-space presentation field. Recording includes the visible edge feather and half a presentation-cell diagonal, so a boundary that reached the screen cannot fall between slower memory samples and return to unknown when the player turns. This keeps remembered boundaries aligned with the view that actually exposed them instead of reconstructing the display from large gameplay cells. The sampled memory coverage is reconstructed with three small separable box passes, then hardened through a narrow inward-biased threshold; that gray channel is regenerated only when its memory revision or camera projection changes. This removes the fine raster's shallow-angle stair steps while retaining a crisp boundary and without changing authoritative knowledge. Unknown interiors remain pure black.

The post-process composite keeps current sight and memory in separate mask channels. Current sight samples the live scene. Memory is rebuilt from GBuffer base colour, world normal, and depth discontinuities into a deliberately dim, stable gray image. A fixed normal term separates surfaces and a narrow depth-gradient term preserves same-colour silhouettes such as a white pillar on a white floor; neither term samples live illumination. Current point lights, flashes, moving shadows, exposure, and other live lighting therefore cannot update an explored-but-not-visible area. The final composition is `LiveScene * CurrentMask + StableGeometryMemory * (RememberedMask - CurrentMask)`; unknown space contributes black.

Fog occlusion deliberately ignores pawns. Shadows cast by nearby enemies are therefore renderer lighting, not visibility knowledge. Player-carried local lights use a higher moving-light Virtual Shadow Map resolution plus small physical source radii, while enemy state-indicator lights do not cast shadows. The current primitive enemy presentation also receives light but does not cast a world shadow: an elongated shadow from a nearby cylinder magnifies Virtual Shadow Map texels into visible waves and does not represent the final character silhouette. Authored skeletal enemies may restore a deliberately tuned shadow policy once their meshes and performance budget are known. None of these presentation choices lets enemy positions alter authoritative line of sight.

## Sight and illumination contract

Sight and illumination are separate native masks. The only local exception is a tiny body-awareness disk:

- `SightMask` is the player's fixed 2200 cm forward cone using the character's actual rate-limited facing and player-to-target wall occlusion;
- holding LMB narrows the sight cone linearly from 52 to 35 degrees half-angle over 1.5 seconds from the initial press; releasing the shot restores the ordinary cone;
- `AwarenessMask` is a compact 120 cm circle centered on the player, independent of facing and light but still clipped by walls;
- `LightMask` is the union of enabled local Unreal lights: point lights contribute circles and spot lights contribute directional cones using their live transform, attenuation radius, and outer cone angle;
- `VisibleMask = AwarenessMask union (SightMask intersect LightMask)`;
- authoritative cell discovery additionally requires an unobstructed light-to-cell trace, so a light whose radius overlaps through a wall cannot leak live actors or exploration knowledge.

The ordinary torch is a point light carried beside the body. Reloading, swinging, and holding may move or resize that light, but they cannot grant long-range sight outside the player's view cone. The lantern base is also a point light; its focus/flash component remains a spotlight. Facility lamps, powered objects, and authored local lights participate automatically while active. Directional and skylight ambience do not count as gameplay illumination. World `Visibility` collision blocks both player sight and authoritative light propagation.

Remembered terrain follows the accumulated 10 cm presentation field, not the 100 cm gameplay grid. Bilinear sampling plus linear-time continuous-contour reconstruction exists only in the screen presentation path; its hardened inward threshold cannot establish gameplay knowledge or bridge an unexplored wall.

## Movement contract

- Base walking speed: 430 cm/s; actual speed continuously follows movement/facing alignment: 100% forward, 78% strafe, and 58% backpedal. Diagonal directions interpolate between those anchors.
- Mouse aim turns at up to 280 degrees/second.
- Base sprint speed: 650 cm/s; sprint turning is limited to 190 degrees/second and uses the same directional speed rule while the body catches up.
- Sprint requires movement input. While sprinting, the character, weapons, lights, and vision cone turn toward movement instead of the cursor.
- Shotgun traces and right-hand light pressure follow the actual character facing.

## Actor knowledge

`IDarkwellFogSubject` is the native presentation contract:

- mobile enemies are rendered only while currently visible;
- world pickups (shells, scrap, mission items, and future removable loot) render and accept facing-proximity interaction only while currently visible; their light components may still participate in illumination discovery, but live light is never composited into gray memory;
- the exit and storage containers preserve their last-seen presentation while explored but not visible;
- reacquiring sight immediately refreshes a fixed facility to its latest authoritative state.

Current fixed actors do not change gameplay collision through fog. Any future remotely controlled door, trap, machine, or destructible that can change off-screen must implement the same contract and separate authoritative gameplay state from its cached last-known presentation.

## World-authoring requirements

- Walls, closed doors, and large occluders must block the `Visibility` trace channel at player eye height.
- Decorative meshes must not accidentally block that channel unless they should hide what is behind them.
- Encounter and pickup UI must query current visibility rather than screen position or distance alone.
- New removable, movable, or otherwise transient world actors must implement `IDarkwellFogSubject` with current-sight-only presentation. Stable architecture and fixed interactables may remain in gray memory; remotely changing fixed actors must cache their last-seen presentation.
- Every world interaction uses a 300 cm, 60-degree-half-angle facing cone and never requires cursor precision. If several visible interactables are eligible, the one closest to the character's facing centerline wins; distance breaks an angular tie. The mouse controls desired facing only and performs no interaction selection trace.
- Both fog grids are global and sparse, so authored maps do not need a fixed-size world render target or bounds volume. Large campaign spaces should still be partitioned by authored level/continuation boundaries so presentation-memory growth remains bounded.

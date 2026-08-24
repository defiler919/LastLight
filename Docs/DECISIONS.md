# Technical and design decisions

| Decision | Current choice |
| --- | --- |
| Game name | `DARKWELL` |
| Unreal project/module | `Darkwell` |
| Engine | Unreal Engine 5.8.1 |
| Primary platform | Windows desktop, Win64 |
| Runtime model | Single-player, offline |
| Gameplay implementation | C++ owns core rules; assets/Blueprints are presentation and tuning layers |
| Perspective | 3D top-down |
| Prototype camera | Absolute top-down: fixed directly above the player at `Pitch -90°`, `Yaw 0°` |
| Prototype movement and aiming | Camera-relative WASD movement; mouse cursor aims at the traced world point |
| Turning and sprinting | Mouse aim is rate-limited to 280 degrees/second; forward/strafe/backpedal movement uses 100%/78%/58% of the active base speed with continuous angular interpolation. Holding Left Shift while moving raises the base speed from 430 to 650 cm/s and turns at 190 degrees/second toward movement, forcing the view cone forward |
| Player knowledge | A native sparse XY grid owns authoritative actor visibility and explored memory; the per-frame visual mask evaluates the same native sight sources continuously and never changes gameplay knowledge |
| Fog of war | StarCraft-style three states: unexplored cells are pure black, explored cells show a dim last-known presentation, and currently visible cells show live state; sight sources are blocked by world visibility collision |
| Fog subjects | Mobile enemies disappear outside current sight; stateful fixed facilities keep their last-seen presentation and refresh to current state only when reacquired |
| First playable loop | Find the generator fuse, survive the stalker and fuse-guard Warden, return to the powered emergency exit, and escape; native C++ owns progress and interaction rules |
| Durable prototype state | Native Gameplay Tags represent player life, torch/lantern actions, shotgun, enemy archetypes/light-control, and door states |
| Prototype content strategy | A lightweight saved map uses a C++-spawned room and actors until approved art assets arrive |
| First enemy brain | Native C++ AIController using AI Perception sight/hearing and NavMesh movement |
| First enemy light reaction | A held torch makes a visible stalker retreat to and remain outside its aimed deterrence boundary. Lantern focus does not change behavior while exposure builds; a full meter produces a five-second stun. Torch swings and lantern flashes retain immediate short control windows |
| Second enemy archetype | The Warden is a slow 260-health, 28-damage fuse guard with a distinct armored greybox silhouette. It advances through the outer torch field until a closer 58% stand-off boundary, while full lantern focus still creates a five-second stun |
| Prototype player danger | The stalker deals timed close-range damage; overlapping hits receive brief invulnerability, damage flashes red, and death presents an explicit `R` restart flow |
| Prototype mission state | Native GameState and Gameplay Tags represent find-fuse, reach-exit, and escaped states; `R` restarts after either defeat or victory |
| Required mission passage | The central divider door follows the standard facing-proximity `F` interaction and uses amber/green state lighting to communicate its state |
| Reload torch behavior | A lit torch becomes a small foot-level light pool for the player, but enemies treat it as extinguished until reload completes |
| Hand input layout | `Q` left-hand weapon wheel, `E` right-hand tool wheel, `F` interaction, LMB/RMB use the equipped left/right item |
| Equipment extensibility | Native equipment tags distinguish left-hand firearms and right-hand tools; shotgun, torch, and lantern are implemented, and releasing E cycles the two right-hand choices in the current wheel prototype |
| Inventory model | Native fixed-slot inventory component; one stack occupies one slot, with no weight system; player, chests, and cabinets share the same transaction rules |
| Inventory interaction | `Tab` opens the backpack; two left-clicks move/merge/swap within one panel; right-click splits within that panel; `Ctrl+LMB` quick-transfers a whole stack; successful take-all closes both panels |
| Container presentation | Chests and cabinets use separate native moving panels; focus and loot availability drive a cold signal light, the panel opens only while its inventory context is active, and an empty container is dimmed and left slightly ajar as a searched-state cue |
| Item definitions | Native Primary Data Assets keyed by Gameplay Tags provide localizable `FText` names, categories, stack limits, colors, and soft UI-icon references; a C++ fallback catalog supplies localizable names/descriptions and keeps essential items usable if an asset is unavailable |
| Item presentation localization | The Unreal GatherText pipeline compiles native English and Simplified Chinese (`zh-Hans`) resources; item and crafting HUD strings use stable localization keys, while generated source art remains under `SourceArt/UI/Items` and is imported through the Editor Python API |
| Crafting integration | Interacting with a workbench opens the backpack and recipe panel together; material consumption and output insertion commit as one atomic inventory transaction |
| Crafting recipes | Native Primary Data Assets provide recipe display names, tagged inputs, tagged outputs, and quantities; C++ owns validation and the atomic transaction |
| Save architecture | A versioned native `USaveGame` payload and `UGameInstanceSubsystem` capture durable state, write/read one continuation slot asynchronously, reload the saved map, then restore runtime actors; v3 adds torch heat and lantern fuel, v4 adds sparse authoritative explored-fog knowledge, v5 adds fine presentation memory, v6 persists its world-space cell size, and v1-v6 remain accepted |
| Persistent world identity | Containers and runtime pickups receive stable native `FName` IDs; save data never depends on transient actor names or spawn order |
| Save product flow | Native main/pause/settings menus own New Game, Continue, manual save/load, return-to-menu, quit, and display mode; fuse collection and successful crafting autosave into the same continuation slot; F5/F9 remain development-only shortcuts |
| Safe loading | Restore grants three seconds of player damage immunity and clears stalker awareness/movement for the same grace period |
| Primary weapon | Left-hand, one-handed sawed-off double-barrel shotgun |
| Fire input | LMB quick release fires a wide 9-degree-half-angle shotgun blast; holding beyond 0.18 seconds enters aimed fire, while sight and spread tighten linearly from the initial press over 1.5 seconds (52 to 35 degrees of sight, 9 to 3 degrees of spread) before release fires |
| Right-hand tools | Torch for close group control; lantern for baseline information and focused high-threat control |
| Torch capabilities | Passive illumination, RMB tap swing with heat, RMB hold stand-off boundary, dangerous reload lowering; future throwing/ignition |
| Lantern capabilities | Passive area illumination, RMB hold focus beam with approximately three-second stun buildup and five-second full-meter stun, RMB tap fuel-cost flash with cooldown and a short immediate control window |
| Ammunition | Scarce; shells crafted at workbenches |
| First milestone | Greybox gameplay prototype before a polished vertical slice |

## Deferred decisions

- Final camera pitch, distance, occlusion behavior, and aim-assist behavior.
- Shotgun barrel selection and whether double-fire exists.
- Tactical versus emergency reload behavior and empty-shell recovery.
- Torch/lantern resource tuning, relighting/refueling cost, and fire/light-reaction enemy taxonomy.
- Final rendering features, including hardware ray tracing.
- Final save-slot/profile count and campaign-scale migration policy.

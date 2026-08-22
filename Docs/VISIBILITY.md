# DARKWELL visibility and fog contract

## Player knowledge states

Every world-space fog cell has exactly one player-facing state:

1. `Unexplored`: pure black. Geometry, actors, prompts, and HUD threat data are unavailable.
2. `Explored`: a dim gray last-known presentation. This is memory, not a live sensor.
3. `Visible`: the current world state is authoritative and rendered live.

A visible cell becomes explored when sight leaves it. Explored cells never return to unexplored during a continuation. Save v4 persists the sparse explored-cell set; current visibility is always recomputed after loading.

The saved 100 cm knowledge grid is gameplay data, not the final display resolution. Current sight is rendered from continuous circle/cone margins plus a per-frame 360-ray wall-occlusion field, so movement and turning never wait for the 10 Hz gameplay refresh. Explored memory is reconstructed into a transient quarter-resolution screen mask and passed through a wide linear-time separable filter before compositing. This keeps unknown interiors pure black while preventing either live sight or remembered terrain from exposing square-cell seams.

## Visibility sources

The native visibility component combines:

- a short omnidirectional awareness radius around the player;
- the player's forward unlit vision cone;
- the equipped torch cone, including its held deterrent reach and reduced reload pool;
- the lantern's base radius, focus beam, and flash cone.

Every source uses the character's actual rate-limited facing, not the raw cursor direction. World `Visibility` collision blocks cell discovery, so walls and closed passages prevent information leakage.

## Movement contract

- Walking speed: 430 cm/s; mouse aim turns at up to 240 degrees/second.
- Sprint speed: 650 cm/s; sprint turning is limited to 165 degrees/second.
- Sprint requires movement input. While sprinting, the character, weapons, lights, and vision cone turn toward movement instead of the cursor.
- Shotgun traces and right-hand light pressure follow the actual character facing.

## Actor knowledge

`IDarkwellFogSubject` is the native presentation contract:

- mobile enemies are rendered only while currently visible;
- the exit and storage containers preserve their last-seen presentation while explored but not visible;
- reacquiring sight immediately refreshes a fixed facility to its latest authoritative state.

Current fixed actors do not change gameplay collision through fog. Any future remotely controlled door, trap, machine, or destructible that can change off-screen must implement the same contract and separate authoritative gameplay state from its cached last-known presentation.

## World-authoring requirements

- Walls, closed doors, and large occluders must block the `Visibility` trace channel at player eye height.
- Decorative meshes must not accidentally block that channel unless they should hide what is behind them.
- Encounter and pickup UI must query current visibility rather than screen position or distance alone.
- Cursor interaction uses a dedicated trace that skips non-interactable overhead geometry. Visibility is evaluated at the actual interaction hit point, and doors provide a generous moving hit proxy so walls and door frames do not steal focus in the top-down view.
- The fog grid is global and sparse, so authored maps do not need a fixed-size render target or world bounds volume.

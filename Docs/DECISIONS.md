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
| Primary weapon | Left-hand, one-handed sawed-off double-barrel shotgun |
| Fire input | Left mouse button |
| Right-hand tool | Torch first; lantern later |
| Torch capabilities | Illumination, melee attack, enemy deterrence, future throwing/ignition |
| Ammunition | Scarce; shells crafted at workbenches |
| First milestone | Greybox gameplay prototype before a polished vertical slice |

## Deferred decisions

- Exact camera pitch, distance, occlusion behavior, and aiming model.
- Shotgun barrel selection and whether double-fire exists.
- Tactical versus emergency reload behavior and empty-shell recovery.
- Torch burn duration, relighting cost, and fire-reaction enemy taxonomy.
- Final rendering features, including hardware ray tracing.

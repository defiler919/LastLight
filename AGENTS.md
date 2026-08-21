# DARKWELL repository guidance

## Environment

- Unreal Engine version: 5.8.1.
- Default engine root on Windows: `D:\UE_5.8`.
- Override the engine root with `DARKWELL_UE_ROOT` when necessary.
- Primary target: Windows desktop, Win64, single-player, offline.
- Build the editor target with `Scripts/BuildEditor.ps1`.

## Architecture rules

- Put runtime gameplay logic in C++ under `Source/Darkwell`.
- Keep Blueprint logic minimal. Blueprints and Unreal assets may bind models, animation, audio, VFX, UI layout, and tuning data, but must not own core gameplay rules.
- Prefer small actor components and plain Unreal gameplay framework classes over a large generic framework.
- Do not introduce Lyra, Gameplay Ability System, Mass, CommonUI, multiplayer infrastructure, or third-party gameplay plugins without explicit approval.
- Define durable gameplay states and categories with native Gameplay Tags instead of scattered booleans or string comparisons.
- Build enemy sensing on Unreal AI Perception and navigation. The first enemy brain should remain C++-driven; reconsider StateTree only when behavior complexity justifies an asset graph.
- Keep editor-only automation out of runtime modules. Place repeatable editor automation in a future editor module or `Content/Python`.

## Asset safety

- Never move, rename, delete, or rewrite `.uasset` and `.umap` files with ordinary filesystem commands.
- Use Unreal Editor, official Unreal MCP tools, or Unreal Editor Python APIs for asset operations so internal references remain valid.
- Keep generated directories (`Binaries`, `DerivedDataCache`, `Intermediate`, and `Saved`) out of version control.
- Store Unreal binary assets and large source media through Git LFS.

## Verification

- Run a full `DarkwellEditor Win64 Development` build after C++ or build configuration changes.
- Add C++ automation coverage for stable gameplay rules as systems appear.
- Treat editor Live Coding as an iteration aid, not as final build evidence.
- Before handoff, inspect the Git diff and report build or test commands and results.

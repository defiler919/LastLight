# DARKWELL development handoff

Last updated: 2026-08-22

## Completed playable systems

- Absolute top-down movement, cursor aiming, two-hand input, shotgun, switchable torch/lantern tools, and dangerous reload presentation.
- Contextual RMB short/long actions: torch swing plus heat, a held torch boundary field, lantern base light, a held focus beam with stun buildup, and a fuel-cost flash with cooldown.
- Stalker perception, pursuit, attacks, damage/death feedback, stable held-torch stand-off behavior, visible lantern focus buildup, five-second full-meter stun, flash/swing light stun, and restart flow. Lantern focus does not interrupt or repel the enemy before the meter fills.
- Native Warden archetype guarding the fuse: 260 health, slower movement, 28-damage attacks, a broad armored silhouette, magenta threat presentation, a closer 58% torch boundary, and the same full-meter five-second lantern stun. Native archetype Gameplay Tags and per-enemy HUD rows keep both threats distinct.
- Generator-fuse mission, powered exit, escape state, and replay flow.
- Cursor-focused `F` interactions and manually operated central passage door.
- Native 16-slot player backpack with stack capacity, splitting, reserve shotgun-shell integration, and scrap.
- Shared eight-slot chest/cabinet storage with whole-stack quick transfer, half-stack transfer, and take-all.
- Distinct animated chest and cabinet presentation: focused loot signals brighten, opening the inventory moves the lid/door, closing it restores the world state, and searched-empty containers become dim and remain slightly ajar. The inventory panel and interaction prompt expose live item totals and empty state.
- Shell workbench that opens beside the backpack and atomically crafts two shells from two scrap.
- Data-driven native item-definition assets for shotgun shells and scrap, plus a native workbench recipe asset; Gameplay Tags remain the stable item identity and C++ fallbacks protect the playable loop. Formal 1254px shell/scrap icons are bound as UI textures, inventory and recipe cards render them at square aspect, and the item/workbench presentation layer has compiled English and Simplified Chinese localization resources.
- Versioned native continuation save v3 with asynchronous disk I/O and stable world-object IDs. It restores player transform/health/state, backpack contents and capacity, loaded shells, torch charge/heat, lantern fuel, equipped hands, mission and door state, container contents, runtime pickup quantities, and every spawned native enemy subclass by persistent ID after reconstructing the saved level. Versions 1 and 2 remain supported with right-tool resource defaults.
- Native product shell with main, pause, and settings menus: New Game, Continue, resume, save/load, display mode, return to main menu, and quit. Continue/load disable themselves when no slot exists.
- Fuse collection and successful workbench crafting create asynchronous autosaves. Loading grants the player and all native enemy AIs a three-second safety grace before combat resumes.

## Inventory controls

- `Tab`: open/close backpack.
- Left-click a stack, then a target slot in the same panel: move, merge, or swap it.
- Right-click a stack: split it into a free slot in the same inventory panel.
- `Ctrl + LMB` between backpack and container: transfer a whole stack.
- `T` or `TAKE ALL`: move everything that fits from a container, then close both panels after a successful transfer.
- `F` on workbench: open backpack plus crafting; click the shell recipe to craft.
- `F`, `Tab`, or `Escape`: close the inventory context.
- `F5`: development quick-save to `Darkwell_Continue`.
- `F9`: development quick-load from `Darkwell_Continue`.

## Next development candidates

1. Tune the two-enemy encounter economy and replace greybox Warden/light feedback with authored animation, audio, and VFX.
2. Add authored audio/VFX hooks for interaction, container motion, crafting, save feedback, swings, focus, flash, and overheat.
3. Expand the continuation policy to multiple slots and explicit save migrations when the campaign structure exists.

Core runtime rules remain native C++; UI art and tuning data can migrate to assets without replacing inventory transactions.

## Verification at handoff

- `Scripts/BuildEditor.ps1`: passed (`DarkwellEditor Win64 Development`).
- Unreal automation: 19/19 DARKWELL tests passed, including native Stalker/Warden archetype rules, the Warden's reduced torch boundary, right-tool gesture/heat rules, lantern stun buildup/decay, wheel release commit, save v3 serialization, and v1/v2/v3 compatibility policy.
- PIE enemy-roster regression: the native game mode spawned exactly one base Stalker and one Warden. Runtime inspection confirmed `Enemy.Archetype.Warden`, persistent ID `Enemy.Warden.FuseGuard`, 260 health, 28 damage, 1.65-second attack interval, 58% torch range, three-second lantern fill, five-second stun, location `(650,120)`, and the distinct `GreyboxBody`/`FacingMarker`/`ArmorShell` mesh set. The HUD rendered separate named threat rows for `STALKER` and `WARDEN`.
- PIE held-torch regression: real RMB input kept the stalker in `HELD AT BAY` at both one and three seconds while player health remained at 100%; the enemy retreated to the deterrence boundary and did not resume approaching or attacking while the hold remained active.
- PIE lantern-focus regression: after one second of uninterrupted focus the visible stun meter reached 23% while the stalker remained `HUNTING`; after approximately three seconds it changed to `LIGHT-STUNNED 4.8s`, and two seconds later the countdown showed 3.0 seconds while player health stopped falling. This confirms that focus has no partial behavioral effect and that a full meter applies the five-second stun.
- Unreal item-presentation import: both 1254x1254 source images imported through the Editor Python API, use `TC_EditorIcon`/`TEXTUREGROUP_UI`, are non-streaming sRGB UI textures, and are referenced by the shell and scrap Primary Data Assets.
- Localization gather/import/compile: English and `zh-Hans` manifests, archives, PO files, and locres compiled successfully. PIE game-language preview resolved shell labels to `霰弹枪弹药`/`弹药`, scrap labels to `废料`, the backpack title to `背包`, and the recipe name to `霰弹枪弹药`.
- PIE item-presentation smoke test: Continue restored the existing save; `Tab` displayed both formal square icons, localized short labels, quantity overlays, and the localized backpack heading. PIE was stopped cleanly and Save All completed afterward.
- Restarted-editor asset scan: Asset Manager discovered both native item-definition assets and the shell workbench recipe; the catalog did not rely on its C++ fallback entries.
- PIE data-driven HUD smoke test: the backpack opened normally after restart and rendered the shell stack from the item catalog; PIE was stopped cleanly afterward.
- PIE: confirmed 16 backpack slots, four reserve shells stored as an item stack, two populated storage containers, two scrap pickups, one workbench, and reliable Enhanced Input open/close through `Tab`.
- PIE click regression: confirmed right-click split (`x4` into `x2 + x2`), held `Ctrl+LMB` whole-stack transfer, `T` take-all, and recipe-button crafting (`2 scrap -> 2 shells`) through the real mouse input path.
- Inventory hit testing now uses the active game viewport dimensions outside `DrawHUD`; LMB/RMB are dispatched by Enhanced Input events instead of frame-polled mouse edges.
- PIE inventory ownership regression: confirmed backpack splitting stays in the backpack, container splitting stays in the container, two-click movement places a stack into the selected local slot, and successful take-all closes both panels.
- PIE disk persistence regression: quick-saved at player `(0,0)` with one loaded shell, moved to `(500,-500)`, fired to empty, then quick-loaded. The level reconstructed and restored `(0,0)`, one loaded shell, weapon-ready state, full health, and both equipped-hand tags.
- PIE function-key regression: project input config removes UE's default `F5` shader-complexity and `F9` screenshot debug bindings. After an editor restart, standalone `F5` updated the save while the viewport remained Lit, and `F9` restored the player without creating a screenshot.
- PIE product-flow regression: the initial native main menu rendered correctly; real OS mouse clicks opened Settings, returned through Back, and activated Continue. Continue loaded the existing continuation into gameplay, and the controller returned to full gameplay input with no menu left open. Menu hit testing uses stable viewport dimensions instead of the render-only HUD Canvas.
- PIE storage-presentation regression: an existing continuation restored both searched-empty containers in their distinct chest/cabinet styles. Real cursor focus plus `F` opened the empty supply chest, drove its lid to `-78` degrees, opened the backpack/container context with `Supply chest - EMPTY`, and disabled take-all as `CONTAINER EMPTY`; a second `F` closed the context and returned the lid to the searched-state `-14` degrees. Runtime-only actor moves used to isolate the test were discarded when PIE stopped.
- PIE v2 persistence regression: `F5` wrote a 12,421-byte v2 continuation at `2026-08-22 13:39:55`; `F9` then reconstructed the level and logged a successful v2 restore from that exact save timestamp, exercising the enemy-bearing payload through the real disk path.
- Final editor state: PIE stopped cleanly, Save All completed, and no modal or abnormal editor window remained open.
- Editor PIE input regression: the project remaps Unreal Editor's Stop Play Session chord from `Escape` to `Shift+F8`. Real input confirmed the first `Escape` opens Pause while PIE remains active, a second `Escape` resumes, and `Shift+F8` stops PIE.

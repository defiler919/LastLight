# DARKWELL

`DARKWELL` is a 3D top-down survival-horror game built with Unreal Engine 5.8.1.

The player carries a one-handed sawed-off double-barrel shotgun in the left hand and a light/tool weapon in the right hand. Ammunition and light are limited survival resources; shotgun shells are produced at workbenches.

## Current status

The project now has a completable native greybox loop. It includes an absolute top-down camera, camera-relative walking and sprinting, rate-limited mouse aiming, angle-dependent forward/strafe/backpedal speed, contextual hip-fire/aimed shotgun input, and two switchable right-hand light tools. Sprinting forces the character and view forward along the movement direction. A sparse native visibility grid combines a close awareness radius, the forward view cone, equipped light sources, and wall occlusion. It drives StarCraft-style three-state fog: unexplored space is pure black, explored space retains a dim last-known presentation, and current sight shows live state. Mobile enemies disappear outside current sight; stateful fixed facilities freeze their last-seen presentation so off-screen changes do not leak through fog. The torch has an RMB tap swing that builds heat and an RMB hold deterrence cone; enemies pushed inside their archetype-specific boundary retreat and then remain outside it. The lantern has passive area light, an RMB hold focus beam that builds a visible per-enemy stun meter without changing behavior until full, and an RMB tap flash that creates a short control window. A full focus meter stuns the target for five seconds. Facing-proximity interactions, damage feedback, death/restart, and a generator-fuse objective that powers an emergency exit are also playable. A fixed-slot, weightless native inventory owns reserve shells and scrap, supports stack splitting and container transfers, and powers a shell-crafting workbench. Storage chests and cabinets now provide distinct native presentation, animate while their inventory is open, signal available loot, and remain visibly searched after they are emptied. Item presentation and crafting inputs/outputs are supplied by native Primary Data Asset types keyed by Gameplay Tags; shell and scrap definitions have formal generated icons, localizable `FText` labels, and native localizable description fallbacks, while inventory and crafting transactions remain in C++. A versioned native continuation save persists the player, inventory, equipment, right-hand resources, explored fog knowledge, mission and door states, containers, pickups, and all native enemy subclasses through asynchronous disk I/O. Native main/pause/settings menus provide New Game, Continue, save/load, display-mode selection, return-to-menu, and quit flows; fuse collection and successful workbench crafting create autosaves. The minimal native HUD presents resources, visible threats, per-target lantern stun buildup, inventory/container panels, save status, the current objective, defeat, and successful escape. The saved prototype map is `/Game/Maps/L_Prototype`. The fast Stalker uses the full torch stand-off distance; the slow, armored Warden guards the fuse, has 260 health and 28-damage attacks, and pushes through the outer torch field until reaching its closer 58% boundary. Both use native AI Perception, NavMesh pursuit, immediate swing/flash control, and full-meter five-second lantern stuns. Core gameplay rules remain in C++.

The immediate milestone remains a greybox gameplay prototype that proves movement, top-down aiming, the left-hand shotgun, the right-hand torch, light-aware enemies, and dangerous two-handed reloading.

## Prototype controls

- `WASD`: move relative to the top-down camera. Movement reaches full speed only along body facing; strafing is slower and backpedalling is slowest.
- Hold `Left Shift` while moving: sprint. Sprinting forces the facing direction and vision cone toward movement; releasing Shift restores rate-limited mouse aiming.
- Mouse cursor: choose the desired aim point on the horizontal gameplay plane or first visible world surface. The body, weapons, and view cone turn toward it at a finite angular speed.
- Left mouse button: quick-release to fire the shotgun immediately with a wide spread. Holding enters aim after 0.18 seconds; from the initial press, the view cone and pellet spread tighten linearly over 1.5 seconds to a 35-degree half-angle view and the tightest spread, then release to fire.
- Right mouse button: use the equipped right-hand tool. Tap swings the torch or flashes the lantern. Holding the torch makes an aimed enemy retreat to and remain outside the deterrence boundary. Holding the lantern focuses its beam; hitting an enemy builds the HUD stun meter without interrupting that enemy, and a full meter stuns it for five seconds. Releasing ends a held action.
- `R`: reload both barrels from reserve shells. A lit torch is lowered to a small foot-level light pool and cannot repel enemies until the reload finishes.
- `R` while dead: restart the current level.
- Hold `Q`: open the left-hand weapon wheel. The prototype currently contains the shotgun slot.
- Hold `E`: open the right-hand tool wheel. Releasing `E` switches between the implemented torch and lantern.
- `F`: interact with the currently visible object by moving close and facing it. The mouse never selects an interactable; when several objects are in front, the one nearest the facing centerline is highlighted, with distance breaking an angular tie.
- `Tab`: open or close the backpack.
- In either inventory, left-click a stack and then a target slot to move, merge, or swap it within that same inventory.
- Right-click splits a stack into a free slot in the inventory that was clicked; it never sends the split half to the other panel.
- With a chest or cabinet open, `Ctrl + Left Mouse Button` quickly transfers a whole stack between panels.
- `T` or the `TAKE ALL` button transfers everything that fits, then closes the backpack and container when at least one item moved.
- `F` on the shell workbench opens the backpack and crafting panel together; click the recipe to exchange scrap for shells.
- Storage lights brighten when focused, the lid or door opens with its inventory, and an emptied container becomes dim and remains slightly ajar. Container panels show their live item count or `EMPTY` state.
- Unexplored space is black. Previously explored space remains as a dim last-known image, while only current line of sight shows live enemies and facility state.
- `F5`: quick-save the current continuation slot, including explored fog knowledge, during development.
- `F9`: quick-load the continuation slot during development. Loading reconstructs the level before restoring durable state.
- `Escape`: open the pause menu; resume, save/load, change display mode, or return to the main menu. The initial main menu offers New Game and Continue, with Continue disabled until a continuation save exists. `Enter` activates Continue when available, otherwise New Game; in the pause menu it resumes play.
- In Unreal Editor, `Shift+F8` stops PIE. The project remaps the editor command so `Escape` reaches DARKWELL's pause menu during in-viewport testing.
- Mission: find the green generator fuse, return to the red emergency exit, and press `F` to escape. The exit light turns green when powered.
- `R` after escaping: restart the current level and play again.

The prototype room, interactable actors, storage containers, scrap pickups, shell workbench, Stalker, and fuse-guard Warden are spawned by the native game mode when absent. The saved map contains a navigation bounds volume and uses dynamic navigation generation so the runtime greybox architecture is traversable while external art assets are unavailable.

## Requirements

- Unreal Engine 5.8.1, default location `D:\UE_5.8`
- Visual Studio Community 2026 with C++ game development tools
- Git and Git LFS

## Build

From PowerShell in the repository root:

```powershell
.\Scripts\BuildEditor.ps1
```

If Unreal Engine is installed elsewhere:

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'X:\Path\To\UE_5.8'
```

External full builds can be blocked while Unreal Live Coding is active. Close the editor or disable Live Coding before treating the build as final verification.

## Unreal MCP

The project configures Epic's experimental Unreal MCP server at `http://127.0.0.1:8000/mcp`. Start the editor before opening a Codex task rooted at this repository so Codex can discover the server.

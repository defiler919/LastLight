# DARKWELL

`DARKWELL` is a 3D top-down survival-horror game built with Unreal Engine 5.8.1.

The player carries a one-handed sawed-off double-barrel shotgun in the left hand and a light/tool weapon in the right hand. Ammunition and light are limited survival resources; shotgun shells are produced at workbenches.

## Current status

Project bootstrap. The immediate goal is a greybox gameplay prototype that proves movement, top-down aiming, the left-hand shotgun, the right-hand torch, light-aware enemies, and dangerous two-handed reloading.

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

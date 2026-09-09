[CmdletBinding()]
param([string]$EngineRoot = '')
$ErrorActionPreference = 'Stop'
if (!$EngineRoot) { $EngineRoot = $env:DARKWELL_UE_ROOT }
if (!$EngineRoot) { $EngineRoot = 'D:\UE_5.8' }
$repo = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editor)) { throw "Unreal Editor not found: $editor" }
# Real manual game window: no scripted route, save loading or automatic Blackout.
& $editor "$repo/Darkwell.uproject" '/Game/Maps/L_SightWeaveApartmentLab' -game -d3d12 -sm6 -windowed -ResX=1280 -ResY=720
exit $LASTEXITCODE

[CmdletBinding()]
param([string]$EngineRoot = 'D:\UE_5.8')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editor)) { throw "Unreal Editor not found: $editor" }
# Opens the requested interactive Lab game window; no automated route or activation.
& $editor "$repo/Darkwell.uproject" '/Game/Maps/L_BlackRegionLab' -game -d3d12 -sm6 -windowed -ResX=1280 -ResY=720
exit $LASTEXITCODE

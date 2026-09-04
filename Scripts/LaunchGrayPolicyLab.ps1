[CmdletBinding()]
param(
    [string]$EngineRoot = 'D:\UE_5.8',
    [string]$ProjectPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Darkwell.uproject')
)

$ErrorActionPreference = 'Stop'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor not found: $editor" }
if (-not (Test-Path -LiteralPath $ProjectPath)) { throw "Project not found: $ProjectPath" }

# Opens the dedicated map in normal D3D12/SM6 Editor mode. It deliberately does
# not start PIE; the user remains in control of the Play button.
& $editor $ProjectPath '/Game/Maps/L_SightWeaveGrayPolicyLab' -d3d12 -sm6
exit $LASTEXITCODE

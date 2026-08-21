[CmdletBinding()]
param(
    [ValidateSet('Debug', 'DebugGame', 'Development', 'Shipping', 'Test')]
    [string]$Configuration = 'Development',

    [string]$EngineRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = $env:DARKWELL_UE_ROOT
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = 'D:\UE_5.8'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$buildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}

if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
    throw "Unreal build script not found: $buildScript"
}

if (Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue) {
    Write-Warning 'Unreal Editor is running. Disable Live Coding or close the editor if UnrealBuildTool refuses the full build.'
}

& $buildScript 'DarkwellEditor' 'Win64' $Configuration $projectFile '-WaitMutex' '-FromMsBuild'
if ($LASTEXITCODE -ne 0) {
    throw "DarkwellEditor build failed with exit code $LASTEXITCODE."
}

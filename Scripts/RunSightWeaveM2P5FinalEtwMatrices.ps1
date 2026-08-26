[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$LabelPrefix = '',

    [string]$EngineRoot = '',

    [string]$FinalOutputRoot = '',

    [string]$CalibrationOutputRoot = '',

    [string]$AttributionOutputRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not ('SightWeaveConsoleMode' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class SightWeaveConsoleMode
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr GetStdHandle(int nStdHandle);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);
}
'@
}

$consoleInputModeBefore = $null
$consoleInputModeAfter = $null
$quickEditDisabled = $false
$stdInputHandle = [SightWeaveConsoleMode]::GetStdHandle(-10)
$mode = [uint32]0
if ($stdInputHandle -ne [IntPtr]::Zero -and
    [SightWeaveConsoleMode]::GetConsoleMode($stdInputHandle, [ref]$mode)) {
    $consoleInputModeBefore = $mode
    $modeWithoutQuickEdit = [uint32](($mode -bor 0x0080) -band (-bnot 0x0040))
    if ($modeWithoutQuickEdit -ne $mode) {
        if (-not [SightWeaveConsoleMode]::SetConsoleMode($stdInputHandle, $modeWithoutQuickEdit)) {
            throw "Failed to disable QuickEdit for the M2P.5 elevated console. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }
        $quickEditDisabled = $true
    }
    $verifiedMode = [uint32]0
    if (-not [SightWeaveConsoleMode]::GetConsoleMode($stdInputHandle, [ref]$verifiedMode)) {
        throw "Failed to verify the M2P.5 elevated console mode. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $consoleInputModeAfter = $verifiedMode
    if (($verifiedMode -band 0x0040) -ne 0) {
        throw 'M2P.5 elevated console still has QuickEdit enabled; refusing a selection-pausable ETW orchestration.'
    }
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = $env:DARKWELL_UE_ROOT
}
if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = 'D:\UE_5.8'
}
if ([string]::IsNullOrWhiteSpace($LabelPrefix)) {
    $LabelPrefix = 'post-vision-tail-final-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$calibrationScript = Join-Path $PSScriptRoot 'RunSightWeaveM2P4EtwCalibration.ps1'
$attributionScript = Join-Path $PSScriptRoot 'RunSightWeaveM2P4EtwAttribution.ps1'
if ([string]::IsNullOrWhiteSpace($FinalOutputRoot)) {
    $FinalOutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P5\Final'
}
if ([string]::IsNullOrWhiteSpace($CalibrationOutputRoot)) {
    $CalibrationOutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P5\EtwCalibration'
}
if ([string]::IsNullOrWhiteSpace($AttributionOutputRoot)) {
    $AttributionOutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P5\EtwAttribution'
}

foreach ($requiredScript in @($calibrationScript, $attributionScript)) {
    if (-not (Test-Path -LiteralPath $requiredScript -PathType Leaf)) {
        throw "Required M2P.4 ETW workflow is missing: $requiredScript"
    }
}

$runRoot = Join-Path $FinalOutputRoot $LabelPrefix
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing M2P.5 final ETW orchestration: $runRoot"
}
New-Item -ItemType Directory -Path $runRoot | Out-Null

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$integrityLine = @(whoami /groups | Select-String 'Mandatory Label|Mandatory Level') -join '; '
$fltmcText = (& fltmc 2>&1 | Out-String).Trim()
$fltmcExit = $LASTEXITCODE
$capability = [ordered]@{
    schema = 1
    user = $identity.Name
    administrator = $isAdministrator
    integrity = $integrityLine
    fltmc_exit = $fltmcExit
    fltmc_output = $fltmcText
    wpr = (Get-Command wpr.exe -ErrorAction SilentlyContinue).Source
    wpaexporter = (Get-Command wpaexporter.exe -ErrorAction SilentlyContinue).Source
    console_input_mode_before = $consoleInputModeBefore
    console_input_mode_after = $consoleInputModeAfter
    quick_edit_disabled = $quickEditDisabled
    required_kernel_events = @(
        'Process',
        'Thread',
        'CSwitch',
        'ReadyThread',
        'Profile',
        'PageFault')
}
$capabilityPath = Join-Path $runRoot 'elevated-capability.json'
$capability | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $capabilityPath -Encoding utf8NoBOM
if (-not $isAdministrator -or
    $integrityLine -notmatch 'High Mandatory Level' -or
    $fltmcExit -ne 0 -or
    [string]::IsNullOrWhiteSpace([string]$capability.wpr) -or
    [string]::IsNullOrWhiteSpace([string]$capability.wpaexporter)) {
    throw "M2P.5 final ETW matrices require a high-integrity administrator token and complete ETW toolchain; fail-closed evidence: $capabilityPath"
}

$calibrationLabel = "$LabelPrefix-calibration"
$matrixALabel = "$LabelPrefix-matrix-a"
$matrixBLabel = "$LabelPrefix-matrix-b"
$logPath = Join-Path $runRoot 'elevated-orchestration.log'
$startedAt = Get-Date

try {
    & $calibrationScript `
        -Label $calibrationLabel `
        -EngineRoot $EngineRoot `
        -OutputRoot $CalibrationOutputRoot *>&1 |
        Tee-Object -FilePath $logPath -Append

    & $attributionScript `
        -RunCount 10 `
        -Label $matrixALabel `
        -TraceProfile GeneralProfile `
        -EngineRoot $EngineRoot `
        -OutputRoot $AttributionOutputRoot *>&1 |
        Tee-Object -FilePath $logPath -Append

    & $attributionScript `
        -RunCount 10 `
        -Label $matrixBLabel `
        -TraceProfile GeneralProfile `
        -EngineRoot $EngineRoot `
        -OutputRoot $AttributionOutputRoot *>&1 |
        Tee-Object -FilePath $logPath -Append
}
catch {
    $_ | Out-String | Add-Content -LiteralPath $logPath -Encoding utf8NoBOM
    throw
}

$summary = [ordered]@{
    schema = 1
    status = 'complete'
    started_at = $startedAt.ToString('o')
    completed_at = (Get-Date).ToString('o')
    engine_root = $EngineRoot
    calibration = Join-Path $CalibrationOutputRoot $calibrationLabel
    matrix_a = Join-Path $AttributionOutputRoot $matrixALabel
    matrix_b = Join-Path $AttributionOutputRoot $matrixBLabel
    run_count_per_matrix = 10
    trace_profile = 'GeneralProfile.Verbose.File'
    affinity_or_priority_modified = $false
}
$summaryPath = Join-Path $runRoot 'elevated-stage-complete.json'
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM
$summary | ConvertTo-Json -Depth 5

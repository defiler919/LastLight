[CmdletBinding()]
param(
    [ValidateSet('NullRHI', 'Rendered')]
    [string]$Mode,

    [ValidateRange(36000, 1000000)]
    [int]$Frames = 36000,

    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$Label = '',

    [string]$EngineRoot = '',

    [string]$OutputRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = $env:DARKWELL_UE_ROOT
}
if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = 'D:\UE_5.8'
}
if ([string]::IsNullOrWhiteSpace($Label)) {
    $Label = $Mode.ToLowerInvariant() + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P3\Soak'
}
$runRoot = Join-Path $OutputRoot $Label
$rawRoot = Join-Path $runRoot 'Raw'
$reportRoot = Join-Path $runRoot 'Report'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing soak run: $runRoot"
}
New-Item -ItemType Directory -Path $rawRoot, $reportRoot | Out-Null

$arguments = [System.Collections.Generic.List[string]]::new()
$arguments.Add($projectFile)
$arguments.Add('-unattended')
$arguments.Add('-nop4')
$arguments.Add('-nosplash')
$arguments.Add('-NoSound')
$arguments.Add('-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0')
if ($Mode -eq 'NullRHI') {
    $arguments.Add('-NullRHI')
}
else {
    $arguments.Add('-RenderOffscreen')
}
$arguments.Add('-SightWeaveM2P3SoakCapture')
$arguments.Add("-SightWeaveM2P3SoakMode=$($Mode.ToLowerInvariant())")
$arguments.Add("-SightWeaveM2P3SoakFrames=$Frames")
$arguments.Add("-SightWeaveM2P3SoakOutput=$rawRoot")
$arguments.Add('-ExecCmds=Automation RunTests SightWeave.M2P3.Soak.FrameLevel')
$arguments.Add('-TestExit=Automation Test Queue Empty')
$arguments.Add("-ReportExportPath=$reportRoot")
$arguments.Add("-AbsLog=$(Join-Path $runRoot 'soak.log')")

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $editorCmd
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
foreach ($argument in $arguments) {
    [void]$startInfo.ArgumentList.Add($argument)
}
$process = [System.Diagnostics.Process]::Start($startInfo)
$process.WaitForExit()
if ($process.ExitCode -ne 0) {
    throw "SightWeave $Mode soak failed with exit code $($process.ExitCode). See $runRoot"
}

$reportPath = Join-Path $reportRoot 'index.json'
$summaryPath = Join-Path $rawRoot 'summary.json'
$csvPath = Join-Path $rawRoot 'frames.csv'
foreach ($path in @($reportPath, $summaryPath, $csvPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required soak artifact missing: $path"
    }
}
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.inProcess -ne 0 -or $report.succeeded -ne 1) {
    throw "Soak report is not exactly one clean success: $reportPath"
}
$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
if ($summary.frames -ne $Frames -or $summary.simulated_seconds -lt 600.0) {
    throw "Soak summary does not prove the requested ten-minute-equivalent frame count: $summaryPath"
}
if ($summary.mode -ne $Mode.ToLowerInvariant()) {
    throw "Soak mode mismatch in summary: $summaryPath"
}
$csvRows = @(Import-Csv -LiteralPath $csvPath)
if ($csvRows.Count -ne $Frames) {
    throw "Expected $Frames frame rows, found $($csvRows.Count): $csvPath"
}

Get-Item -LiteralPath $reportPath, $summaryPath, $csvPath |
    Select-Object FullName, Length, LastWriteTime

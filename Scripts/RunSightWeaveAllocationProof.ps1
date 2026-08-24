[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$Label,

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

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P1\AllocationProof'
}
$runRoot = Join-Path $OutputRoot $Label

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing allocation-proof run: $runRoot"
}

New-Item -ItemType Directory -Path $runRoot | Out-Null
$tracePath = Join-Path $runRoot "$Label.utrace"
$csvPath = Join-Path $runRoot "$Label.csv"
$captureReport = Join-Path $runRoot 'CaptureReport'
$analyzeReport = Join-Path $runRoot 'AnalyzeReport'

function Invoke-UnrealEditorCommandlet {
    param([string[]]$Arguments)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $editorCmd
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    foreach ($argument in $Arguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    return $process.ExitCode
}

$captureArguments = @(
    $projectFile,
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NullRHI',
    '-NoSound',
    '-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0',
    '-SightWeaveAllocationCapture',
    '-trace=memory,sightweaveallocation',
    "-tracefile=$tracePath",
    '-ExecCmds=Automation RunTests SightWeave.M2P1.Allocation.Capture',
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$captureReport",
    "-AbsLog=$(Join-Path $runRoot 'capture.log')"
)
$captureExitCode = Invoke-UnrealEditorCommandlet -Arguments $captureArguments
if ($captureExitCode -ne 0) {
    throw "Allocation capture failed with exit code $captureExitCode. See $runRoot"
}

$analyzeArguments = @(
    $projectFile,
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NullRHI',
    '-NoSound',
    '-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0',
    '-SightWeaveAllocationAnalyze',
    '-ExecCmds=Automation RunTests SightWeave.M2P1.Allocation.Analyze',
    '-TestExit=Automation Test Queue Empty',
    "-SightWeaveAllocationTrace=$tracePath",
    "-SightWeaveAllocationReport=$csvPath",
    "-ReportExportPath=$analyzeReport",
    "-AbsLog=$(Join-Path $runRoot 'analyze.log')"
)
$analyzeExitCode = Invoke-UnrealEditorCommandlet -Arguments $analyzeArguments
if ($analyzeExitCode -ne 0) {
    throw "Allocation analysis failed with exit code $analyzeExitCode. See $runRoot"
}

$captureIndex = Join-Path $captureReport 'index.json'
$analyzeIndex = Join-Path $analyzeReport 'index.json'
foreach ($reportPath in @($captureIndex, $analyzeIndex)) {
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw "Automation report not found: $reportPath"
    }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.inProcess -ne 0 -or $report.succeeded -ne 1) {
        throw "Automation report is not one clean success: $reportPath"
    }
}
if (-not (Test-Path -LiteralPath $csvPath -PathType Leaf)) {
    throw "Allocation CSV not found: $csvPath"
}

Get-Item -LiteralPath $tracePath, $csvPath, $captureIndex, $analyzeIndex |
    Select-Object FullName, Length, LastWriteTime

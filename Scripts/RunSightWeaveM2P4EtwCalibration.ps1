[CmdletBinding()]
param(
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
    $Label = 'elevated-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P4\EtwCalibration'
}
$runRoot = Join-Path $OutputRoot $Label
$rawRoot = Join-Path $runRoot 'Raw'
$captureReportRoot = Join-Path $runRoot 'CaptureReport'
$analysisReportRoot = Join-Path $runRoot 'AnalysisReport'
$etlPath = Join-Path $runRoot 'scheduling.etl'
$analysisCsv = Join-Path $runRoot 'attribution.csv'
$summaryPath = Join-Path $runRoot 'summary.json'
$capabilityPath = Join-Path $runRoot 'capability.json'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing M2P.4 ETW calibration: $runRoot"
}
New-Item -ItemType Directory -Path $rawRoot | Out-Null

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
    profile = 'GeneralProfile.Verbose.File'
    required_kernel_events = @('Process', 'Thread', 'CSwitch', 'ReadyThread', 'Profile', 'PageFault')
}
$capability | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $capabilityPath -Encoding utf8NoBOM
if (-not $isAdministrator -or $fltmcExit -ne 0) {
    throw "M2P.4 ETW calibration requires a high-integrity administrator token; fail-closed evidence: $capabilityPath"
}

$wprStarted = $false
try {
    & wpr.exe -start GeneralProfile -filemode
    if ($LASTEXITCODE -ne 0) {
        throw "WPR GeneralProfile start failed with exit code $LASTEXITCODE"
    }
    $wprStarted = $true
    $captureArguments = @(
        $projectFile,
        '-unattended',
        '-nop4',
        '-nosplash',
        '-NullRHI',
        '-NoSound',
        '-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0',
        '-SightWeaveM2P4EtwCalibrationCapture',
        "-SightWeaveM2P4Output=$rawRoot",
        '-ExecCmds=Automation RunTests SightWeave.M2P4.ETW.Calibration.Capture',
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$captureReportRoot",
        "-AbsLog=$(Join-Path $runRoot 'capture.log')"
    )
    $captureExit = Invoke-UnrealEditorCommandlet -Arguments $captureArguments
    if ($captureExit -ne 0) {
        throw "M2P.4 ETW calibration capture failed with exit code $captureExit"
    }
}
finally {
    if ($wprStarted) {
        & wpr.exe -stop $etlPath
        if ($LASTEXITCODE -ne 0) {
            throw "WPR stop failed with exit code $LASTEXITCODE"
        }
    }
}

$captureReportPath = Join-Path $captureReportRoot 'index.json'
$captureReport = Get-Content -LiteralPath $captureReportPath -Raw | ConvertFrom-Json
if ($captureReport.failed -ne 0 -or $captureReport.inProcess -ne 0 -or $captureReport.succeeded -ne 1) {
    throw "ETW calibration capture is not exactly one clean success: $captureReportPath"
}

$analysisArguments = @(
    $projectFile,
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NullRHI',
    '-NoSound',
    '-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0',
    '-SightWeaveM2P4EtwAnalyze',
    "-SightWeaveM2P4EtwTrace=$etlPath",
    "-SightWeaveM2P4EtwMarkerDirectory=$rawRoot",
    "-SightWeaveM2P4EtwReport=$analysisCsv",
    '-ExecCmds=Automation RunTests SightWeave.M2P4.ETW.Analyze',
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$analysisReportRoot",
    "-AbsLog=$(Join-Path $runRoot 'analysis.log')"
)
$analysisExit = Invoke-UnrealEditorCommandlet -Arguments $analysisArguments
if ($analysisExit -ne 0) {
    throw "M2P.4 ETW analysis failed with exit code $analysisExit"
}
$analysisReportPath = Join-Path $analysisReportRoot 'index.json'
$analysisReport = Get-Content -LiteralPath $analysisReportPath -Raw | ConvertFrom-Json
if ($analysisReport.failed -ne 0 -or $analysisReport.inProcess -ne 0 -or $analysisReport.succeeded -ne 1) {
    throw "ETW calibration analysis is not exactly one clean success: $analysisReportPath"
}

$rows = @(Import-Csv -LiteralPath $analysisCsv)
if ($rows.Count -ne 188) {
    throw "Expected 188 ETW calibration markers, found $($rows.Count): $analysisCsv"
}
if (@($rows | Where-Object { $_.timeline_closed -ne '1' }).Count -ne 0) {
    throw "ETW calibration contains unclosed timelines: $analysisCsv"
}
if (@($rows | Where-Object { $_.events_lost -ne '0' -or $_.buffers_lost -ne '0' }).Count -ne 0) {
    throw "ETW calibration contains lost events or buffers: $analysisCsv"
}
foreach ($required in @(
    'empty',
    'fixed_compute',
    'fixed_memory',
    'stage_marker_probe',
    'sleep_10ms',
    'yield_256',
    'fixed_compute_under_load',
    'yield_256_under_load')) {
    if (@($rows | Where-Object operation -eq $required).Count -eq 0) {
        throw "ETW calibration is missing required workload $required"
    }
}

function Get-NearestRank {
    param(
        [double[]]$Values,
        [double]$Fraction
    )

    if ($Values.Count -eq 0) {
        return 0.0
    }
    [Array]::Sort($Values)
    $index = [Math]::Max(0, [Math]::Min(
        $Values.Count - 1,
        [Math]::Ceiling($Fraction * $Values.Count) - 1))
    return $Values[$index]
}

function Get-OperationSummary {
    param(
        [object[]]$Rows,
        [string]$Operation
    )

    $operationRows = @($Rows | Where-Object operation -eq $Operation)
    $metric = [ordered]@{}
    foreach ($property in @('wall_us', 'on_cpu_us', 'ready_us', 'blocked_us')) {
        [double[]]$values = @($operationRows | ForEach-Object { [double]$_.$property })
        $metric[$property] = [ordered]@{
            p50 = Get-NearestRank -Values $values.Clone() -Fraction 0.50
            p95 = Get-NearestRank -Values $values.Clone() -Fraction 0.95
            p99 = Get-NearestRank -Values $values.Clone() -Fraction 0.99
            max = Get-NearestRank -Values $values.Clone() -Fraction 1.00
        }
    }
    return [ordered]@{
        samples = $operationRows.Count
        timing = $metric
        context_switches = [int](($operationRows | Measure-Object context_switches -Sum).Sum)
        preemptions = [int](($operationRows | Measure-Object preemptions -Sum).Sum)
        migrations = [int](($operationRows | Measure-Object migrations -Sum).Sum)
    }
}

$operationNames = @(
    'empty',
    'fixed_compute',
    'fixed_memory',
    'stage_marker_probe',
    'sleep_10ms',
    'yield_256',
    'fixed_compute_under_load',
    'yield_256_under_load')
$operations = [ordered]@{}
foreach ($operation in $operationNames) {
    $operations[$operation] = Get-OperationSummary -Rows $rows -Operation $operation
}

$emptyMedian = [double]$operations.empty.timing.on_cpu_us.p50
$computeMedian = [double]$operations.fixed_compute.timing.on_cpu_us.p50
$memoryMedian = [double]$operations.fixed_memory.timing.on_cpu_us.p50
$sleepWallMedian = [double]$operations.sleep_10ms.timing.wall_us.p50
$sleepCpuMedian = [double]$operations.sleep_10ms.timing.on_cpu_us.p50
$sleepOffMedian = [double]$operations.sleep_10ms.timing.ready_us.p50 +
    [double]$operations.sleep_10ms.timing.blocked_us.p50
$loadedYieldOffMax = [double]$operations.yield_256_under_load.timing.ready_us.max +
    [double]$operations.yield_256_under_load.timing.blocked_us.max
$loadedYieldSwitches = [int]$operations.yield_256_under_load.context_switches
$migrationCount = [int](($rows | Measure-Object migrations -Sum).Sum)

if ($computeMedian -le $emptyMedian + 20.0) {
    throw "Fixed compute did not produce authoritative on-CPU growth: $summaryPath"
}
if ($memoryMedian -le $emptyMedian + 20.0) {
    throw "Fixed memory did not produce authoritative on-CPU growth: $summaryPath"
}
if ($sleepWallMedian -lt 5000.0 -or $sleepCpuMedian -ge 1000.0 -or $sleepOffMedian -lt 5000.0) {
    throw "Sleep calibration did not separate wall from on/off-CPU time: $summaryPath"
}
if ($loadedYieldOffMax -le 0.0 -or $loadedYieldSwitches -le 0) {
    throw "Loaded yield calibration did not expose scheduler/descheduled time: $summaryPath"
}
if ($migrationCount -le 0) {
    throw "ETW calibration did not observe a core migration: $summaryPath"
}

$summary = [ordered]@{
    schema = 1
    label = $Label
    profile = 'GeneralProfile.Verbose.File'
    high_integrity_administrator = $true
    qpc_authority = $true
    marker_rows = $rows.Count
    events_lost = 0
    buffers_lost = 0
    unknown = 0
    timeline_closed = $rows.Count
    instrumentation_stage_probe_on_cpu_us = $operations.stage_marker_probe.timing.on_cpu_us
    calibration_assertions = [ordered]@{
        compute_increases_on_cpu = $true
        memory_increases_on_cpu = $true
        sleep_increases_off_cpu_not_on_cpu = $true
        loaded_yield_exposes_scheduler_time = $true
        migration_observed = $true
        loss_fails_closed = 'covered by SightWeave.M2P4.ETW.SyntheticCorrelation'
        pid_tid_isolation = 'covered by SightWeave.M2P4.ETW.SyntheticCorrelation and ETL lifecycle validation'
    }
    operations = $operations
    capability = $capabilityPath
    trace = $etlPath
    attribution = $analysisCsv
}
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Get-Item -LiteralPath $capabilityPath, $etlPath, $analysisCsv, $summaryPath, $captureReportPath, $analysisReportPath |
    Select-Object FullName, Length, LastWriteTime

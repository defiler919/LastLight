[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$RunCount = 10,

    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$Label = '',

    [ValidateSet('GeneralProfile', 'CPU')]
    [string]$TraceProfile = 'GeneralProfile',

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
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P4\EtwAttribution'
}
$runRoot = Join-Path $OutputRoot $Label
$capabilityPath = Join-Path $runRoot 'capability.json'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing M2P.4 ETW attribution: $runRoot"
}
New-Item -ItemType Directory -Path $runRoot | Out-Null

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

function Assert-AutomationReport {
    param(
        [string]$Path,
        [int]$ExpectedSuccesses,
        [string]$Context
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Context automation report is missing: $Path"
    }
    $report = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or
        $report.inProcess -ne 0 -or
        $report.succeededWithWarnings -ne 0 -or
        $report.succeeded -ne $ExpectedSuccesses) {
        throw "$Context automation report is not exactly $ExpectedSuccesses clean successes: $Path"
    }
}

function Merge-CsvFiles {
    param(
        [System.IO.FileInfo[]]$Files,
        [string]$Destination,
        [int]$ExpectedCount
    )

    if ($Files.Count -ne $ExpectedCount) {
        throw "Expected $ExpectedCount source CSV files for $Destination, found $($Files.Count)."
    }
    $writer = [System.IO.StreamWriter]::new(
        $Destination,
        $false,
        [System.Text.UTF8Encoding]::new($false))
    try {
        $headerWritten = $false
        foreach ($file in $Files) {
            $reader = [System.IO.StreamReader]::new($file.FullName)
            try {
                $header = $reader.ReadLine()
                if (-not $headerWritten) {
                    $writer.WriteLine($header)
                    $headerWritten = $true
                }
                while (-not $reader.EndOfStream) {
                    $line = $reader.ReadLine()
                    if (-not [string]::IsNullOrWhiteSpace($line)) {
                        $writer.WriteLine($line)
                    }
                }
            }
            finally {
                $reader.Dispose()
            }
        }
    }
    finally {
        $writer.Dispose()
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

function Get-MetricSummary {
    param(
        [object[]]$Rows,
        [string]$Property
    )

    [double[]]$values = @($Rows | ForEach-Object { [double]$_.$Property })
    return [ordered]@{
        p50 = Get-NearestRank -Values $values.Clone() -Fraction 0.50
        p95 = Get-NearestRank -Values $values.Clone() -Fraction 0.95
        p99 = Get-NearestRank -Values $values.Clone() -Fraction 0.99
        p99_9 = Get-NearestRank -Values $values.Clone() -Fraction 0.999
        max = Get-NearestRank -Values $values.Clone() -Fraction 1.00
    }
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
    profile = "$TraceProfile.Verbose.File"
    required_kernel_events = @(
        'Process',
        'Thread',
        'CSwitch',
        'ReadyThread',
        'Profile',
        'PageFault')
}
$capability | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $capabilityPath -Encoding utf8NoBOM
if (-not $isAdministrator -or $fltmcExit -ne 0) {
    throw "M2P.4 formal ETW attribution requires a high-integrity administrator token; fail-closed evidence: $capabilityPath"
}

for ($runIndex = 1; $runIndex -le $RunCount; ++$runIndex) {
    $runName = 'run-{0:D2}' -f $runIndex
    $processRoot = Join-Path $runRoot $runName
    $rawRoot = Join-Path $processRoot 'Raw'
    $captureReportRoot = Join-Path $processRoot 'CaptureReport'
    $analysisReportRoot = Join-Path $processRoot 'AnalysisReport'
    $etlPath = Join-Path $processRoot 'scheduling.etl'
    $analysisCsv = Join-Path $processRoot 'attribution.csv'
    New-Item -ItemType Directory -Path $rawRoot | Out-Null

    $captureExit = -1
    $wprStarted = $false
    try {
        & wpr.exe -start $TraceProfile -filemode
        if ($LASTEXITCODE -ne 0) {
            throw "WPR $TraceProfile start failed for $runName with exit code $LASTEXITCODE"
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
            '-SightWeaveM2P3AttributionCapture',
            '-SightWeaveM2P4EtwMarkers',
            "-SightWeaveM2P3Run=$runName",
            "-SightWeaveM2P3Output=$rawRoot",
            '-ExecCmds=Automation RunTests SightWeave.M2P3.Attribution',
            '-TestExit=Automation Test Queue Empty',
            "-ReportExportPath=$captureReportRoot",
            "-AbsLog=$(Join-Path $processRoot 'capture.log')"
        )
        $captureExit = Invoke-UnrealEditorCommandlet -Arguments $captureArguments
    }
    finally {
        if ($wprStarted) {
            & wpr.exe -stop $etlPath
            if ($LASTEXITCODE -ne 0) {
                throw "WPR stop failed for $runName with exit code $LASTEXITCODE"
            }
        }
    }
    if ($captureExit -ne 0) {
        throw "M2P.4 capture $runName failed with exit code $captureExit. See $processRoot"
    }

    $captureReportPath = Join-Path $captureReportRoot 'index.json'
    Assert-AutomationReport -Path $captureReportPath -ExpectedSuccesses 2 -Context "$runName capture"
    foreach ($fileName in @(
        'batch.csv',
        'door.csv',
        'batch-etw-markers.csv',
        'door-etw-markers.csv')) {
        $path = Join-Path $rawRoot $fileName
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "$runName raw output is missing: $path"
        }
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
        "-AbsLog=$(Join-Path $processRoot 'analysis.log')"
    )
    $analysisExit = Invoke-UnrealEditorCommandlet -Arguments $analysisArguments
    if ($analysisExit -ne 0) {
        throw "M2P.4 analysis $runName failed with exit code $analysisExit. See $processRoot"
    }
    $analysisReportPath = Join-Path $analysisReportRoot 'index.json'
    Assert-AutomationReport -Path $analysisReportPath -ExpectedSuccesses 1 -Context "$runName analysis"

    $analysisRows = @(Import-Csv -LiteralPath $analysisCsv)
    if (@($analysisRows | Where-Object { $_.timeline_closed -ne '1' }).Count -ne 0) {
        throw "$runName contains unclosed ETW timelines: $analysisCsv"
    }
    if (@($analysisRows | Where-Object {
        $_.events_lost -ne '0' -or $_.buffers_lost -ne '0'
    }).Count -ne 0) {
        throw "$runName contains lost ETW events or buffers: $analysisCsv"
    }
    if (@($analysisRows | Where-Object classification -eq 'Unknown').Count -ne 0) {
        throw "$runName contains Unknown ETW attribution: $analysisCsv"
    }
    $totalRows = @($analysisRows | Where-Object scope -eq 'total')
    $batchTotalRows = @($totalRows | Where-Object operation -eq 'batch_512')
    $doorTotalRows = @($totalRows | Where-Object operation -like 'dynamic_door_*')
    if ($batchTotalRows.Count -ne 1010) {
        throw "$runName expected 1,010 Batch totals, found $($batchTotalRows.Count)."
    }
    if ($doorTotalRows.Count -ne 314) {
        throw "$runName expected 314 Dynamic Door totals, found $($doorTotalRows.Count)."
    }

    $manifest = [ordered]@{
        run = $runName
        process_ids = @($totalRows | Select-Object -ExpandProperty pid -Unique)
        batch_total_samples = $batchTotalRows.Count
        door_total_samples = $doorTotalRows.Count
        marker_rows = $analysisRows.Count
        timeline_closed = $analysisRows.Count
        events_lost = 0
        buffers_lost = 0
        unknown = 0
        trace_bytes = (Get-Item -LiteralPath $etlPath).Length
        trace = $etlPath
        attribution = $analysisCsv
        capture_report = $captureReportPath
        analysis_report = $analysisReportPath
    }
    $manifest | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $processRoot 'manifest.json') -Encoding utf8NoBOM
}

$batchFiles = @(Get-ChildItem -LiteralPath $runRoot -Recurse -Filter 'batch.csv' |
    Sort-Object FullName)
$doorFiles = @(Get-ChildItem -LiteralPath $runRoot -Recurse -Filter 'door.csv' |
    Sort-Object FullName)
$attributionFiles = @(Get-ChildItem -LiteralPath $runRoot -Recurse -Filter 'attribution.csv' |
    Sort-Object FullName)
$combinedBatchPath = Join-Path $runRoot 'batch-all.csv'
$combinedDoorPath = Join-Path $runRoot 'door-all.csv'
$combinedAttributionPath = Join-Path $runRoot 'attribution-all.csv'
Merge-CsvFiles -Files $batchFiles -Destination $combinedBatchPath -ExpectedCount $RunCount
Merge-CsvFiles -Files $doorFiles -Destination $combinedDoorPath -ExpectedCount $RunCount
Merge-CsvFiles -Files $attributionFiles -Destination $combinedAttributionPath -ExpectedCount $RunCount

$batchRows = @(Import-Csv -LiteralPath $combinedBatchPath)
$doorRows = @(Import-Csv -LiteralPath $combinedDoorPath)
$attributionRows = @(Import-Csv -LiteralPath $combinedAttributionPath)
$expectedBatchRows = $RunCount * 10 * 101
$expectedDoorRows = $RunCount * (101 * 3 + 11)
if ($batchRows.Count -ne $expectedBatchRows) {
    throw "Expected $expectedBatchRows Batch rows, found $($batchRows.Count)."
}
if ($doorRows.Count -ne $expectedDoorRows) {
    throw "Expected $expectedDoorRows door rows, found $($doorRows.Count)."
}
$distributionCount = @($batchRows | Select-Object run, distribution -Unique).Count
if ($distributionCount -ne $RunCount * 10) {
    throw "Expected $($RunCount * 10) independent Batch distributions, found $distributionCount."
}
if (@($attributionRows | Where-Object {
    $_.timeline_closed -ne '1' -or
    $_.events_lost -ne '0' -or
    $_.buffers_lost -ne '0' -or
    $_.classification -eq 'Unknown'
}).Count -ne 0) {
    throw "Combined ETW attribution failed the loss/closure/Unknown hard gate: $combinedAttributionPath"
}

$totalAttributionRows = @($attributionRows | Where-Object scope -eq 'total')
$workloads = @()
foreach ($group in ($totalAttributionRows | Group-Object operation)) {
    $classification = [ordered]@{}
    foreach ($classGroup in ($group.Group | Group-Object classification)) {
        $classification[$classGroup.Name] = $classGroup.Count
    }
    $workloads += [ordered]@{
        operation = $group.Name
        samples = $group.Count
        wall_us = Get-MetricSummary -Rows $group.Group -Property 'wall_us'
        on_cpu_us = Get-MetricSummary -Rows $group.Group -Property 'on_cpu_us'
        ready_us = Get-MetricSummary -Rows $group.Group -Property 'ready_us'
        blocked_us = Get-MetricSummary -Rows $group.Group -Property 'blocked_us'
        context_switches = [int](($group.Group | Measure-Object context_switches -Sum).Sum)
        preemptions = [int](($group.Group | Measure-Object preemptions -Sum).Sum)
        migrations = [int](($group.Group | Measure-Object migrations -Sum).Sum)
        classifications = $classification
    }
}

$stageWorkloads = @()
foreach ($group in ($attributionRows |
    Where-Object scope -eq 'stage' |
    Group-Object operation, stage)) {
    $first = $group.Group[0]
    $stageWorkloads += [ordered]@{
        operation = $first.operation
        stage = $first.stage
        invocations = $group.Count
        wall_us = Get-MetricSummary -Rows $group.Group -Property 'wall_us'
        on_cpu_us = Get-MetricSummary -Rows $group.Group -Property 'on_cpu_us'
    }
}

$summary = [ordered]@{
    schema = 1
    label = $Label
    elevated_processes = $RunCount
    trace_profile = "$TraceProfile.Verbose.File"
    affinity_or_priority_modified = $false
    batch_distributions = $distributionCount
    batch_total_samples = @($totalAttributionRows | Where-Object operation -eq 'batch_512').Count
    door_total_samples = @($totalAttributionRows | Where-Object operation -like 'dynamic_door_*').Count
    attribution_marker_rows = $attributionRows.Count
    events_lost = 0
    buffers_lost = 0
    unknown = 0
    workloads = $workloads
    stages = $stageWorkloads
    capability = $capabilityPath
    raw_batch = $combinedBatchPath
    raw_door = $combinedDoorPath
    attribution = $combinedAttributionPath
}
$summaryPath = Join-Path $runRoot 'summary.json'
$summary | ConvertTo-Json -Depth 10 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Get-Item -LiteralPath $capabilityPath, $combinedBatchPath, $combinedDoorPath, $combinedAttributionPath, $summaryPath |
    Select-Object FullName, Length, LastWriteTime

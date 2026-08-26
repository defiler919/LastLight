[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$RunCount = 10,

    [ValidateRange(1, 20)]
    [int]$CalibrationPairCount = 5,

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
    $Label = 'vision-tail-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P5\VisionTailAttribution'
}
$runRoot = Join-Path $OutputRoot $Label
$calibrationRoot = Join-Path $runRoot 'Calibration'
$formalRoot = Join-Path $runRoot 'Formal'
$capabilityPath = Join-Path $runRoot 'capability.json'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing M2P.5 vision-tail attribution: $runRoot"
}
New-Item -ItemType Directory -Path $calibrationRoot, $formalRoot | Out-Null

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
        max = Get-NearestRank -Values $values.Clone() -Fraction 1.00
    }
}

function New-CaptureArguments {
    param(
        [string]$ProcessRoot,
        [string]$RunName,
        [bool]$Detailed
    )

    $rawRoot = Join-Path $ProcessRoot 'Raw'
    $reportRoot = Join-Path $ProcessRoot 'CaptureReport'
    New-Item -ItemType Directory -Path $rawRoot | Out-Null
    $arguments = @(
        $projectFile,
        '-unattended',
        '-nop4',
        '-nosplash',
        '-NullRHI',
        '-NoSound',
        '-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0',
        '-SightWeaveM2P3AttributionCapture',
        '-SightWeaveM2P5BroadDoorOnly',
        "-SightWeaveM2P3Run=$RunName",
        "-SightWeaveM2P3Output=$rawRoot",
        '-ExecCmds=Automation RunTests SightWeave.M2P3.Attribution.DynamicDoor',
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$reportRoot",
        "-AbsLog=$(Join-Path $ProcessRoot 'capture.log')"
    )
    if ($Detailed) {
        $arguments += @(
            '-SightWeaveM2P4EtwMarkers',
            '-SightWeaveM2P3DetailedStages',
            '-SightWeaveM2P5DetailedVisionStages',
            '-SightWeaveM2P5LightweightStageTimers'
        )
    }
    return ,$arguments
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
    profile = "$TraceProfile.Verbose.File"
    required_kernel_events = @('Process', 'Thread', 'CSwitch', 'ReadyThread')
    affinity_or_priority_modified = $false
    defender_or_security_services_modified = $false
}
$capability | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $capabilityPath -Encoding utf8NoBOM
if (-not $isAdministrator -or $fltmcExit -ne 0 -or $integrityLine -notmatch 'High Mandatory Level') {
    throw "M2P.5 attribution requires a high-integrity administrator token; fail-closed evidence: $capabilityPath"
}

$calibrationRows = @()
for ($pairIndex = 1; $pairIndex -le $CalibrationPairCount; ++$pairIndex) {
    foreach ($mode in @('control', 'detailed')) {
        $runName = 'cal-{0:D2}-{1}' -f $pairIndex, $mode
        $processRoot = Join-Path $calibrationRoot $runName
        New-Item -ItemType Directory -Path $processRoot | Out-Null
        $arguments = New-CaptureArguments `
            -ProcessRoot $processRoot `
            -RunName $runName `
            -Detailed ($mode -eq 'detailed')
        $exitCode = Invoke-UnrealEditorCommandlet -Arguments $arguments
        if ($exitCode -ne 0) {
            throw "M2P.5 calibration $runName failed with exit code $exitCode."
        }
        Assert-AutomationReport `
            -Path (Join-Path $processRoot 'CaptureReport\index.json') `
            -ExpectedSuccesses 1 `
            -Context $runName
        $doorPath = Join-Path $processRoot 'Raw\door.csv'
        $rows = @(Import-Csv -LiteralPath $doorPath)
        if ($rows.Count -ne 101 -or
            @($rows | Where-Object workload -ne 'dynamic_door_broad_4v2l').Count -ne 0) {
            throw "$runName calibration did not produce exactly 101 broad-door samples."
        }
        foreach ($row in $rows) {
            $calibrationRows += [pscustomobject]@{
                pair = $pairIndex
                mode = $mode
                sample = [int]$row.sample
                wall_us = [double]$row.wall_us
            }
        }
    }
}
$calibrationCsvPath = Join-Path $calibrationRoot 'marker-overhead.csv'
$calibrationRows | Export-Csv -LiteralPath $calibrationCsvPath -NoTypeInformation -Encoding utf8NoBOM
$controlCalibration = @($calibrationRows | Where-Object mode -eq 'control')
$detailedCalibration = @($calibrationRows | Where-Object mode -eq 'detailed')
$controlSummary = Get-MetricSummary -Rows $controlCalibration -Property 'wall_us'
$detailedSummary = Get-MetricSummary -Rows $detailedCalibration -Property 'wall_us'
$calibrationSummary = [ordered]@{
    schema = 1
    independent_process_pairs = $CalibrationPairCount
    samples_per_mode = $CalibrationPairCount * 101
    control_wall_us = $controlSummary
    detailed_wall_us = $detailedSummary
    detailed_minus_control_us = [ordered]@{
        p50 = [double]$detailedSummary.p50 - [double]$controlSummary.p50
        p95 = [double]$detailedSummary.p95 - [double]$controlSummary.p95
        p99 = [double]$detailedSummary.p99 - [double]$controlSummary.p99
        max = [double]$detailedSummary.max - [double]$controlSummary.max
    }
    timers = 'QPC/platform-cycle lightweight begin/end markers; no per-ray micro timers in formal capture'
    interpretation = 'Calibration quantifies diagnostic perturbation only. No subtraction is applied to ETW authority.'
}
$calibrationSummaryPath = Join-Path $calibrationRoot 'marker-overhead-summary.json'
$calibrationSummary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $calibrationSummaryPath -Encoding utf8NoBOM

for ($runIndex = 1; $runIndex -le $RunCount; ++$runIndex) {
    $runName = 'run-{0:D2}' -f $runIndex
    $processRoot = Join-Path $formalRoot $runName
    $rawRoot = Join-Path $processRoot 'Raw'
    $etlPath = Join-Path $processRoot 'scheduling.etl'
    $analysisCsv = Join-Path $processRoot 'attribution.csv'
    $analysisReportRoot = Join-Path $processRoot 'AnalysisReport'
    New-Item -ItemType Directory -Path $processRoot | Out-Null

    $captureExit = -1
    $wprStarted = $false
    try {
        & wpr.exe -start $TraceProfile -filemode
        if ($LASTEXITCODE -ne 0) {
            throw "WPR $TraceProfile start failed for $runName with exit code $LASTEXITCODE"
        }
        $wprStarted = $true
        $captureArguments = New-CaptureArguments `
            -ProcessRoot $processRoot `
            -RunName $runName `
            -Detailed $true
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
        throw "M2P.5 formal capture $runName failed with exit code $captureExit."
    }
    Assert-AutomationReport `
        -Path (Join-Path $processRoot 'CaptureReport\index.json') `
        -ExpectedSuccesses 1 `
        -Context "$runName capture"
    foreach ($fileName in @('door.csv', 'door-etw-markers.csv', 'door-vision-detail.csv')) {
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
        throw "M2P.5 analysis $runName failed with exit code $analysisExit."
    }
    Assert-AutomationReport `
        -Path (Join-Path $analysisReportRoot 'index.json') `
        -ExpectedSuccesses 1 `
        -Context "$runName analysis"

    $analysisRows = @(Import-Csv -LiteralPath $analysisCsv)
    if (@($analysisRows | Where-Object {
        $_.timeline_closed -ne '1' -or
        $_.events_lost -ne '0' -or
        $_.buffers_lost -ne '0' -or
        $_.classification -eq 'Unknown'
    }).Count -ne 0) {
        throw "$runName fails ETW loss/closure/ownership/Unknown requirements: $analysisCsv"
    }
    $totals = @($analysisRows | Where-Object {
        $_.scope -eq 'total' -and $_.operation -eq 'dynamic_door_broad_4v2l'
    })
    if ($totals.Count -ne 101) {
        throw "$runName expected 101 broad-door ETW totals, found $($totals.Count)."
    }
    $details = @(Import-Csv -LiteralPath (Join-Path $rawRoot 'door-vision-detail.csv'))
    if ($details.Count -ne 6565 -or
        @($details | Where-Object source_diagnostic_overflow_count -ne '0').Count -ne 0) {
        throw "$runName vision detail cardinality/overflow check failed."
    }

    $manifest = [ordered]@{
        run = $runName
        process_ids = @($totals | Select-Object -ExpandProperty pid -Unique)
        broad_total_samples = $totals.Count
        detail_rows = $details.Count
        marker_rows = $analysisRows.Count
        events_lost = 0
        buffers_lost = 0
        ownership_conflicts = 0
        unclosed_timelines = 0
        unknown = 0
        trace_bytes = (Get-Item -LiteralPath $etlPath).Length
        trace = $etlPath
        attribution = $analysisCsv
    }
    if ($manifest.process_ids.Count -ne 1) {
        throw "$runName does not contain exactly one capture PID."
    }
    $manifest | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $processRoot 'manifest.json') -Encoding utf8NoBOM
}

$doorFiles = @(Get-ChildItem -LiteralPath $formalRoot -Recurse -Filter 'door.csv' | Sort-Object FullName)
$detailFiles = @(Get-ChildItem -LiteralPath $formalRoot -Recurse -Filter 'door-vision-detail.csv' | Sort-Object FullName)
$attributionFiles = @(Get-ChildItem -LiteralPath $formalRoot -Recurse -Filter 'attribution.csv' | Sort-Object FullName)
$combinedDoorPath = Join-Path $formalRoot 'door-all.csv'
$combinedDetailPath = Join-Path $formalRoot 'door-vision-detail-all.csv'
$combinedAttributionPath = Join-Path $formalRoot 'attribution-all.csv'
Merge-CsvFiles -Files $doorFiles -Destination $combinedDoorPath -ExpectedCount $RunCount
Merge-CsvFiles -Files $detailFiles -Destination $combinedDetailPath -ExpectedCount $RunCount
Merge-CsvFiles -Files $attributionFiles -Destination $combinedAttributionPath -ExpectedCount $RunCount

$doorRows = @(Import-Csv -LiteralPath $combinedDoorPath)
$detailRows = @(Import-Csv -LiteralPath $combinedDetailPath)
$attributionRows = @(Import-Csv -LiteralPath $combinedAttributionPath)
if ($doorRows.Count -ne $RunCount * 101 -or
    $detailRows.Count -ne $RunCount * 6565) {
    throw 'Combined broad-door/detail sample count is not exact.'
}
$totalRows = @($attributionRows | Where-Object {
    $_.scope -eq 'total' -and $_.operation -eq 'dynamic_door_broad_4v2l'
})
$capturePids = @($totalRows | Select-Object -ExpandProperty pid -Unique)
if ($totalRows.Count -ne $RunCount * 101 -or $capturePids.Count -ne $RunCount) {
    throw 'Combined ETW authority does not contain the required independent PIDs/samples.'
}
if (@($attributionRows | Where-Object {
    $_.timeline_closed -ne '1' -or
    $_.events_lost -ne '0' -or
    $_.buffers_lost -ne '0' -or
    $_.classification -eq 'Unknown'
}).Count -ne 0) {
    throw 'Combined ETW authority failed closed.'
}

$stageAttribution = @{}
foreach ($row in @($attributionRows | Where-Object scope -eq 'stage')) {
    $key = "$($row.run)|$($row.pid)|$($row.sample_id)|$($row.stage)|$($row.invocation)"
    if ($stageAttribution.ContainsKey($key)) {
        throw "Duplicate stage attribution key: $key"
    }
    $stageAttribution[$key] = $row
}
$rayInvocationBySource = @{}
foreach ($row in @($detailRows | Where-Object {
    $_.stage -eq 'ray_sweep' -and $_.etw_marker_available -eq '1'
})) {
    $key = "$($row.run)|$($row.pid)|$($row.sample_id)|$($row.source_id)"
    $rayInvocationBySource[$key] = $row.etw_marker_invocation
}
$attributedDetails = @()
foreach ($row in $detailRows) {
    $exact = $null
    if ($row.etw_marker_available -eq '1') {
        $key = "$($row.run)|$($row.pid)|$($row.sample_id)|$($row.stage)|$($row.etw_marker_invocation)"
        $exact = $stageAttribution[$key]
        if ($null -eq $exact) {
            throw "Missing exact ETW attribution for detail key: $key"
        }
    }
    $parent = $null
    if ($null -eq $exact -and $row.source_id -ne '0') {
        $sourceKey = "$($row.run)|$($row.pid)|$($row.sample_id)|$($row.source_id)"
        $parentInvocation = $rayInvocationBySource[$sourceKey]
        if ($null -ne $parentInvocation) {
            $parentKey = "$($row.run)|$($row.pid)|$($row.sample_id)|ray_sweep|$parentInvocation"
            $parent = $stageAttribution[$parentKey]
        }
    }
    $copy = $row.PSObject.Copy()
    $copy | Add-Member -NotePropertyName etw_on_cpu_us -NotePropertyValue $(
        if ($null -ne $exact) { $exact.on_cpu_us } else { '' })
    $copy | Add-Member -NotePropertyName etw_ready_us -NotePropertyValue $(
        if ($null -ne $exact) { $exact.ready_us } else { '' })
    $copy | Add-Member -NotePropertyName etw_blocked_us -NotePropertyValue $(
        if ($null -ne $exact) { $exact.blocked_us } else { '' })
    $copy | Add-Member -NotePropertyName etw_preemptions -NotePropertyValue $(
        if ($null -ne $exact) { $exact.preemptions } else { '' })
    $copy | Add-Member -NotePropertyName etw_migrations -NotePropertyValue $(
        if ($null -ne $exact) { $exact.migrations } else { '' })
    $copy | Add-Member -NotePropertyName parent_ray_sweep_on_cpu_us -NotePropertyValue $(
        if ($null -ne $parent) { $parent.on_cpu_us } else { '' })
    $copy | Add-Member -NotePropertyName etw_attribution_scope -NotePropertyValue $(
        if ($null -ne $exact) { 'exact_substage' }
        elseif ($null -ne $parent) { 'parent_ray_sweep_only' }
        else { 'not_applicable' })
    $attributedDetails += $copy
}
$attributedDetailPath = Join-Path $formalRoot 'door-vision-detail-attributed.csv'
$attributedDetails | Export-Csv -LiteralPath $attributedDetailPath -NoTypeInformation -Encoding utf8NoBOM

$classifications = [ordered]@{
    'Within budget' = 0
    'Plugin CPU' = 0
    'Scheduler/Preemption' = 0
    'GPU/Driver' = 0
    'Unknown' = 0
}
foreach ($row in $totalRows) {
    if ([double]$row.wall_us -le 250.0) {
        ++$classifications['Within budget']
    }
    elseif ([double]$row.on_cpu_us -gt 250.0) {
        ++$classifications['Plugin CPU']
    }
    elseif ([double]$row.ready_us + [double]$row.blocked_us -gt 0.0) {
        ++$classifications['Scheduler/Preemption']
    }
    else {
        ++$classifications['Unknown']
    }
}
if ($classifications['Unknown'] -ne 0) {
    throw 'Final M2P.5 phase-one classification contains Unknown samples.'
}

$stageSummaries = @()
foreach ($group in ($attributionRows | Where-Object scope -eq 'stage' | Group-Object stage)) {
    $stageSummaries += [ordered]@{
        stage = $group.Name
        invocations = $group.Count
        wall_us = Get-MetricSummary -Rows $group.Group -Property 'wall_us'
        on_cpu_us = Get-MetricSummary -Rows $group.Group -Property 'on_cpu_us'
        ready_us = Get-MetricSummary -Rows $group.Group -Property 'ready_us'
        blocked_us = Get-MetricSummary -Rows $group.Group -Property 'blocked_us'
    }
}
$summary = [ordered]@{
    schema = 1
    label = $Label
    purpose = 'M2P.5 pre-production-change fine-grained broad-door intrinsic attribution'
    elevated_processes = $RunCount
    unique_capture_pids = $capturePids.Count
    broad_total_samples = $totalRows.Count
    detail_rows = $detailRows.Count
    trace_profile = "$TraceProfile.Verbose.File"
    event_loss = 0
    buffer_loss = 0
    ownership_conflicts = 0
    unclosed_timelines = 0
    unknown = 0
    broad_wall_us = Get-MetricSummary -Rows $totalRows -Property 'wall_us'
    broad_on_cpu_us = Get-MetricSummary -Rows $totalRows -Property 'on_cpu_us'
    broad_ready_us = Get-MetricSummary -Rows $totalRows -Property 'ready_us'
    broad_blocked_us = Get-MetricSummary -Rows $totalRows -Property 'blocked_us'
    context_switches = [int](($totalRows | Measure-Object context_switches -Sum).Sum)
    preemptions = [int](($totalRows | Measure-Object preemptions -Sum).Sum)
    migrations = [int](($totalRows | Measure-Object migrations -Sum).Sum)
    classifications = $classifications
    exact_substage_etw_rows = @($attributedDetails | Where-Object etw_attribution_scope -eq 'exact_substage').Count
    parent_ray_sweep_only_rows = @($attributedDetails | Where-Object etw_attribution_scope -eq 'parent_ray_sweep_only').Count
    micro_stage_policy = 'Micro-stage wall/platform cycles remain diagnostic only; only exact substage or parent ray_sweep CSwitch ETW is reported as on-CPU.'
    marker_overhead = $calibrationSummary
    stages = $stageSummaries
    capability = $capabilityPath
    calibration = $calibrationSummaryPath
    raw_door = $combinedDoorPath
    raw_detail = $combinedDetailPath
    attribution = $combinedAttributionPath
    attributed_detail = $attributedDetailPath
}
$summaryPath = Join-Path $runRoot 'summary.json'
$summary | ConvertTo-Json -Depth 12 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Get-Item -LiteralPath `
    $capabilityPath, `
    $calibrationSummaryPath, `
    $combinedDoorPath, `
    $combinedAttributionPath, `
    $attributedDetailPath, `
    $summaryPath |
    Select-Object FullName, Length, LastWriteTime

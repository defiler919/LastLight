[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$AttributionRoot,

    [Parameter(Mandatory)]
    [string]$NullRhiSoakRoot,

    [Parameter(Mandatory)]
    [string]$RenderedSoakRoot,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RequiredPath {
    param(
        [string]$Path,
        [string]$Description,
        [ValidateSet('Leaf', 'Container')]
        [string]$PathType
    )

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved.Path -PathType $PathType)) {
        throw "$Description is not a $PathType path: $Path"
    }
    return $resolved.Path
}

function Get-NearestRank {
    param(
        [double[]]$Values,
        [ValidateRange(0.0, 1.0)]
        [double]$Fraction
    )

    if ($Values.Count -eq 0) {
        return 0.0
    }
    [Array]::Sort($Values)
    $index = [Math]::Max(
        0,
        [Math]::Min($Values.Count - 1, [Math]::Ceiling($Fraction * $Values.Count) - 1))
    return $Values[$index]
}

function Get-Distribution {
    param([object[]]$Rows)

    [double[]]$wall = @($Rows | ForEach-Object { [double]$_.wall_us })
    return [ordered]@{
        p50 = Get-NearestRank -Values $wall.Clone() -Fraction 0.50
        p95 = Get-NearestRank -Values $wall.Clone() -Fraction 0.95
        p99 = Get-NearestRank -Values $wall.Clone() -Fraction 0.99
        max = Get-NearestRank -Values $wall.Clone() -Fraction 1.00
    }
}

function Get-ClassificationCount {
    param(
        [object[]]$Rows,
        [string]$Name
    )

    return @($Rows | Where-Object { $_.classification -eq $Name }).Count
}

function Read-Soak {
    param(
        [string]$Root,
        [string]$ExpectedMode
    )

    $summaryPath = Resolve-RequiredPath `
        -Path (Join-Path $Root 'Raw\summary.json') `
        -Description "$ExpectedMode soak summary" `
        -PathType Leaf
    $reportPath = Resolve-RequiredPath `
        -Path (Join-Path $Root 'Report\index.json') `
        -Description "$ExpectedMode soak report" `
        -PathType Leaf
    $csvPath = Resolve-RequiredPath `
        -Path (Join-Path $Root 'Raw\frames.csv') `
        -Description "$ExpectedMode soak frame CSV" `
        -PathType Leaf
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    $frameRows = @(Import-Csv -LiteralPath $csvPath)
    $reportMode = if ($report.devices.Count -eq 1) { $report.devices[0].rHI } else { '' }
    $cleanReport = $report.succeeded -eq 1 `
        -and $report.succeededWithWarnings -eq 0 `
        -and $report.failed -eq 0 `
        -and $report.notRun -eq 0 `
        -and $report.inProcess -eq 0
    $renderEnvironmentValid = if ($ExpectedMode -eq 'rendered') {
        $reportMode -like 'D3D12*'
    }
    else {
        [string]::IsNullOrWhiteSpace($reportMode)
    }
    $hardGate = $cleanReport `
        -and $summary.mode -eq $ExpectedMode `
        -and $summary.frames -eq 36000 `
        -and $summary.simulated_seconds -ge 600.0 `
        -and $frameRows.Count -eq 36000 `
        -and $summary.correctness_failures -eq 0 `
        -and $summary.capacity_growth_bytes_max -eq 0 `
        -and $summary.prepared.hits -gt 0 `
        -and $summary.prepared.rebuilds -gt 0 `
        -and $summary.max_consecutive_above_slow_threshold -le 60 `
        -and $renderEnvironmentValid

    return [ordered]@{
        mode = $ExpectedMode
        hard_gate_passed = $hardGate
        clean_automation_report = $cleanReport
        rendered_rhi = $reportMode
        frames = $summary.frames
        simulated_seconds = $summary.simulated_seconds
        wall_us = $summary.wall_us
        thread_cycles = $summary.thread_cycles
        max_consecutive_above_p99 = $summary.max_consecutive_above_p99
        max_consecutive_above_1ms = $summary.max_consecutive_above_slow_threshold
        classifications = $summary.classifications
        capacity_growth_bytes_max = $summary.capacity_growth_bytes_max
        correctness_failures = $summary.correctness_failures
        summary = $summaryPath
        frames_csv = $csvPath
        report = $reportPath
    }
}

$attributionRoot = Resolve-RequiredPath `
    -Path $AttributionRoot `
    -Description 'Attribution root' `
    -PathType Container
$nullRhiSoakRoot = Resolve-RequiredPath `
    -Path $NullRhiSoakRoot `
    -Description 'NullRHI soak root' `
    -PathType Container
$renderedSoakRoot = Resolve-RequiredPath `
    -Path $RenderedSoakRoot `
    -Description 'Rendered soak root' `
    -PathType Container

$attributionSummaryPath = Resolve-RequiredPath `
    -Path (Join-Path $attributionRoot 'summary.json') `
    -Description 'Attribution summary' `
    -PathType Leaf
$batchPath = Resolve-RequiredPath `
    -Path (Join-Path $attributionRoot 'batch-all.csv') `
    -Description 'Merged Batch attribution CSV' `
    -PathType Leaf
$doorPath = Resolve-RequiredPath `
    -Path (Join-Path $attributionRoot 'door-all.csv') `
    -Description 'Merged door attribution CSV' `
    -PathType Leaf

$attributionSummary = Get-Content -LiteralPath $attributionSummaryPath -Raw | ConvertFrom-Json
$batchRows = @(Import-Csv -LiteralPath $batchPath)
$doorRows = @(Import-Csv -LiteralPath $doorPath)
$batchGroups = @($batchRows | Group-Object run, distribution)
$batchDistributions = @($batchGroups | ForEach-Object {
    $distribution = Get-Distribution -Rows $_.Group
    [ordered]@{
        id = $_.Name
        p50 = $distribution.p50
        p95 = $distribution.p95
        p99 = $distribution.p99
        max = $distribution.max
        wall_gate_passed = $distribution.p50 -le 150.0 `
            -and $distribution.p95 -le 180.0 `
            -and $distribution.p99 -le 200.0
    }
})
$batchWallPassCount = @($batchDistributions | Where-Object { $_.wall_gate_passed }).Count
$batchPluginOverruns = Get-ClassificationCount -Rows $batchRows -Name 'Plugin CPU overrun'
$batchUnknown = Get-ClassificationCount -Rows $batchRows -Name 'Unknown'
$batchNonZeroThreadTime = @($batchRows | Where-Object { [double]$_.thread_cpu_us -gt 0.0 }).Count
$batchEvidenceComplete = $attributionSummary.ordinary_processes -eq 10 `
    -and -not $attributionSummary.affinity_or_priority_modified `
    -and $attributionSummary.batch_distributions -eq 100 `
    -and $attributionSummary.batch_rows -eq 10100 `
    -and $batchRows.Count -eq 10100 `
    -and $batchDistributions.Count -eq 100

$doorRuns = @($doorRows | Group-Object workload, run | ForEach-Object {
    $distribution = Get-Distribution -Rows $_.Group
    [ordered]@{
        id = $_.Name
        workload = $_.Group[0].workload
        p50 = $distribution.p50
        p95 = $distribution.p95
        p99 = $distribution.p99
        max = $distribution.max
        wall_gate_passed = $distribution.p50 -lt 250.0 -and $distribution.p99 -lt 250.0
    }
})
$broadDoorRows = @($doorRows | Where-Object { $_.workload -eq 'dynamic_door_broad_4v2l' })
$broadDoorRuns = @($doorRuns | Where-Object { $_.workload -eq 'dynamic_door_broad_4v2l' })
$dedicatedDoorRuns = @($doorRuns | Where-Object { $_.workload -eq 'dynamic_door_dedicated' })
$broadDoorPluginOverruns = Get-ClassificationCount -Rows $broadDoorRows -Name 'Plugin CPU overrun'
$broadDoorUnknown = Get-ClassificationCount -Rows $broadDoorRows -Name 'Unknown'
$broadDoorNonZeroThreadTime = @($broadDoorRows | Where-Object { [double]$_.thread_cpu_us -gt 0.0 }).Count
$doorEvidenceComplete = $attributionSummary.ordinary_processes -eq 10 `
    -and -not $attributionSummary.affinity_or_priority_modified `
    -and $attributionSummary.door_rows -eq 3140 `
    -and $doorRows.Count -eq 3140 `
    -and $broadDoorRuns.Count -eq 10 `
    -and $dedicatedDoorRuns.Count -eq 10

$nullRhiSoak = Read-Soak -Root $nullRhiSoakRoot -ExpectedMode 'nullrhi'
$renderedSoak = Read-Soak -Root $renderedSoakRoot -ExpectedMode 'rendered'

# GetThreadTimes advances in a 15.625 ms quantum on this host and raw cycle
# deltas cannot legally be converted to elapsed microseconds. Until an ETW
# ContextSwitch trace supplies running intervals, this audit must fail closed:
# cycles remain attribution evidence, not an intrinsic-microsecond gate.
$intrinsicCpuAuthorityAvailable = $batchNonZeroThreadTime -eq $batchRows.Count `
    -and $broadDoorNonZeroThreadTime -eq $broadDoorRows.Count `
    -and $attributionSummary.context_switch_etw -eq 'available with complete running intervals'
$classificationGatePassed = $batchPluginOverruns -eq 0 `
    -and $batchUnknown -eq 0 `
    -and $broadDoorPluginOverruns -eq 0 `
    -and $broadDoorUnknown -eq 0
$legacyWallGatePassed = $batchWallPassCount -eq 100 `
    -and @($broadDoorRuns | Where-Object { $_.wall_gate_passed }).Count -eq 10
$frameSoakGatePassed = $nullRhiSoak.hard_gate_passed -and $renderedSoak.hard_gate_passed
$complete = $batchEvidenceComplete `
    -and $doorEvidenceComplete `
    -and $intrinsicCpuAuthorityAvailable `
    -and $classificationGatePassed `
    -and $legacyWallGatePassed `
    -and $frameSoakGatePassed

$report = [ordered]@{
    schema = 1
    status = if ($complete) { 'COMPLETED' } else { 'PARTIAL' }
    fail_closed = $true
    legacy_wall_assertions_relaxed_or_removed = $false
    priority_or_affinity_modified = $false
    intrinsic_cpu = [ordered]@{
        authoritative_gate_available = $intrinsicCpuAuthorityAvailable
        gate_passed = $intrinsicCpuAuthorityAvailable -and $classificationGatePassed
        unit = 'unavailable; raw cycles are not converted to microseconds'
        get_thread_times_nonzero_batch_samples = $batchNonZeroThreadTime
        get_thread_times_nonzero_broad_door_samples = $broadDoorNonZeroThreadTime
        context_switch_etw = $attributionSummary.context_switch_etw
        reason = 'Per-sample GetThreadTimes resolution is insufficient and no ContextSwitch ETW running intervals are present.'
    }
    batch_512 = [ordered]@{
        evidence_complete = $batchEvidenceComplete
        ordinary_processes = $attributionSummary.ordinary_processes
        distributions = $batchDistributions.Count
        rows = $batchRows.Count
        wall_gate_passed_distributions = $batchWallPassCount
        wall_gate_failed_distributions = $batchDistributions.Count - $batchWallPassCount
        plugin_cpu_overrun_samples = $batchPluginOverruns
        unknown_samples = $batchUnknown
        distributions_detail = $batchDistributions
        raw_csv = $batchPath
    }
    dynamic_door = [ordered]@{
        evidence_complete = $doorEvidenceComplete
        ordinary_processes = $attributionSummary.ordinary_processes
        rows = $doorRows.Count
        broad_wall_gate_passed_processes = @($broadDoorRuns | Where-Object { $_.wall_gate_passed }).Count
        broad_wall_gate_failed_processes = @($broadDoorRuns | Where-Object { -not $_.wall_gate_passed }).Count
        dedicated_wall_gate_passed_processes = @($dedicatedDoorRuns | Where-Object { $_.wall_gate_passed }).Count
        broad_plugin_cpu_overrun_samples = $broadDoorPluginOverruns
        broad_unknown_samples = $broadDoorUnknown
        process_detail = $doorRuns
        raw_csv = $doorPath
    }
    frame_soak = [ordered]@{
        gate_passed = $frameSoakGatePassed
        nullrhi = $nullRhiSoak
        rendered = $renderedSoak
    }
    gates = [ordered]@{
        evidence_counts = $batchEvidenceComplete -and $doorEvidenceComplete
        intrinsic_cpu_authority = $intrinsicCpuAuthorityAvailable
        no_plugin_or_unknown_overruns = $classificationGatePassed
        unchanged_legacy_wall = $legacyWallGatePassed
        frame_soak = $frameSoakGatePassed
    }
    attribution_summary = $attributionSummaryPath
}

if (Test-Path -LiteralPath $OutputPath) {
    throw "Refusing to overwrite an existing performance-contract report: $OutputPath"
}
$outputDirectory = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
Get-Item -LiteralPath $OutputPath | Select-Object FullName, Length, LastWriteTime

if (-not $complete) {
    throw "SightWeave M2P.3 performance contracts remain PARTIAL; fail-closed report: $OutputPath"
}

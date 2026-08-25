[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunRoot,

    [string]$AttributionPath = '',

    [ValidateRange(0.1, 1000.0)]
    [double]$MinimumStageGrowthMicroseconds = 5.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RunRoot = [System.IO.Path]::GetFullPath($RunRoot)
if ([string]::IsNullOrWhiteSpace($AttributionPath)) {
    $AttributionPath = Join-Path $RunRoot 'attribution-all.csv'
}
else {
    $AttributionPath = [System.IO.Path]::GetFullPath($AttributionPath)
}
$captureSummaryPath = Join-Path $RunRoot 'summary.json'
$classificationCsvPath = Join-Path $RunRoot 'authority-classification.csv'
$classificationJsonPath = Join-Path $RunRoot 'authority-classification.json'
if (-not (Test-Path -LiteralPath $AttributionPath -PathType Leaf)) {
    throw "Combined ETW attribution is missing: $AttributionPath"
}
if (-not (Test-Path -LiteralPath $captureSummaryPath -PathType Leaf)) {
    throw "ETW capture summary is missing: $captureSummaryPath"
}
if ((Test-Path -LiteralPath $classificationCsvPath) -or
    (Test-Path -LiteralPath $classificationJsonPath)) {
    throw "Refusing to overwrite an existing M2P.4 authority classification under $RunRoot"
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

function Get-BudgetMicroseconds {
    param([string]$Operation)

    if ($Operation -eq 'batch_512') {
        return 200.0
    }
    if ($Operation -like 'dynamic_door_*') {
        return 250.0
    }
    return [double]::PositiveInfinity
}

$captureSummary = Get-Content -LiteralPath $captureSummaryPath -Raw | ConvertFrom-Json
$allRows = @(Import-Csv -LiteralPath $AttributionPath)
if ($allRows.Count -ne [int]$captureSummary.attribution_marker_rows) {
    throw "Capture summary/CSV marker count mismatch: $captureSummaryPath"
}
if (@($allRows | Where-Object {
    $_.timeline_closed -ne '1' -or
    $_.events_lost -ne '0' -or
    $_.buffers_lost -ne '0'
}).Count -ne 0) {
    throw "Cannot classify ETW data with loss or an unclosed timeline: $AttributionPath"
}

$stageSamples = @()
$stageRows = @($allRows | Where-Object scope -eq 'stage')
foreach ($group in ($stageRows | Group-Object run, pid, operation, sample_id, stage)) {
    $first = $group.Group[0]
    $sampleKey = "$($first.run)|$($first.pid)|$($first.operation)|$($first.sample_id)"
    $stageSamples += [pscustomobject]@{
        sample_key = $sampleKey
        operation = $first.operation
        stage = $first.stage
        on_cpu_us = [double](($group.Group | Measure-Object on_cpu_us -Sum).Sum)
    }
}

$stageBaselines = @{}
foreach ($group in ($stageSamples | Group-Object operation, stage)) {
    $first = $group.Group[0]
    [double[]]$values = @($group.Group | ForEach-Object { [double]$_.on_cpu_us })
    $stageBaselines["$($first.operation)|$($first.stage)"] =
        Get-NearestRank -Values $values -Fraction 0.50
}
$stagesBySample = @{}
foreach ($stageSample in $stageSamples) {
    if (-not $stagesBySample.ContainsKey($stageSample.sample_key)) {
        $stagesBySample[$stageSample.sample_key] = @()
    }
    $stagesBySample[$stageSample.sample_key] += $stageSample
}

$classifiedRows = @()
$totalRows = @($allRows | Where-Object scope -eq 'total')
foreach ($row in $totalRows) {
    $budget = Get-BudgetMicroseconds -Operation $row.operation
    $wall = [double]$row.wall_us
    $onCpu = [double]$row.on_cpu_us
    $ready = [double]$row.ready_us
    $blocked = [double]$row.blocked_us
    $finalClassification = 'Within budget'
    $topStage = ''
    $topStageCpu = 0.0
    $topStageBaseline = 0.0
    $topStageGrowth = 0.0

    if ($wall -gt $budget) {
        if ($onCpu -gt $budget) {
            $sampleKey = "$($row.run)|$($row.pid)|$($row.operation)|$($row.sample_id)"
            $bestStage = $null
            foreach ($stageSample in @($stagesBySample[$sampleKey])) {
                $baselineKey = "$($stageSample.operation)|$($stageSample.stage)"
                $baseline = [double]$stageBaselines[$baselineKey]
                $growth = [double]$stageSample.on_cpu_us - $baseline
                if ($null -eq $bestStage -or $growth -gt $bestStage.growth) {
                    $bestStage = [pscustomobject]@{
                        name = $stageSample.stage
                        cpu = [double]$stageSample.on_cpu_us
                        baseline = $baseline
                        growth = $growth
                    }
                }
            }
            if ($null -ne $bestStage -and
                $bestStage.growth -ge $MinimumStageGrowthMicroseconds) {
                $finalClassification = 'Plugin CPU'
                $topStage = $bestStage.name
                $topStageCpu = $bestStage.cpu
                $topStageBaseline = $bestStage.baseline
                $topStageGrowth = $bestStage.growth
            }
            else {
                $finalClassification = 'Unknown'
            }
        }
        elseif ($ready + $blocked -gt 0.0) {
            $finalClassification = 'Scheduler/Preemption'
        }
        else {
            $finalClassification = 'Unknown'
        }
    }

    $classifiedRows += [pscustomobject]@{
        run = $row.run
        operation = $row.operation
        distribution = [int]$row.distribution
        sample = [int]$row.sample
        sample_id = $row.sample_id
        pid = $row.pid
        tid = $row.tid
        qpc_begin = $row.qpc_begin
        qpc_end = $row.qpc_end
        budget_us = $budget
        wall_us = $wall
        on_cpu_us = $onCpu
        ready_us = $ready
        blocked_us = $blocked
        context_switches = [int]$row.context_switches
        preemptions = [int]$row.preemptions
        migrations = [int]$row.migrations
        core_residency = $row.core_residency
        timeline = $row.timeline
        final_classification = $finalClassification
        top_stage = $topStage
        top_stage_on_cpu_us = [Math]::Round($topStageCpu, 3)
        top_stage_baseline_p50_us = [Math]::Round($topStageBaseline, 3)
        top_stage_growth_us = [Math]::Round($topStageGrowth, 3)
    }
}

$classifiedRows | Export-Csv -LiteralPath $classificationCsvPath -NoTypeInformation -Encoding utf8NoBOM

$workloads = @()
foreach ($group in ($classifiedRows | Group-Object operation)) {
    $classCounts = [ordered]@{
        'Within budget' = 0
        'Plugin CPU' = 0
        'Scheduler/Preemption' = 0
        'Migration/Frequency' = 0
        'Unknown' = 0
    }
    foreach ($classGroup in ($group.Group | Group-Object final_classification)) {
        $classCounts[$classGroup.Name] = $classGroup.Count
    }
    $contributors = [ordered]@{}
    foreach ($stageGroup in ($group.Group |
        Where-Object final_classification -eq 'Plugin CPU' |
        Group-Object top_stage |
        Sort-Object Count -Descending)) {
        $contributors[$stageGroup.Name] = $stageGroup.Count
    }
    $representatives = @()
    foreach ($classification in @(
        'Plugin CPU',
        'Scheduler/Preemption',
        'Migration/Frequency',
        'Unknown')) {
        $representative = $group.Group |
            Where-Object final_classification -eq $classification |
            Sort-Object wall_us -Descending |
            Select-Object -First 1
        if ($null -ne $representative) {
            $representatives += [ordered]@{
                classification = $classification
                run = $representative.run
                sample = $representative.sample
                pid = $representative.pid
                tid = $representative.tid
                wall_us = $representative.wall_us
                on_cpu_us = $representative.on_cpu_us
                ready_us = $representative.ready_us
                blocked_us = $representative.blocked_us
                migrations = $representative.migrations
                core_residency = $representative.core_residency
                timeline = $representative.timeline
                top_stage = $representative.top_stage
                top_stage_on_cpu_us = $representative.top_stage_on_cpu_us
                top_stage_growth_us = $representative.top_stage_growth_us
            }
        }
    }
    $workloads += [ordered]@{
        operation = $group.Name
        samples = $group.Count
        budget_us = Get-BudgetMicroseconds -Operation $group.Name
        wall_us = Get-MetricSummary -Rows $group.Group -Property 'wall_us'
        on_cpu_us = Get-MetricSummary -Rows $group.Group -Property 'on_cpu_us'
        ready_us = Get-MetricSummary -Rows $group.Group -Property 'ready_us'
        blocked_us = Get-MetricSummary -Rows $group.Group -Property 'blocked_us'
        classifications = $classCounts
        plugin_stage_contributors = $contributors
        representatives = $representatives
    }
}

$batch = $workloads | Where-Object operation -eq 'batch_512'
$broadDoor = $workloads | Where-Object operation -eq 'dynamic_door_broad_4v2l'
$slow400 = @($classifiedRows |
    Where-Object wall_us -ge 400.0 |
    Sort-Object wall_us -Descending |
    Select-Object -First 20 |
    ForEach-Object {
        [ordered]@{
            operation = $_.operation
            run = $_.run
            sample = $_.sample
            pid = $_.pid
            tid = $_.tid
            wall_us = $_.wall_us
            on_cpu_us = $_.on_cpu_us
            ready_us = $_.ready_us
            blocked_us = $_.blocked_us
            classification = $_.final_classification
            top_stage = $_.top_stage
            top_stage_on_cpu_us = $_.top_stage_on_cpu_us
            core_residency = $_.core_residency
            timeline = $_.timeline
        }
    })
$unknownCount = @($classifiedRows | Where-Object final_classification -eq 'Unknown').Count
$summary = [ordered]@{
    schema = 1
    source_label = $captureSummary.label
    source_processes = $captureSummary.elevated_processes
    minimum_stage_growth_us = $MinimumStageGrowthMicroseconds
    total_samples = $classifiedRows.Count
    events_lost = 0
    buffers_lost = 0
    unknown = $unknownCount
    migration_frequency = [ordered]@{
        count = 0
        reason = 'No independent frequency/power evidence promoted a sample; migrated off-CPU intervals remain Scheduler/Preemption.'
    }
    batch_intrinsic_gate = [ordered]@{
        median_le_150 = [double]$batch.on_cpu_us.p50 -le 150.0
        p95_le_180 = [double]$batch.on_cpu_us.p95 -le 180.0
        p99_le_200 = [double]$batch.on_cpu_us.p99 -le 200.0
        pass = [double]$batch.on_cpu_us.p50 -le 150.0 -and
            [double]$batch.on_cpu_us.p95 -le 180.0 -and
            [double]$batch.on_cpu_us.p99 -le 200.0
        decision = 'Do not rewrite Batch when pass=true; retain all wall telemetry and classified tails.'
    }
    broad_door_intrinsic_gate = [ordered]@{
        median_lt_250 = [double]$broadDoor.on_cpu_us.p50 -lt 250.0
        p99_lt_250 = [double]$broadDoor.on_cpu_us.p99 -lt 250.0
        pass = [double]$broadDoor.on_cpu_us.p50 -lt 250.0 -and
            [double]$broadDoor.on_cpu_us.p99 -lt 250.0
        decision = 'Implement exact incremental dynamic angular-sector update when pass=false.'
    }
    workloads = $workloads
    representative_wall_ge_400_us = $slow400
    source_attribution = $AttributionPath
    classification_csv = $classificationCsvPath
}
$summary | ConvertTo-Json -Depth 12 |
    Set-Content -LiteralPath $classificationJsonPath -Encoding utf8NoBOM

if ($unknownCount -ne 0) {
    throw "M2P.4 authority classification contains $unknownCount Unknown samples: $classificationJsonPath"
}

Get-Item -LiteralPath $classificationCsvPath, $classificationJsonPath |
    Select-Object FullName, Length, LastWriteTime

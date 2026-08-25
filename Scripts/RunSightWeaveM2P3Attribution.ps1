[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$RunCount = 10,

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
    $Label = 'ordinary-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $repositoryRoot 'Darkwell.uproject'
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot 'Saved\SightWeaveM2P3\Attribution'
}
$runRoot = Join-Path $OutputRoot $Label

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd not found: $editorCmd"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to overwrite existing attribution run: $runRoot"
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

for ($runIndex = 1; $runIndex -le $RunCount; ++$runIndex) {
    $runName = 'run-{0:D2}' -f $runIndex
    $processRoot = Join-Path $runRoot $runName
    $rawRoot = Join-Path $processRoot 'Raw'
    $reportRoot = Join-Path $processRoot 'Report'
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
        "-SightWeaveM2P3Run=$runName",
        "-SightWeaveM2P3Output=$rawRoot",
        '-ExecCmds=Automation RunTests SightWeave.M2P3.Attribution',
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$reportRoot",
        "-AbsLog=$(Join-Path $processRoot 'attribution.log')"
    )
    $exitCode = Invoke-UnrealEditorCommandlet -Arguments $arguments
    if ($exitCode -ne 0) {
        throw "Attribution process $runName failed with exit code $exitCode. See $processRoot"
    }

    $reportPath = Join-Path $reportRoot 'index.json'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw "Automation report missing for ${runName}: $reportPath"
    }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.inProcess -ne 0 -or $report.succeeded -ne 2) {
        throw "Attribution process $runName is not exactly two clean successes: $reportPath"
    }
    foreach ($fileName in @('batch.csv', 'door.csv')) {
        $csvPath = Join-Path $rawRoot $fileName
        if (-not (Test-Path -LiteralPath $csvPath -PathType Leaf)) {
            throw "Raw attribution CSV missing for ${runName}: $csvPath"
        }
    }
}

function Merge-CsvFiles {
    param(
        [System.IO.FileInfo[]]$Files,
        [string]$Destination
    )

    if ($Files.Count -ne $RunCount) {
        throw "Expected $RunCount source CSV files for $Destination, found $($Files.Count)."
    }
    $writer = [System.IO.StreamWriter]::new($Destination, $false, [System.Text.UTF8Encoding]::new($false))
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
                    $writer.WriteLine($reader.ReadLine())
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

$batchFiles = Get-ChildItem -LiteralPath $runRoot -Recurse -Filter 'batch.csv' | Sort-Object FullName
$doorFiles = Get-ChildItem -LiteralPath $runRoot -Recurse -Filter 'door.csv' | Sort-Object FullName
$combinedBatchPath = Join-Path $runRoot 'batch-all.csv'
$combinedDoorPath = Join-Path $runRoot 'door-all.csv'
Merge-CsvFiles -Files $batchFiles -Destination $combinedBatchPath
Merge-CsvFiles -Files $doorFiles -Destination $combinedDoorPath

$batchRows = @(Import-Csv -LiteralPath $combinedBatchPath)
$doorRows = @(Import-Csv -LiteralPath $combinedDoorPath)
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

function Get-NearestRank {
    param(
        [double[]]$Values,
        [double]$Fraction
    )

    if ($Values.Count -eq 0) {
        return 0.0
    }
    [Array]::Sort($Values)
    $index = [Math]::Max(0, [Math]::Min($Values.Count - 1, [Math]::Ceiling($Fraction * $Values.Count) - 1))
    return $Values[$index]
}

function Get-WorkloadSummary {
    param([object[]]$Rows)

    $summaries = @()
    foreach ($group in ($Rows | Group-Object workload)) {
        [double[]]$wall = @($group.Group | ForEach-Object { [double]$_.wall_us })
        [double[]]$cycles = @($group.Group | ForEach-Object { [double]$_.thread_cycles })
        $classification = @{}
        foreach ($classGroup in ($group.Group | Group-Object classification)) {
            $classification[$classGroup.Name] = $classGroup.Count
        }
        $summaries += [ordered]@{
            workload = $group.Name
            samples = $group.Count
            wall_us = [ordered]@{
                p50 = Get-NearestRank -Values $wall.Clone() -Fraction 0.50
                p95 = Get-NearestRank -Values $wall.Clone() -Fraction 0.95
                p99 = Get-NearestRank -Values $wall.Clone() -Fraction 0.99
                max = Get-NearestRank -Values $wall.Clone() -Fraction 1.00
            }
            thread_cycles = [ordered]@{
                p50 = Get-NearestRank -Values $cycles.Clone() -Fraction 0.50
                p95 = Get-NearestRank -Values $cycles.Clone() -Fraction 0.95
                p99 = Get-NearestRank -Values $cycles.Clone() -Fraction 0.99
                max = Get-NearestRank -Values $cycles.Clone() -Fraction 1.00
            }
            classifications = $classification
        }
    }
    return $summaries
}

$summary = [ordered]@{
    label = $Label
    ordinary_processes = $RunCount
    affinity_or_priority_modified = $false
    context_switch_etw = 'not requested in ordinary acceptance processes'
    batch_distributions = $distributionCount
    batch_rows = $batchRows.Count
    door_rows = $doorRows.Count
    batch = Get-WorkloadSummary -Rows $batchRows
    door = Get-WorkloadSummary -Rows $doorRows
    raw_batch = $combinedBatchPath
    raw_door = $combinedDoorPath
}
$summaryPath = Join-Path $runRoot 'summary.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Get-Item -LiteralPath $combinedBatchPath, $combinedDoorPath, $summaryPath |
    Select-Object FullName, Length, LastWriteTime

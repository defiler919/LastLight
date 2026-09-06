[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName, [string]$EngineRoot='D:\UE_5.8', [switch]$EventsOnly)
$ErrorActionPreference='Stop'
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use a simple run name' }
if (Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) { throw 'Export after the measured process has exited' }
$repo=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repo "Saved/Stabilization/$RunName"
if (-not (Test-Path "$output/capture.utrace")) { throw 'Trace is missing' }
$commands=@(
    "TimingInsights.ExportThreads $output/threads.csv",
    "TimingInsights.ExportTimers $output/timers.csv",
    "TimingInsights.ExportTimerStatistics $output/stats_{region}.csv -region=Empty,OneWhole,EightWhole,ThirtyTwoWhole,PartialNewThenRepeat,Overlap64,SameIdentity64,Distributed184,FastSweep90,FastSweep160,StationaryStop,LongRepeatDistributed,ActualNewKnowledge,NoGuidance,NoWorldLabels,NoUi,NoCoverageDraw,Restored -sortBy=TotalInclusiveTime",
    "TimingInsights.ExportTimerStatistics $output/stats_all.csv -sortBy=TotalInclusiveTime"
)
# UE 5.8 aggregation always includes GPU queues despite -threads. Export
# individual events with actual ThreadId/TimerId for unambiguous attribution.
$eventsPath = "$output/events_critical_all_threads.csv"
# Occlusion waits can execute on task workers; a RenderThread-only filter loses them.
$commands += "TimingInsights.ExportTimingEvents $eventsPath -timers=Frame,*WaitingForGPUForOcclusionQueries*,GameThreadWaitForTask,Darkwell_SightWeave_SourceUpdate,SightWeave_MemoryWriteEffectiveLive,SceneRender,*TemporalSuperResolution*,LumenScreenProbeGather"
$responseFile = if ($EventsOnly) { "$output/export-events-all-threads.rsp" } else { "$output/export.rsp" }
$analysisLog = if ($EventsOnly) { "$output/insights-events-all-threads.log" } else { "$output/insights.log" }
if ($EventsOnly) {
    if (Test-Path $eventsPath) { throw 'Critical event evidence already exists' }
    $commands = @($commands[-1])
}
$commands | Set-Content $responseFile
$arguments=@("-OpenTraceFile=$output/capture.utrace",'-AutoQuit','-NoUI',"-ExecOnAnalysisCompleteCmd=@=$responseFile","-abslog=$analysisLog")
$process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealInsights.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$expected = if ($EventsOnly) { $eventsPath } else { "$output/stats_all.csv" }
if ($process.ExitCode -ne 0 -or -not (Test-Path $expected)) { throw "Insights export failed: $($process.ExitCode)" }
Get-Item $expected | Select-Object Name,Length

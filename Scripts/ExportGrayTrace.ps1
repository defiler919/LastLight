[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName, [string]$EngineRoot='D:\UE_5.8')
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
$commands | Set-Content "$output/export.rsp"
$arguments=@("-OpenTraceFile=$output/capture.utrace",'-AutoQuit','-NoUI',"-ExecOnAnalysisCompleteCmd=@=$output/export.rsp","-abslog=$output/insights.log")
$process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealInsights.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
if ($process.ExitCode -ne 0 -or -not (Test-Path "$output/stats_all.csv")) { throw "Insights export failed: $($process.ExitCode)" }
Get-Item "$output/stats_*.csv" | Select-Object Name,Length

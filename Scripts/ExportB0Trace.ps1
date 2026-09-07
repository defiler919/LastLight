[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName,[string]$EngineRoot='D:\UE_5.8')
$ErrorActionPreference='Stop'
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple run name required'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Export only after measurement exits'}
$repo=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repo "Saved/Stabilization/$RunName"
if(Test-Path "$output/b0-events.csv"){throw 'Export exists'}
@(
 "TimingInsights.ExportThreads $output/b0-threads.csv",
 "TimingInsights.ExportTimers $output/b0-timers.csv",
 "TimingInsights.ExportTimerStatistics $output/b0-stats.csv -sortBy=TotalInclusiveTime",
 "TimingInsights.ExportTimingEvents $output/b0-events.csv -timers=Darkwell_B0_*,Darkwell_GrayHistory_Cap*,Darkwell_Resources_*,Frame,*AddPrimitive*,*UpdateTexture*,*InitRHI*,*WaitForTask*,*CreateRenderState*,*UpdateAllPrimitiveSceneInfos*"
) | Set-Content "$output/b0-export.rsp"
$arguments=@("-OpenTraceFile=$output/capture.utrace",'-AutoQuit','-NoUI',"-ExecOnAnalysisCompleteCmd=@=$output/b0-export.rsp","-abslog=$output/b0-insights.log")
$p=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealInsights.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru
$p.WaitForExit()
if($p.ExitCode -ne 0 -or !(Test-Path "$output/b0-events.csv")){throw 'B0 export failed'}
Get-Item "$output/b0-events.csv" | Select-Object Name,Length

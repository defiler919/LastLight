[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot='D:\UE_5.8',
    [ValidateRange(120,900)][int]$TimeoutSeconds=900
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a unique simple run name'}
$output=Join-Path $repo "Saved/GrayObjectPolicy/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence exists: $output"}
$conflicts=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|ShaderCompileWorker|MSBuild|cl|link|UnrealBuildTool)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool') })
if($conflicts.Count){throw 'Conflicting Unreal/build workload'}
New-Item -ItemType Directory -Path $output | Out-Null
$prior=$env:DARKWELL_BLACKOUT_TIMING
$process=$null
$start=Get-Date
try {
    $env:DARKWELL_BLACKOUT_TIMING='1'
    & git -C $repo diff HEAD --binary | Set-Content "$output/source.patch"
    Copy-Item -LiteralPath "$repo/Source/Darkwell/Private/VisionPresentation/DarkwellBlackoutTiming.h" -Destination "$output/timing-source.h"
    [ordered]@{head=(& git -C $repo rev-parse HEAD); started=$start.ToString('o'); cycles=20; warmup_frames=300; measured_frames=3000; rhi='D3D12/SM6'; executable_mode='native game'; timeout_seconds=$TimeoutSeconds} | ConvertTo-Json | Set-Content "$output/source.json"
    $args=@("`"$repo/Darkwell.uproject`"",'/Game/Maps/L_BlackRegionLab','-game','-d3d12','-sm6','-windowed','-ResX=1280','-ResY=720',
        '-NoSound','-unattended','-DarkwellBlackoutProbe',"`"-DarkwellBlackoutProbeOutput=$output`"","`"-abslog=$output/game.log`"",'-ExecCmds="t.MaxFPS 60"')
    # The requested real viewport owns the foreground; the in-game probe rejects focus loss.
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $args -WindowStyle Normal -PassThru
    $process.Id | Set-Content "$output/pid.txt"
    if(!$process.WaitForExit($TimeoutSeconds*1000)){
        Stop-Process -Id $process.Id
        throw 'Native probe exceeded its bounded workload timeout'
    }
    [ordered]@{exit_code=$process.ExitCode; wall_seconds=((Get-Date)-$start).TotalSeconds} | ConvertTo-Json | Set-Content "$output/process.json"
    if($process.ExitCode -ne 0){throw "Native probe exited $($process.ExitCode)"}
    & python "$PSScriptRoot/SummarizeBlackoutTransitions.py" $output
    if($LASTEXITCODE -ne 0){throw 'Native transition acceptance failed; keep raw evidence'}
} finally {
    $env:DARKWELL_BLACKOUT_TIMING=$prior
}

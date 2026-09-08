[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName,[ValidateRange(30,900)][int]$Seconds=300,[switch]$OldLab,[string]$EngineRoot='D:\UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a unique simple run name'}
if(Get-Process UnrealEditor -ErrorAction SilentlyContinue){throw 'Close the existing editor before the bounded run'}
$output=Join-Path $repo "Saved/BlackRegionSoak/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence already exists: $output"}
New-Item -ItemType Directory -Path $output | Out-Null
$map=if($OldLab){'/Game/Maps/L_ProjectFogPropGameplayLab?PropLabOriginal?InWorldControls'}else{'/Game/Maps/L_BlackRegionLab'}
$args=@("`"$repo/Darkwell.uproject`"",$map,'-game','-d3d12','-sm6','-windowed','-ResX=1280','-ResY=720','-unattended',"-DarkwellMemorySoakSeconds=$Seconds","`"-DarkwellMemorySoakOutput=$output`"","`"-abslog=$output/run.log`"")
$process=$null
try {
 $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $args -WindowStyle Normal -PassThru
 $process.Id | Set-Content "$output/pid.txt"
 $deadline=(Get-Date).AddSeconds($Seconds+120)
 while(!$process.HasExited -and (Get-Date) -lt $deadline){Start-Sleep -Milliseconds 500}
 if(!$process.HasExited){throw 'Bounded soak timeout'}
 $text=Get-Content "$output/run.log" -Raw
 $passed=$process.ExitCode -eq 0 -and $text.Contains('MEMORY_SOAK_COMPLETE') -and $text -notmatch 'appError called|Fatal error'
 [ordered]@{head=(& git -C $repo rev-parse HEAD);map=$map;seconds=$Seconds;exit_code=$process.ExitCode;passed=$passed} | ConvertTo-Json | Set-Content "$output/result.json"
 Select-String -Path "$output/run.log" -Pattern 'MEMORY_STORAGE' | ForEach-Object {$_.Line} | Set-Content "$output/storage.log"
 Get-Content "$output/result.json"
 if(!$passed){throw 'Soak did not complete successfully'}
} finally {
 if($process -and !$process.HasExited){$null=$process.CloseMainWindow(); if(!$process.WaitForExit(5000)){$process.Kill()}}
}

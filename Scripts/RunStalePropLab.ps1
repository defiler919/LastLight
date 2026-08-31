[CmdletBinding()]
param([int[]]$Widths=@(1920),[int[]]$Modes=@(0,1,2),[int[]]$Cases=@(0,1,2,3,4,5),[string]$Label='Stale',[string]$ZenDataPath='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$engine=if($env:DARKWELL_UE_ROOT){$env:DARKWELL_UE_ROOT}else{'D:\UE_5.8'}
$exe=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
foreach($width in $Widths) { foreach($mode in $Modes) { foreach($case in $Cases) {
    $height=[int]($width*9/16)
    $runName="${Label}_${width}_M${mode}_$([char](65+$case))"
    $root=Join-Path $repo 'Saved/PropGameplayLab'
    $log=Join-Path $root "$runName.log"
    if(Test-Path -LiteralPath $log){throw "Evidence already exists: $runName"}
    New-Item -ItemType Directory -Force -Path (Join-Path $root $runName) | Out-Null
    $args=@("`"$(Join-Path $repo 'Darkwell.uproject')`"",'/Game/Maps/L_ProjectFogPropGameplayLab','-game','-windowed',"-ResX=$width","-ResY=$height",'-ForceRes','-d3d12','-sm6','-nosound','-unattended','-NoVSync','-UseFixedTimeStep','-FixedSeed','-FPS=30',
      '-ExecCmds="r.AntiAliasingMethod 4"','-PropLabAsyncCapture','-StaleLabAuto',"-StaleLabMode=$mode","-StaleLabCase=$case","-StaleLabCapture=$runName","-abslog=`"$log`"")
    if($ZenDataPath){$args+="-ZenDataPath=`"$ZenDataPath`""}
    $process=Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
    if(!$process.WaitForExit(300000)){Stop-Process -Id $process.Id;throw "Timeout $runName"}
    $text=Get-Content -LiteralPath $log -Raw
    if($process.ExitCode -ne 0 -or $text -notmatch 'STALE_CAPTURE_COMPLETE' -or $text -match 'STALE_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|Failed to compile Material'){throw "Failed $runName"}
    Write-Output "PASS $runName"
} } }

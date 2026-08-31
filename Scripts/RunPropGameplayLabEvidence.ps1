[CmdletBinding()]
param([int[]]$Widths=@(1920,2560),[int[]]$Modes=@(0,1,2),[int[]]$Policies=@(0,1),[int[]]$Routes=@(1,2,3,4,5,6,7,8,9,10),[string]$Label='Matrix')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$engine=if($env:DARKWELL_UE_ROOT){$env:DARKWELL_UE_ROOT}else{'D:\UE_5.8'}
$exe=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
foreach($width in $Widths) { foreach($mode in $Modes) { foreach($policy in $Policies) { foreach($route in $Routes) {
    $height=[int]($width*9/16)
    $runName="${Label}_${width}_M${mode}_P${policy}_R${route}"
    $runRoot=Join-Path $repo 'Saved/PropGameplayLab'
    New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
    $log=Join-Path $runRoot "$runName.log"
    if(Test-Path $log) { throw "Evidence exists; use a new Label instead of overwriting $log" }
    New-Item -ItemType Directory -Force -Path (Join-Path $runRoot $runName) | Out-Null
    $args=@("`"$(Join-Path $repo 'Darkwell.uproject')`"",'/Game/Maps/L_ProjectFogPropGameplayLab','-game','-windowed',"-ResX=$width","-ResY=$height",'-ForceRes','-d3d12','-sm6','-nosound','-unattended','-NoVSync',
      "-ExecCmds=`"r.AntiAliasingMethod 4,r.Darkwell.ProjectFogVisual.PropPresentationMode $mode,r.Darkwell.ProjectFogVisual.PropRelocationPolicy $policy,r.Darkwell.ProjectFogVisual.LabRoute $route`"",
      '-PropLabAsyncCapture',"-PropLabCapture=$runName","-abslog=`"$log`"")
    $process=Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
    if(!$process.WaitForExit(180000)) { Stop-Process -Id $process.Id; throw "Timed out: $runName" }
    if($process.ExitCode -ne 0) { throw "Process failed: $runName" }
    $text=Get-Content $log -Raw
    if($text -notmatch 'LAB_CAPTURE_COMPLETE' -or $text -notmatch 'PropLab activated') { throw "Incomplete authority/capture: $runName" }
    if($text -match 'LAB_CONTRACT_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|activation failed|Failed to compile Material') { throw "Severe lab failure: $runName" }
    Write-Output "PASS $runName"
} } } }

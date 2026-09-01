[CmdletBinding()]
param([int[]]$Widths=@(1920,2560),[int[]]$Modes=@(0,1,2),[int[]]$Routes=@(1,3,11,12,13),[string]$Label='Comparison')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$engine=if($env:DARKWELL_UE_ROOT){$env:DARKWELL_UE_ROOT}else{'D:\UE_5.8'}
$exe=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
foreach($width in $Widths) { foreach($mode in $Modes) { foreach($route in $Routes) {
    $height=[int]($width*9/16)
    $fps=if($route -eq 1){30}else{15}
    $runName="${Label}_${width}_M${mode}_SpatialEvidenceOnly_R${route}"
    $root=Join-Path $repo 'Saved/PropGameplayLab'
    $log=Join-Path $root "$runName.log"
    if(Test-Path -LiteralPath $log){throw "Evidence already exists: $runName"}
    New-Item -ItemType Directory -Force -Path (Join-Path $root $runName) | Out-Null
    $args=@("`"$(Join-Path $repo 'Darkwell.uproject')`"",'/Game/Maps/L_ProjectFogPropGameplayLab','-game','-windowed',"-ResX=$width","-ResY=$height",'-ForceRes','-d3d12','-sm6','-nosound','-unattended','-NoVSync','-UseFixedTimeStep','-FixedSeed',"-FPS=$fps",
      "-ExecCmds=`"r.AntiAliasingMethod 4,r.Darkwell.ProjectFogVisual.PropPresentationMode $mode,r.Darkwell.ProjectFogVisual.LabRoute $route`"",
      '-PropLabAsyncCapture','-PropLabComparisonCapture',"-PropLabCapture=$runName","-abslog=`"$log`"")
    $process=Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
    if(!$process.WaitForExit(300000)){Stop-Process -Id $process.Id;throw "Timeout $runName"}
    $text=Get-Content -LiteralPath $log -Raw
    if($process.ExitCode -ne 0 -or $text -notmatch 'LAB_CAPTURE_COMPLETE' -or $text -match 'LAB_CONTRACT_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|Failed to compile Material'){throw "Failed $runName"}
    Write-Output "PASS $runName"
} } }

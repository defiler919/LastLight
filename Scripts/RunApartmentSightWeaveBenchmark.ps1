param([Parameter(Mandatory=$true)][string]$RunName,[switch]$Reference,[switch]$Still,[switch]$Trace,[ValidateSet('','PairGray','PairUnknown')][string]$Pair='')
$ErrorActionPreference='Stop'
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple unique run name'}
$repo=Split-Path $PSScriptRoot -Parent
$engine=if($env:DARKWELL_UE_ROOT){$env:DARKWELL_UE_ROOT}else{'D:/UE_5.8'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close other Unreal workloads first'}
$out="$repo/Saved/StaticKnowledge/$RunName"
if(Test-Path $out){throw "Existing evidence: $out"}
New-Item -ItemType Directory $out | Out-Null
if(-not ('GrayBenchmarkSession' -as [type])){Add-Type -Path "$PSScriptRoot/GrayBenchmarkSession.cs"}
$guard=[GrayBenchmarkSession]::new()
$mode=if($Still){'Still'}else{'A'}
if($Pair){$mode=$Pair}
$args=@("`"$repo/Darkwell.uproject`"",'/Game/Maps/L_SightWeaveApartmentLab','-game','-d3d12','-sm6','-windowed','-ForceRes','-ResX=1280','-ResY=720','-NoSound','-NoSplash','-NoVSync',"-ApartmentBench=$mode","-ApartmentBenchOutput=$out","-abslog=$out/game.log",'-ExecCmds="r.ScreenPercentage 100,r.DynamicRes.OperationMode 0,r.VSync 0,t.MaxFPS 0"')
if($Reference){$args+='-ApartmentObjectArchitectureReference'}
if($Trace){$args+=@('-trace=cpu,gpu,frame,bookmark,region',"-tracefile=$out/capture.utrace")}
$args -join ' ' | Set-Content "$out/command.txt"
[ordered]@{head=(& git -C $repo rev-parse HEAD); reference=[bool]$Reference; still=[bool]$Still; started=(Get-Date).ToString('o'); binary_sha256=(Get-FileHash "$repo/Binaries/Win64/UnrealEditor-Darkwell.dll" -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$out/source.json"
$p=$null
try{
 $p=Start-Process "$engine/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $args -WindowStyle Normal -PassThru
 $deadline=(Get-Date).AddSeconds(240)
 while(!$p.HasExited){
  if((Get-Date)-gt$deadline){throw 'Benchmark timeout'}
  $p.Refresh()
  if(!(Test-Path "$out/initial-storage.txt") -and $p.MainWindowHandle -ne 0){[GrayBenchmarkSession]::Activate($p.MainWindowHandle,[uint32]$p.Id)|Out-Null}
  Start-Sleep -Seconds 2
 }
 if(!(Test-Path "$out/complete.txt") -or (Test-Path "$out/invalid.txt")){throw 'Invalid benchmark; preserve evidence'}
 Get-Content "$out/complete.txt"
}finally{
 if($p -and !$p.HasExited){Stop-Process -Id $p.Id}
 $guard.Dispose()
}

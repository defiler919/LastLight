[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot='D:\UE_5.8',
    [string]$Tests='Darkwell.UnknownPartial+Darkwell.UnknownRegion+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.ObjectMemory.PresentationResidency+Darkwell.PropLab.GrayObjectPolicy.WholeObjectConfirmedStaticHistory+Darkwell.PropLab.GrayObjectPolicy.SpatialPartialStaticKeepsLegalCap'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a unique simple run name'}
$output=Join-Path $repo "Saved/GrayObjectPolicy/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence exists: $output"}
$workloads=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|ShaderCompileWorker|MSBuild|cl|link|UnrealBuildTool)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool') })
if($workloads.Count){throw 'Conflicting Unreal/build workload'}
New-Item -ItemType Directory -Path $output | Out-Null
$report=Join-Path $output "${RunName}_Report"
$log=Join-Path $output "$RunName.log"
$priorOutput=$env:DARKWELL_UNKNOWN_TEST_OUTPUT
$priorSelector=$env:DARKWELL_UNKNOWN_TEST_SELECTOR
$process=$null; $guard=$null; $approved=$false; $attempts=0; $failure=$null; $code=-1
$start=Get-Date
[ordered]@{head=(& git -C $repo rev-parse HEAD); started=$start.ToString('o'); selector=$Tests} | ConvertTo-Json | Set-Content "$output/source.json"
& git -C $repo diff HEAD --binary | Set-Content "$output/source.patch"
try {
    if(-not ('GrayBenchmarkSession' -as [type])){Add-Type -Path "$PSScriptRoot/GrayBenchmarkSession.cs"}
    $guard=[GrayBenchmarkSession]::new()
    if(!$guard.ExecutionState){throw 'Cannot acquire unattended system/display guard'}
    $env:DARKWELL_UNKNOWN_TEST_OUTPUT=$output
    $env:DARKWELL_UNKNOWN_TEST_SELECTOR=$Tests
    $arguments=@("`"$repo/Darkwell.uproject`"",'-d3d12','-sm6','-unattended','-nop4','-nosplash','-NoSound',
        "`"-ExecutePythonScript=$repo/Content/Python/run_unknown_partial_cut_tests.py`"",'-TestExit="Automation Test Queue Empty"',
        "`"-ReportExportPath=$report`"","`"-abslog=$log`"")
    # Visible window is required by the user's unattended foreground contract.
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $arguments -WindowStyle Normal -PassThru
    $process.Id | Set-Content "$output/pid.txt"
    $deadline=(Get-Date).AddSeconds(90); $nextAttempt=Get-Date
    while(!$process.HasExited -and !$approved){
        if((Get-Date) -ge $deadline){throw 'Bounded foreground startup timeout'}
        if(Test-Path "$output/failed.txt"){throw 'Engine driver failed'}
        $process.Refresh(); $live=$null
        if(Test-Path "$output/foreground-live.json"){
            $stream=$null; $reader=$null
            try {
                $stream=[IO.File]::Open("$output/foreground-live.json",[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                $reader=[IO.StreamReader]::new($stream); $live=$reader.ReadToEnd() | ConvertFrom-Json
            } catch {$live=$null}
            finally {if($reader){$reader.Dispose()}elseif($stream){$stream.Dispose()}}
        }
        $fresh=$live -and ((Get-Date)-(Get-Item "$output/foreground-live.json").LastWriteTime).TotalSeconds -lt 2
        if($fresh -and $live.ready -and $live.engine.foreground -eq 1 -and !$live.engine.minimized){
            $approved=$true
            [ordered]@{time=(Get-Date).ToString('o'); attempts=$attempts; engine=$live.engine; stable_frames=$live.consecutive} | ConvertTo-Json -Depth 6 | Set-Content "$output/foreground-approved.json"
            break
        }
        if((Test-Path "$output/viewport-ready.json") -and $process.MainWindowHandle -ne 0 -and (Get-Date) -ge $nextAttempt -and !($fresh -and $live.engine.foreground -eq 1)){
            if($attempts -ge 8){throw 'Foreground activation exhausted eight attempts'}
            $attempts++
            [ordered]@{attempt=$attempts; pid=$process.Id; win32_result=[GrayBenchmarkSession]::Activate($process.MainWindowHandle,$process.Id)} | ConvertTo-Json -Compress | Add-Content "$output/window-activation.jsonl"
            $nextAttempt=(Get-Date).AddSeconds(2)
        }
        Start-Sleep -Milliseconds 100
    }
    if(!$approved){throw 'Editor exited before foreground approval'}
    $deadline=(Get-Date).AddSeconds(180)
    while(!$process.HasExited){
        if((Get-Date) -ge $deadline){throw 'Bounded functional run timeout'}
        if((Test-Path "$output/failed.txt") -or (Test-Path "$output/foreground-lost.json")){throw 'Driver or foreground validation failed'}
        Start-Sleep -Milliseconds 200
    }
    $code=$process.ExitCode
} catch {
    $failure=$_.Exception.Message
    [ordered]@{reason=$failure; attempts=$attempts; approved=$approved} | ConvertTo-Json | Set-Content "$output/foreground-abort.json"
    if($process -and !$process.HasExited -and !$process.WaitForExit(10000)){
        $null=$process.CloseMainWindow()
        if(!$process.WaitForExit(5000)){$process.Kill(); $process.WaitForExit()}
    }
    if($process -and $process.HasExited){$code=$process.ExitCode}
} finally {
    if($guard){$guard.Dispose(); [ordered]@{acquired=$guard.ExecutionState; restored=$guard.RestoreState} | ConvertTo-Json | Set-Content "$output/power-guard.json"}
    $env:DARKWELL_UNKNOWN_TEST_OUTPUT=$priorOutput
    $env:DARKWELL_UNKNOWN_TEST_SELECTOR=$priorSelector
}
$r=if(Test-Path "$report/index.json"){Get-Content "$report/index.json" -Raw | ConvertFrom-Json}else{$null}
$severe=if(Test-Path $log){@(Select-String -LiteralPath $log -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE_REMOVED|DXGI_ERROR_DEVICE_HUNG|EXCEPTION_ACCESS_VIOLATION|Traceback').Count}else{1}
$summary=[ordered]@{run=$RunName; selector=$Tests; runner_failure=$failure; exit_code=$code; foreground_approved=$approved; foreground_attempts=$attempts; wall_seconds=((Get-Date)-$start).TotalSeconds; total=($r.succeeded+$r.succeededWithWarnings+$r.failed+$r.notRun); clean=$r.succeeded; warnings=$r.succeededWithWarnings; failed=$r.failed; not_run=$r.notRun; severe_lines=$severe; requested_rhi='D3D12/SM6'; passed=(!$failure -and $approved -and $code -eq 0 -and $r -and ($r.succeeded+$r.succeededWithWarnings) -gt 0 -and !$r.failed -and !$r.notRun -and !$severe)}
$summary | ConvertTo-Json | Set-Content "$output/$RunName.summary.json"
$summary | ConvertTo-Json
if(!$summary.passed){exit 1}

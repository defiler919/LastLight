[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [ValidateSet('PIE','Standalone')][string]$Mode='PIE',
    [ValidateSet('Smoke','Knowledge','Attribution','FrameAudit','Batch','Matrix','LongRun','Reference','WholePreparation','ParentMaterial','A1')][string]$Protocol='Matrix',
    [string]$Map='',
    [string]$EngineRoot='D:\UE_5.8',
    [switch]$NoAuthoringToolsets,
    [ValidateSet(0,1)][int]$HistoryResidencyMode=0,
    [switch]$A1Visual,
    [switch]$SerialSealedOwnership,
    [switch]$LegacyCapturePreparation,
    [switch]$SerialCapBuild,
    [switch]$SerialOccupancy,
    [switch]$LegacyRecordResources,
    [switch]$LegacyWholePreparationBudget,
    [switch]$LegacyHistoryParent,
    [ValidateRange(0,2)][int]$WholeGeometryPreparationMode=0,
    [switch]$Trace,
    [ValidateRange(1,180)][int]$ForegroundTimeoutSeconds=90,
    [ValidateRange(30,1800)][int]$RunTimeoutSeconds=1200
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use a unique simple run name' }
if ($A1Visual -and ($Protocol -ne 'A1' -or $Mode -ne 'PIE')) { throw 'A1 viewport readback requires the PIE visual protocol; use Standalone without screenshots for performance' }
$workloads=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealInsights|ShaderCompileWorker|SpaceCraft|MSBuild|cl|link|UnrealBuildTool|AutomationTool)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool') })
if($workloads.Count) { throw "Conflicting workload: $($workloads.Name -join ', ')" }
$output=Join-Path $repo "Saved/Stabilization/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence exists: $output"}
New-Item -ItemType Directory -Path $output | Out-Null
if (!$Map) { $Map = if ($Protocol -eq 'Reference') { '/Game/Maps/L_Prototype' } else { '/Game/Maps/L_SightWeaveGrayPolicyLab' } }
if ($Protocol -ne 'Reference' -and $Map -ne '/Game/Maps/L_SightWeaveGrayPolicyLab') { throw 'The gray Matrix is frozen to its Lab map' }
$driver=if ($Protocol -eq 'Reference') { "$repo/Content/Python/profile_gray_project_reference.py" } else { "$repo/Content/Python/profile_gray_stabilization.py" }
if ($Protocol -eq 'A1') { $driver="$repo/Content/Python/profile_gray_old_history_demand.py" }
Copy-Item -LiteralPath $driver -Destination "$output/driver.py"
Copy-Item -LiteralPath "$repo/Content/Python/gray_benchmark_foreground.py" -Destination "$output/gray_benchmark_foreground.py"
Copy-Item -LiteralPath "$PSScriptRoot/GrayBenchmarkSession.cs" -Destination "$output/GrayBenchmarkSession.cs"
git -C $repo diff HEAD --binary | Set-Content "$output/source.patch"
git -C $repo status --porcelain=v1 | Set-Content "$output/worktree.txt"
Copy-Item -LiteralPath "$repo/Config/DefaultEngine.ini" -Destination "$output/DefaultEngine.ini"
$binary=Get-Item "$repo/Binaries/Win64/UnrealEditor-Darkwell.dll"
$start=Get-Date
$arguments=@("$repo/Darkwell.uproject",$Map,'-d3d12','-sm6','-NoSound','-unattended','-NoSplash','-NoVSync','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-WinX=30','-WinY=30',"-abslog=$output/editor.log")
$disabledPlugins=@()
if ($LegacyHistoryParent -or $Protocol -eq 'ParentMaterial') {
    # Device-profile command-line CVars apply before world/source registration.
    $arguments += '-DPCvars=r.Darkwell.ObjectMemory.SceneHistoryParent=0'
}
if ($NoAuthoringToolsets) {
    # Explicit diagnostic environment only. The regular Editor/project remains
    # unchanged. Disable parents too: dependencies can re-enable ToolsetRegistry.
    $disabledPlugins=@('ToolsetRegistry','AIAssistant','ModelContextProtocol') +
        @(Get-ChildItem "$EngineRoot/Engine/Plugins/Experimental/Toolsets" -Directory | Select-Object -ExpandProperty Name)
    $arguments += "-DisablePlugins=$($disabledPlugins -join ',')"
}
if($Mode -eq 'Standalone') {
    $startupCommands = @("r.Darkwell.ObjectMemory.HistoryResidency $HistoryResidencyMode","r.Darkwell.ObjectMemory.WholeGeometryPreparation $WholeGeometryPreparationMode")
    if ($LegacyWholePreparationBudget) { $startupCommands += 'r.Darkwell.ObjectMemory.WholePreparationFrameGuard 0' }
    if ($SerialSealedOwnership) { $startupCommands += 'r.Darkwell.ObjectMemory.JoinedSealedOwnership 0' }
    if ($SerialCapBuild) { $startupCommands += 'r.Darkwell.ObjectMemory.JoinedCapBuild 0' }
    if ($SerialOccupancy) { $startupCommands += 'r.Darkwell.ObjectMemory.JoinedOccupancy 0' }
    if ($LegacyRecordResources) { $startupCommands += 'r.Darkwell.ObjectMemory.RecordScopedResources 0' }
    if ($LegacyCapturePreparation) { $startupCommands += 'r.Darkwell.ObjectMemory.StagedCapturePreparation 0' }
    $startupCommands += "py $output/driver.py"
    $startup = $startupCommands -join ','
    $arguments+=@('-game','-EnablePython',"-ExecCmds=`"$startup`"")
}
else { $arguments+=@("-ExecutePythonScript=$output/driver.py", "-ExecCmds=`"r.Darkwell.ObjectMemory.HistoryResidency $HistoryResidencyMode`"") }
if ($SerialSealedOwnership -and $Mode -ne 'Standalone') { throw 'Serial ownership comparison is scoped to Standalone' }
if ($LegacyCapturePreparation -and $Mode -ne 'Standalone') { throw 'Capture comparison is scoped to Standalone' }
if ($SerialCapBuild -and $Mode -ne 'Standalone') { throw 'Cap comparison is scoped to Standalone' }
if ($SerialOccupancy -and $Mode -ne 'Standalone') { throw 'Occupancy comparison is scoped to Standalone' }
if ($LegacyRecordResources -and $Mode -ne 'Standalone') { throw 'Resource comparison is scoped to Standalone' }
if($Trace) { $arguments+=@('-trace=cpu,gpu,frame,bookmark,region',"-tracefile=$output/capture.utrace") }
$metadata=[ordered]@{
    schema=1; sha=(& git -C $repo rev-parse HEAD); started_utc=$start.ToUniversalTime().ToString('o'); mode=$Mode; protocol=$Protocol
    engine=(Get-Content "$EngineRoot/Engine/Build/Build.version" -Raw | ConvertFrom-Json)
    cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate)
    ram_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    binary_built_utc=$binary.LastWriteTimeUtc.ToString('o'); binary_sha256=(Get-FileHash $binary.FullName).Hash
    driver_sha256=(Get-FileHash $driver).Hash; map=$Map; seed=0
    screenshots=($Protocol -eq 'Reference' -or [bool]$A1Visual); history_residency_mode=$HistoryResidencyMode; trace=[bool]$Trace; debugger=$false; fixed_timestep=$false; other_engine_build_processes=$workloads
    arguments=$arguments; processes_at_start=@(Get-Process | Select-Object Name,Id,CPU,WorkingSet64)
    disabled_authoring_plugins=$disabledPlugins
    serial_sealed_ownership=[bool]$SerialSealedOwnership
    legacy_capture_preparation=[bool]$LegacyCapturePreparation
    serial_cap_build=[bool]$SerialCapBuild
    serial_occupancy=[bool]$SerialOccupancy
    legacy_record_resources=[bool]$LegacyRecordResources
    legacy_whole_preparation_budget=[bool]$LegacyWholePreparationBudget
    legacy_history_parent=[bool]$LegacyHistoryParent
    editor_binary_sha256=(Get-FileHash "$repo/Binaries/Win64/UnrealEditor-DarkwellEditor.dll").Hash
    timing_note='Wall intervals between distinct game updates include Python measurement cost; engine GT/RT/RHI/GPU counters are delayed and overlap. PIE global Render/RHI counters can be overwritten by Slate window updates; use separate Insights capture for attribution. No subtraction attribution.'
}
$metadata | ConvertTo-Json -Depth 8 | Set-Content "$output/environment.json"
$priorA1Visual=$env:DARKWELL_A1_VISUAL
$env:DARKWELL_A1_VISUAL=if($A1Visual){"1"}else{"0"}
$priorOutput=$env:DARKWELL_STABILIZATION_OUTPUT
$priorMode=$env:DARKWELL_STABILIZATION_MODE
$priorProtocol=$env:DARKWELL_STABILIZATION_PROTOCOL
$priorMap=$env:DARKWELL_STABILIZATION_MAP
$priorParent=$env:DARKWELL_HISTORY_PARENT_MODE
$process=$null
$guard=$null
$failure=$null
$code=-1
$attempts=0
$approved=$false
try {
    if (-not ('GrayBenchmarkSession' -as [type])) { Add-Type -Path "$PSScriptRoot/GrayBenchmarkSession.cs" }
    $guard=[GrayBenchmarkSession]::new()
    if (!$guard.ExecutionState) { throw 'SetThreadExecutionState failed; unattended run cannot start' }
    [ordered]@{ acquired=$guard.ExecutionState; scope='runner lifetime; system and display; dedicated thread' } | ConvertTo-Json | Set-Content "$output/power-guard.json"
    $env:DARKWELL_STABILIZATION_OUTPUT=$output
    $env:DARKWELL_STABILIZATION_MODE=$Mode
    $env:DARKWELL_STABILIZATION_PROTOCOL=$Protocol
    $env:DARKWELL_STABILIZATION_MAP=$Map
    $env:DARKWELL_HISTORY_PARENT_MODE=if($LegacyHistoryParent){'0'}else{'1'}
    # This is the user's requested visible, foreground interactive benchmark.
    # SW_HIDE suppresses the native game window even when Slate reports active.
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $arguments -WindowStyle Normal -PassThru
    $process.Id | Set-Content "$output/pid.txt"
    # Driver cannot start measuring before our approval file. We stop all
    # activation calls before writing that file, closing the quality.json race.
    $activationDeadline=(Get-Date).AddSeconds($ForegroundTimeoutSeconds)
    $nextAttempt=Get-Date
    while (!$process.HasExited -and !$approved) {
        if ((Get-Date) -ge $activationDeadline) { throw 'Bounded foreground startup timeout' }
        if (Test-Path "$output/failed.txt") { throw 'Driver failed before foreground approval' }
        $process.Refresh()
        $live=$null
        if (Test-Path "$output/foreground-live.json") {
            try { $live=Get-Content "$output/foreground-live.json" -Raw | ConvertFrom-Json } catch { $live=$null }
        }
        $fresh=$live -and ((Get-Date)-(Get-Item "$output/foreground-live.json").LastWriteTime).TotalSeconds -lt 2
        if ($fresh -and $live.ready -and $live.engine.foreground -eq 1 -and !$live.engine.minimized) {
            $approved=$true
            [ordered]@{ time=(Get-Date).ToString('o'); attempts=$attempts; engine=$live.engine; stable_frames=$live.consecutive } |
                ConvertTo-Json -Depth 6 | Set-Content "$output/foreground-approved.json"
            break
        }
        if ((Test-Path "$output/quality.json") -or (Test-Path "$output/foreground-confirmed.json")) {
            throw 'Driver entered measurement without runner approval'
        }
        if ((Test-Path "$output/viewport-ready.json") -and $process.MainWindowHandle -ne 0 -and (Get-Date) -ge $nextAttempt -and
            !($fresh -and $live.engine.foreground -eq 1)) {
            if ($attempts -ge 8) { throw 'Foreground activation exhausted eight attempts' }
            $attempts++
            [ordered]@{ time=(Get-Date).ToString('o'); attempt=$attempts; pid=$process.Id; handle=$process.MainWindowHandle.ToInt64();
                win32_result=[GrayBenchmarkSession]::Activate($process.MainWindowHandle,$process.Id); telemetry=$live } |
                ConvertTo-Json -Depth 6 -Compress | Add-Content "$output/window-activation.jsonl"
            $nextAttempt=(Get-Date).AddSeconds(2)
        }
        Start-Sleep -Milliseconds 100
    }
    if (!$approved) { throw 'UE exited before foreground approval' }
    if (!$process.WaitForExit($RunTimeoutSeconds*1000)) { throw 'Bounded performance run timeout' }
    $code=$process.ExitCode
} catch {
    $failure=$_.Exception.Message
    [ordered]@{ time=(Get-Date).ToString('o'); reason=$failure; attempts=$attempts; approved=$approved } |
        ConvertTo-Json | Set-Content "$output/foreground-abort.json"
    # Give the Python driver a chance to log/QUIT, then close only our own UE.
    if ($process -and !$process.HasExited -and !$process.WaitForExit(10000)) {
        $null=$process.CloseMainWindow()
        if (!$process.WaitForExit(5000)) { $process.Kill(); $process.WaitForExit() }
    }
    if ($process -and $process.HasExited) { $code=$process.ExitCode }

} finally {
    if ($guard) {
        $guard.Dispose()
        [ordered]@{ acquired=$guard.ExecutionState; restored=$guard.RestoreState; finished=(Get-Date).ToString('o') } |
            ConvertTo-Json | Set-Content "$output/power-guard.json"
    }
    $env:DARKWELL_STABILIZATION_OUTPUT=$priorOutput
    $env:DARKWELL_STABILIZATION_MODE=$priorMode
    $env:DARKWELL_STABILIZATION_PROTOCOL=$priorProtocol
    $env:DARKWELL_STABILIZATION_MAP=$priorMap
    $env:DARKWELL_HISTORY_PARENT_MODE=$priorParent
    $env:DARKWELL_A1_VISUAL=$priorA1Visual
}
$log=if(Test-Path "$output/editor.log"){Get-Content "$output/editor.log" -Raw}else{""}
$summary=[ordered]@{ runner_failure=$failure; foreground_approved=$approved; foreground_attempts=$attempts; exit_code=$code; exit_hex=('0x{0:X8}' -f ($code -band 0xffffffffL)); wall_seconds=((Get-Date)-$start).TotalSeconds; complete=(Test-Path "$output/complete.json"); severe_lines=([regex]::Matches($log,'Fatal error:|Assertion failed:|Ensure condition failed:|EXCEPTION_ACCESS_VIOLATION|Traceback')).Count; log_closed=$log.Contains('Log file closed'); d3d12_sm6=$log.Contains('D3D12') -and $log.Contains('PCD3D_SM6') }
$summary | ConvertTo-Json | Set-Content "$output/summary.json"
$summary | ConvertTo-Json
if($failure -or !$approved -or (Test-Path "$output/failed.txt") -or (Test-Path "$output/foreground-lost.json") -or $code -ne 0 -or -not $summary.complete -or $summary.severe_lines -gt 0){exit 1}

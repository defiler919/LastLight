[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [ValidateSet('PIE','Standalone')][string]$Mode='PIE',
    [ValidateSet('Smoke','Knowledge','Attribution','Matrix','LongRun','Reference')][string]$Protocol='Matrix',
    [string]$Map='',
    [string]$EngineRoot='D:\UE_5.8',
    [switch]$NoAuthoringToolsets,
    [switch]$Trace
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use a unique simple run name' }
$workloads=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealInsights|ShaderCompileWorker|SpaceCraft|MSBuild|cl|link|UnrealBuildTool|AutomationTool)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool') })
if($workloads.Count) { throw "Conflicting workload: $($workloads.Name -join ', ')" }
$output=Join-Path $repo "Saved/Stabilization/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence exists: $output"}
New-Item -ItemType Directory -Path $output | Out-Null
if (!$Map) { $Map = if ($Protocol -eq 'Reference') { '/Game/Maps/L_Prototype' } else { '/Game/Maps/L_SightWeaveGrayPolicyLab' } }
if ($Protocol -ne 'Reference' -and $Map -ne '/Game/Maps/L_SightWeaveGrayPolicyLab') { throw 'The gray Matrix is frozen to its Lab map' }
$driver=if ($Protocol -eq 'Reference') { "$repo/Content/Python/profile_gray_project_reference.py" } else { "$repo/Content/Python/profile_gray_stabilization.py" }
Copy-Item -LiteralPath $driver -Destination "$output/driver.py"
git -C $repo diff HEAD --binary | Set-Content "$output/source.patch"
git -C $repo status --porcelain=v1 | Set-Content "$output/worktree.txt"
Copy-Item -LiteralPath "$repo/Config/DefaultEngine.ini" -Destination "$output/DefaultEngine.ini"
$binary=Get-Item "$repo/Binaries/Win64/UnrealEditor-Darkwell.dll"
$start=Get-Date
$arguments=@("$repo/Darkwell.uproject",$Map,'-d3d12','-sm6','-NoSound','-unattended','-NoSplash','-NoVSync','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-WinX=30','-WinY=30',"-abslog=$output/editor.log")
$disabledPlugins=@()
if ($NoAuthoringToolsets) {
    # Explicit diagnostic environment only. The regular Editor/project remains
    # unchanged. Disable parents too: dependencies can re-enable ToolsetRegistry.
    $disabledPlugins=@('ToolsetRegistry','AIAssistant','ModelContextProtocol') +
        @(Get-ChildItem "$EngineRoot/Engine/Plugins/Experimental/Toolsets" -Directory | Select-Object -ExpandProperty Name)
    $arguments += "-DisablePlugins=$($disabledPlugins -join ',')"
}
if($Mode -eq 'Standalone') { $arguments+=@('-game','-EnablePython',"-ExecCmds=`"py $output/driver.py`"") }
else { $arguments+="-ExecutePythonScript=$output/driver.py" }
if($Trace) { $arguments+=@('-trace=cpu,gpu,frame,bookmark,region',"-tracefile=$output/capture.utrace") }
$metadata=[ordered]@{
    schema=1; sha=(& git -C $repo rev-parse HEAD); started_utc=$start.ToUniversalTime().ToString('o'); mode=$Mode; protocol=$Protocol
    engine=(Get-Content "$EngineRoot/Engine/Build/Build.version" -Raw | ConvertFrom-Json)
    cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate)
    ram_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    binary_built_utc=$binary.LastWriteTimeUtc.ToString('o'); binary_sha256=(Get-FileHash $binary.FullName).Hash
    driver_sha256=(Get-FileHash $driver).Hash; map=$Map; seed=0
    screenshots=($Protocol -eq 'Reference'); trace=[bool]$Trace; debugger=$false; fixed_timestep=$false; other_engine_build_processes=$workloads
    arguments=$arguments; processes_at_start=@(Get-Process | Select-Object Name,Id,CPU,WorkingSet64)
    disabled_authoring_plugins=$disabledPlugins
    editor_binary_sha256=(Get-FileHash "$repo/Binaries/Win64/UnrealEditor-DarkwellEditor.dll").Hash
    timing_note='Wall intervals between distinct game updates include Python measurement cost; engine GT/RT/RHI/GPU counters are delayed and overlap. PIE global Render/RHI counters can be overwritten by Slate window updates; use separate Insights capture for attribution. No subtraction attribution.'
}
$metadata | ConvertTo-Json -Depth 8 | Set-Content "$output/environment.json"
$priorOutput=$env:DARKWELL_STABILIZATION_OUTPUT
$priorMode=$env:DARKWELL_STABILIZATION_MODE
$priorProtocol=$env:DARKWELL_STABILIZATION_PROTOCOL
$priorMap=$env:DARKWELL_STABILIZATION_MAP
try {
    $env:DARKWELL_STABILIZATION_OUTPUT=$output
    $env:DARKWELL_STABILIZATION_MODE=$Mode
    $env:DARKWELL_STABILIZATION_PROTOCOL=$Protocol
    $env:DARKWELL_STABILIZATION_MAP=$Map
    # This is the user's requested visible, foreground interactive benchmark.
    # SW_HIDE suppresses the native game window even when Slate reports active.
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $arguments -WindowStyle Normal -PassThru
    $process.Id | Set-Content "$output/pid.txt"
    $process.WaitForExit()
    $code=$process.ExitCode
} finally {
    $env:DARKWELL_STABILIZATION_OUTPUT=$priorOutput
    $env:DARKWELL_STABILIZATION_MODE=$priorMode
    $env:DARKWELL_STABILIZATION_PROTOCOL=$priorProtocol
    $env:DARKWELL_STABILIZATION_MAP=$priorMap
}
$log=Get-Content "$output/editor.log" -Raw
$summary=[ordered]@{ exit_code=$code; exit_hex=('0x{0:X8}' -f ($code -band 0xffffffffL)); wall_seconds=((Get-Date)-$start).TotalSeconds; complete=(Test-Path "$output/complete.json"); severe_lines=@(Select-String "$output/editor.log" -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|EXCEPTION_ACCESS_VIOLATION|Traceback').Count; log_closed=$log.Contains('Log file closed'); d3d12_sm6=$log.Contains('D3D12') -and $log.Contains('PCD3D_SM6') }
$summary | ConvertTo-Json | Set-Content "$output/summary.json"
$summary | ConvertTo-Json
if($code -ne 0 -or -not $summary.complete -or $summary.severe_lines -gt 0){exit 1}

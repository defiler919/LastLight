[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName, [string]$EngineRoot='D:\UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if($RunName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple unique name'}
$output=Join-Path $repo "Saved/ArchitectureAudit/$RunName"
if(Test-Path -LiteralPath $output){throw "Evidence already exists: $output"}
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -LiteralPath "$repo/Content/Python/profile_gray_memory_transitions.py" -Destination "$output/driver.py"
git -C $repo rev-parse HEAD | Set-Content "$output/source.txt"
git -C $repo diff --binary | Set-Content "$output/source.patch"
$prior=$env:DARKWELL_AUDIT_OUTPUT
$env:DARKWELL_AUDIT_OUTPUT=$output
$start=Get-Date
try {
 $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList @(
  "$repo/Darkwell.uproject",'/Game/Maps/L_SightWeaveGrayPolicyLab','-d3d12','-sm6','-nosound','-unattended',
  '-trace=cpu,frame,bookmark,log',"-tracefile=$output/capture.utrace",
  "-ExecutePythonScript=$output/driver.py","-abslog=$output/editor.log"
 ) -WindowStyle Hidden -PassThru -Wait
} finally {$env:DARKWELL_AUDIT_OUTPUT=$prior}
$log=Get-Content "$output/editor.log" -Raw
$summary=[ordered]@{
 exit_code=$process.ExitCode
 wall_seconds=((Get-Date)-$start).TotalSeconds
 protocol_complete=(Test-Path "$output/complete.json")
 teardown_complete=$log.Contains('MEMORY_TRANSITION_STOPPED')
 severe_lines=@(Select-String "$output/editor.log" -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|EXCEPTION_ACCESS_VIOLATION|Traceback').Count
 d3d12_sm6=$log.Contains('D3D12') -and $log.Contains('PCD3D_SM6')
}
$summary.protocol_pass=$summary.exit_code -eq 0 -and $summary.protocol_complete -and $summary.teardown_complete -and $summary.severe_lines -eq 0 -and $summary.d3d12_sm6
$summary | ConvertTo-Json | Set-Content "$output/summary.json"
$summary | ConvertTo-Json
# Timing thresholds are assessed separately; protocol completion alone is not a performance pass.
if(-not $summary.protocol_pass){exit 1}

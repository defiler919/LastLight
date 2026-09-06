[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [ValidateSet('MainWindow','Python')][string]$Mode='Python',
    [ValidateRange(0,10)][int]$Cycles=0,
    [string]$EngineRoot='D:\UE_5.8',
    [string]$Map='/Game/Maps/L_SightWeaveGrayPolicyLab',
    [switch]$Debugger,
    [string[]]$ExtraEditorArgs=@()
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use a unique simple run name' }
if (Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) { throw 'An Editor is already running' }
$output=Join-Path $repo "Saved/Stabilization/$RunName"
if (Test-Path -LiteralPath $output) { throw "Evidence exists: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
git -C $repo diff HEAD --binary | Set-Content "$output/source.patch"
$start=Get-Date
$source=[ordered]@{ sha=(& git -C $repo rev-parse HEAD); mode=$Mode; cycles=$Cycles; map=$Map; debugger=[bool]$Debugger; started=$start.ToString('o'); extra_arguments=$ExtraEditorArgs }
$source | ConvertTo-Json | Set-Content "$output/source.json"
$priorOutput=$env:DARKWELL_EXIT_OUTPUT
$priorCycles=$env:DARKWELL_EXIT_CYCLES
try {
    $env:DARKWELL_EXIT_OUTPUT=$output
    $env:DARKWELL_EXIT_CYCLES="$Cycles"
    $arguments=@("$repo/Darkwell.uproject",$Map,'-d3d12','-sm6','-NoSound','-NoSplash',"-abslog=$output/editor.log") + $ExtraEditorArgs
    if ($Mode -eq 'Python') {
        Copy-Item -LiteralPath "$repo/Content/Python/audit_gray_exit.py" -Destination "$output/driver.py"
        $arguments += @('-unattended',"-ExecutePythonScript=$output/driver.py")
    }
    # Visible main window is the interactive target of the requested close test.
    $style=if($Mode -eq 'MainWindow'){'Normal'}else{'Hidden'}
    $executable="$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe"
    if ($Debugger) {
        $arguments=@($output,$executable)+$arguments
        $executable="$repo/Saved/ArchitectureAudit/WatchUnrealExit.exe"
        if (-not (Test-Path -LiteralPath $executable)) { throw 'Build Scripts/WatchUnrealExit.cpp before using the debugger' }
    }
    $process=Start-Process $executable -ArgumentList $arguments -WindowStyle $style -PassThru
    $process.Id | Set-Content "$output/pid.txt"
    $process.WaitForExit()
    $code=$process.ExitCode
} finally {
    $env:DARKWELL_EXIT_OUTPUT=$priorOutput
    $env:DARKWELL_EXIT_CYCLES=$priorCycles
}
$log=Get-Content "$output/editor.log" -Raw
$summary=[ordered]@{ exit_code=$code; exit_hex=('0x{0:X8}' -f ($code -band 0xffffffffL)); debugger=[bool]$Debugger; wall_seconds=((Get-Date)-$start).TotalSeconds; mode=$Mode; cycles=$Cycles; log_closed=$log.Contains('Log file closed'); protocol_complete=($Mode -eq 'MainWindow' -or (Test-Path "$output/complete.json")); severe_lines=@(Select-String "$output/editor.log" -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|EXCEPTION_ACCESS_VIOLATION|Traceback').Count }
$summary | ConvertTo-Json | Set-Content "$output/summary.json"
$summary | ConvertTo-Json
if ($code -ne 0 -or -not $summary.protocol_complete -or $summary.severe_lines -gt 0) { exit 1 }

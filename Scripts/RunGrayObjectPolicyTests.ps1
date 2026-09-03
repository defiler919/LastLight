[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$Tests = 'Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1+SightWeave.ObjectPolicy+SightWeave.RevealPolicy',
    [string]$EngineRoot = 'D:\UE_5.8',
    [string[]]$ExtraEditorArgs = @()
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'RunName must be a simple unique name' }
$evidence = Join-Path (Join-Path $repo 'Saved/GrayObjectPolicy') $RunName
if (Test-Path -LiteralPath $evidence) { throw "Evidence already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$log = Join-Path $evidence "$RunName.log"
$report = Join-Path $evidence "${RunName}_Report"
if (Test-Path -LiteralPath $log) { throw "Evidence already exists: $log" }
$begin = Get-Date
$source = [ordered]@{ head=(& git -C $repo rev-parse HEAD); started=$begin.ToString('o'); editor_args=$ExtraEditorArgs }
$source | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'source.json')
& git -C $repo diff --binary | Set-Content -LiteralPath (Join-Path $evidence 'source.patch')
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$repo/Darkwell.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound "-ExecCmds=Automation RunTests $Tests" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$log" @ExtraEditorArgs *> (Join-Path $evidence "$RunName.stdout.txt")
$code = $LASTEXITCODE
$summary = [ordered]@{ run=$RunName; selector=$Tests; exit_code=$code; wall_seconds=((Get-Date)-$begin).TotalSeconds }
if (Test-Path -LiteralPath "$report/index.json") {
    $r = Get-Content -LiteralPath "$report/index.json" -Raw | ConvertFrom-Json
    $summary.total = $r.succeeded + $r.succeededWithWarnings + $r.failed + $r.notRun
    $summary.clean = $r.succeeded
    $summary.warnings = $r.succeededWithWarnings
    $summary.failed = $r.failed
    $summary.not_run = $r.notRun
    $summary.duration = $r.totalDuration
}
$summary.severe_lines = @(Select-String -LiteralPath $log -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE_REMOVED|DXGI_ERROR_DEVICE_HUNG|EXCEPTION_ACCESS_VIOLATION').Count
$summary.passed = $code -eq 0 -and $summary.total -gt 0 -and $summary.failed -eq 0 -and $summary.not_run -eq 0 -and $summary.severe_lines -eq 0
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence "$RunName.summary.json")
$summary | ConvertTo-Json
if (-not $summary.passed -and $code -eq 0) { exit 1 }
exit $code

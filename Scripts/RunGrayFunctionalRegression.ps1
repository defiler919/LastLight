[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$RunName, [string]$EngineRoot='D:\UE_5.8')
$ErrorActionPreference='Stop'
$manifest = Get-Content -LiteralPath "$PSScriptRoot/TestManifests/GrayFunctional.json" -Raw | ConvertFrom-Json
& "$PSScriptRoot/RunGrayObjectPolicyTests.ps1" -RunName $RunName -EngineRoot $EngineRoot -Tests $manifest.selector
$runCode = $LASTEXITCODE
$reportPath = Join-Path (Split-Path $PSScriptRoot -Parent) "Saved/GrayObjectPolicy/$RunName/${RunName}_Report/index.json"
if (-not (Test-Path -LiteralPath $reportPath)) { throw 'Functional report is missing' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$missing = @($manifest.baseline_test_names | Where-Object { $_ -notin $report.tests.fullTestPath })
$coverage = [ordered]@{ baseline_count=$manifest.expected_baseline_count; actual_count=@($report.tests).Count; missing=$missing; new_tests=@($report.tests.fullTestPath | Where-Object { $_ -notin $manifest.baseline_test_names }) }
$coverage | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path (Split-Path $reportPath -Parent) 'baseline-coverage.json')
if ($missing.Count -gt 0) { throw "Frozen regression cases missing: $($missing -join ', ')" }
if ($runCode -ne 0) { exit $runCode }

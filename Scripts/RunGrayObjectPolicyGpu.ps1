[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot = 'D:\UE_5.8',
    [switch]$LongInteraction
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'RunName must be a simple unique name' }
$evidence = Join-Path (Join-Path $repo 'Saved/GrayObjectPolicy') $RunName
if (Test-Path -LiteralPath $evidence) { throw "Evidence already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$log = Join-Path $evidence "$RunName.log"
$driver = Join-Path $repo 'Content/Python/verify_gray_object_policy.py'
$begin = Get-Date
$extraArgs = @()
if ($LongInteraction) { $extraArgs += '-GrayGpuLongInteraction' }
[ordered]@{head=(& git -C $repo rev-parse HEAD);started=$begin.ToString('o');driver_hash=(Get-FileHash -LiteralPath $driver).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'source.json')
& git -C $repo diff --binary | Set-Content -LiteralPath (Join-Path $evidence 'source.patch')
Copy-Item -LiteralPath $driver -Destination (Join-Path $evidence 'driver.py')
$editorArguments = @("$repo/Darkwell.uproject", '/Game/Maps/L_ProjectFogPropGameplayLab', '-d3d12', '-sm6', '-PropLabMovingControls', '-PropLabGrayObjectPolicies', '-PropLabAsyncCapture', '-nosound', '-unattended', '-UseFixedTimeStep', '-FPS=60', "-ExecutePythonScript=$driver", "-abslog=$log") + $extraArgs
$process = Start-Process -FilePath "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $editorArguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $evidence "$RunName.stdout.txt") -RedirectStandardError (Join-Path $evidence "$RunName.stderr.txt")
$code = $process.ExitCode
$content = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
$summary = [ordered]@{run=$RunName;exit_code=$code;wall_seconds=((Get-Date)-$begin).TotalSeconds}
$summary.severe_lines = @(Select-String -LiteralPath $log -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE_REMOVED|DXGI_ERROR_DEVICE_HUNG|EXCEPTION_ACCESS_VIOLATION|GRAY_GPU_FAIL').Count
$summary.gpu_pass = $content.Contains('GRAY_GPU_PASS ')
$summary.pie_stopped = $content.Contains('GRAY_GPU_PIE_STOPPED')
$summary.callback_unregistered = $content.Contains('GRAY_GPU_CALLBACK_UNREGISTERED')
$summary.d3d12_sm6 = $content -match 'D3D12' -and $content -match 'PCD3D_SM6'
$summary.passed = $code -eq 0 -and $summary.severe_lines -eq 0 -and $summary.gpu_pass -and $summary.pie_stopped -and $summary.callback_unregistered -and $summary.d3d12_sm6
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence "$RunName.summary.json")
$summary | ConvertTo-Json
if (-not $summary.passed -and $code -eq 0) { exit 1 }
exit $code

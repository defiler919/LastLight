[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot = 'D:\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($RunName -notmatch '^[A-Za-z0-9_-]+$') { throw 'RunName must be a simple unique name' }
$evidence = Join-Path $repo "Saved/GrayPolicyLabV2/Company_20260904/$RunName"
if (Test-Path -LiteralPath $evidence) { throw "Evidence already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$log = Join-Path $evidence "$RunName.log"
$driver = Join-Path $repo 'Content/Python/verify_gray_policy_lab_v2.py'
$begin = Get-Date
[ordered]@{
    head = (& git -C $repo rev-parse HEAD)
    started = $begin.ToString('o')
    driver_hash = (Get-FileHash -LiteralPath $driver).Hash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'source.json')
& git -C $repo diff --binary | Set-Content -LiteralPath (Join-Path $evidence 'source.patch')
Copy-Item -LiteralPath $driver -Destination (Join-Path $evidence 'driver.py')
$arguments = @(
    "$repo/Darkwell.uproject",
    '/Game/Maps/L_SightWeaveGrayPolicyLab',
    '-d3d12', '-sm6', '-nosound', '-unattended', '-UseFixedTimeStep', '-FPS=60',
    "-ExecutePythonScript=$driver", "-abslog=$log"
)
$process = Start-Process -FilePath "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" `
    -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $evidence "$RunName.stdout.txt") `
    -RedirectStandardError (Join-Path $evidence "$RunName.stderr.txt")
$content = if (Test-Path -LiteralPath $log) { Get-Content -Raw -LiteralPath $log } else { '' }
$summary = [ordered]@{
    run = $RunName
    exit_code = $process.ExitCode
    wall_seconds = ((Get-Date) - $begin).TotalSeconds
    severe_lines = @(Select-String -LiteralPath $log -Pattern 'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE_REMOVED|DXGI_ERROR_DEVICE_HUNG|EXCEPTION_ACCESS_VIOLATION|SightWeave activation failed|M6P1.*request failed|GRAY_POLICY_LAB_V2_GPU_FAIL').Count
    gpu_pass = $content.Contains('GRAY_POLICY_LAB_V2_GPU_PASS ')
    pie_stopped = $content.Contains('GRAY_POLICY_LAB_V2_PIE_STOPPED')
    callback_unregistered = $content.Contains('GRAY_POLICY_LAB_V2_CALLBACK_UNREGISTERED')
    d3d12_sm6 = ($content -match 'D3D12') -and ($content -match 'PCD3D_SM6')
}
$summary.passed = $summary.exit_code -eq 0 -and $summary.severe_lines -eq 0 `
    -and $summary.gpu_pass -and $summary.pie_stopped `
    -and $summary.callback_unregistered -and $summary.d3d12_sm6
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence "$RunName.summary.json")
$summary | ConvertTo-Json
if (-not $summary.passed -and $process.ExitCode -eq 0) { exit 1 }
exit $process.ExitCode

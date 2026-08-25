[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('default', 'no-uba')]
    [string]$Group,

    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 99)]
    [int]$RunIndex,

    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [string]$EngineRoot = 'D:\UE_5.8',

    [string]$OutputRoot = 'D:\UE_projects\LastLight\Saved\SightWeaveM2P4\Toolchain\epic-verify-20260825'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Get-ProcessSnapshot {
    $blockedPatterns = @(
        '^UnrealEditor(?:-Cmd)?\.exe$',
        '^UnrealBuildTool\.exe$',
        '^AutomationTool\.exe$',
        '^dotnet\.exe$',
        '^cl\.exe$',
        '^link\.exe$',
        '^msbuild\.exe$',
        '^devenv\.exe$',
        '^LiveCodingConsole\.exe$',
        '^ShaderCompileWorker\.exe$',
        '^Uba.*\.exe$'
    )

    @(Get-CimInstance Win32_Process | Where-Object {
        $name = [string]$_.Name
        $blockedPatterns | Where-Object { $name -match $_ }
    } | Select-Object ProcessId, ParentProcessId, Name, ExecutablePath, CommandLine)
}

function Get-EventRecords {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogName,
        [Parameter(Mandatory = $true)]
        [int[]]$Ids,
        [Parameter(Mandatory = $true)]
        [datetime]$StartTime,
        [Parameter(Mandatory = $true)]
        [datetime]$EndTime
    )

    try {
        @(Get-WinEvent -FilterHashtable @{
            LogName = $LogName
            Id = $Ids
            StartTime = $StartTime
            EndTime = $EndTime
        } -ErrorAction Stop | Select-Object TimeCreated, Id, ProviderName, LevelDisplayName, RecordId, Message)
    }
    catch [System.Exception] {
        if ($_.Exception.Message -match 'No events were found') {
            return @()
        }
        throw
    }
}

function Get-NewFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Roots,
        [Parameter(Mandatory = $true)]
        [datetime]$Since
    )

    $items = foreach ($root in $Roots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }
        try {
            Get-ChildItem -LiteralPath $root -File -Recurse -ErrorAction Stop |
                Where-Object { $_.LastWriteTime -ge $Since } |
                Select-Object FullName, Length, CreationTime, LastWriteTime
        }
        catch {
            Write-Warning "Unable to enumerate diagnostic root '$root': $($_.Exception.Message)"
        }
    }
    @($items)
}

$projectRootPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
$engineRootPath = (Resolve-Path -LiteralPath $EngineRoot).Path
$projectPath = Join-Path $projectRootPath 'Darkwell.uproject'
$buildBat = Join-Path $engineRootPath 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path -LiteralPath $projectPath)) {
    throw "Project not found: $projectPath"
}
if (-not (Test-Path -LiteralPath $buildBat)) {
    throw "Build.bat not found: $buildBat"
}

$runDirectory = Join-Path (Join-Path $OutputRoot $Group) ('run-{0:D2}' -f $RunIndex)
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null

$preExistingProcesses = @(Get-ProcessSnapshot)
$preExistingProcesses | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'pre-build-processes.json') -Encoding utf8
if ($preExistingProcesses.Count -ne 0) {
    throw "Concurrent build/editor process detected. See pre-build-processes.json in $runDirectory"
}

$arguments = @('DarkwellEditor', 'Win64', 'Development', $projectPath, '-WaitMutex', '-FromMsBuild')
if ($Group -eq 'no-uba') {
    $arguments += '-NoUBA'
}

$displayArguments = $arguments | ForEach-Object {
    if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
}
$commandLine = '"{0}" {1}' -f $buildBat, ($displayArguments -join ' ')
$commandLine | Set-Content -LiteralPath (Join-Path $runDirectory 'command.txt') -Encoding utf8

$start = Get-Date
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$outputPath = Join-Path $runDirectory 'build-output.txt'

Push-Location $projectRootPath
try {
    & $buildBat @arguments 2>&1 | Tee-Object -FilePath $outputPath
    $outerExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
    $stopwatch.Stop()
}
$end = Get-Date
Start-Sleep -Seconds 2
$eventQueryEnd = (Get-Date).AddSeconds(2)

$outputText = if (Test-Path -LiteralPath $outputPath) {
    Get-Content -LiteralPath $outputPath -Raw
} else {
    ''
}
$ubtResultMatches = [regex]::Matches($outputText, '(?im)^Result:\s*([^\r\n]+)')
$ubtResult = if ($ubtResultMatches.Count -gt 0) {
    $ubtResultMatches[$ubtResultMatches.Count - 1].Groups[1].Value.Trim()
} else {
    $null
}

$systemEvents = @(Get-EventRecords -LogName System -Ids @(26) -StartTime $start.AddSeconds(-2) -EndTime $eventQueryEnd)
$applicationEvents = @(Get-EventRecords -LogName Application -Ids @(1026, 1000, 1001) -StartTime $start.AddSeconds(-2) -EndTime $eventQueryEnd)
$relevantEventPattern = '(?i)dotnet(?:\.exe)?|UnrealBuildTool|0xE0434352'
$relevantSystemEvents = @($systemEvents | Where-Object { [string]$_.Message -match $relevantEventPattern })
$relevantApplicationEvents = @($applicationEvents | Where-Object { [string]$_.Message -match $relevantEventPattern })
$eventPayload = [ordered]@{
    QueryStart = $start.AddSeconds(-2).ToString('o')
    QueryEnd = $eventQueryEnd.ToString('o')
    System26 = $systemEvents
    RelevantSystem26 = $relevantSystemEvents
    Application1026_1000_1001 = $applicationEvents
    RelevantApplication1026_1000_1001 = $relevantApplicationEvents
}
$eventPayload | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runDirectory 'events.json') -Encoding utf8

$visibleFaultProcesses = @(Get-Process | Where-Object {
    $_.ProcessName -in @('dotnet', 'WerFault', 'WerFaultSecure') -and -not [string]::IsNullOrWhiteSpace($_.MainWindowTitle)
} | Select-Object Id, ProcessName, MainWindowTitle, Path)
$visibleFaultProcesses | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'visible-fault-windows.json') -Encoding utf8

$artifactCandidates = @(
    Join-Path $env:LOCALAPPDATA 'UnrealBuildTool\Log.txt'
    Join-Path $env:LOCALAPPDATA 'UnrealBuildTool\Log.json'
    Join-Path $env:LOCALAPPDATA 'UnrealBuildTool\Trace.uba'
    Join-Path $projectRootPath 'Saved\UnrealBuildTool\Log.txt'
    Join-Path $projectRootPath 'Saved\UnrealBuildTool\Log.json'
    Join-Path $projectRootPath 'Saved\UnrealBuildTool\Trace.uba'
)
$copiedArtifacts = @()
foreach ($candidate in $artifactCandidates) {
    if (-not (Test-Path -LiteralPath $candidate)) {
        continue
    }
    $item = Get-Item -LiteralPath $candidate
    $destinationName = if ($candidate.StartsWith($env:LOCALAPPDATA, [System.StringComparison]::OrdinalIgnoreCase)) {
        'LocalAppData-' + $item.Name
    } else {
        'Project-' + $item.Name
    }
    $destination = Join-Path $runDirectory $destinationName
    Copy-Item -LiteralPath $candidate -Destination $destination -Force
    $copiedArtifacts += [pscustomobject]@{
        Source = $candidate
        Copy = $destination
        Length = $item.Length
        LastWriteTime = $item.LastWriteTime
        Sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    }
}
$copiedArtifacts | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'ubt-artifacts.json') -Encoding utf8

$werRoots = @(
    Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\WER\ReportArchive'
    Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\WER\ReportQueue'
    Join-Path $env:ProgramData 'Microsoft\Windows\WER\ReportArchive'
    Join-Path $env:ProgramData 'Microsoft\Windows\WER\ReportQueue'
)
$dumpRoots = @(
    Join-Path $env:LOCALAPPDATA 'CrashDumps'
    Join-Path $OutputRoot 'dumps'
)
$werFiles = @(Get-NewFiles -Roots $werRoots -Since $start.AddSeconds(-2))
$dumpFiles = @(Get-NewFiles -Roots $dumpRoots -Since $start.AddSeconds(-2))
$werFiles | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'wer-files.json') -Encoding utf8
$dumpFiles | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'dump-files.json') -Encoding utf8

$outerExitUnsigned = [uint32]([int64]$outerExitCode -band 0xffffffffL)
$outerExitHex = '0x{0:X8}' -f $outerExitUnsigned
$popupObserved = ($relevantSystemEvents.Count -gt 0) -or ($visibleFaultProcesses.Count -gt 0)
$applicationCrashObserved = $relevantApplicationEvents.Count -gt 0
$passed = ($ubtResult -eq 'Succeeded') -and ($outerExitCode -eq 0) -and (-not $popupObserved) -and (-not $applicationCrashObserved)

$summary = [ordered]@{
    Group = $Group
    RunIndex = $RunIndex
    ProjectRoot = $projectRootPath
    Project = $projectPath
    EngineRoot = $engineRootPath
    Command = $commandLine
    StartTime = $start.ToString('o')
    EndTime = $end.ToString('o')
    ElapsedSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    UbtResult = $ubtResult
    OuterExitCodeSigned = $outerExitCode
    OuterExitCodeHex = $outerExitHex
    PopupObserved = $popupObserved
    VisibleFaultWindows = $visibleFaultProcesses
    SystemEvent26Count = $systemEvents.Count
    RelevantSystemEvent26Count = $relevantSystemEvents.Count
    Application1026_1000_1001Count = $applicationEvents.Count
    RelevantApplication1026_1000_1001Count = $relevantApplicationEvents.Count
    ApplicationCrashObserved = $applicationCrashObserved
    WerFilesGenerated = $werFiles.Count
    DumpFilesGenerated = $dumpFiles.Count
    UbtArtifacts = $copiedArtifacts
    Passed = $passed
}
$summaryPath = Join-Path $runDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8
$summary | ConvertTo-Json -Depth 8

if (-not $passed) {
    exit 1
}
exit 0

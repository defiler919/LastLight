[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [Parameter(Mandatory=$true)][string]$EvidenceName,
    [Parameter(Mandatory=$true)][string]$Toolset,
    [Parameter(Mandatory=$true)][string]$ToolName,
    [hashtable]$Arguments=@{}
)
$ErrorActionPreference='Stop'
if ($RunName -notmatch '^[A-Za-z0-9_-]+$' -or $EvidenceName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use simple evidence names' }
$repo=Split-Path $PSScriptRoot -Parent
$directory=Join-Path $repo "Saved/Stabilization/$RunName"
if (!(Test-Path -LiteralPath $directory)) { throw 'Start the Editor evidence runner first' }
$output=Join-Path $directory "$EvidenceName.json"
if (Test-Path -LiteralPath $output) { throw 'Evidence already exists' }
$uri='http://127.0.0.1:8000/mcp'
$headers=@{Accept='application/json, text/event-stream';'MCP-Protocol-Version'='2025-11-25'}
$sessionPath=Join-Path $directory 'mcp-init-headers.json'
if (!(Test-Path -LiteralPath $sessionPath)) {
    $body=@{jsonrpc='2.0';id=1;method='initialize';params=@{protocolVersion='2025-11-25';capabilities=@{};clientInfo=@{name='DarkwellStabilization';version='1'}}}
    $response=Invoke-WebRequest -Uri $uri -Method Post -ContentType application/json -Headers $headers -Body ($body|ConvertTo-Json -Depth 8 -Compress)
    $response.Headers|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $sessionPath
    $response.Content|Set-Content -LiteralPath (Join-Path $directory 'mcp-init.json')
    $headers['Mcp-Session-Id']=$response.Headers['Mcp-Session-Id'][0]
    Invoke-WebRequest -Uri $uri -Method Post -ContentType application/json -Headers $headers -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}'|Out-Null
} else {
    $headers['Mcp-Session-Id']=(Get-Content -LiteralPath $sessionPath -Raw|ConvertFrom-Json).'Mcp-Session-Id'[0]
}
$body=@{jsonrpc='2.0';id=2;method='tools/call';params=@{name='call_tool';arguments=@{toolset_name=$Toolset;tool_name=$ToolName;arguments=$Arguments}}}
$receipt=[ordered]@{started=(Get-Date).ToString('o');request=$body}
$receipt|ConvertTo-Json -Depth 15|Set-Content -LiteralPath $output
try {
    $response=Invoke-WebRequest -Uri $uri -Method Post -ContentType application/json -Headers $headers -Body ($body|ConvertTo-Json -Depth 15 -Compress) -TimeoutSec 45
    $receipt.result=$response.Content|ConvertFrom-Json
    $receipt.finished=(Get-Date).ToString('o')
    $receipt|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $output
    $response.Content
    if ($receipt.result.error -or $receipt.result.result.isError) { throw 'Unreal MCP reported an error; see receipt' }
} catch {
    $receipt.transport_or_tool_error=$_.Exception.Message
    $receipt|ConvertTo-Json -Depth 30|Set-Content -LiteralPath $output
    throw
}

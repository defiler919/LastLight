param(
 [string]$DataRoot=(Join-Path (Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent) 'Saved/StaticKnowledge'),
 [string]$OutputPath=(Join-Path $PSScriptRoot 'performance.json')
)
$ErrorActionPreference='Stop'
function Stats($Values) {
 $a=@($Values | Sort-Object)
 if(!$a.Count){return $null}
 [ordered]@{mean=($a | Measure-Object -Average).Average; p95=$a[[Math]::Ceiling($a.Count*.95)-1]; p99=$a[[Math]::Ceiling($a.Count*.99)-1]; max=$a[-1]}
}
$result=[ordered]@{}
foreach($name in @('HistNative2','HardUnifiedNative_r1','HardUnifiedNative_r2','HardUnifiedNative_r3','HardUnifiedNative_r4','HardUnifiedNative_r5','HardUnifiedNativeFinal','HardUnifiedReleaseNative')) {
 $dir=Join-Path $DataRoot $name
 $rows=@(Get-Content -LiteralPath (Join-Path $dir 'frames.jsonl') | ConvertFrom-Json)
 $entry=[ordered]@{frames=$rows.Count; source=(Get-Content -LiteralPath (Join-Path $dir 'source.json') -Raw | ConvertFrom-Json); completion=(Get-Content -LiteralPath (Join-Path $dir 'complete.txt') -Raw).Trim(); valid=!(Test-Path (Join-Path $dir 'invalid.txt'))}
 foreach($field in @('game_ms','render_ms','rhi_ms','gpu_ms')){$entry[$field]=Stats $rows.engine.$field}
 $entry['object_ms']=Stats ($rows | ForEach-Object {$_.memory.frame_data.game_thread_us/1000})
 $entry['historical_ms']=Stats ($rows | ForEach-Object {$_.memory.frame_data.historical_us/1000})
 $entry['static_ms']=Stats ($rows | ForEach-Object {$_.static.observe_us/1000})
 foreach($field in @('hard_build_last_us','hard_prepare_us','hard_raster_us','hard_upload_us')) {
  $entry[$field]=Stats ($rows | Where-Object {$null -ne $_.fog.$field} | ForEach-Object {$_.fog.$field})
 }
 $entry['rhi_texture_bytes']=Stats $rows.engine.rhi_texture_bytes
 $entry['conditions']=@($rows | ForEach-Object { '{0}x{1},AA={2},screen={3},foreground={4}' -f $_.engine.viewport[0],$_.engine.viewport[1],$_.engine.aa,$_.engine.screen_percentage,$_.engine.foreground } | Sort-Object -Unique)
 $result[$name]=$entry
}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputPath

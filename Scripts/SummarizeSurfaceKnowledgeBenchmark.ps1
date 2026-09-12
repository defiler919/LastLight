param([Parameter(Mandatory=$true)][string]$InputDirectory,[string]$ReferenceDirectory='',[string]$OutputPath='')
$ErrorActionPreference='Stop'
function Read-Frames([string]$Directory) {
 if(!(Test-Path -LiteralPath "$Directory/complete.txt") -or (Test-Path -LiteralPath "$Directory/invalid.txt")){throw "Incomplete benchmark: $Directory"}
 @(Get-Content -LiteralPath "$Directory/frames.jsonl" | ConvertFrom-Json)
}
function Quantile($Values,[double]$Fraction) {
 $sorted=@($Values|Sort-Object)
 if(!$sorted.Count){return $null}
 $sorted[[Math]::Max(0,[Math]::Ceiling($sorted.Count*$Fraction)-1)]
}
$rows=Read-Frames $InputDirectory
$dirty=@($rows|Where-Object {$_.surface.certified_regions -gt 0 -or $_.surface.authority_queries -gt 0 -or $_.surface.upload_bytes -gt 0})
$command=Get-Content -LiteralPath "$InputDirectory/command.txt" -Raw
$result=[ordered]@{
 run=(Split-Path $InputDirectory -Leaf)
 source=(Get-Content -LiteralPath "$InputDirectory/source.json" -Raw|ConvertFrom-Json)
 completion=(Get-Content -LiteralPath "$InputDirectory/complete.txt" -Raw).Trim()
 diagnostic_timing=($command.Contains('SurfaceGameParity') -or $command.Contains('SightWeave.Surface.Profile=1'))
 frames=$rows.Count; dirty_frames=$dirty.Count
 surface_dirty_p50_us=(Quantile $dirty.surface.update_us .5)
 surface_dirty_p95_us=(Quantile $dirty.surface.update_us .95)
 surface_dirty_max_us=(Quantile $dirty.surface.update_us 1)
 observe_p95_us=(Quantile $dirty.surface.observe_us .95)
 recognition_p95_us=(Quantile $dirty.surface.recognition_us .95)
 publish_p95_us=(Quantile $dirty.surface.publish_us .95)
 engine_game_p95_ms=(Quantile $rows.engine.game_ms .95)
 wall_p95_ms=(Quantile $rows.wall_ms .95)
 max_authority_queries=(Quantile $dirty.surface.authority_queries 1)
 max_upload_bytes=(Quantile $dirty.surface.upload_bytes 1)
 domains=(Quantile $rows.surface.domains 1)
 cells=(Quantile $rows.surface.cells 1)
 atlas_bytes=(Quantile $rows.surface.atlas_bytes 1)
}
if($ReferenceDirectory) {
 $reference=Read-Frames $ReferenceDirectory
 if($rows.Count -ne $reference.Count){throw 'Pose-paired comparison requires equal frame counts'}
 $mismatches=0
 for($i=0;$i -lt $rows.Count;$i++) {
  foreach($field in 'step','x','y','yaw','source_x','source_y') {
   if($null -eq $rows[$i].$field -or $rows[$i].$field -ne $reference[$i].$field){$mismatches++;break}
  }
 }
 $result.reference_run=Split-Path $ReferenceDirectory -Leaf
 $result.pose_mismatches=$mismatches
 $result.matched_poses=$mismatches -eq 0
}
$json=$result|ConvertTo-Json -Depth 8
if($OutputPath){$json|Set-Content -LiteralPath $OutputPath}
$json

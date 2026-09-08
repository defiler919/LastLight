[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot = 'D:\UE_5.8'
)
# Reuse the bounded foreground/D3D12 runner and the prior authority/visual regressions.
& "$PSScriptRoot/RunUnknownPartialCutTests.ps1" -RunName $RunName -EngineRoot $EngineRoot -Tests 'Darkwell.CurrentGrid+Darkwell.BlackRegion.Contract+Darkwell.BlackRegion.CleanLab+Darkwell.BlackRegion.CurrentPartialProbe+Darkwell.BlackRegion.EventDemo+Darkwell.BlackRegion.LabEntry+Darkwell.BlackRegion.TemporalSurface+Darkwell.UnknownRegion+Darkwell.UnknownPartial+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.ObjectMemory.PresentationResidency+Darkwell.PropLab.GrayObjectPolicy.WholeObjectConfirmedStaticHistory+Darkwell.PropLab.GrayObjectPolicy.SpatialPartialStaticKeepsLegalCap+Darkwell.PropLab.GrayObjectPolicy.ConfirmedWholeStopsSpanAndDenseObservationWork+Darkwell.PropLab.MovingLiveContinuity.RotationAcrossWorldBoundsDimensionSwap+Darkwell.PropLab.MovingLiveContinuity.TopologyChangeRequiresExplicitReset'
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
# Keep the real-overlap/death sequence in its own bounded run, without dropping prior tests.
& "$PSScriptRoot/RunUnknownPartialCutTests.ps1" -RunName "${RunName}_Volume" -EngineRoot $EngineRoot -Tests 'Darkwell.BlackRegion.VolumeDemo'
exit $LASTEXITCODE

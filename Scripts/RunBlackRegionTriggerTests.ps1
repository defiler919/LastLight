[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunName,
    [string]$EngineRoot = 'D:\UE_5.8'
)
# Reuse the bounded foreground/D3D12 runner and the prior authority/visual regressions.
& "$PSScriptRoot/RunUnknownPartialCutTests.ps1" -RunName $RunName -EngineRoot $EngineRoot -Tests 'Darkwell.BlackRegion+Darkwell.UnknownRegion+Darkwell.UnknownPartial+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.ObjectMemory.PresentationResidency+Darkwell.PropLab.GrayObjectPolicy.WholeObjectConfirmedStaticHistory+Darkwell.PropLab.GrayObjectPolicy.SpatialPartialStaticKeepsLegalCap'
exit $LASTEXITCODE

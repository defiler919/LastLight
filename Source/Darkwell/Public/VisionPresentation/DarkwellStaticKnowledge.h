#pragma once
#include "CoreMinimal.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

/** Shared immutable-space knowledge. 2.5 cm authority cells retain 4x4 fine support.
 * No actor identity, observation episode, pose history or current raster is stored. */
class DARKWELL_API FDarkwellStaticKnowledge
{
public:
 static constexpr int32 Cells=32;
 static constexpr int32 Subsamples=4;
 static constexpr int32 Side=Cells*Subsamples;
 static constexpr double CellCm=2.5;
 static constexpr double SampleCm=CellCm/Subsamples;
 static constexpr double TileCm=Cells*CellCm;
 struct FTile { TBitArray<> Known; int32 Count=0; FTile():Known(false,Side*Side){} };
 struct FStats { uint64 Candidates=0,TouchedTiles=0,TestedSamples=0,Queries=0,WrittenSamples=0,UniformProofs=0; double UpdateUs=0; } Stats;
 void Declare(const FBox2D& ImmutableFootprint);
 void Observe(const FDarkwellFogVisualSourceSnapshot& Source,TConstArrayView<FDarkwellFogVisualSegment> Segments,const FBox2D& DirtyCoverage);
 void Clear(const FBox2D& Region);
 void SetBlock(const FBox2D& Region,bool Enabled) { Block=Enabled?Region:FBox2D(ForceInit); }
 bool HasMemory(FVector2D P) const;
 bool IsBlocked(FVector2D P) const;
 const TMap<FIntPoint,FTile>& GetTiles() const { return Tiles; }
 TSet<FIntPoint> TakeDirty() { TSet<FIntPoint> Out=MoveTemp(Dirty); Dirty.Reset();return Out; }
 int32 DeclaredTiles() const {return Eligible.Num();}
 static FIntPoint Key(FVector2D P);
 static FBox2D Bounds(FIntPoint K);
 /** Convex common-segment shadow proof only; ambiguous edges use the original oracle. */
 static bool FullyOccluded(const FDarkwellFogVisualSourceSnapshot& Source,const FBox2D& B,TConstArrayView<FDarkwellFogVisualSegment> Segments);
private:
 TSet<FIntPoint> Eligible,Dirty;
 TMap<FIntPoint,FTile> Tiles;
 FBox2D Block=FBox2D(ForceInit);
};

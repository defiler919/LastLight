#pragma once
#include "CoreMinimal.h"
#include "SightWeaveQueries.h"

class USightWeaveWorldSubsystem;

/** Game-thread reusable view of one immutable authority revision at one height.
 * Region proofs only skip queries where no input predicate can change. Ambiguous
 * regions always fall back to the very same Runtime EffectiveLive evaluator.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveHardCoverage
{
public:
 FSightWeaveHardCoverage(USightWeaveWorldSubsystem* Runtime,
  TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Snapshot,
  FSightWeaveKnowledgeOwnerId Owner,FSightWeaveFloorId Floor,double Height);
 bool Contains(FVector2D Point) const;
 bool TryUniform(const FBox2D& Bounds,bool& Value) const;
 void RasterizeConservative(const FBox2D& Bounds,FIntPoint Size,TArray<float>& Values) const;
 /** Presentation-only 4x4 area raster; scanline intervals skip only predicates
  * proven constant between the same boundary set used by TryUniform. */
 void RasterizeArea(const FBox2D& Bounds,FIntPoint Size,TArray<float>& Values) const;
 int64 Revision() const {return Frame->Revision.GetValue();}
 double Height() const {return Z;}
 mutable uint64 ExactQueries=0,UniformHits=0;
private:
 TWeakObjectPtr<USightWeaveWorldSubsystem> Runtime;
 TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Frame;
 FSightWeaveKnowledgeOwnerId Owner;
 FSightWeaveFloorId Floor;
 double Z=0;
 TArray<FBox2D> Boundaries;
 TArray<FBox2D> PossibleSupport;
 TArray<TPair<FVector2D,FVector2D>> Edges;
 TArray<int32> EdgePredicates,CirclePredicates;
 TArray<int32> EligibleVisions,EligibleLights;
 int32 BuildingPredicate=-1;
 double BoundaryPadding=0;
 TArray<TPair<FVector2D,double>> Circles;
 TMap<FIntPoint,TArray<int32>> BoundaryBins;
 mutable FSightWeaveVisibilityQueryResult Scratch;
 mutable TMap<FVector2D,bool> Points;
 struct FRegionKey
 {
  FVector2D Min,Max;
  bool operator==(const FRegionKey& O) const {return Min==O.Min && Max==O.Max;}
  friend uint32 GetTypeHash(const FRegionKey& K){return HashCombine(GetTypeHash(K.Min),GetTypeHash(K.Max));}
 };
 mutable TMap<FRegionKey,int8> Regions;
 void AddBoundary(FVector2D A,FVector2D B,double Padding);
 bool ContainsUncached(FVector2D Point) const;
};

/** Height partitions are derived only from predicates already present in Runtime.
 * Exact endpoints have their own plane because both neighboring closed bands
 * may contribute there. No authored floor/height rule is invented here.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveHardCoverageSet
{
public:
 FSightWeaveHardCoverageSet(USightWeaveWorldSubsystem& Runtime,
  TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Snapshot,
  FSightWeaveKnowledgeOwnerId Owner,FSightWeaveFloorId Floor);
 TSharedRef<FSightWeaveHardCoverage> AtHeight(double Z) const;
 bool IsReady() const;
 const TArray<double>& HeightCuts() const {return Cuts;}
 FBox2D CoverageBounds() const {return Bounds;}
 int64 Revision() const {return Frame->Revision.GetValue();}
private:
 TWeakObjectPtr<USightWeaveWorldSubsystem> Runtime;
 TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Frame;
 FSightWeaveKnowledgeOwnerId Owner;
 FSightWeaveFloorId Floor;
 TArray<double> Cuts;
 FBox2D Bounds=FBox2D(ForceInit);
 TArray<int32> HeightClasses;
 mutable TMap<int32,TSharedRef<FSightWeaveHardCoverage>> Planes;
};

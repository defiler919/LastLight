#pragma once
#include "CoreMinimal.h"
#include "SightWeaveGeometry.h"

// Stable native box face categories; ordering is part of the V1 receiver contract.
enum class ESightWeaveBoxFace : uint8 { NegativeX, PositiveX, NegativeY, PositiveY, Bottom, Top };

/** Explicit registration, independent of render meshes and Whole recognition.
 * V1 supports rigid transforms only; dimensions belong in HalfExtent (cm).
 * Id is unique within one world. Re-registration creates a new lifetime token. */
struct SIGHTWEAVERUNTIME_API FSightWeaveSurfaceBox
{
 FName Id;
 FSightWeaveFloorId Floor;
 FTransform Pose=FTransform::Identity;
 FVector HalfExtent=FVector(50);
 bool IsValid() const;
 bool Resolve(ESightWeaveBoxFace Face,FVector2D UV,FVector& Point,FVector& Normal) const;
};

struct FSightWeaveSurfaceSample
{
 FName Receiver;
 ESightWeaveBoxFace Face=ESightWeaveBoxFace::Top;
 FVector2D UV=FVector2D::ZeroVector; // normalized [-1,1] local face domain
 bool operator==(const FSightWeaveSurfaceSample& B) const {return Receiver==B.Receiver && Face==B.Face && UV==B.UV;}
 friend uint32 GetTypeHash(const FSightWeaveSurfaceSample& S) {return HashCombine(GetTypeHash(S.Receiver),HashCombine(uint32(S.Face),GetTypeHash(S.UV)));}
};

struct FSightWeaveSurfaceResult
{
 FSightWeaveSurfaceSample Sample;
 FSightWeaveVisibilityQueryResult Hard;
 uint64 ReceiverRevision=0; // geometry AND registration lifetime, never reused
 FVector WorldPoint=FVector::ZeroVector;
};
/** Source-restricted corner evidence; cannot borrow a different source's policy. */
struct FSightWeaveSurfaceRegionCorner
{
 FSightWeaveSurfaceSample Sample;
 FSightWeaveVisionSourceHandle Source;
 bool operator==(const FSightWeaveSurfaceRegionCorner& B) const {return Sample==B.Sample && Source==B.Source;}
 friend uint32 GetTypeHash(const FSightWeaveSurfaceRegionCorner& K){return HashCombine(GetTypeHash(K.Sample),GetTypeHash(K.Source));}
};

struct FSightWeaveSurfaceQueryStats
{
 uint64 ExactSamples=0, CacheHits=0, NodeVisits=0, PrimitiveTests=0;
};

struct FSightWeaveFrameSnapshot;
/** Caller-owned, bounded exact sample memo. Reset on ANY frame/owner change.
 * Game thread only. Never use cached results to prove an unsampled region. */
struct FSightWeaveSurfaceQueryCache
{
 TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Frame;
 FSightWeaveKnowledgeOwnerId Owner;
 TMap<FSightWeaveSurfaceSample,FSightWeaveSurfaceResult> Samples;
};

/** Immutable spatial acceleration shared across observer-only publications. */
class SIGHTWEAVERUNTIME_API FSightWeaveSurfaceScene
{
public:
 struct FReceiver {FSightWeaveSurfaceBox Box; uint64 Revision=0;};
 FSightWeaveSurfaceScene(TArray<FReceiver> Receivers,TConstArrayView<FSightWeaveSegment2D> Walls);
 const FReceiver* Find(FName Id) const;
 bool Unoccluded(FSightWeaveFloorId Floor,FVector Origin,FVector Target,FSightWeaveSurfaceQueryStats* Stats) const;
 /** Conservative beam AABB proof. False means ambiguous, never occluded. */
 bool BeamClear(FSightWeaveFloorId Floor,FName Receiver,FVector Origin,TConstArrayView<FVector> Corners) const;
 /** A single convex blocker whose shadow contains the entire target rectangle. */
 bool BeamBlocked(FSightWeaveFloorId Floor,FVector Origin,FVector Normal,TConstArrayView<FVector> Corners) const;
private:
 struct FPrimitive {FBox Bounds; int32 BoxIndex=INDEX_NONE; FSightWeaveSegment2D Wall;};
 struct FNode {FBox Bounds; int32 Left=INDEX_NONE,Right=INDEX_NONE,Primitive=INDEX_NONE;};
 TArray<FReceiver> Boxes;
 TMap<FName,int32> Ids;
 TArray<FPrimitive> Primitives;
 TArray<FNode> Nodes;
 int32 Build(TArray<int32>& Indices,int32 Begin,int32 End);
};

// Internal geometry context passed into the SAME EffectiveLive evaluator.
struct FSightWeaveSurfaceContext
{
 const FSightWeaveSurfaceScene* Scene=nullptr;
 FVector Normal=FVector::ZeroVector;
 FSightWeaveSurfaceQueryStats* Stats=nullptr;
};

#pragma once
#include "CoreMinimal.h"
#include "SightWeaveSurface.h"
class USightWeaveWorldSubsystem;

/** One explicit stationary box domain. No actor, material, Whole or XY inference.
 * Durable key = owner/floor + authored domain id + content version + face + cell.
 * The Runtime receiver token is ephemeral and is never persisted. */
struct DARKWELL_API FDarkwellSurfaceKnowledge
{
 static constexpr double SampleCm=.625; // existing 2.5cm / 4 fine support
 struct FFace {FIntPoint Size;int Offset=0;TBitArray<> Known,Live,Hidden;FIntRect Dirty=FIntRect(0,0,0,0);};
 FSightWeaveSurfaceBox Box;
 FSightWeaveKnowledgeOwnerId Owner;
 uint32 ContentVersion=1;
 TArray<FFace> Faces;
 FIntPoint AtlasSize=FIntPoint::ZeroValue;
 uint64 LastFrame=MAX_uint64,Proofs=0,ExactSamples=0;
 uint64 LastModifier=MAX_uint64;
 uint64 UnresolvedCells=0;
 FBox2D Block=FBox2D(ForceInit);
 bool bDirty=true;
 bool Initialize(const FSightWeaveSurfaceBox& InBox,FSightWeaveKnowledgeOwnerId InOwner,uint32 Version=1);
 bool Observe(USightWeaveWorldSubsystem& Runtime);
 void Clear(const FBox2D& Region);
 void SetBlock(const FBox2D& Region,bool Enabled);
 bool IsKnown(ESightWeaveBoxFace Face,FVector2D UV) const;
 bool IsLive(ESightWeaveBoxFace Face,FVector2D UV) const;
 double LiveSpanCm() const;
 void Pixels(TArray<FColor>& Out) const;
 /** Transactional import; validates schema, scope and exact domain. No Live saved. */
 void Save(TArray<uint8>& Out) const;
 bool Load(const TArray<uint8>& Data);
private:
 FVector Position(int Face,int X,int Y) const;
 int Index(int Face,FVector2D UV) const;
 void MarkDirty(int Face,int X0,int Y0,int X1,int Y1);
};

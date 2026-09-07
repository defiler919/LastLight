#include "VisionPresentation/DarkwellMemoryRegionSubsystem.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NativeGameplayTags.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/UObjectIterator.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RegionUnknown,"Darkwell.Knowledge.Unknown");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RegionRemembered,"Darkwell.Knowledge.Remembered");
FGameplayTag UDarkwellMemoryRegionSubsystem::Unknown() { return TAG_RegionUnknown; }
FGameplayTag UDarkwellMemoryRegionSubsystem::Remembered() { return TAG_RegionRemembered; }

bool UDarkwellMemoryRegionSubsystem::ValidateObjectBoundaries() const
{
 for(TActorIterator<ADarkwellObjectMemoryScene> It(GetWorld());It;++It)
  if(!It->CanApplyMemoryRegion(Bounds)) return false;
 return true;
}

bool UDarkwellMemoryRegionSubsystem::ConfigureRegion(FVector2D Min,FVector2D Max)
{
 if(Min.ContainsNaN() || Max.ContainsNaN() || Max.X<=Min.X || Max.Y<=Min.Y) return false;
 if(IsConfigured()) return Bounds.Min==Min && Bounds.Max==Max && ValidateRuntimeScope();
 // Region commands are legal only after the player's floor authority is ready.
 // Otherwise a later adapter activation could bypass an unregistered core block.
 if(!GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>()->IsActive()
  || !GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->GetExplorationMemoryScope(RuntimeRegion.Scope)) return false;
 const FVector2D Cells=(Max-Min)/2.5;
 if(Cells.X>256 || Cells.Y>256) return false;
 Bounds=FBox2D(Min,Max);
 if(!ValidateObjectBoundaries()) { Bounds=FBox2D(ForceInit); return false; }
 bHasRuntimeScope=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->GetExplorationMemoryScope(RuntimeRegion.Scope);
 RuntimeRegion.Shape=ESightWeaveMemoryRegionShape::AxisAlignedBox;
 RuntimeRegion.Center=Bounds.GetCenter(); RuntimeRegion.HalfExtents=Bounds.GetExtent();
 RuntimeRegion.HeightRange.ZMin=RuntimeRegion.Scope.FloorPlaneZ-100000;
 RuntimeRegion.HeightRange.ZMax=RuntimeRegion.Scope.FloorPlaneZ+100000;
 Size=FIntPoint(FMath::CeilToInt(Cells.X),FMath::CeilToInt(Cells.Y));
 // Preserve the frozen gray phase's explicit RememberedFromStart ground policy.
 RememberedBits.Init(true,Size.X*Size.Y);
 PresentationTexture=UTexture2D::CreateTransient(Size.X,Size.Y,PF_B8G8R8A8);
 if(!PresentationTexture) { Bounds=FBox2D(ForceInit); RememberedBits.Empty(); return false; }
 PresentationTexture->SRGB=false; PresentationTexture->Filter=TF_Nearest;
 PresentationTexture->AddressX=TA_Clamp; PresentationTexture->AddressY=TA_Clamp;
 PresentationTexture->UpdateResource();
 ++AuthorityRevision; Publish();
 return true;
}

bool UDarkwellMemoryRegionSubsystem::ClearMemory()
{
 if(!IsConfigured() || !ValidateObjectBoundaries() || !ValidateRuntimeScope()) return false;
 if(bHasRuntimeScope && !GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->ClearExplorationMemory(RuntimeRegion)) return false;
 for(TActorIterator<ADarkwellObjectMemoryScene> It(GetWorld());It;++It) It->ClearMemoryInRegion(Bounds);
 RememberedBits.Init(false,RememberedBits.Num());
 ++AuthorityRevision; Publish();
 return true;
}

bool UDarkwellMemoryRegionSubsystem::SetBlockMemoryWrites(bool bEnabled)
{
 if(!IsConfigured() || !ValidateRuntimeScope() || (bEnabled && !ValidateObjectBoundaries())) return false;
 if(bBlocked==bEnabled) return true;
 if(bHasRuntimeScope)
 {
  auto* Runtime=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>();
  if(bEnabled)
  {
   FSightWeaveMemoryModifierDescription D; D.Region=RuntimeRegion;
   RuntimeBlock=Runtime->RegisterMemoryModifier(D);
   if(!RuntimeBlock.IsValid()) return false;
  }
  else if(!Runtime->UnregisterMemoryModifier(RuntimeBlock)) return false;
 }
 // The pre-block live observation may seal its existing eligible knowledge.
 for(TActorIterator<ADarkwellObjectMemoryScene> It(GetWorld());It;++It) It->SetMemoryWriteBlock(Bounds,bEnabled);
 bBlocked=bEnabled; ++AuthorityRevision; Publish();
 return true;
}

int32 UDarkwellMemoryRegionSubsystem::IndexAt(FVector2D P) const
{
 if(!IsConfigured() || P.X<Bounds.Min.X || P.Y<Bounds.Min.Y || P.X>=Bounds.Max.X || P.Y>=Bounds.Max.Y) return INDEX_NONE;
 const auto UV=(P-Bounds.Min)/Bounds.GetSize();
 return FMath::Min(Size.Y-1,FMath::FloorToInt(UV.Y*Size.Y))*Size.X+FMath::Min(Size.X-1,FMath::FloorToInt(UV.X*Size.X));
}
bool UDarkwellMemoryRegionSubsystem::HasStoredMemory(FVector2D P) const
{
 if(P.ContainsNaN()) return false;
 const int32 I=IndexAt(P); return I==INDEX_NONE || RememberedBits[I];
}
FGameplayTag UDarkwellMemoryRegionSubsystem::QueryKnowledge(FVector2D P) const
{
 if(P.ContainsNaN()) return Unknown();
 return IndexAt(P)==INDEX_NONE || (!bBlocked && HasStoredMemory(P)) ? Remembered() : Unknown();
}

void UDarkwellMemoryRegionSubsystem::ObservePublishedCoverage(const UDarkwellFogVisualSubsystem& Fog)
{
 if(!IsConfigured()) return;
 bool Changed=false;
 if(!bBlocked && ValidateRuntimeScope())
 {
  TArray<float> Coverage; uint64 Queries=0;
  const auto Result=Fog.QueryCanonicalCoverageRaster(Bounds,Size,Coverage,Queries);
  if(Result.bValid && Coverage.Num()==RememberedBits.Num())
   for(int32 I=0;I<Coverage.Num();++I)
    if(!RememberedBits[I] && Coverage[I]>=.99f) { RememberedBits[I]=true; Changed=true; }
 }
 if(Changed) { ++AuthorityRevision; Publish(); }
 // Bind newly created surface MIDs as well; bounded slice, no performance claim.
 else for(TObjectIterator<UMeshComponent> It;It;++It)
  if(It->GetWorld()==GetWorld()) for(int32 I=0;I<It->GetNumMaterials();++I)
   BindMaterial(Cast<UMaterialInstanceDynamic>(It->GetMaterial(I)));
}

void UDarkwellMemoryRegionSubsystem::BindMaterial(UMaterialInstanceDynamic* M) const
{
 if(!M || !IsConfigured()) return;
 M->SetScalarParameterValue(TEXT("MemoryRegionEnabled"),1);
 M->SetVectorParameterValue(TEXT("MemoryRegionMin"),FLinearColor(Bounds.Min.X,Bounds.Min.Y,0,0));
 M->SetVectorParameterValue(TEXT("MemoryRegionInvExtent"),FLinearColor(1./Bounds.GetSize().X,1./Bounds.GetSize().Y,0,0));
 M->SetTextureParameterValue(TEXT("MemoryRegionKnowledge"),PresentationTexture);
}

void UDarkwellMemoryRegionSubsystem::Publish()
{
 if(PresentationTexture && PresentationTexture->GetResource())
 {
  auto* Pixels=new FColor[RememberedBits.Num()];
  for(int32 I=0;I<RememberedBits.Num();++I) Pixels[I]=!bBlocked && RememberedBits[I] ? FColor::White : FColor::Black;
  auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Size.X,Size.Y);
  PresentationTexture->UpdateTextureRegions(0,1,Region,Size.X*sizeof(FColor),sizeof(FColor),reinterpret_cast<uint8*>(Pixels),
   [](uint8* Data,const FUpdateTextureRegion2D* R){ delete[] reinterpret_cast<FColor*>(Data); delete R; });
 }
 for(TObjectIterator<UMeshComponent> It;It;++It)
  if(It->GetWorld()==GetWorld()) for(int32 I=0;I<It->GetNumMaterials();++I)
   BindMaterial(Cast<UMaterialInstanceDynamic>(It->GetMaterial(I)));
}

bool UDarkwellMemoryRegionSubsystem::ValidateRuntimeScope() const
{
 if(!bHasRuntimeScope) return true;
 FSightWeaveMemoryScopeKey Current;
 return GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->GetExplorationMemoryScope(Current)
  && Current.IsEquivalentTo(RuntimeRegion.Scope);
}

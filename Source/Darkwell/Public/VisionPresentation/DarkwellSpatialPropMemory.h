#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** Manual-lab spatial knowledge. No actors, geometry, materials or snapshot writes. */
struct DARKWELL_API FDarkwellSpatialPropMemory
{
 static constexpr float LegalCoverage=.99f;
 static constexpr float EnterSeconds=.20f;
 static constexpr float ExitSeconds=.18f;
 static constexpr float ExitHoldSeconds=1.f/30.f;
 static constexpr float EmptyConfirmationSeconds=.10f;
 static constexpr float EmptyFadeSeconds=.20f;
 struct FCell
 {
  float CurrentLegalCoverage=0;
  float DiscoveredPresent=0;
  float VerifiedEmpty=0;
  float InitialRemembered=0;
  float RemainingStale=0;
  float AppearanceBlend=0;
  float LiveBlend=0;
  float StaleOpacity=0;
  float ExitAge=0;
  float EmptyDwell=0;
 };
 void Initialize(FName InStableId,const FBox2D& InBounds,float CellSize=2.5f);
 void BeginPresent();
 void BeginAbsent();
 /** One conservative legal coverage value per fixed cell, from the existing adapter. */
 bool Advance(float DeltaSeconds,TConstArrayView<float> Coverage);
 bool IsPresent() const;
 bool IsAbsent() const;
 FName GetStableId() const { return StableId; }
 uint32 GetGeneration() const { return Generation; }
 const FBox2D& GetBounds() const { return Bounds; }
 FIntPoint GetSize() const { return Size; }
 TConstArrayView<FCell> GetCells() const { return Cells; }
 /** R: current source opacity; G: live/gray blend; B: existing proxy opacity. */
 FLinearColor Presentation(int32 Index) const;
 /** Builds a denser, inward-feathered display field without mutating D/V/R authority. */
 FIntPoint BuildConservativePresentation(int32 SamplesPerCell,TArray<FLinearColor>& OutPixels) const;
private:
 FName StableId;
 FGameplayTag ActualState;
 uint32 Generation=0;
 FBox2D Bounds;
 FIntPoint Size=FIntPoint::ZeroValue;
 TArray<FCell> Cells;
};

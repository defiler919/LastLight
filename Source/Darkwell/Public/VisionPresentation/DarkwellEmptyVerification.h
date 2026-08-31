#pragma once

#include "CoreMinimal.h"

/** Lab-only 2.5D occupied-footprint evidence. No presentation hysteresis or identity inference. */
struct DARKWELL_API FDarkwellEmptyVerification
{
 static constexpr float LegalCoverage = .99f;
 static constexpr float ConfirmationSeconds = .10f;
 static constexpr float FadeSeconds = .20f;
 static constexpr float CellSize = 10.f;
 struct FCell { float Dwell = 0; float VerifiedAt = -1; };
 FBox2D Bounds;
 FIntPoint Size = FIntPoint::ZeroValue;
 TArray<FCell> Cells;
 void Initialize(const FBox2D& InBounds);
 FBox2D CellBounds(int32 Index) const;
 /** Occupied includes any current solid furniture, regardless of StableID. */
 void Observe(float DeltaSeconds, float Seconds, TFunctionRef<float(FVector2D)> Coverage,
  TFunctionRef<bool(const FBox2D&)> Occupied);
 float VerifiedFraction() const;
 bool IsObjectEmpty() const;
 float Opacity(int32 Index, int32 Mode, float Seconds) const;
};

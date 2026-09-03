#include "VisionPresentation/DarkwellHistoricalVisibilitySweep.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"

namespace
{
	double Yaw(const FVector2D& Forward) { return FMath::Atan2(Forward.Y, Forward.X); }
	double Delta(const FDarkwellFogVisualSourceSnapshot& A, const FDarkwellFogVisualSourceSnapshot& B)
	{ return FMath::UnwindRadians(Yaw(B.ConeForward) - Yaw(A.ConeForward)); }
}

bool FDarkwellHistoricalVisibilitySweep::IsSupported(
	const FDarkwellFogVisualSourceSnapshot& A, const FDarkwellFogVisualSourceSnapshot& B)
{
	// Without input-path/occluder history a teleport, origin move, lighting change
	// or ambiguous >=170 degree turn cannot be proven. Fail closed, never guess.
	return A.IsValid() && B.IsValid() && A.bConeLegallyLive && B.bConeLegallyLive
		&& A.BodyCenter.Equals(B.BodyCenter, 1.e-4) && A.ConeOrigin.Equals(B.ConeOrigin, 1.e-4)
		&& FMath::IsNearlyEqual(A.BodyRadiusCentimeters, B.BodyRadiusCentimeters, 1.e-4f)
		&& FMath::IsNearlyEqual(A.ConeRangeCentimeters, B.ConeRangeCentimeters, 1.e-4f)
		&& FMath::IsNearlyEqual(A.ConeHalfAngleDegrees, B.ConeHalfAngleDegrees, 1.e-4f)
		&& FMath::Abs(Delta(A,B)) > 1.e-7 && FMath::Abs(Delta(A,B)) < FMath::DegreesToRadians(170.0);
}

bool FDarkwellHistoricalVisibilitySweep::MayAffectBounds(
	const FDarkwellFogVisualSourceSnapshot& A, const FDarkwellFogVisualSourceSnapshot& B,
	const FBox2D& Bounds)
{
	if (!Bounds.bIsValid || !IsSupported(A,B)) return false;
	const FVector2D Relative = Bounds.GetCenter() - A.ConeOrigin;
	const double Radius = Bounds.GetExtent().Size();
	const double Distance = Relative.Size();
	if (Distance - Radius > A.ConeRangeCentimeters) return false;
	if (Distance <= Radius) return true;
	const double Mid = Yaw(A.ConeForward) + Delta(A,B) * .5;
	const double Extent = FMath::Abs(Delta(A,B)) * .5
		+ FMath::DegreesToRadians(double(A.ConeHalfAngleDegrees)) + FMath::Asin(Radius / Distance);
	return FMath::Abs(FMath::UnwindRadians(Yaw(Relative) - Mid)) <= Extent;
}

bool FDarkwellHistoricalVisibilitySweep::ProveEmptyFootprintCoverage(
	const FDarkwellFogVisualSourceSnapshot& A, const FDarkwellFogVisualSourceSnapshot& B,
	const TConstArrayView<FDarkwellFogVisualSegment> Occluders,
	const FBox2D& Footprint, uint64& OutCoverageQueries)
{
	if (!MayAffectBounds(A,B,Footprint)) return false;
 const FVector2D Points[] = {Footprint.Min,FVector2D(Footprint.Max.X,Footprint.Min.Y),Footprint.Max,
  FVector2D(Footprint.Min.X,Footprint.Max.Y),Footprint.GetCenter()};
 return ProvePointSetCoverage(A,B,Occluders,Points,OutCoverageQueries);
}

bool FDarkwellHistoricalVisibilitySweep::MayAddIntermediateSamples(const FDarkwellFogVisualSourceSnapshot& A,
 const FDarkwellFogVisualSourceSnapshot& B,const FBox2D& Bounds)
{
 if(!MayAffectBounds(A,B,Bounds)) return false;
 const auto O=A.ConeOrigin;
 const double Distance=FVector2D::Distance(O,FVector2D(FMath::Clamp(O.X,Bounds.Min.X,Bounds.Max.X),FMath::Clamp(O.Y,Bounds.Min.Y,Bounds.Max.Y)));
 if(Distance<=4) return true;
 // Original current cells have <=2.5 cm axes and <4 cm diameter. At fixed
 // origin, their common legal angular interval has this conservative lower
 // bound. A shorter rotation cannot contain a missed complete interval.
 const double Window=2*FMath::DegreesToRadians(double(A.ConeHalfAngleDegrees))
  -2*FMath::Asin(FMath::Min(1.,1.225/Distance))-2*FMath::Asin(4./Distance);
 return FMath::Abs(Delta(A,B))>Window-1.e-6;
}

bool FDarkwellHistoricalVisibilitySweep::ProvePointSetCoverage(const FDarkwellFogVisualSourceSnapshot& A,
 const FDarkwellFogVisualSourceSnapshot& B,TConstArrayView<FDarkwellFogVisualSegment> Occluders,
 TConstArrayView<FVector2D> Points,uint64& OutCoverageQueries)
{
 if(!IsSupported(A,B) || Points.IsEmpty()) return false;
 const double Turn=Delta(A,B),Start=Yaw(A.ConeForward);
 double Low=FMath::Min(0.,Turn),High=FMath::Max(0.,Turn);
	// Same 2.5cm signed-distance transition and .99 legal threshold as FogVisual.
	// Interval intersection proves simultaneous footprint coverage, not a per-corner UNION.
	const double Margin = (FDarkwellSpatialPropMemory::LegalCoverage - .5) * 2.5;
	for (const FVector2D Point : Points)
	{
		const FVector2D Relative = Point - A.ConeOrigin;
		const double Distance = Relative.Size();
		if (Distance <= Margin || A.ConeRangeCentimeters - Distance < Margin) return false;
		const double Half = FMath::DegreesToRadians(double(A.ConeHalfAngleDegrees)) - FMath::Asin(Margin / Distance);
		if (Half <= 0) return false;
		const double Bearing = FMath::UnwindRadians(Yaw(Relative) - Start);
		bool bFound = false;
		for (const int32 Wrap : {-1,0,1})
		{
			const double Center = Bearing + Wrap * UE_TWO_PI;
			const double L = FMath::Max(Low,Center-Half), H = FMath::Min(High,Center+Half);
			if (L <= H) { Low=L;High=H;bFound=true;break; }
		}
		if (!bFound) return false;
	}
	FDarkwellFogVisualSourceSnapshot Probe = A;
	const double Angle = Start + (Low+High)*.5;
	Probe.ConeForward = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	for (const FVector2D Point : Points)
	{
		++OutCoverageQueries;
		const auto Query = FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(Probe,Point,Occluders);
		if (!Query.bValid || Query.Coverage < FDarkwellSpatialPropMemory::LegalCoverage) return false;
	}
	return true;
}

#pragma once
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

// Opt-in development evidence. No logging or clocks in Shipping.
#if !UE_BUILD_SHIPPING
namespace Darkwell::BlackoutTiming
{
struct FRow { double Inclusive=0, Exclusive=0; int32 Calls=0; };
struct FContext { int32 Depth=0; TMap<FString,FRow> Rows; };
inline thread_local FContext Context;
inline bool Enabled() { static const bool Value=FPlatformMisc::GetEnvironmentVariable(TEXT("DARKWELL_BLACKOUT_TIMING"))==TEXT("1"); return Value; }
struct FScope
{
 const TCHAR* Name; double Start=0, Child=0; FScope* Parent=nullptr;
 static inline thread_local FScope* Active=nullptr;
 FScope(const TCHAR* InName,bool Root=false):Name(InName)
 {
  if(!Enabled() || (!Root && !Active)) return;
  Parent=Active; Active=this; ++Context.Depth; Start=FPlatformTime::Seconds();
 }
 ~FScope()
 {
  if(!Start) return;
  const double Ms=(FPlatformTime::Seconds()-Start)*1000;
  auto& R=Context.Rows.FindOrAdd(Name); R.Inclusive+=Ms; R.Exclusive+=Ms-Child; ++R.Calls;
  Active=Parent; if(Parent) Parent->Child+=Ms;
  if(--Context.Depth==0)
  {
   for(const auto& Pair:Context.Rows) UE_LOG(LogTemp,Display,TEXT("BLACKOUT_TIMING frame=%llu root=%s stage=%s calls=%d inclusive_ms=%.6f exclusive_ms=%.6f"),GFrameCounter,Name,*Pair.Key,Pair.Value.Calls,Pair.Value.Inclusive,Pair.Value.Exclusive);
   Context.Rows.Reset();
  }
 }
};
inline void Count(const TCHAR* Name,int32 Value) { if(FScope::Active) Context.Rows.FindOrAdd(Name).Calls+=Value; }
}
#define DW_BLACKOUT_COUNT(Name,Value) Darkwell::BlackoutTiming::Count(TEXT(#Name),Value)
#define DW_BT_JOIN_INNER(A,B) A##B
#define DW_BT_JOIN(A,B) DW_BT_JOIN_INNER(A,B)
#define DW_BLACKOUT_SCOPE(Name) Darkwell::BlackoutTiming::FScope DW_BT_JOIN(BlackoutScope_,__LINE__)(TEXT(#Name))
#define DW_BLACKOUT_ROOT(Name) Darkwell::BlackoutTiming::FScope DW_BT_JOIN(BlackoutScope_,__LINE__)(TEXT(#Name),true)
#else
#define DW_BLACKOUT_COUNT(Name,Value)
#define DW_BLACKOUT_SCOPE(Name)
#define DW_BLACKOUT_ROOT(Name)
#endif

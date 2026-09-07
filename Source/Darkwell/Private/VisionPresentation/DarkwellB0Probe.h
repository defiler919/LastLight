#pragma once
#include "CoreMinimal.h"
class UActorComponent;
class UWorld;
namespace Darkwell::B0
{
// Diagnostic only: 0 off, 1 exclusive timing oracle, 2 same-frame batched registration.
enum ECost { Other, Pixels, Float16, Signatures, CapCPU, TextureObject, CapObject,
 ProxyObject, MeshObject, MIDObject, Register, Material, TextureSubmit, CapSubmit, Visibility, Count };
struct FScope
{
 explicit FScope(ECost Cost);
 ~FScope();
 bool Enabled=false;
 ECost Previous=Other;
};
struct FRecord
{
 explicit FRecord(bool Whole);
 ~FRecord();
 bool Enabled=false;
};
void Begin();
void Flush(UWorld* World);
void End();
void RegisterComponent(UActorComponent* Component);
FString Telemetry();
}
#define DARKWELL_B0_SCOPE(Cost) Darkwell::B0::FScope UE_JOIN(B0Scope_, __LINE__)(Darkwell::B0::Cost)
